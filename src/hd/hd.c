#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <linux/pci.h>

#include "hd.h"
#include "hddb.h"
#include "hd_int.h"
#include "memory.h"
#include "isapnp.h"
#include "monitor.h"
#include "pci.h"
#include "cpu.h"
#include "misc.h"
#include "mouse.h"
#include "floppy.h"
#include "ide.h"
#include "scsi.h"
#include "cdrom.h"
#include "bios.h"
#include "serial.h"
#include "net.h"
#include "version.h"
#include "usb.h"
#include "adb.h"
#include "modem.h"
#include "parallel.h"
#include "isa.h"
#include "dac960.h"
#include "smart.h"
#include "isdn.h"
#include "kbd.h"
#include "prom.h"
#include "sbus.h"
#include "int.h"
#include "braille.h"
#include "sys.h"
#include "dasd.h"
#include "i2o.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * various functions commmon to all probing modules
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#define LIBHD_MEMCHECK

#ifdef __i386__
#define HD_ARCH "ix86"
#endif

#ifdef __ia64__
#define HD_ARCH "ia64"
#endif

#ifdef __alpha__
#define HD_ARCH "axp"
#endif

#ifdef __PPC__
#define HD_ARCH "ppc"
#endif

#ifdef __sparc__
#define HD_ARCH "sparc"
#endif

#ifdef __s390__
#define HD_ARCH "s390"
#endif

#if defined(__s390__)
#define WITH_ISDN	0
#else
#define WITH_ISDN	1
#endif

typedef struct disk_s {
  struct disk_s *next;
  unsigned crc;
  unsigned crc_match:1;
  unsigned hd_idx;
  char *dev_name;
  unsigned data_len;
  unsigned char data[0x200];
} disk_t;

static struct s_pr_flags *get_pr_flags(enum probe_feature feature);
static void fix_probe_features(hd_data_t *hd_data);
static void set_probe_feature(hd_data_t *hd_data, enum probe_feature feature, unsigned val);
static void free_old_hd_entries(hd_data_t *hd_data);
static hd_t *add_hd_entry2(hd_t **hd, hd_t *new_hd);
static hd_res_t *get_res(hd_t *h, enum resource_types t, unsigned index);
#if WITH_ISDN
static int chk_free_biosmem(hd_data_t *hd_data, unsigned addr, unsigned len);
static isdn_parm_t *new_isdn_parm(isdn_parm_t **ip);
static driver_info_t *isdn_driver(hd_data_t *hd_data, hd_t *hd, ihw_card_info *ici);
#endif
static char *module_cmd(hd_t *, char *);
static void timeout_alarm_handler(int signal);
static void get_probe_env(hd_data_t *hd_data);
static void hd_scan_xtra(hd_data_t *hd_data);
static void hd_add_id(hd_t *hd);

#ifndef __i386__
#undef LIBHD_MEMCHECK
#endif

#ifdef LIBHD_MEMCHECK
static FILE *libhd_log = NULL;
#endif

/*
 * Names of the probing modules.
 * Cf. enum mod_idx in hd_int.h.
 */
static struct s_mod_names {
  unsigned val;
  char *name;
} pr_modules[] = {
  { mod_none, "none"},
  { mod_memory, "memory"},
  { mod_pci, "pci"},
  { mod_isapnp, "isapnp"},
  { mod_pnpdump, "pnpdump"},
  { mod_cdrom, "cdrom"},
  { mod_net, "net"},
  { mod_floppy, "floppy"},
  { mod_misc, "misc" },
  { mod_bios, "bios"},
  { mod_cpu, "cpu"},
  { mod_monitor, "monitor"},
  { mod_serial, "serial"},
  { mod_mouse, "mouse"},
  { mod_ide, "ide"},
  { mod_scsi, "scsi"},
  { mod_usb, "usb"},
  { mod_adb, "adb"},
  { mod_modem, "modem"},
  { mod_parallel, "parallel" },
  { mod_isa, "isa" },
  { mod_dac960, "dac960" },
  { mod_smart, "smart" },
  { mod_isdn, "isdn" },
  { mod_kbd, "kbd" },
  { mod_prom, "prom" },
  { mod_sbus, "sbus" },
  { mod_int, "int" },
  { mod_braille, "braille" },
  { mod_xtra, "hd" },
  { mod_sys, "sys" },
  { mod_dasd, "dasd" },
  { mod_i2o, "i2o" }
};

/*
 * Names for the probe flags. Used for debugging and command line parsing in
 * hw.c. Cf. enum probe_feature, hd_data_t.probe.
 */
static struct s_pr_flags {
  enum probe_feature val, parent;
  unsigned mask;	/* bit 0: default, bit 1: all, bit 2: max, bit 3: linuxrc */
  char *name;
} pr_flags[] = {
  { pr_default,     -1,                 1, "default"      },
  { pr_all,         -1,               2  , "all"          },
  { pr_max,         -1,             4    , "max"          },
  { pr_lxrc,        -1,           8      , "lxrc"         },
  { pr_memory,       0,           8|4|2|1, "memory"       },
  { pr_pci,          0,           8|4|2|1, "pci"          },
  { pr_pci_range,    pr_pci,        4|2  , "pci.range"    },
  { pr_pci_ext,      pr_pci,        4|2  , "pci.ext"      },
  { pr_isapnp,       0,           8|4|2|1, "isapnp"       },
  { pr_isapnp,       0,                 0, "pnpdump"      },	/* alias for isapnp */
  { pr_cdrom,        0,           8|4|2|1, "cdrom"        },
  { pr_cdrom_info,   pr_cdrom,    8|4|2|1, "cdrom.info"   },
  { pr_net,          0,           8|4|2|1, "net"          },
  { pr_floppy,       0,           8|4|2|1, "floppy"       },
  { pr_misc,         0,           8|4|2|1, "misc"         },
  { pr_misc_serial,  pr_misc,     8|4|2|1, "misc.serial"  },
  { pr_misc_par,     pr_misc,       4|2|1, "misc.par"     },
  { pr_misc_floppy,  pr_misc,     8|4|2|1, "misc.floppy"  },
  { pr_bios,         0,           8|4|2|1, "bios"         },
  { pr_cpu,          0,           8|4|2|1, "cpu"          },
  { pr_monitor,      0,           8|4|2|1, "monitor"      },
  { pr_serial,       0,             4|2|1, "serial"       },
#if defined(__sparc__)
  /* Probe for mouse on SPARC */
  { pr_mouse,        0,           8|4|2|1, "mouse"        },
#else
  { pr_mouse,        0,             4|2|1, "mouse"        },
#endif
  { pr_ide,          0,           8|4|2|1, "ide"          },
  { pr_scsi,         0,           8|4|2|1, "scsi"         },
  { pr_scsi_geo,     pr_scsi,       4|2  , "scsi.geo"     },
  { pr_usb,          0,           8|4|2|1, "usb"          },
  { pr_usb_mods,     0,             4    , "usb.mods"     },
  { pr_adb,          0,           8|4|2|1, "adb"          },
  { pr_modem,        0,             4|2|1, "modem"        },
  { pr_modem_usb,    pr_modem,      4|2|1, "modem.usb"    },
  { pr_parallel,     0,             4|2|1, "parallel"     },
  { pr_parallel_lp,  pr_parallel,   4|2|1, "parallel.lp"  },
  { pr_parallel_zip, pr_parallel,   4|2|1, "parallel.zip" },
  { pr_isa,          0,             4|2|1, "isa"          },
  { pr_isa_isdn,     pr_isa,        4|2|1, "isa.isdn"     },
  { pr_dac960,       0,           8|4|2|1, "dac960"       },
  { pr_smart,        0,           8|4|2|1, "smart"        },
  { pr_isdn,         0,             4|2|1, "isdn"         },
  { pr_kbd,          0,           8|4|2|1, "kbd"          },
  { pr_prom,         0,           8|4|2|1, "prom"         },
  { pr_sbus,         0,           8|4|2|1, "sbus"         },
  { pr_int,          0,           8|4|2|1, "int"          },
#ifdef __i386__
  { pr_braille,      0,           8|4|2|1, "braille"      },
  { pr_braille_alva, pr_braille,        0, "braille.alva" },
  { pr_braille_fhp,  pr_braille,    4|2|1, "braille.fhp"  },
  { pr_braille_ht,   pr_braille,    4|2|1, "braille.ht"   },
#else
  { pr_braille,      0,             4|2  , "braille"      },
  { pr_braille_alva, pr_braille,        0, "braille.alva" },
  { pr_braille_fhp,  pr_braille,    4|2  , "braille.fhp"  },
  { pr_braille_ht,   pr_braille,    4|2  , "braille.ht"   },
#endif
  { pr_ignx11,       0,                 0, "ignx11"       },
  { pr_sys,          0,           8|4|2|1, "sys"          },
  { pr_dasd,         0,           8|4|2|1, "dasd"         },
  { pr_i2o,          0,           8|4|2|1, "i2o"          }
};

struct s_pr_flags *get_pr_flags(enum probe_feature feature)
{
  int i;

  for(i = 0; i < sizeof pr_flags / sizeof *pr_flags; i++) {
    if(feature == pr_flags[i].val) return pr_flags + i;
  }

  return NULL;
}

void fix_probe_features(hd_data_t *hd_data)
{
  int i;

  for(i = 0; i < sizeof hd_data->probe; i++) {
    hd_data->probe[i] |= hd_data->probe_set[i];
    hd_data->probe[i] &= ~hd_data->probe_clr[i];
  }
}

void set_probe_feature(hd_data_t *hd_data, enum probe_feature feature, unsigned val)
{
  unsigned ofs, bit, mask;
  int i;
  struct s_pr_flags *pr;

  if(!(pr = get_pr_flags(feature))) return;

  if(pr->parent == -1) {
    mask = pr->mask;
    for(i = 0; i < sizeof pr_flags / sizeof *pr_flags; i++) {
      if(pr_flags[i].parent != -1 && (pr_flags[i].mask & mask))
        set_probe_feature(hd_data, pr_flags[i].val, val);
    }
  }
  else {
    ofs = feature >> 3; bit = feature & 7;
    if(ofs < sizeof hd_data->probe) {
      if(val) {
        hd_data->probe_set[ofs] |= 1 << bit;
        hd_data->probe_clr[ofs] &= ~(1 << bit);
      }
      else {
        hd_data->probe_clr[ofs] |= 1 << bit;
        hd_data->probe_set[ofs] &= ~(1 << bit);
      }
    }
    if(pr->parent) set_probe_feature(hd_data, pr->parent, val);
  }

  fix_probe_features(hd_data);
}

void hd_set_probe_feature(hd_data_t *hd_data, enum probe_feature feature)
{
  unsigned ofs, bit, mask;
  int i;
  struct s_pr_flags *pr;

  if(!(pr = get_pr_flags(feature))) return;

  if(pr->parent == -1) {
    mask = pr->mask;
    for(i = 0; i < sizeof pr_flags / sizeof *pr_flags; i++) {
      if(pr_flags[i].parent != -1 && (pr_flags[i].mask & mask))
        hd_set_probe_feature(hd_data, pr_flags[i].val);
    }
  }
  else {
    ofs = feature >> 3; bit = feature & 7;
    if(ofs < sizeof hd_data->probe)
      hd_data->probe[ofs] |= 1 << bit;
    if(pr->parent) hd_set_probe_feature(hd_data, pr->parent);
  }

  fix_probe_features(hd_data);
}

void hd_clear_probe_feature(hd_data_t *hd_data, enum probe_feature feature)
{
  unsigned ofs, bit, mask;
  int i;
  struct s_pr_flags *pr;

  if(!(pr = get_pr_flags(feature))) return;

  if(pr->parent == -1) {
    mask = pr->mask;
    for(i = 0; i < sizeof pr_flags / sizeof *pr_flags; i++) {
      if(pr_flags[i].parent != -1 && (pr_flags[i].mask & mask))
        hd_clear_probe_feature(hd_data, pr_flags[i].val);
    }
  }
  else {
    ofs = feature >> 3; bit = feature & 7;
    if(ofs < sizeof hd_data->probe)
      hd_data->probe[ofs] &= ~(1 << bit);
  }
}

int hd_probe_feature(hd_data_t *hd_data, enum probe_feature feature)
{
  if(feature < 0 || feature >= pr_default) return 0;

  return hd_data->probe[feature >> 3] & (1 << (feature & 7)) ? 1 : 0;
}


/*
 * Free all data associated with a hd_data_t struct. *Not* the struct itself.
 */
hd_data_t *hd_free_hd_data(hd_data_t *hd_data)
{
  add_hd_entry2(&hd_data->old_hd, hd_data->hd); hd_data->hd = NULL;
  hd_data->log = free_mem(hd_data->log);
  free_old_hd_entries(hd_data);		// hd_data->old_hd
  // hd_data->pci is always NULL
  // hd_data->isapnp
  hd_data->cdrom = free_str_list(hd_data->cdrom);
  hd_data->net = free_str_list(hd_data->net);
  hd_data->floppy = free_str_list(hd_data->floppy);
  hd_data->misc = free_misc(hd_data->misc);
  // hd_data->serial
  // hd_data->scsi is always NULL
  // hd_data->ser_mouse
  // hd_data->ser_modem
  hd_data->cpu = free_str_list(hd_data->cpu);
  hd_data->klog = free_str_list(hd_data->klog);
  hd_data->proc_usb = free_str_list(hd_data->proc_usb);
  // hd_data->usb

  if(hd_data->hddb_dev) {
    free_mem(hd_data->hddb_dev->data);
    free_mem(hd_data->hddb_dev->names);
    hd_data->hddb_dev = free_mem(hd_data->hddb_dev);
  }
  if(hd_data->hddb_drv) {
    free_mem(hd_data->hddb_drv->data);
    free_mem(hd_data->hddb_drv->names);
    hd_data->hddb_drv = free_mem(hd_data->hddb_drv);
  }
  hd_data->kmods = free_str_list(hd_data->kmods);
  hd_data->bios_rom = free_mem(hd_data->bios_rom);
  hd_data->bios_ram = free_mem(hd_data->bios_ram);
  hd_data->cmd_line = free_mem(hd_data->cmd_line);
  hd_data->xtra_hd = free_str_list(hd_data->xtra_hd);
  // hd_data->devtree

  return NULL;
}


/*
 * Free all data associated with a driver_info_t struct. Even the struct itself.
 */
driver_info_t *hd_free_driver_info(driver_info_t *di)
{
  driver_info_t *next;

  for(; di; di = next) {
    next = di->next;

    switch(di->any.type) {
      case di_any:
      case di_display:
        break;

      case di_module:
        free_mem(di->module.name);
        free_mem(di->module.mod_args);
        free_mem(di->module.conf);
        break;

      case di_mouse:
        free_mem(di->mouse.xf86);
        free_mem(di->mouse.gpm);
        break;

      case di_x11:
        free_mem(di->x11.server);
        free_mem(di->x11.xf86_ver);
        free_str_list(di->x11.packages);
        free_str_list(di->x11.extensions);
        free_str_list(di->x11.options);
        free_str_list(di->x11.raw);
        break;

      case di_isdn:
        free_mem(di->isdn.i4l_name);
        if(di->isdn.params) {
          isdn_parm_t *p = di->isdn.params, *next;
          for(; p; p = next) {
            next = p->next;
            free_mem(p->name);
            free_mem(p->alt_value);
            free_mem(p);
          }
        }
        break;

      case di_kbd:
        free_mem(di->kbd.XkbRules);
        free_mem(di->kbd.XkbModel);
        free_mem(di->kbd.XkbLayout);
        free_mem(di->kbd.keymap);
        break;
    }

    free_str_list(di->any.hddb0);
    free_str_list(di->any.hddb1);

    free_mem(di);
  }

  return NULL;
}


int exists_hd_entry(hd_data_t *hd_data, hd_t *hd_ex)
{
  hd_t *hd;

  if(!hd_ex) return 0;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd == hd_ex) return 1;
  }
  for(hd = hd_data->old_hd; hd; hd = hd->next) {
    if(hd == hd_ex) return 1;
  }

  return 0;
}


/*
 * Free a hd_t list. *Not* the data referred to by the hd_t structs.
 */
hd_t *hd_free_hd_list(hd_t *hd)
{
  hd_t *h;

  /* do nothing unless the list holds only copies of hd_t entries */
  for(h = hd; h; h = h->next) if(!h->ref) return NULL;

  for(; hd; hd = (h = hd)->next, free_mem(h));

  return NULL;
}


hd_t *free_hd_entry(hd_t *hd)
{
  free_mem(hd->dev_name);
  free_mem(hd->vend_name);
  free_mem(hd->sub_dev_name);
  free_mem(hd->sub_vend_name);
  free_mem(hd->rev_name);
  free_mem(hd->serial);
  free_mem(hd->unix_dev_name);
  free_mem(hd->rom_id);
  free_mem(hd->unique_id);

  free_res_list(hd->res);

  if(hd->detail) {
    switch(hd->detail->type) {
      case hd_detail_pci: {
          pci_t *p = hd->detail->pci.data;

          free_mem(p->log);
          free_mem(p);
        }
        break;

      case hd_detail_usb:
        free_mem(hd->detail->usb.data);
        break;

      case hd_detail_isapnp:
        free_mem(hd->detail->isapnp.data);
        break;

      case hd_detail_cdrom:
        free_mem(hd->detail->cdrom.data);
        break;

      case hd_detail_floppy:
        free_mem(hd->detail->floppy.data);
        break;

      case hd_detail_bios:
        free_mem(hd->detail->bios.data);
        break;

      case hd_detail_cpu:
        {
          cpu_info_t *c = hd->detail->cpu.data;

          free_mem(c->vend_name);
          free_mem(c->model_name);
          free_mem(c->platform);
          free_str_list(c->features);
          free_mem(c);
        }
        break;

      case hd_detail_prom:
        free_mem(hd->detail->prom.data);
        break;

      case hd_detail_monitor:
        free_mem(hd->detail->monitor.data);
        break;

      case hd_detail_sys:
        free_mem(hd->detail->sys.data);
        break;

      case hd_detail_scsi:
        free_scsi(hd->detail->scsi.data, 1);
        break;

      case hd_detail_devtree:
        free_mem(hd->detail->devtree.data);
        break;
    }

    free_mem(hd->detail);
  }

  memset(hd, 0, sizeof *hd);

  return NULL;
}

misc_t *free_misc(misc_t *m)
{
  int i, j;

  if(!m) return NULL;

  for(i = 0; i < m->io_len; i++) {
    free_mem(m->io[i].dev);
  }
  free_mem(m->io);

  for(i = 0; i < m->dma_len; i++) {
    free_mem(m->dma[i].dev);
  }
  free_mem(m->dma);

  for(i = 0; i < m->irq_len; i++) {
    for(j = 0; j < m->irq[i].devs; j++) {
      free_mem(m->irq[i].dev[j]);
    }
    free_mem(m->irq[i].dev);
  }
  free_mem(m->irq);

  free_str_list(m->proc_io);
  free_str_list(m->proc_dma);
  free_str_list(m->proc_irq);

  free_mem(m);

  return NULL;
}

scsi_t *free_scsi(scsi_t *scsi, int free_all)
{
  scsi_t *next;

  for(; scsi; scsi = next) {
    next = scsi->next;

    free_mem(scsi->dev_name);
    free_mem(scsi->guessed_dev_name);
    free_mem(scsi->vendor);
    free_mem(scsi->model);
    free_mem(scsi->rev);
    free_mem(scsi->type_str);
    free_mem(scsi->serial);
    free_mem(scsi->proc_dir);
    free_mem(scsi->driver);
    free_mem(scsi->info);

    if(!free_all) {
      next = scsi->next;
      memset(scsi, 0, sizeof scsi);
      scsi->next = next;
      break;
    }

    free_mem(scsi);
  }

  return NULL;
}


/*
 * Removes all hd_data->old_hd entries and frees their memory.
 */
void free_old_hd_entries(hd_data_t *hd_data)
{
  hd_t *hd, *next;

  for(hd = hd_data->old_hd; hd; hd = next) {
    next = hd->next;

    if(exists_hd_entry(hd_data, hd->ref) && hd->ref->ref_cnt) hd->ref->ref_cnt--;

    if(!hd->ref) free_hd_entry(hd);

    free_mem(hd);
  }

  hd_data->old_hd = NULL;
}


void *new_mem(size_t size)
{
  void *p;

  if(size == 0) return NULL;

  p = calloc(size, 1);

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &size)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, "%p\t%p\t0x%x\n", f, p, size);
  }
#endif

  if(p) return p;

  fprintf(stderr, "memory oops 1\n");
  exit(11);
  /*NOTREACHED*/
  return 0;
}

void *resize_mem(void *p, size_t n)
{
#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &p)[-1] - 5;
    if(libhd_log && p) fprintf(libhd_log, "%p\t%p\n", f, p);
  }
#endif

  p = realloc(p, n);

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &p)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, "%p\t%p\t0x%x\n", f, p, n);
  }
#endif

  if(!p) {
    fprintf(stderr, "memory oops 7\n");
    exit(17);
  }

  return p;
}

void *add_mem(void *p, size_t elem_size, size_t n)
{
#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &p)[-1] - 5;
    if(libhd_log && p) fprintf(libhd_log, "%p\t%p\n", f, p);
  }
#endif

  p = realloc(p, (n + 1) * elem_size);

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &p)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, "%p\t%p\t0x%x\n", f, p, (n + 1) * elem_size);
  }
#endif

  if(!p) {
    fprintf(stderr, "memory oops 7\n");
    exit(17);
  }

  memset(p + n * elem_size, 0, elem_size);

  return p;
}

char *new_str(const char *s)
{
  char *t;

  if(!s) return NULL;

  t = strdup(s);

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &s)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, "%p\t%p\t0x%x\n", f, t, strlen(t) + 1);
  }
#endif

  if(t) return t;

  fprintf(stderr, "memory oops 2\n");
  /*NOTREACHED*/
  exit(12);

  return NULL;
}

void *free_mem(void *p)
{
#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &p)[-1] - 5;
    if(libhd_log && p) fprintf(libhd_log, "%p\t%p\n", f, p);
  }
#endif

  if(p) free(p);

  return NULL;
}

void join_res_io(hd_res_t **res1, hd_res_t *res2)
{
  hd_res_t *res;

  /*
   * see if we must add an i/o range (tricky...)
   *
   * We look for identical i/o bases and add a range if one was missing. If
   * no matching pair was found, add the i/o resource.
   */
  for(; res2; res2 = res2->next) {
    if(res2->io.type == res_io) {
      for(res = *res1; res; res = res->next) {
        if(res->io.type == res_io) {
          if(res->io.base == res2->io.base) {
            /* identical bases: take maximum of both ranges */
            if(res2->io.range > res->io.range) {
              res->io.range = res2->io.range;
            }
            break;
          }
          else if(
            res->io.range &&
            res2->io.range &&
            res->io.base + res->io.range == res2->io.base)
          {
            /* res2 directly follows res1: extend res1 to cover res2 */
            res->io.range += res2->io.range;
            break;
          }
          else if(
            res2->io.base >= res->io.base &&
            res2->io.base < res->io.base + res->io.range
          ) {
            /* res2 is totally contained in res1: ignore it */
            break;
          }
        }
      }
      if(!res) {
        res = add_res_entry(res1, new_mem(sizeof *res));
        *res = *res2;	/* *copy* the struct */
        res->next = NULL;
      }
    }
  }
}

void join_res_irq(hd_res_t **res1, hd_res_t *res2)
{
  hd_res_t *res;

  /* see if we must add an dma channel */
  for(; res2; res2 = res2->next) {
    if(res2->irq.type == res_irq) {
      for(res = *res1; res; res = res->next) {
        if(res->irq.type == res_irq && res->irq.base == res2->irq.base) break;
      }
      if(!res) {
        res = add_res_entry(res1, new_mem(sizeof *res));
        *res = *res2;	/* *copy* the struct */
        res->next = NULL;
      }
    }
  }
}


void join_res_dma(hd_res_t **res1, hd_res_t *res2)
{
  hd_res_t *res;

  /* see if we must add an dma channel */
  for(; res2; res2 = res2->next) {
    if(res2->dma.type == res_dma) {
      for(res = *res1; res; res = res->next) {
        if(res->dma.type == res_dma && res->dma.base == res2->dma.base) break;
      }
      if(!res) {
        res = add_res_entry(res1, new_mem(sizeof *res));
        *res = *res2;	/* *copy* the struct */
        res->next = NULL;
      }
    }
  }
}


/*
 * Check whether both resource lists have common entries.
 */
int have_common_res(hd_res_t *res1, hd_res_t *res2)
{
  hd_res_t *res;

  for(; res1; res1 = res1->next) {
    for(res = res2; res; res = res->next) {
      if(res->any.type == res1->any.type) {
        switch(res->any.type) {
          case res_io:
            if(res->io.base == res1->io.base) return 1;
            break;

          case res_irq:
            if(res->irq.base == res1->irq.base) return 1;
            break;

          case res_dma:
            if(res->dma.base == res1->dma.base) return 1;
            break;

          default: /* gcc -Wall */
        }
      }
    }
  }

  return 0;
}


/*
 * Free the memory allocated by a resource list.
 */
hd_res_t *free_res_list(hd_res_t *res)
{
  hd_res_t *r;

  for(; res; res = (r = res)->next, free_mem(r));

  return NULL;
}



/*
 * Note: new_res is directly inserted into the list, so you *must* make sure
 * that new_res points to a malloc'ed pice of memory.
 */
hd_res_t *add_res_entry(hd_res_t **res, hd_res_t *new_res)
{
  while(*res) res = &(*res)->next;

  return *res = new_res;
}


hd_t *add_hd_entry(hd_data_t *hd_data, unsigned line, unsigned count)
{
  hd_t *hd;

  hd = add_hd_entry2(&hd_data->hd, new_mem(sizeof *hd));

  hd->idx = ++(hd_data->last_idx);
  hd->module = hd_data->module;
  hd->line = line;
  hd->count = count;

  return hd;
}


hd_t *add_hd_entry2(hd_t **hd, hd_t *new_hd)
{
  while(*hd) hd = &(*hd)->next;

  return *hd = new_hd;
}


void hd_scan(hd_data_t *hd_data)
{
  char *s = NULL;
  int i, j;
  unsigned u;
  hd_t *hd;
  uint64_t irqs;
  str_list_t *sl, *sl0;
  driver_info_t *di;

#ifdef LIBHD_MEMCHECK
  if(!libhd_log) {
    char *s = getenv("LIBHD_MEMCHECK");

    if(s && *s) {
      libhd_log = fopen(s, "w");
      if(libhd_log) setlinebuf(libhd_log);
    }
  }
#endif

  /* log the debug & probe flags */
  if(hd_data->debug && !hd_data->flags.internal) {
    ADD2LOG("libhd version %s%s (%s)\n", HD_VERSION, getuid() ? "u" : "", HD_ARCH);
  }

  /* needed only on 1st call */
  if(hd_data->last_idx == 0) {
    get_probe_env(hd_data);
  }

  fix_probe_features(hd_data);

  if(hd_data->debug && !hd_data->flags.internal) {
    for(i = sizeof hd_data->probe - 1; i >= 0; i--) {
      str_printf(&s, -1, "%02x", hd_data->probe[i]);
    }
    ADD2LOG("debug = 0x%x\nprobe = 0x%s (", hd_data->debug, s);
    s = free_mem(s);

    for(i = 1; i < pr_default; i++) {		/* 1 because of pr_memory */
      if((s = hd_probe_feature_by_value(i))) {
        ADD2LOG("%s%c%s", i == 1 ? "" : " ", hd_probe_feature(hd_data, i) ? '+' : '-', s);
      }
    }

    ADD2LOG(")\n");
  }

  /* init driver info database */
  init_hddb(hd_data);

  /*
   * for various reasons, do it befor scan_misc(); but on
   * ppc, do it after scan_prom()
   */
#ifndef __PPC__
  hd_scan_floppy(hd_data);
#endif

  /* get basic system info */
  hd_scan_misc(hd_data);

  /* start the detection  */
  hd_scan_cpu(hd_data);
  hd_scan_memory(hd_data);

  hd_scan_pci(hd_data);

#if defined(__i386__)
  hd_scan_bios(hd_data);
#endif
#if defined(__PPC__)
  hd_scan_prom(hd_data);
#endif

#ifdef __PPC__
  /* see comment above */
  hd_scan_floppy(hd_data);
#endif

  hd_scan_sys(hd_data);

  /* after hd_scan_prom() */
  hd_scan_monitor(hd_data);

#if defined(__i386__) || defined(__alpha__)
  hd_scan_isapnp(hd_data);
#endif

#if defined(__i386__)
  hd_scan_isa(hd_data);
#endif

  hd_scan_serial(hd_data);

  /* merge basic system info & the easy stuff */
  hd_scan_misc2(hd_data);

#ifndef LIBHD_TINY
  hd_scan_parallel(hd_data);	/* after hd_scan_misc*() */
#endif
  hd_scan_ide(hd_data);
  hd_scan_scsi(hd_data);
  hd_scan_dac960(hd_data);
  hd_scan_smart(hd_data);
  hd_scan_i2o(hd_data);
#ifdef __s390__
  hd_scan_dasd(hd_data);
#endif
  hd_scan_usb(hd_data);
#if defined(__PPC__)
  hd_scan_adb(hd_data);
#endif
  hd_scan_kbd(hd_data);
  hd_scan_braille(hd_data);
#ifndef LIBHD_TINY
  hd_scan_modem(hd_data);	/* do it before hd_scan_mouse() */
#endif
  hd_scan_mouse(hd_data);
  hd_scan_sbus(hd_data);

  /* keep these at the end of the list */
  hd_scan_cdrom(hd_data);
  hd_scan_net(hd_data);

  /* add test entries */
  hd_scan_xtra(hd_data);

  /* some final fixup's */
#if WITH_ISDN
  hd_scan_isdn(hd_data);
#endif
  hd_scan_int(hd_data);

  for(hd = hd_data->hd; hd; hd = hd->next) hd_add_id(hd);

  /* we are done... */
  hd_data->module = mod_none;

  if(hd_data->debug && !hd_data->flags.internal) {
    sl0 = read_file(PROC_MODULES, 0, 0);
    ADD2LOG("----- /proc/modules -----\n");
    for(sl = sl0; sl; sl = sl->next) {
      ADD2LOG("  %s", sl->str);
    }
    ADD2LOG("----- /proc/modules end -----\n");
    free_str_list(sl0);
  }

  update_irq_usage(hd_data);

  if(hd_data->debug && !hd_data->flags.internal) {
    irqs = hd_data->used_irqs;

    ADD2LOG("  used irqs:");
    for(i = j = 0; i < 64; i++, irqs >>= 1) {
      if((irqs & 1)) {
        ADD2LOG("%c%d", j ? ',' : ' ', i);
        j = 1;
      }
    }
    ADD2LOG("\n");
  }

  if(hd_data->debug && !hd_data->flags.internal) {
    i = hd_usb_support(hd_data);
    ADD2LOG("  usb support: %s\n", i == 2 ? "ohci" : i == 1 ? "uhci" : "none");
    ADD2LOG("  pcmcia support: %d\n", hd_has_pcmcia(hd_data));
    ADD2LOG("  special eide chipset: %d\n", hd_has_special_eide(hd_data));
    ADD2LOG("  apm status: %sabled\n", hd_apm_enabled(hd_data) ? "en" : "dis");
    ADD2LOG("  smp board: %s\n", hd_smp_support(hd_data) ? "yes" : "no");
    switch(hd_cpu_arch(hd_data)) {
      case arch_intel:
        s = "ia32";
        break;
      case arch_alpha:
        s = "alpha";
        break;
      case arch_sparc:
        s = "sparc";
        break;
      case arch_sparc64:
        s = "sparc64";
        break;
      case arch_ppc:
        s = "ppc";
        break;
      case arch_68k:
        s = "68k";
        break;
      case arch_s390:
        s = "s390";
        break;
      case arch_ia64:
        s = "ia64";
        break;
      default:
        s = "unknown";
    }
    ADD2LOG("  architecture: %s\n", s);
    switch(hd_boot_arch(hd_data)) {
      case boot_lilo:
        s = "lilo";
        break;
      case boot_milo:
        s = "milo";
        break;
      case boot_aboot:
        s = "aboot";
        break;
      case boot_silo:
        s = "silo";
        break;
      case boot_ppc:
        s = "ppc";
        break;
      case boot_ia64:
        s = "ia64";
        break;
      case boot_s390:
        s = "s390";
        break;
      default:
        s = "unknown";
    }
    ADD2LOG("  boot concept: %s\n", s);
    hd = hd_get_device_by_idx(hd_data, hd_boot_disk(hd_data, &i));
    s = "unknown"; if(hd && (s = hd->unix_dev_name));
    ADD2LOG("  boot device: %s (%d)\n", s, i);

    u = hd_display_adapter(hd_data);
    hd = hd_get_device_by_idx(hd_data, u);
    di = hd_driver_info(hd_data, hd);
    s = "unknown";
    if(di && di->any.type == di_x11 && di->x11.server) s = di->x11.server;
    ADD2LOG("  display adapter: #%u (%s)\n", u, s);
    hd_free_driver_info(di);
    i = hd_color(hd_data);
    if(i != -1) {
      ADD2LOG("  color code: 0x%02x\n", i);
    }
  }
}


/*
 * Note: due to byte order problems decoding the id is really a mess...
 * And, we use upper case for hex numbers!
 */
char *isa_id2str(unsigned id)
{
  char *s = new_mem(8);
  unsigned u = ((id & 0xff) << 8) + ((id >> 8) & 0xff);
  unsigned v = ((id >> 8) & 0xff00) + ((id >> 24) & 0xff);

  s[0] = ((u >> 10) & 0x1f) + 'A' - 1;
  s[1] = ((u >>  5) & 0x1f) + 'A' - 1;
  s[2] = ( u        & 0x1f) + 'A' - 1;

  sprintf(s + 3, "%04X", v);

  return s;
}

char *eisa_vendor_str(unsigned v)
{
  static char s[4];

  s[0] = ((v >> 10) & 0x1f) + 'A' - 1;
  s[1] = ((v >>  5) & 0x1f) + 'A' - 1;
  s[2] = ( v        & 0x1f) + 'A' - 1;
  s[3] = 0;

  return s;
}

unsigned name2eisa_id(char *s)
{
  int i;
  unsigned u = 0;

  for(i = 0; i < 3; i++) {
    u <<= 5;
    if(s[i] < 'A' - 1 || s[i] > 'A' - 1 + 0x1f) return 0;
    u += s[i] - 'A' + 1;
  }

  return MAKE_ID(TAG_EISA, u);
}


/*
 * Create a 'canonical' version, i.e. no spaces at start and end.
 *
 * Note: removes chars >= 0x80 as well (due to (char *))! This
 * is currently considered a feature.
 */
char *canon_str(char *s, int len)
{
  char *m2, *m1, *m0 = new_mem(len + 1);
  int i;

  for(m1 = m0, i = 0; i < len; i++) {
    if(m1 == m0 && s[i] <= ' ') continue;
    *m1++ = s[i];
  }
  *m1 = 0;
  while(m1 > m0 && m1[-1] <= ' ') {
    *--m1 = 0;
  }

  m2 = new_str(m0);
  free_mem(m0);

  return m2;
}


/*
 * Convert a n-digit hex number to its numerical value.
 */
int hex(char *s, int n)
{
  int i = 0, j;

  while(n--) {
    if(sscanf(s++, "%1x", &j) != 1) return -1;
    i = (i << 4) + j;
  }

  return i;
}


/* simple 32 bit fixed point numbers with n decimals */
int str2float(char *s, int n)
{
  int i = 0;
  int dot = 0;

  while(*s) {
    if(*s == '.') {
      if(dot++) return 0;
    }
    else if(*s >= '0' && *s <= '9') {
      if(dot) {
        if(!n) return i;
        n--;
      }
      i *= 10;
      i += *s - '0';
    }
    else {
      return 0;
    }

    s++;
  }

  while(n--) i *= 10;

  return i;
}


/* simple 32 bit fixed point numbers with n decimals */
char *float2str(int f, int n)
{
  int i = 1, j, m = n;
  static char buf[32];

  while(n--) i *= 10;

  j = f / i;
  i = f % i;

  while(i && !(i % 10)) i /= 10, m--;

  if(i) {
    sprintf(buf, "%d.%0*d", j, m, i);
  }
  else {
    sprintf(buf, "%d", j);
  }

  return buf;
}


/*
 * find hardware entry with given index
 */
hd_t *hd_get_device_by_idx(hd_data_t *hd_data, int idx)
{
  hd_t *hd;

  if(!idx) return NULL;		/* early out: idx is always != 0 */

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->idx == idx) return hd;
  }

  return NULL;
}


/*
 * Give the actual name of the probing module.
 */
char *mod_name_by_idx(unsigned idx)
{
  unsigned u;

  for(u = 0; u < sizeof pr_modules / sizeof *pr_modules; u++)
    if(idx == pr_modules[u].val) return pr_modules[u].name;

  return "";
}


/*
 * Print to a string.
 * Note: *buf must point to a malloc'd memory area (or be NULL).
 *
 * Use an offset of -1 or -2 to append the new string.
 *
 * As this function is quite often used to extend our log messages, there
 * is a cache that holds the length of the last string we created. This way
 * we speed this up somewhat. Use an offset of -2 to use this feature.
 * Note: this only works as long as str_printf() is used *exclusively* to
 * extend the string.
 */
void str_printf(char **buf, int offset, char *format, ...)
{
  static char *last_buf = NULL;
  static int last_len = 0;
  int len, use_cache;
  char b[1024];
  va_list args;

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &buf)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, ">%p\n", f);
  }
#endif

  use_cache = offset == -2 ? 1 : 0;

  if(*buf) {
    if(offset == -1) {
      offset = strlen(*buf);
    }
    else if(offset == -2) {
      if(last_buf == *buf && last_len && !(*buf)[last_len])
        offset = last_len;
      else
        offset = strlen(*buf);
    }
  }
  else {
    offset = 0;
  }

  va_start(args, format);
  vsnprintf(b, sizeof b, format, args);
  va_end(args);

  *buf = resize_mem(*buf, (len = offset + strlen(b)) + 1);
  strcpy(*buf + offset, b);

  if(use_cache) {
    last_buf = *buf;
    last_len = len;
  }

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &buf)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, "<%p\n", f);
  }
#endif
}


void hexdump(char **buf, int with_ascii, unsigned data_len, unsigned char *data)
{
  unsigned i;

  for(i = 0; i < data_len; i++) {
    if(i)
      str_printf(buf, -2, " %02x", data[i]);
    else
      str_printf(buf, -2, "%02x", data[i]);
  }

  if(with_ascii) {
    str_printf(buf, -2, "  \"");
    for(i = 0; i < data_len; i++) {
      str_printf(buf, -2, "%c", data[i] < ' ' || data[i] >= 0x7f ? '.' : data[i]);
    }
    str_printf(buf, -2, "\"");
  }
}


/*
 * Search a string list for a string.
 */
str_list_t *search_str_list(str_list_t *sl, char *str)
{
  if(!str) return NULL;

  for(; sl; sl = sl->next) if(!strcmp(sl->str, str)) return sl;

  return NULL;
}


/*
 * Add a string to a string list; just for convenience.
 *
 * The new string (str) will be *copied*!
 */
str_list_t *add_str_list(str_list_t **sl, char *str)
{
#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &sl)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, ">%p\n", f);
  }
#endif

  while(*sl) sl = &(*sl)->next;

  *sl = new_mem(sizeof **sl);
  (*sl)->str = new_str(str);

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &sl)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, "<%p\n", f);
  }
#endif

  return *sl;
}


/*
 * Free the memory allocated by a string list.
 */
str_list_t *free_str_list(str_list_t *list)
{
  str_list_t *l;

  for(; list; list = (l = list)->next, free_mem(l)) {
    free_mem(list->str);
  }

  return NULL;
}


/*
 * Read a file; return a linked list of lines.
 *
 * start_line is zero-based; lines == 0 -> all lines
 */
str_list_t *read_file(char *file_name, unsigned start_line, unsigned lines)
{
  FILE *f;
  char buf[1024];
  int pipe = 0;
  str_list_t *sl_start = NULL, *sl_end = NULL, *sl;

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &file_name)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, ">%p\n", f);
  }
#endif

  if(*file_name == '|') {
    pipe = 1;
    file_name++;
    if(!(f = popen(file_name, "r"))) {
#ifdef LIBHD_MEMCHECK
      {
        void *f = (void *) ((unsigned *) &file_name)[-1] - 5;
        if(libhd_log) fprintf(libhd_log, "<%p\n", f);
      }
#endif
      return NULL;
    }
  }
  else {
    if(!(f = fopen(file_name, "r"))) {
#ifdef LIBHD_MEMCHECK
      {
        void *f = (void *) ((unsigned *) &file_name)[-1] - 5;
        if(libhd_log) fprintf(libhd_log, "<%p\n", f);
      }
#endif
      return NULL;
    }
  }

  while(fgets(buf, sizeof buf, f)) {
    if(start_line) {
      start_line--;
      continue;
    }
    sl = new_mem(sizeof *sl);
    sl->str = new_str(buf);
    if(sl_start)
      sl_end->next = sl;
    else
      sl_start = sl;
    sl_end = sl;

    if(lines == 1) break;
    lines--;
  }

  if(pipe)
    pclose(f);
  else
    fclose(f);

#ifdef LIBHD_MEMCHECK
  {
    void *f = (void *) ((unsigned *) &file_name)[-1] - 5;
    if(libhd_log) fprintf(libhd_log, "<%p\n", f);
  }
#endif

  return sl_start;
}


/*
 * Log the hardware detection progress.
 */
void progress(hd_data_t *hd_data, unsigned pos, unsigned count, char *msg)
{
  char buf1[32], buf2[32], buf3[128], *fn;

  if(!msg) msg = "";

  sprintf(buf1, "%u", hd_data->module);
  sprintf(buf2, ".%u", count);
  fn = mod_name_by_idx(hd_data->module);

  sprintf(buf3, "%s.%u%s", *fn ? fn : buf1, pos, count ? buf2 : "");

  if((hd_data->debug & HD_DEB_PROGRESS))
    ADD2LOG(">> %s: %s\n", buf3, msg);

  if(hd_data->progress) hd_data->progress(buf3, msg);
}



/*
 * Returns a probe feature suitable for hd_*probe_feature().
 * If name is not a valid probe feature, 0 is returned.
 *
 */
enum probe_feature hd_probe_feature_by_name(char *name)
{
  int u;

  for(u = 0; u < sizeof pr_flags / sizeof *pr_flags; u++)
    if(!strcmp(name, pr_flags[u].name)) return pr_flags[u].val;

  return 0;
}


/*
 * Coverts a enum probe_feature to a string.
 * If it fails, NULL is returned.
 */
char *hd_probe_feature_by_value(enum probe_feature feature)
{
  int u;

  for(u = 0; u < sizeof pr_flags / sizeof *pr_flags; u++)
    if(feature == pr_flags[u].val) return pr_flags[u].name;

  return NULL;
}


/*
 * Removes all hd_data->hd entries created by the current module from the
 * list. The old entries are added to hd_data->old_hd.
 */
void remove_hd_entries(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->module == hd_data->module) {
      hd->tag.remove = 1;
    }
  }

  remove_tagged_hd_entries(hd_data);
}


/*
 * Removes all hd_data->hd entries that have the remove tag set from the
 * list. The old entries are added to hd_data->old_hd.
 */
void remove_tagged_hd_entries(hd_data_t *hd_data)
{
  hd_t *hd, **prev, **h;

  for(hd = *(prev = &hd_data->hd); hd;) {
    if(hd->tag.remove) {
      /* find end of the old list... */
      h = &hd_data->old_hd;
      while(*h) h = &(*h)->next;
      *h = hd;		/* ...and append the entry */

      hd = *prev = hd->next;
      (*h)->next = NULL;
    }
    else {
      hd = *(prev = &hd->next);
    }
  }
}

#if WITH_ISDN
int chk_free_biosmem(hd_data_t *hd_data, unsigned addr, unsigned len)
{
  unsigned u;
  unsigned char c;

  addr -= BIOS_ROM_START;
  if(
    !hd_data->bios_rom ||
    addr > BIOS_ROM_SIZE ||
    addr + len > BIOS_ROM_SIZE
  ) return 0;

  for(c = 0xff, u = addr; u < addr + len; u++) {
    c &= hd_data->bios_rom[u];
  }

  return c == 0xff ? 1 : 0;
}

isdn_parm_t *new_isdn_parm(isdn_parm_t **ip)
{
  while(*ip) ip = &(*ip)->next;

  return *ip = new_mem(sizeof **ip);
}
#endif		/* WITH_ISDN */

driver_info_t *kbd_driver(hd_data_t *hd_data, hd_t *hd)
{
  driver_info_t *di;
  driver_info_kbd_t *ki;
  int arch = hd_cpu_arch(hd_data);
  unsigned u;
  char *s1, *s2;
  hd_t *hd_tmp;

  if(hd->sub_class == sc_keyboard_console) return NULL;

  di = new_mem(sizeof *di);
  di->kbd.type = di_kbd;
  ki = &(di->kbd);

  ki->XkbRules = new_str(arch == arch_sparc || arch == arch_sparc64 ? "sun" : "xfree86");

  switch(arch) {
    case arch_intel:
    case arch_alpha:
      ki->XkbRules = new_str("xfree86");
      ki->XkbModel = new_str("pc104");
      break;
    case arch_ppc:
      ki->XkbRules = new_str("xfree86");
      ki->XkbModel = new_str("macintosh");
      for(hd_tmp = hd_data->hd; hd_tmp; hd_tmp = hd_tmp->next) {
        if(hd_tmp->base_class == bc_internal && hd_tmp->sub_class == sc_int_cpu) {
          s1 = hd_tmp->detail->cpu.data->vend_name;
          if(s1 && (strstr(s1, "CHRP ") == s1 || strstr(s1, "PReP ") == s1)) {
            ki->XkbModel = new_str("powerpcps2");
          }
        }
      }
      break;
    case arch_sparc:
    case arch_sparc64:
      if(hd->vend == MAKE_ID(TAG_SPECIAL, 0x0202)) {
        ki->XkbRules = new_str("sun");
        u = ID_VALUE(hd->dev);
        if(u == 4) ki->XkbModel = new_str("type4");
        if(u == 5) {
          ki->XkbModel = new_str(ID_VALUE(hd->sub_dev) == 2 ? "type5_euro" : "type5");
        }
        s1 = s2 = NULL;
        switch(hd->prog_if) {
          case  2:
            s1 = "fr"; s2 = "sunt5-fr-latin1";		// fr_BE?
            break;

          case  3:
            s1 = "ca";
            break;

          case  4: case 36: case 83:
            s1 = "dk";
            break;

          case  5: case 37: case 84:
            s1 = "de"; s2 = "sunt5-de-latin1";
            break;

          case  6: case 38: case 85:
            s1 = "it";
            break;

          case  7: case 39: case 86:
            s1 = "nl";
            break;

          case  8: case 40: case 87:
            s1 = "no";
            if(u == 4) s2 = "sunt4-no-latin1";
            break;

          case  9: case 41: case 88:
            s1 = "pt";
            break;

          case 10: case 42: case 89:
            s1 = "es";
            s2 = u == 4 ? "sunt4-es" : "sunt5-es";
            break;

          case 11: case 43: case 90:
            s1 = "se"; s2 = "sunt5-fi-latin1";		// se == fi???
            break;

          case 12: case 44: case 91:
            s1 = "fr"; s2 = "sunt5-fr-latin1";		// fr_CH
            break;

          case 13: case 45: case 92:
            s1 = "de"; s2 = "sunt5-de-latin1";		// de_CH
            break;

          case 14: case 46: case 93:
            s1 = "us"; s2 = "sunkeymap";		// en_US
            break;

          case 16: case 47: case 94:
            // korea???
            break;

          case 17: case 48: case 95:
            s1 = "tw";
            break;

          case 32: case 49: case 96:
            s1 = "jp";
            break;

          case 50: case 97:
            s1 = "fr"; s2 = "sunt5-fr-latin1";		// fr_CA
            break;

          case 51:
            s1 = "hu";
            break;

          case 52:
            s1 = "pl"; s2 = "sun-pl";
            break;

          case 53:
            s1 = "cs";
            break;

          case 54:
            s1 = "ru"; s2 = "sunt5-ru";
            break;

          case  0: case  1: case 33: case 34:
          default:
            s1 = "us"; s2 = "sunkeymap";
            break;
        }
        ki->XkbLayout = new_str(s1);
        ki->keymap = new_str(s2);
      }
      else
	{
	  ki->XkbRules = new_str ("xfree86");
	  ki->XkbModel = new_str ("pc104");
	}
      break;
  }

  return di;
}


#if WITH_ISDN
driver_info_t *isdn_driver(hd_data_t *hd_data, hd_t *hd, ihw_card_info *ici)
{
  driver_info_t *di;
  ihw_para_info *ipi0, *ipi;
  isdn_parm_t *ip;
  hd_res_t *res;
  uint64_t irqs, irqs2;
  int i, irq_val;

  di = new_mem(sizeof *di);
  ipi0 = new_mem(sizeof *ipi0);

  di->isdn.type = di_isdn;
  di->isdn.i4l_type = ici->type;
  di->isdn.i4l_subtype = ici->subtype;
  di->isdn.i4l_name = new_str(ici->name);

  if(hd->bus == bus_pci) return di;

  ipi0->handle = -1;
  while((ipi = ihw_get_parameter(ici->handle, ipi0))) {
    ip = new_isdn_parm(&di->isdn.params);
    ip->name = new_str(ipi->name);
    ip->type = ipi->type & P_TYPE_MASK;
    ip->flags = ipi->flags & P_PROPERTY_MASK;
    ip->def_value = ipi->def_value;
    if(ipi->list) ip->alt_values = *ipi->list;
    ip->alt_value = new_mem(ip->alt_values * sizeof *ip->alt_value);
    for(i = 0; i < ip->alt_values; i++) {
      ip->alt_value[i] = ipi->list[i + 1];
    }
    ip->valid = 1;

    if((ip->flags & P_SOFTSET)) {
      switch(ip->type) {
        case P_IRQ:
          update_irq_usage(hd_data);
          irqs = 0;
          for(i = 0; i < ip->alt_values; i++) {
            irqs |= 1ull << ip->alt_value[i];
          }
          irqs &= ~(hd_data->used_irqs | hd_data->assigned_irqs);
#ifdef __i386__
          irqs &= 0xffffull;	/* max. 16 on intel */
          /*
           * The point is, that this is relevant for isa boards only
           * and those have irq values < 16 anyway. So it really
           * doesn't matter if we mask with 0xffff or not.
           */
#endif
          if(!irqs) {
            ip->conflict = 1;
            ip->valid = 0;
          }
          else {
            irqs2 = irqs & ~0xc018ull;
            /* see if we can avoid irqs 3,4,14,15 */
            if(irqs2) irqs = irqs2;
            irq_val = -1;
            /* try default value first */
            if(ip->def_value && (irqs & (1ull << ip->def_value))) {
              irq_val = ip->def_value;
            }
            else {
              for(i = 0; i < 64 && irqs; i++, irqs >>= 1) {
                if((irqs & 1)) irq_val = i;
              }
            }
            if(irq_val >= 0) {
              ip->value = irq_val;
              hd_data->assigned_irqs |= 1ull << irq_val;
            }
            else {
              ip->valid = 0;
            }
          }
          break;
        case P_MEM:
          if(!hd_data->bios_rom) {
            if(ip->def_value) {
              ip->value = ip->def_value;
            }
          }
          else {
            /* ###### 0x2000 is just guessing -> should be provided by libihw */
            if(ip->def_value && chk_free_biosmem(hd_data, ip->def_value, 0x2000)) {
              ip->value = ip->def_value;
            }
            else {
              for(i = ip->alt_values - 1; i >= 0; i--) {
                if(chk_free_biosmem(hd_data, ip->alt_value[i], 0x2000)) {
                  ip->value = ip->alt_value[i];
                  break;
                }
              }
            }
          }
          if(!ip->value) ip->conflict = 1;
          break;
        default:
          ip->valid = 0;
      }
    }
    else if((ip->flags & P_DEFINE)) {
      res = NULL;
      switch(ip->type) {
        case P_IRQ:
          res = get_res(hd, res_irq, 0);
          if(res) ip->value = res->irq.base;
          break;
        case P_MEM:
          res = get_res(hd, res_mem, 0);
          if(res) ip->value = res->mem.base;
          break;
        case P_IO:
          res = get_res(hd, res_io, 0);
          if(res) ip->value = res->io.base;
          break;
        case P_IO0:
        case P_IO1:
        case P_IO2:
          res = get_res(hd, res_io, ip->type - P_IO0);
          if(res) ip->value = res->io.base;
          break;
        // ##### might break for 64bit pci entries?
        case P_BASE0:
        case P_BASE1:
        case P_BASE2:
        case P_BASE3:
        case P_BASE4:
        case P_BASE5:
          res = get_res(hd, res_mem, ip->type - P_BASE0);
          if(res) ip->value = res->mem.base;
          break;
        default:
          ip->valid = 0;
      }
      if(!res) ip->valid = 0;
    }

  }

  return di;
}
#endif		/* WITH_ISDN */

driver_info_t *monitor_driver(hd_data_t *hd_data, hd_t *hd)
{
  driver_info_t *di = NULL;
  driver_info_display_t *ddi;
  monitor_info_t *mi;
  hd_res_t *res;
  unsigned width = 640, height = 480;

  if(
    hd->detail &&
    hd->detail->type == hd_detail_monitor &&
    (mi = hd->detail->monitor.data) &&
    mi->min_hsync
  ) {
    di = new_mem(sizeof *di);
    di->display.type = di_display;
    ddi = &(di->display);

    ddi->min_vsync = mi->min_vsync;
    ddi->max_vsync = mi->max_vsync;
    ddi->min_hsync = mi->min_hsync;
    ddi->max_hsync = mi->max_hsync;

    for(res = hd->res; res; res = res->next) {
      if(res->any.type == res_monitor) {
        if(res->monitor.width * res->monitor.height > width * height ) {
          width = res->monitor.width;
          height = res->monitor.height;
        }
      }
    }

    ddi->width = width;
    ddi->height = height;
  }

  return di;
}

driver_info_t *reorder_x11(driver_info_t *di0, char *info)
{
  driver_info_t *di, *di_new, **di_list;
  int i, dis, found;

  for(dis = 0, di = di0; di; di = di->next) dis++;

  di_list = new_mem(dis * sizeof *di_list);

  for(i = 0, di = di0; di; di = di->next) {
    di_list[i++] = di;
  }

  di = di_new = NULL;
  for(i = found = 0; i < dis; i++) {
    if(
      !strcmp(di_list[i]->x11.xf86_ver, info) ||
      !strcmp(di_list[i]->x11.server, info)
    ) {
      found = 1;
      if(di) {
        di = di->next = di_list[i];
      }
      else {
        di = di_new = di_list[i];
      }
      di->next = NULL;
      di_list[i] = NULL;
    }
  }

  for(i = 0; i < dis; i++) {
    if(di_list[i]) {
      if(di) {
        di = di->next = di_list[i];
      }
      else {
        di = di_new = di_list[i];
      }
      di->next = NULL;
      di_list[i] = NULL;
    }
  }

  free_mem(di_list);

  if(!found && strlen(info) > 1) {
    hd_free_driver_info(di_new);
    di_new = new_mem(sizeof *di_new);
    di_new->any.type = di_x11;
    di_new->x11.server = new_str(info);
    di_new->x11.xf86_ver = new_str(*info >= 'A' && *info <= 'Z' ? "3" : "4");
  }

  return di_new;
}


/*
 * Reads the driver info.
 *
 * If the driver is a module, checks if the module is already loaded.
 *
 * If the command line returned is the empty string (""), we could not
 * figure out what to do.
 */
driver_info_t *hd_driver_info(hd_data_t *hd_data, hd_t *hd)
{
  int i;
  unsigned u1, u2;
  char *s, *t, *t0;
  driver_info_t *di, *di0 = NULL;
#if WITH_ISDN
  ihw_card_info *ici;
#endif
  str_list_t *sl;
  hd_t *bridge_hd;

  if(!hd) return NULL;

  /* ignore card bus cards */
  if((bridge_hd = hd_get_device_by_idx(hd_data, hd->attached_to))) {
    if(
      bridge_hd->base_class == bc_bridge &&
      bridge_hd->sub_class == sc_bridge_cardbus
    ) return NULL;
  }

  if(hd->sub_vend || hd->sub_dev) {
    di0 = sub_device_driver(hd_data, hd->vend, hd->dev, hd->sub_vend, hd->sub_dev);
  }

  if(!di0 && (hd->vend || hd->dev)) {
    di0 = device_driver(hd_data, hd->vend, hd->dev);

//    fprintf(stderr, "\n\n******* %p 0x%x 0x%x\n\n", di0, hd->vend, hd->dev);

  }

  if(!di0 && (hd->compat_vend || hd->compat_dev)) {
    di0 = device_driver(hd_data, hd->compat_vend, hd->compat_dev);
  }

#if WITH_ISDN
  if((ici = get_isdn_info(hd))) {
    di0 = isdn_driver(hd_data, hd, ici);
    if(di0) return di0;
  }
#endif

  if(hd->base_class == bc_keyboard) {
    di0 = kbd_driver(hd_data, hd);
    if(di0) return di0;
  }

  if(!di0 && hd->base_class == bc_monitor) {
    di0 = monitor_driver(hd_data, hd);
    if(di0) return di0;
  }

  if(!di0) return hd_free_driver_info(di0);

  for(di = di0; di; di = di->next) {
    switch(di->any.type) {
      case di_display:
        for(i = 0, sl = di->display.hddb0; sl; sl = sl->next, i++) {
          if(i == 0 && sscanf(sl->str, "%ux%u", &u1, &u2) == 2) {
            di->display.width = u1;
            di->display.height = u2;
          }
          else if(i == 1 && sscanf(sl->str, "%u-%u", &u1, &u2) == 2) {
            di->display.min_vsync = u1;
            di->display.max_vsync = u2;
          }
          else if(i == 2 && sscanf(sl->str, "%u-%u", &u1, &u2) == 2) {
            di->display.min_hsync = u1;
            di->display.max_hsync = u2;
          }
          else if(i == 3 && sscanf(sl->str, "%u", &u1) == 1) {
            di->display.bandwidth = u1;
          }
        }
        break;

      case di_module:
        for(i = 0, sl = di->module.hddb0; sl; sl = sl->next, i++) {
          if(i == 0) {
            di->module.name = new_str(sl->str);
            di->module.active = hd_module_is_active(hd_data, di->module.name);
          }
          else if(i == 1) {
            di->module.mod_args = new_str(module_cmd(hd, sl->str));
          }
        }
        for(i = 0, sl = di->module.hddb1; sl; sl = sl->next, i++) {
          str_printf(&di->module.conf, -1, "%s\n", module_cmd(hd, sl->str));
        }
        break;

      case di_mouse:
        for(i = 0, sl = di->mouse.hddb0; sl; sl = sl->next, i++) {
          if(i == 0) {
            di->mouse.xf86 = new_str(sl->str);
          }
          else if(i == 1) {
            di->mouse.gpm = new_str(sl->str);
          }
        }
        break;

      case di_x11:
        for(i = 0, sl = di->x11.hddb0; sl; sl = sl->next, i++) {
          if(i == 0) {
            di->x11.xf86_ver = new_str(sl->str);
          }
          else if(i == 1) {
            di->x11.server = new_str(sl->str);
          }
          else if(i == 2) {
            if(!strcmp(sl->str, "3d")) di->x11.x3d = 1;
          }
          else if(i == 3) {
            s = new_str(sl->str);
            for(t0 = s; (t = strsep(&t0, ",")); ) {
              add_str_list(&di->x11.packages, t);
            }
            free_mem(s);
          }
          else if(i == 4) {
            s = new_str(sl->str);
            for(t0 = s; (t = strsep(&t0, ",")); ) {
              add_str_list(&di->x11.extensions, t);
            }
            free_mem(s);
          }
          else if(i == 5) {
            s = new_str(sl->str);
            for(t0 = s; (t = strsep(&t0, ",")); ) {
              add_str_list(&di->x11.options, t);
            }
            free_mem(s);
          }
          else if(i == 6) {
            di->x11.colors.all = strtol(sl->str, NULL, 16);
            if(di->x11.colors.all & (1 << 0)) di->x11.colors.c8 = 1;
            if(di->x11.colors.all & (1 << 1)) di->x11.colors.c15 = 1;
            if(di->x11.colors.all & (1 << 2)) di->x11.colors.c16 = 1;
            if(di->x11.colors.all & (1 << 3)) di->x11.colors.c24 = 1;
            if(di->x11.colors.all & (1 << 4)) di->x11.colors.c32 = 1;
          }
          else if(i == 7) {
            di->x11.dacspeed = strtol(sl->str, NULL, 10);
          }
        }
        for(i = 0, sl = di->x11.hddb1; sl; sl = sl->next, i++) {
          add_str_list(&di->x11.raw, sl->str);
        }
        break;

      default:
        break;
    }
  }

  if(di0 && di0->any.type == di_x11 && !hd_probe_feature(hd_data, pr_ignx11)) {
    s = get_cmdline(hd_data, "x11");
    if(s && *s) {
      di0 = reorder_x11(di0, s);
    }
    free_mem(s);
  }

  return di0;
}


int hd_module_is_active(hd_data_t *hd_data, char *mod)
{
  str_list_t *sl, *sl0 = read_kmods(hd_data);
  int active = 0;
#ifdef __PPC__
  char *s1, *s2;
#endif

  for(sl = sl0; sl; sl = sl->next) {
    if(!strcmp(sl->str, mod)) break;
  }

  free_str_list(sl0);
  active = sl ? 1 : 0;

  if(active) return active;

#ifdef __PPC__
  /* temporary hack for ppc */
  if(!strcmp(mod, "gmac")) {
    s1 = "<6>eth";
    s2 = " GMAC ";
  }
  else if(!strcmp(mod, "mace")) {
    s1 = "<6>eth";
    s2 = " MACE ";
  }
  else if(!strcmp(mod, "bmac")) {
    s1 = "<6>eth";
    s2 = " BMAC";
  }
  else if(!strcmp(mod, "mac53c94")) {
    s1 = "<4>scsi";
    s2 = " 53C94";
  }
  else if(!strcmp(mod, "mesh")) {
    s1 = "<4>scsi";
    s2 = " MESH";
  }
  else if(!strcmp(mod, "swim3")) {
    s1 = "<6>fd";
    s2 = " SWIM3 ";
  }
  else {
    s1 = s2 = NULL;
  }

  if(s1) {
    for(sl = hd_data->klog; sl; sl = sl->next) {
      if(strstr(sl->str, s1) == sl->str && strstr(sl->str, s2)) {
        active = 1;
        break;
      }
    }
  }
#endif

  return active;
}

hd_res_t *get_res(hd_t *hd, enum resource_types t, unsigned index)
{
  hd_res_t *res;

  for(res = hd->res; res; res = res->next) {
    if(res->any.type == t) {
      if(!index) return res;
      index--;
    }
  }

  return NULL;
}


char *module_cmd(hd_t *hd, char *cmd)
{
  static char buf[256];
  char *s = buf;
  int idx, ofs;
  hd_res_t *res;

  // skip inactive PnP cards
  // ##### Really necessary here?
  if(
    hd->detail &&
    hd->detail->type == hd_detail_isapnp &&
    !(hd->detail->isapnp.data->flags & (1 << isapnp_flag_act))
  ) return NULL;

  *buf = 0;
  while(*cmd) {
    if(sscanf(cmd, "<io%u>%n", &idx, &ofs) >= 1) {
      if((res = get_res(hd, res_io, idx))) {
        s += sprintf(s, "0x%02"PRIx64, res->io.base);
        cmd += ofs;
      }
      else {
        return NULL;
      }
    }
    else if(sscanf(cmd, "<irq%u>%n", &idx, &ofs) >= 1) {
      if((res = get_res(hd, res_irq, idx))) {
        s += sprintf(s, "%u", res->irq.base);
        cmd += ofs;
      }
      else {
        return NULL;
      }
    }
    else {
      *s++ = *cmd++;
    }

    if(s - buf > sizeof buf - 20) return NULL;
  }

  *s = 0;
  return buf;
}


/*
 * cf. /usr/src/linux/drivers/block/ide-pci.c
 */
#ifndef PCI_DEVICE_ID_ARTOP_ATP860
#define PCI_DEVICE_ID_ARTOP_ATP860	0x0006
#endif

#ifndef PCI_DEVICE_ID_ARTOP_ATP860R
#define PCI_DEVICE_ID_ARTOP_ATP860R	0x0007
#endif

#ifndef PCI_DEVICE_ID_CMD_649
#define PCI_DEVICE_ID_CMD_649		0x0649
#endif

#ifndef PCI_DEVICE_ID_PROMISE_20267
#define PCI_DEVICE_ID_PROMISE_20267	0x4d30
#endif

#ifndef PCI_DEVICE_ID_INTEL_82443MX_1
#define PCI_DEVICE_ID_INTEL_82443MX_1	0x7199
#endif

#ifndef PCI_DEVICE_ID_INTEL_82372FB_1
#define PCI_DEVICE_ID_INTEL_82372FB_1	0x7601
#endif

#ifndef PCI_DEVICE_ID_INTEL_82820FW_5
#define PCI_DEVICE_ID_INTEL_82820FW_5	0x244b
#endif

#ifndef PCI_DEVICE_ID_AMD_VIPER_7409
#define PCI_DEVICE_ID_AMD_VIPER_7409	0x7409
#endif

#ifndef PCI_DEVICE_ID_CMD_648
#define PCI_DEVICE_ID_CMD_648		0x0648
#endif

#ifndef PCI_DEVICE_ID_TTI_HPT366
#define PCI_DEVICE_ID_TTI_HPT366	0x0004
#endif

#ifndef PCI_DEVICE_ID_PROMISE_20262
#define PCI_DEVICE_ID_PROMISE_20262	0x4d38
#endif

int hd_has_special_eide(hd_data_t *hd_data)
{
  int i;
  hd_t *hd;
  static unsigned ids[][2] = {
/* CONFIG_BLK_DEV_AEC62XX */
    { PCI_VENDOR_ID_ARTOP, PCI_DEVICE_ID_ARTOP_ATP850UF },
    { PCI_VENDOR_ID_ARTOP, PCI_DEVICE_ID_ARTOP_ATP860 },
    { PCI_VENDOR_ID_ARTOP, PCI_DEVICE_ID_ARTOP_ATP860R },
/* CONFIG_BLK_DEV_ALI15X3 */
    { PCI_VENDOR_ID_AL, PCI_DEVICE_ID_AL_M5229 },
/* CONFIG_BLK_DEV_AMD7409 */
    { PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VIPER_7409 },
/* CONFIG_BLK_DEV_CMD64X */
    { PCI_VENDOR_ID_CMD, PCI_DEVICE_ID_CMD_643 },
    { PCI_VENDOR_ID_CMD, PCI_DEVICE_ID_CMD_646 },
    { PCI_VENDOR_ID_CMD, PCI_DEVICE_ID_CMD_648 },
    { PCI_VENDOR_ID_CMD, PCI_DEVICE_ID_CMD_649 },
/* CONFIG_BLK_DEV_CY82C693 */
    { PCI_VENDOR_ID_CONTAQ, PCI_DEVICE_ID_CONTAQ_82C693 },
/* CONFIG_BLK_DEV_CS5530 */
    { PCI_VENDOR_ID_CYRIX, PCI_DEVICE_ID_CYRIX_5530_IDE },
/* CONFIG_BLK_DEV_HPT34X */
    { PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT343 },
/* CONFIG_BLK_DEV_HPT366 */
    { PCI_VENDOR_ID_TTI, PCI_DEVICE_ID_TTI_HPT366 },
/* CONFIG_BLK_DEV_NS87415 */
    { PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_87410 },
    { PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_87415 },
/* CONFIG_BLK_DEV_OPTI621 */
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C621 },
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C558 },
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C825 },
#if 0
/* CONFIG_BLK_DEV_PIIX */
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371FB_0 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371FB_1 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_1 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801AB_1 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82443MX_1 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801AA_1 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82372FB_1 },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82451NX },
    { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82820FW_5 },
#endif
/* CONFIG_BLK_DEV_PDC202XX */
    { PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20246 },
    { PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20262 },
    { PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20267 },
/* CONFIG_BLK_DEV_SIS5513 */
    { PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_5513 },
/* CONFIG_BLK_DEV_TRM290 */
    { PCI_VENDOR_ID_TEKRAM, PCI_DEVICE_ID_TEKRAM_DC290 },
/* CONFIG_BLK_DEV_VIA82CXXX */
    { PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C561 },
    { PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_1 }
  };

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->bus == bus_pci) {
      for(i = 0; i < sizeof ids / sizeof *ids; i++) {
        if(hd->vend == ids[i][0] && hd->dev == ids[i][1]) return 1;
      }
    }
  }

  return 0;
}

/*
 * cf. pcmcia-cs-*:cardmgr/probe.c
 */
int hd_has_pcmcia(hd_data_t *hd_data)
{
  int i;
  hd_t *hd;
  static unsigned ids[][2] = {
    { 0x1013, 0x1100 },
    { 0x1013, 0x1110 },
    { 0x10b3, 0xb106 },
    { 0x1180, 0x0465 },
    { 0x1180, 0x0466 },
    { 0x1180, 0x0475 },
    { 0x1180, 0x0476 },
    { 0x1180, 0x0478 },
    { 0x104c, 0xac12 },
    { 0x104c, 0xac13 },
    { 0x104c, 0xac15 },
    { 0x104c, 0xac16 },
    { 0x104c, 0xac17 },
    { 0x104c, 0xac19 },
    { 0x104c, 0xac1a },
    { 0x104c, 0xac1d },
    { 0x104c, 0xac1f },
    { 0x104c, 0xac1b },
    { 0x104c, 0xac1c },
    { 0x104c, 0xac1e },
    { 0x104c, 0xac51 },
    { 0x1217, 0x6729 },
    { 0x1217, 0x673a },
    { 0x1217, 0x6832 },
    { 0x1217, 0x6836 },
    { 0x1217, 0x6872 },
    { 0x1179, 0x0603 },
    { 0x1179, 0x060a },
    { 0x1179, 0x060f },
    { 0x1179, 0x0617 },
    { 0x119b, 0x1221 },
    { 0x8086, 0x1221 }
  };

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
       hd->base_class == bc_bridge &&
      (hd->sub_class == sc_bridge_pcmcia || hd->sub_class == sc_bridge_cardbus)
    ) return 1;

    /* just in case... */
    if(hd->bus == bus_pci) {
      for(i = 0; i < sizeof ids / sizeof *ids; i++) {
        if(
          ID_VALUE(hd->vend) == ids[i][0] &&
          ID_VALUE(hd->dev) == ids[i][1]
        ) return 1;
      }
    }
  }

  return 0;
}


int hd_apm_enabled(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_internal && hd->sub_class == sc_int_bios) {
      return hd->detail->bios.data->apm_enabled;
    }
  }

  return 0;
}


int hd_usb_support(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_serial && hd->sub_class == sc_ser_usb) {
      return hd->prog_if == pif_usb_ohci ? 2 : 1;	/* 2: ohci, 1: uhci */
    }
  }

  return 0;
}


int hd_smp_support(hd_data_t *hd_data)
{
  int is_smp = 0;
  unsigned u;
  hd_t *hd;
#if 0
  cpu_info_t *ct;
  str_list_t *sl;
#endif

  u = hd_data->flags.internal;
  hd_data->flags.internal = 1;
  hd = hd_list(hd_data, hw_cpu, 0, NULL);
  if(!hd) hd = hd_list(hd_data, hw_cpu, 1, NULL);
  hd_data->flags.internal = u;

  if(hd && hd->next) is_smp = 1;

#if 0
  // ######### this is wrong!!! fix it !!! ########

  if(!hd || !hd->detail || hd->detail->type != hd_detail_cpu) return i;
  if(!(ct = hd->detail->cpu.data)) return i;

  for(sl = ct->features; sl; sl = sl->next) {
    if(!strcmp(sl->str, "apic")) return 1;
  }
#endif

  hd = hd_free_hd_list(hd);

#ifdef __i386__
  if(!is_smp) is_smp = detectSMP() > 0 ? 1 : 0;
#endif

  return is_smp;
}


int hd_color(hd_data_t *hd_data)
{
#if 0
  hd_t *hd;
  prom_info_t *pt;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_internal && hd->sub_class == sc_int_prom &&
      hd->detail && hd->detail->type == hd_detail_prom &&
      (pt = hd->detail->prom.data) &&
      pt->has_color
    ) {
      return pt->color;
    }
  }
#endif

  if(hd_data->color_code) return hd_data->color_code & 0xffff;

  return -1;
}


int hd_mac_color(hd_data_t *hd_data)
{
  return hd_color(hd_data);
}


unsigned hd_display_adapter(hd_data_t *hd_data)
{
  hd_t *hd;
  driver_info_t *di;
  unsigned u = 0;

  if(hd_get_device_by_idx(hd_data, hd_data->display)) return hd_data->display;

  /* guess: 1st vga card */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_display && hd->sub_class == sc_dis_vga) return hd->idx;
  }

  /* guess: 1st display adapter *with* driver info */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_display) {
      di = hd_driver_info(hd_data, hd);
      if(di && di->any.type == di_x11 && di->x11.server) {
        u = hd->idx;
      }
      hd_free_driver_info(di);
      if(u) return u;
    }
  }

  /* last try: 1st display adapter */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_display) return hd->idx;
  }

  return 0;
}


enum cpu_arch hd_cpu_arch(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_internal && hd->sub_class == sc_int_cpu) {
      return hd->detail->cpu.data->architecture;
    }
  }

#ifdef __i386__
  return arch_intel;
#else
#ifdef __alpha__
  return arch_alpha;
#else
#ifdef __PPC__
  return arch_ppc;
#else
#ifdef __sparc__
  return arch_sparc;
#else
#ifdef __s390__
  return arch_s390;
#else
#ifdef __ia64__
  return arch_ia64;
#else
  return arch_unknown;
#endif
#endif
#endif
#endif
#endif
#endif
}


enum boot_arch hd_boot_arch(hd_data_t *hd_data)
{
  return hd_data->boot;
}

hd_t *hd_list(hd_data_t *hd_data, enum hw_item items, int rescan, hd_t *hd_old)
{
  hd_t *hd, *hd1, *hd_list = NULL, *bridge_hd;
  unsigned char probe_save[sizeof hd_data->probe];
  int sc;		/* compare sub_class too */
  int xtra;		/* some special test */
  int add_it, ok;
  unsigned base_class, sub_class;
  driver_info_t *di;

  if(rescan) {
    memcpy(probe_save, hd_data->probe, sizeof probe_save);
    hd_clear_probe_feature(hd_data, pr_all);
    switch(items) {
      case hw_cdrom:
        hd_set_probe_feature(hd_data, pr_cdrom_info);
        break;

      case hw_floppy:
        hd_set_probe_feature(hd_data, pr_floppy);
        hd_set_probe_feature(hd_data, pr_misc_floppy);
        break;

      case hw_disk:
        hd_set_probe_feature(hd_data, pr_ide);
        hd_set_probe_feature(hd_data, pr_scsi);
        hd_set_probe_feature(hd_data, pr_dac960);
        hd_set_probe_feature(hd_data, pr_smart);
        hd_set_probe_feature(hd_data, pr_i2o);
        hd_set_probe_feature(hd_data, pr_dasd);
        hd_set_probe_feature(hd_data, pr_int);
        break;

      case hw_network:
        hd_set_probe_feature(hd_data, pr_net);
        break;

      case hw_display:
        hd_set_probe_feature(hd_data, pr_pci);
        hd_set_probe_feature(hd_data, pr_sbus);
        hd_set_probe_feature(hd_data, pr_prom);
        hd_set_probe_feature(hd_data, pr_misc);		/* for isa cards */
        break;

      case hw_monitor:
        hd_set_probe_feature(hd_data, pr_misc);
        hd_set_probe_feature(hd_data, pr_monitor);
        break;

      case hw_mouse:
        hd_set_probe_feature(hd_data, pr_misc);
        hd_set_probe_feature(hd_data, pr_serial);
        hd_set_probe_feature(hd_data, pr_adb);
        hd_set_probe_feature(hd_data, pr_usb);
        hd_set_probe_feature(hd_data, pr_kbd);
        hd_set_probe_feature(hd_data, pr_sys);
        hd_set_probe_feature(hd_data, pr_mouse);
        break;

      case hw_keyboard:
        hd_set_probe_feature(hd_data, pr_misc);
        hd_set_probe_feature(hd_data, pr_adb);
        hd_set_probe_feature(hd_data, pr_usb);
        hd_set_probe_feature(hd_data, pr_kbd);
#ifdef __PPC__
        hd_set_probe_feature(hd_data, pr_serial);
#endif
        break;

      case hw_sound:
        hd_set_probe_feature(hd_data, pr_pci);
        hd_set_probe_feature(hd_data, pr_isapnp);
        hd_set_probe_feature(hd_data, pr_sbus);
#ifdef __PPC__
        hd_set_probe_feature(hd_data, pr_prom);
        hd_set_probe_feature(hd_data, pr_misc);
#endif
        break;

      case hw_isdn:
        hd_set_probe_feature(hd_data, pr_misc);		/* get basic i/o res */
        hd_set_probe_feature(hd_data, pr_pci);
        hd_set_probe_feature(hd_data, pr_isapnp);
        hd_set_probe_feature(hd_data, pr_isa_isdn);
        hd_set_probe_feature(hd_data, pr_isdn);
        break;

      case hw_modem:
        hd_set_probe_feature(hd_data, pr_misc);
        hd_set_probe_feature(hd_data, pr_serial);
        hd_set_probe_feature(hd_data, pr_usb);
        hd_set_probe_feature(hd_data, pr_modem);
        break;

      case hw_storage_ctrl:
        hd_set_probe_feature(hd_data, pr_pci);
        hd_set_probe_feature(hd_data, pr_sbus);
        hd_set_probe_feature(hd_data, pr_misc_par);
        hd_set_probe_feature(hd_data, pr_parallel_zip);
#ifdef __PPC__
        hd_set_probe_feature(hd_data, pr_prom);
        hd_set_probe_feature(hd_data, pr_misc);
#endif
        break;

      case hw_network_ctrl:
        hd_set_probe_feature(hd_data, pr_pci);
        hd_set_probe_feature(hd_data, pr_sbus);
        hd_set_probe_feature(hd_data, pr_isdn);
#ifdef __PPC__
        hd_set_probe_feature(hd_data, pr_prom);
        hd_set_probe_feature(hd_data, pr_misc);
#endif
#ifdef __s390__
        hd_set_probe_feature(hd_data, pr_net);
#endif
        break;

      case hw_printer:
        hd_set_probe_feature(hd_data, pr_misc);
        hd_set_probe_feature(hd_data, pr_parallel_lp);
        hd_set_probe_feature(hd_data, pr_usb);
        break;

      case hw_tv:
        hd_set_probe_feature(hd_data, pr_pci);
        break;

      case hw_scanner:
        break;

      case hw_braille:
        hd_set_probe_feature(hd_data, pr_misc_serial);
        hd_set_probe_feature(hd_data, pr_serial);
//        hd_set_probe_feature(hd_data, pr_braille_alva);
        hd_set_probe_feature(hd_data, pr_braille_fhp);
        hd_set_probe_feature(hd_data, pr_braille_ht);
        break;

      case hw_sys:
        hd_set_probe_feature(hd_data, pr_bios);
        hd_set_probe_feature(hd_data, pr_prom);
        hd_set_probe_feature(hd_data, pr_sys);
        break;

      case hw_cpu:
        hd_set_probe_feature(hd_data, pr_cpu);
        break;
    }
    hd_scan(hd_data);
    memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);
  }

  sc = 0;
  sub_class = 0;
  xtra = 0;
  switch(items) {
    case hw_cdrom:
      base_class = bc_storage_device;
      sub_class = sc_sdev_cdrom;
      sc = 1;
      break;

    case hw_floppy:
      base_class = bc_storage_device;
      sub_class = sc_sdev_floppy;
      sc = 1;
      break;

    case hw_disk:
      base_class = bc_storage_device;
      sub_class = sc_sdev_disk;
      sc = 1;
      break;

    case hw_network:
      base_class = bc_network_interface;
      break;

    case hw_display:
      base_class = bc_display;
      break;

    case hw_monitor:
      base_class = bc_monitor;
      break;

    case hw_mouse:
      base_class = bc_mouse;
      break;

    case hw_keyboard:
      base_class = bc_keyboard;
      break;

    case hw_sound:
      base_class = bc_multimedia;
      sub_class = sc_multi_audio;
      sc = 1;
      break;

    case hw_isdn:
      base_class = bc_isdn;
      break;

    case hw_modem:
      base_class = bc_modem;
      break;

    case hw_storage_ctrl:
      base_class = bc_storage;
      break;

    case hw_network_ctrl:
      base_class = bc_network;
      break;

    case hw_printer:
      base_class = bc_printer;
      break;

    case hw_tv:
      base_class = bc_multimedia;
      sub_class = sc_multi_video;
      sc = 1;
      xtra = 1;
      break;

    case hw_scanner:
      base_class = -1;
      break;

    case hw_braille:
      base_class = bc_braille;
      break;

    case hw_sys:
      base_class = bc_internal;
      sub_class = sc_int_sys;
      sc = 1;
      break;

    case hw_cpu:
      base_class = bc_internal;
      sub_class = sc_int_cpu;
      sc = 1;
      break;

    default:
      base_class = -1;
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      (
        hd->base_class == base_class &&
        (sc == 0 || hd->sub_class == sub_class)
      )
      ||
      ( /* list other display adapters, too */
        base_class == bc_display &&
        hd->base_class == bc_multimedia &&
        hd->sub_class == sc_multi_video
      )
    ) {
      /* ##### fix? card bus magic: don't list card bus devices */
      if((bridge_hd = hd_get_device_by_idx(hd_data, hd->attached_to))) {
        if(
          bridge_hd->base_class == bc_bridge &&
          bridge_hd->sub_class == sc_bridge_cardbus
        ) continue;
      }

      ok = 0;
      switch(xtra) {
        case 1:		/* tv cards */
          di = hd_driver_info(hd_data, hd);
          if(
            di && di->any.type == di_any &&
            di->any.hddb0 && di->any.hddb0->str &&
            !strcmp(di->any.hddb0->str, "bttv")
          ) {
            ok = 1;
          }
          hd_free_driver_info(di);
          break;

        default:
          ok = 1;
      }
      if(ok) {
        add_it = 1;
        for(hd1 = hd_old; hd1; hd1 = hd1->next) {
          if(!cmp_hd(hd1, hd)) {
            add_it = 0;
            break;
          }
        }
        if(add_it) {
          hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
          *hd1 = *hd;
          hd->ref_cnt++;
          hd1->ref = hd;
          hd1->next = NULL;
        }
      }
    }
  }

  return hd_list;
}


hd_t *hd_base_class_list(hd_data_t *hd_data, unsigned base_class)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  hd_t *bridge_hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {

    /* ###### fix later: card bus magic */
    if((bridge_hd = hd_get_device_by_idx(hd_data, hd->attached_to))) {
      if(
        bridge_hd->base_class == bc_bridge &&
        bridge_hd->sub_class == sc_bridge_cardbus
      ) continue;
    }

    /* add multimedia/sc_multi_video to display */
    if(
      hd->base_class == base_class ||
      (
        base_class == bc_display &&
        hd->base_class == bc_multimedia &&
        hd->sub_class == sc_multi_video
      )
    ) {
      hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
      *hd1 = *hd;
      hd->ref_cnt++;
      hd1->ref = hd;
      hd1->next = NULL;
    }
  }

  return hd_list;
}

hd_t *hd_sub_class_list(hd_data_t *hd_data, unsigned base_class, unsigned sub_class)
{
  hd_t *hd, *hd1, *hd_list = NULL;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == base_class && hd->sub_class == sub_class) {
      hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
      *hd1 = *hd;
      hd->ref_cnt++;
      hd1->ref = hd;
      hd1->next = NULL;
    }
  }

  return hd_list;
}

hd_t *hd_bus_list(hd_data_t *hd_data, unsigned bus)
{
  hd_t *hd, *hd1, *hd_list = NULL;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->bus == bus) {
      hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
      *hd1 = *hd;
      hd->ref_cnt++;
      hd1->ref = hd;
      hd1->next = NULL;
    }
  }

  return hd_list;
}

/*
 * Check if the execution of (*func)() takes longer than timeout seconds.
 * This is useful to work around long kernel-timeouts as in the floppy
 * detection and ps/2 mosue detection.
 */
int timeout(void(*func)(void *), void *arg, int timeout)
{
  int child1, child2;
  int status = 0;

  child1 = fork();
  if(child1 == -1) return -1;

#ifdef LIBHD_MEMCHECK
#endif

  if(child1) {
    if(waitpid(child1, &status, 0) == -1) return -1;
//    fprintf(stderr, ">child1 status: 0x%x\n", status);

    if(WIFEXITED(status)) {
      status = WEXITSTATUS(status);
//      fprintf(stderr, ">normal child1 status: 0x%x\n", status);
      /* != 0 if we timed out */
    }
    else {
      status = 0;
    }
  }
  else {
    /* fork again */

#ifdef LIBHD_MEMCHECK
    /* stop logging in child process */
    if(libhd_log) fclose(libhd_log);
    libhd_log = NULL;
#endif

    child2 = fork();
    if(child2 == -1) return -1;

    if(child2) {
//      fprintf(stderr, ">signal\n");
      signal(SIGALRM, timeout_alarm_handler);
      alarm(timeout);
      if(waitpid(child2, &status, 0) == -1) return -1;
//      fprintf(stderr, ">child2 status: 0x%x\n", status);
      exit(0);
    }
    else {
      (*func)(arg);
      exit(0);
    }
  }

  return status ? 1 : 0;
}

void timeout_alarm_handler(int signal)
{
  exit(63);
}


str_list_t *read_kmods(hd_data_t *hd_data)
{
  str_list_t *sl, *sl0, *sl1 = NULL;
  char *s;

  hd_data->kmods = free_str_list(hd_data->kmods);

  if(!(sl0 = read_file(PROC_MODULES, 0, 0))) return NULL;

  hd_data->kmods = sl0;

  for(sl = sl0; sl; sl = sl->next) {
    s = sl->str;
    add_str_list(&sl1, strsep(&s, " \t"));
  }

  return sl1;
}


char *get_cmdline(hd_data_t *hd_data, char *key)
{
  str_list_t *sl0, *sl1;
  char *s, *t, *t0;
  int i, l = strlen(key);

  if(!hd_data->cmd_line) {
    sl0 = read_file(PROC_CMDLINE, 0, 1);
    sl1 = read_file(LIB_CMDLINE, 0, 1);

    if(sl0) {
      i = strlen(sl0->str);
      if(i && sl0->str[i - 1] == '\n') sl0->str[i - 1] = 0;
      hd_data->cmd_line = new_str(sl0->str);
      if(hd_data->debug) {
        ADD2LOG("----- " PROC_CMDLINE " -----\n");
        ADD2LOG("  %s\n", sl0->str);
        ADD2LOG("----- " PROC_CMDLINE " end -----\n");
      }
    }
    if(sl1) {
      i = strlen(sl1->str);
      if(i && sl1->str[i - 1] == '\n') sl1->str[i - 1] = 0;
      str_printf(&hd_data->cmd_line, -1, " %s", sl1->str);
      if(hd_data->debug) {
        ADD2LOG("----- " LIB_CMDLINE " -----\n");
        ADD2LOG("  %s\n", sl1->str);
        ADD2LOG("----- " LIB_CMDLINE " end -----\n");
      }
    }

    free_str_list(sl0);
    free_str_list(sl1);
  }

  if(!hd_data->cmd_line) return NULL;

  t = t0 = new_str(hd_data->cmd_line);
  while((s = strsep(&t, " "))) {
    if(!*s) continue;
    if(!strncmp(s, key, l) && s[l] == '=') {
      s += l + 1;
      break;
    }
  }

  s = new_str(s);

  free_mem(t0);

  return s;
}


/*
 * Return field 'field' (starting with 0) from the 'SuSE='
 * kernel cmd line parameter.
 */
char *get_cmd_param(hd_data_t *hd_data, int field)
{
  char *s, *t, *t0;

  s = t0 = get_cmdline(hd_data, "SuSE");

  if(!t0) return NULL;

  t = NULL;

  if(s) {
    for(; field; field--) {
      if(!(s = strchr(s, ','))) break;
      s++;
    }

    if(s && (t = strchr(s, ','))) *t = 0;
  }

  t = new_str(s);

  free_mem(t0);

  return t;
}


void get_disk_crc(int fd, disk_t *dl)
{
  int i, sel;
  fd_set set, set0;
  struct timeval to;
  unsigned crc;

  FD_ZERO(&set0);
  FD_SET(fd, &set0);

  for(;;) {
    to.tv_sec = 0; to.tv_usec = 500000;
    set = set0;

    if((sel = select(fd + 1, &set, NULL, NULL, &to)) > 0) {
//    fprintf(stderr, "sel: %d\n", sel);
      if(FD_ISSET(fd, &set)) {
        if((i = read(fd, dl->data + dl->data_len, sizeof dl->data - dl->data_len)) > 0) {
          dl->data_len += i;
        }
//        fprintf(stderr, "%s: got %d\n", dl->dev_name, i);
        if(i <= 0) break;
      }
    }
    else {
      break;
    }
  }

//  fprintf(stderr, "got %d from %s\n", dl->data_len, dl->dev_name);

  crc = -1;
  for(i = 0; i < sizeof dl->data; i++) {
    crc += dl->data[i];
    crc *= 57;
  }

  dl->crc = crc;
}

disk_t *add_disk_entry(disk_t **dl, disk_t *new_dl)
{
  while(*dl) dl = &(*dl)->next;
  return *dl = new_dl;
}

disk_t *free_disk_list(disk_t *dl)
{
  disk_t *l;

  for(; dl; dl = (l = dl)->next, free_mem(l));

  return NULL;
}

int dev_name_duplicate(disk_t *dl, char *dev_name)
{
  for(; dl; dl = dl->next) {
    if(!strcmp(dl->dev_name, dev_name)) return 1;
  }

  return 0;
}

unsigned hd_boot_disk(hd_data_t *hd_data, int *matches)
{
  hd_t *hd;
  unsigned crc, hd_idx = 0;
  char *s;
  int i, j, fd;
  disk_t *dl, *dl0 = NULL, *dl1 = NULL;

  if(matches) *matches = 0;

  if(!(s = get_cmd_param(hd_data, 2))) return 0;

  i = strlen(s);

  if(i > 0 && i <= 8) {
    crc = hex(s, i);
  }
  else {
    free_mem(s);
    return 0;
  }

  s = free_mem(s);

  if((hd_data->debug & HD_DEB_BOOT)) {
    ADD2LOG("    boot dev crc 0x%x\n", crc);
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_storage_device &&
      hd->sub_class == sc_sdev_disk &&
      hd->unix_dev_name
    ) {
      if(dev_name_duplicate(dl0, hd->unix_dev_name)) continue;
      if((fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK)) >= 0) {
        dl = add_disk_entry(&dl0, new_mem(sizeof *dl0));
        dl->dev_name = hd->unix_dev_name;
        dl->hd_idx = hd->idx;
        get_disk_crc(fd, dl);
        close(fd);
      }
    }
  }

  if(!dl0) return 0;

  if((hd_data->debug & HD_DEB_BOOT)) {
    for(dl = dl0; dl; dl = dl->next) {
      ADD2LOG("    crc %s 0x%08x\n", dl->dev_name, dl->crc);
    }
  }

  for(i = 0, dl = dl0; dl; dl = dl->next) {
    if(crc == dl->crc && dl->data_len == sizeof dl->data) {
      dl->crc_match = 1;
      dl1 = dl;
      if(!i++) hd_idx = dl->hd_idx;
    }
  }

  if(i == 1 && dl1 && (hd_data->debug & HD_DEB_BOOT)) {
    ADD2LOG("----- MBR -----\n");
    for(j = 0; j < sizeof dl1->data; j += 0x10) {
      ADD2LOG("  %03x  ", j);
      hexdump(&hd_data->log, 1, 0x10, dl1->data + j);
      ADD2LOG("\n");
    }
    ADD2LOG("----- MBR end -----\n");
  }

  free_disk_list(dl0);

  if(matches) *matches = i;

  hd_data->debug &= ~HD_DEB_BOOT;

  return hd_idx;
}

void update_irq_usage(hd_data_t *hd_data)
{
  hd_t *hd;
  misc_irq_t *mi;
  unsigned u, v;
  uint64_t irqs = 0;
  hd_res_t *res;

  if(hd_data->misc) {
    mi = hd_data->misc->irq;
    for(u = 0; u < hd_data->misc->irq_len; u++) {
      v = mi[u].irq;
      irqs |= 1ull << v;
    }
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    for(res = hd->res; res; res = res->next) {
      if(res->any.type == res_irq) {
        irqs |= 1ull << res->irq.base;
      }
    }
  }

  hd_data->used_irqs = irqs;
}

int run_cmd(hd_data_t *hd_data, char *cmd)
{
  char *xcmd = NULL;
  str_list_t *sl, *sl0;

  ADD2LOG("----- exec: \"%s\" -----\n", cmd);

  if(*cmd == '/') {
    str_printf(&xcmd, 0, "|%s 2>&1", cmd);
    sl0 = read_file(xcmd, 0, 0);
    for(sl = sl0; sl; sl = sl->next) ADD2LOG("  %s", sl->str);
  }

  ADD2LOG("----- return code: ? -----\n");

  free_mem(xcmd);

  return 0;
}

int load_module(hd_data_t *hd_data, char *module)
{
  char *cmd = NULL;

  if(hd_module_is_active(hd_data, module)) return 0;

  str_printf(&cmd, 0, "/sbin/insmod %s", module);

  return run_cmd(hd_data, cmd);
}

int unload_module(hd_data_t *hd_data, char *module)
{
  char *cmd = NULL;

  if(!hd_module_is_active(hd_data, module)) return 0;

  str_printf(&cmd, 0, "/sbin/rmmod %s", module);

  return run_cmd(hd_data, cmd);
}

/*
 * Compare two hd entries and return 0 if they are identical.
 */
int cmp_hd(hd_t *hd1, hd_t *hd2)
{
  if(!hd1 || !hd2) return 1;

  if(
    hd1->bus != hd2->bus ||
    hd1->slot != hd2->slot ||
    hd1->func != hd2->func ||
    hd1->base_class != hd2->base_class ||
    hd1->sub_class != hd2->sub_class ||
    hd1->prog_if != hd2->prog_if ||
    hd1->dev != hd2->dev ||
    hd1->vend != hd2->vend ||
    hd1->sub_vend != hd2->sub_vend ||
    hd1->rev != hd2->rev ||
    hd1->compat_dev != hd2->compat_dev ||

    hd1->module != hd2->module ||
    hd1->line != hd2->line
  ) {
    return 1;
  }

  if(hd1->unix_dev_name || hd2->unix_dev_name) {
    if(hd1->unix_dev_name && hd2->unix_dev_name) {
      if(strcmp(hd1->unix_dev_name, hd2->unix_dev_name)) return 1;
    }
    else {
      return 1;
    }
  }

  return 0;
}


void get_probe_env(hd_data_t *hd_data)
{
  char *s, *t, *env;
  int j, k;
  char buf[10];

  env = getenv("hwprobe");
  if(!env) env = get_cmdline(hd_data, "hwprobe");
  s = env = new_str(env);
  if(!env) return;

  hd_data->xtra_hd = free_str_list(hd_data->xtra_hd);

  while((t = strsep(&s, ","))) {
    if(*t == '+') {
      k = 1; t++;
    }
    else if(*t == '-') {
      k = 0; t++;
    }
    else {
      k = 2;
//      ADD2LOG("hwprobe: +/- missing before \"%s\"\n", t);
//      return;
    }

    if((j = hd_probe_feature_by_name(t))) {
      set_probe_feature(hd_data, j, k ? 1 : 0);
    }
    else if(sscanf(t, "%8[^:]:%8[^:]:%8[^:]", buf, buf, buf) == 3) {
      add_str_list(&hd_data->xtra_hd, t - (k == 2 ? 0 : 1));
    }
    else {
      ADD2LOG("hwprobe: what is \"%s\"?\n", t);
      return;
    }
  }

  free_mem(env);
}

void hd_scan_xtra(hd_data_t *hd_data)
{
  str_list_t *sl;
  hd_t *hd, *hd_tmp;
  unsigned u0, u1, u2, tag;
  int i, err;
  char buf0[10], buf1[10], buf2[10], buf3[64], *s, k;

  hd_data->module = mod_xtra;

  remove_hd_entries(hd_data);

  for(sl = hd_data->xtra_hd; sl; sl = sl->next) {
    s = sl->str;
    err = 0;
    switch(*s) {
      case '+': k = 1; s++; break;
      case '-': k = 0; s++; break;
      default: k = 2;
    }
    if(
      (i = sscanf(s, "%8[^:]:%8[^:]:%8[^:]:%60s", buf0, buf1, buf2, buf3)) >= 3
    ) {
      if(i < 4) *buf3 = 0;

      u0 = strtoul(buf0, &s, 16);
      if(*s) err |= 1;
      if(strlen(buf1) == 3) {
        u1 = name2eisa_id(buf1);
      }
      else {
        tag = TAG_PCI;
        s = buf1;
        switch(*s) {
          case 'p': tag = TAG_PCI; s++; break;
          case 'r': tag = 0; s++; break;
          case 's': tag = TAG_SPECIAL; s++; break;
          case 'u': tag = TAG_USB; s++; break;
        }
        u1 = strtoul(s, &s, 16);
        if(*s) err |= 2;
        u1 = MAKE_ID(tag, u1);
      }
      u2 = strtoul(buf2, &s, 16);
      if(*s) err |= 4;
      u2 = MAKE_ID(ID_TAG(u1), ID_VALUE(u2));
      if((err & 1) && !strcmp(buf0, "*")) {
        u0 = -1;
        err &= ~1;
      }
      if((err & 2) && !strcmp(buf1, "*")) {
        u1 = 0;
        err &= ~2;
      }
      if((err & 4) && !strcmp(buf2, "*")) {
        u2 = 0;
        err &= ~4;
      }
      if(!err) {
        if(k) {
          if(k == 2) {
            /* insert at top */
            hd_tmp = hd_data->hd;
            hd_data->hd = NULL;
            hd = add_hd_entry(hd_data, __LINE__, 0);
            hd->next = hd_tmp;
            hd_tmp = NULL;
          }
          else {
            hd = add_hd_entry(hd_data, __LINE__, 0);
          }
          hd->base_class = u0 >> 8;
          hd->sub_class = u0 & 0xff;
          hd->vend = u1;
          hd->dev = u2;
          if(ID_TAG(hd->vend) == TAG_PCI) hd->bus = bus_pci;
          if(ID_TAG(hd->vend) == TAG_USB) hd->bus = bus_usb;
          if(*buf3) hd->unix_dev_name = new_str(buf3);
        }
        else {
          for(hd = hd_data->hd; hd; hd = hd->next) {
            if(
                (u0 == -1 || (
                  hd->base_class == (u0 >> 8) &&
                  hd->sub_class == (u0 & 0xff)
                )) &&
                (u1 == 0 || hd->vend == u1) &&
                (u2 == 0 || hd->dev == u2) &&
                (*buf3 == 0 || (
                  hd->unix_dev_name &&
                  !strcmp(hd->unix_dev_name, buf3)
                ))
            ) {
              hd->tag.remove = 1;
            }
          }
          remove_tagged_hd_entries(hd_data);
        }
      }
    }
  }
}

unsigned has_something_attached(hd_data_t *hd_data, hd_t *hd)
{
  hd_t *hd1;

  for(hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
    if(hd1->attached_to == hd->idx) return hd1->idx;
  }

  return 0;
}


/* ##### FIX: replace with a real crc later ##### */
void crc64(uint64_t *id, void *p, int len)
{
  unsigned char uc;

  for(; len; len--, p++) {
    uc = *(unsigned char *) p;
    *id += uc + ((uc + 57) << 27);
    *id *= 73;
    *id *= 65521;
  }
}

char *numid2str(uint64_t id, int len)
{
  static char buf[32];

#ifdef NUMERIC_UNIQUE_ID
  /* numeric */

  if(len < (sizeof id << 3)) id &= ~(-1LL << len);
  sprintf(buf, "%0*"PRIx64, len >> 2, id);

#else
  /* base64 like */

  int i;
  unsigned char u;

  memset(buf, 0, sizeof buf);
  for(i = 0; len > 0 && i < sizeof buf - 1; i++, len -= 6, id >>= 6) {
    u = id & 0x3f;
    if(u < 10) {
      u += '0';			/* 0..9 */
    }
    else if(u < 10 + 26) {
      u += 'A' - 10;		/* A..Z */
    }
    else if(u < 10 + 26 + 26) {
      u += 'a' - 10 - 26;	/* a..z */
    }
    else if(u == 63) {
      u = '+';
    }
    else {
      u = '/';
    }
    buf[i] = u;
  }

#endif

  return buf;
}

/*
 * calculate unique ids
 */
#define INT_CRC(a, b)	crc64(&a, &hd->b, sizeof hd->b);
#define STR_CRC(a, b)	if(hd->b) crc64(&a, hd->b, strlen(hd->b) + 1);

void hd_add_id(hd_t *hd)
{
  uint64_t id0 = 0, id1 = 0;

  if(hd->unique_id) return;

  INT_CRC(id0, bus);
  INT_CRC(id0, slot);
  INT_CRC(id0, func);
  INT_CRC(id0, base_class);
  INT_CRC(id0, sub_class);
  INT_CRC(id0, prog_if);
  STR_CRC(id0, unix_dev_name);
  STR_CRC(id0, rom_id);

  INT_CRC(id1, base_class);
  INT_CRC(id1, sub_class);
  INT_CRC(id1, prog_if);
  INT_CRC(id1, dev);
  INT_CRC(id1, vend);
  INT_CRC(id1, sub_dev);
  INT_CRC(id1, sub_vend);
  INT_CRC(id1, rev);
  INT_CRC(id1, compat_dev);
  INT_CRC(id1, compat_vend);
  STR_CRC(id1, dev_name);
  STR_CRC(id1, vend_name);
  STR_CRC(id1, sub_dev_name);
  STR_CRC(id1, sub_vend_name);
  STR_CRC(id1, rev_name);
  STR_CRC(id1, serial);

  id0 += (id0 >> 32);
  str_printf(&hd->unique_id, 0, "%s", numid2str(id0, 24));
  str_printf(&hd->unique_id, -1, ".%s", numid2str(id1, 64));
}
#undef INT_CRC
#undef STR_CRC


#if 0
  if(!hd1 || !hd2) return 1;

  if(
    hd1->bus != hd2->bus ||
    hd1->slot != hd2->slot ||
    hd1->func != hd2->func ||
    hd1->base_class != hd2->base_class ||
    hd1->sub_class != hd2->sub_class ||
    hd1->prog_if != hd2->prog_if ||
    hd1->dev != hd2->dev ||
    hd1->vend != hd2->vend ||
    hd1->sub_vend != hd2->sub_vend ||
    hd1->rev != hd2->rev ||
    hd1->compat_dev != hd2->compat_dev ||

    hd1->module != hd2->module ||
    hd1->line != hd2->line
  ) {
    return 1;
  }

  if(hd1->unix_dev_name || hd2->unix_dev_name) {
    if(hd1->unix_dev_name && hd2->unix_dev_name) {
      if(strcmp(hd1->unix_dev_name, hd2->unix_dev_name)) return 1;
    }
    else {
      return 1;
    }
  }

#endif


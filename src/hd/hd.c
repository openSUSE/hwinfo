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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * various functions commmon to all probing modules
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


#ifdef __i386__
#define HD_ARCH "ix86"
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

#define MOD_INFO_SEP		'|'
#define MOD_INFO_SEP_STR	"|"

static struct s_pr_flags *get_pr_flags(enum probe_feature feature);
static hd_t *add_hd_entry2(hd_t **hd, hd_t *new_hd);
static hd_res_t *get_res(hd_t *h, enum resource_types t, unsigned index);
static int chk_free_biosmem(hd_data_t *hd_data, unsigned addr, unsigned len);
static isdn_parm_t *new_isdn_parm(isdn_parm_t **ip);
static driver_info_t *isdn_driver(hd_data_t *hd_data, hd_t *hd, ihw_card_info *ici);
static char *module_cmd(hd_t *, char *);
static void timeout_alarm_handler(int signal);

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
  { mod_prom, "prom" }
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
  { pr_default,     -1,              1, "default"     },
  { pr_all,         -1,            2  , "all"         },
  { pr_max,         -1,          4    , "max"         },
  { pr_lxrc,        -1,        8      , "lxrc"        },
  { pr_memory,       0,        8|4|2|1, "memory"      },
  { pr_pci,          0,        8|4|2|1, "pci"         },
  { pr_pci_range,    pr_pci,     4|2  , "pci.range"   },
  { pr_pci_ext,      pr_pci,     4|2  , "pci.ext"     },
  { pr_isapnp,       0,        8|4|2|1, "isapnp"      },
  { pr_isapnp,       0,              0, "pnpdump"     },	/* alias for isapnp */
  { pr_cdrom,        0,        8|4|2|1, "cdrom"       },
  { pr_cdrom_info,   pr_cdrom, 8|4|2|1, "cdrom.info"  },
  { pr_net,          0,        8|4|2|1, "net"         },
  { pr_floppy,       0,        8|4|2|1, "floppy"      },
  { pr_misc,         0,        8|4|2|1, "misc"        },
  { pr_misc_serial,  pr_misc,  8|4|2|1, "misc.serial" },
  { pr_misc_par,     pr_misc,    4|2|1, "misc.par"    },
  { pr_misc_floppy,  pr_misc,  8|4|2|1, "misc.floppy" },
  { pr_bios,         0,        8|4|2|1, "bios"        },
  { pr_cpu,          0,        8|4|2|1, "cpu"         },
  { pr_monitor,      0,        8|4|2|1, "monitor"     },
  { pr_serial,       0,          4|2|1, "serial"      },
  { pr_mouse,        0,          4|2|1, "mouse"       },
  { pr_ide,          0,        8|4|2|1, "ide"         },
  { pr_scsi,         0,        8|4|2|1, "scsi"        },
  { pr_scsi_geo,     0,          4|2  , "scsi.geo"    },
  { pr_usb,          0,        8|4|2|1, "usb"         },
  { pr_usb_mods,     0,          4    , "usb.mods"    },
  { pr_adb,          0,        8|4|2|1, "adb"         },
  { pr_modem,        0,          4|2|1, "modem"       },
  { pr_modem_usb,    pr_modem,   4|2|1, "modem.usb"   },
  { pr_parallel,     0,          4|2|1, "parallel"    },
  { pr_isa,          0,          4|2|1, "isa"         },
  { pr_isa_isdn,     pr_isa,     4|2|1, "isa.isdn"    },
  { pr_dac960,       0,        8|4|2|1, "dac960"      },
  { pr_smart,        0,        8|4|2|1, "smart"       },
  { pr_isdn,         0,          4|2|1, "isdn"        },
  { pr_kbd,          0,        8|4|2|1, "kbd"         },
  { pr_prom,         0,        8|4|2|1, "prom"        }
};

struct s_pr_flags *get_pr_flags(enum probe_feature feature)
{
  int i;

  for(i = 0; i < sizeof pr_flags / sizeof *pr_flags; i++) {
    if(feature == pr_flags[i].val) return pr_flags + i;
  }

  return NULL;
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

  return NULL;
}


/*
 * Free all data associated with a driver_info_t struct. Even the struct itself.
 */
driver_info_t *hd_free_driver_info(driver_info_t *di)
{

  return NULL;
}


/*
 * Free a hd_t list. *Not* the data referred to by the hd_t structs.
 */
hd_t *hd_free_hd_list(hd_t *hd)
{
  hd_t *h;

  for(; hd; hd = (h = hd)->next, free_mem(h));

  return NULL;
}


void *new_mem(size_t size)
{
  void *p;

  if(size == 0) return NULL;

  if((p = calloc(size, 1))) return p;

  fprintf(stderr, "memory oops 1\n");
  exit(11);
  /*NOTREACHED*/
  return 0;
}

void *resize_mem(void *p, size_t n)
{
  p = realloc(p, n);

  if(!p) {
    fprintf(stderr, "memory oops 7\n");
    exit(17);
  }

  return p;
}

void *add_mem(void *p, size_t elem_size, size_t n)
{
  p = realloc(p, (n + 1) * elem_size);

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

  if((t = strdup(s))) return t;

  fprintf(stderr, "memory oops 2\n");
  /*NOTREACHED*/
  exit(12);

  return NULL;
}

void *free_mem(void *p)
{
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

  /* log the debug & probe flags */
  if(hd_data->debug) {
    for(i = sizeof hd_data->probe - 1; i >= 0; i--) {
      str_printf(&s, -1, "%02x", hd_data->probe[i]);
    }
    ADD2LOG(
      "libhd version %s%s (%s)\ndebug = 0x%x\nprobe = 0x%s (",
      HD_VERSION, getuid() ? "u" : "", HD_ARCH, hd_data->debug, s
    );
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

  /* for various reasons, do it befor scan_misc() */
  hd_scan_floppy(hd_data);

  /* get basic system info */
  hd_scan_misc(hd_data);

#ifndef LIBHD_TINY
  /* start the detection  */
  hd_scan_cpu(hd_data);
  hd_scan_memory(hd_data);
  hd_scan_monitor(hd_data);

#if defined(__i386__)
  hd_scan_bios(hd_data);
#endif
#if defined(__PPC__)
  hd_scan_prom(hd_data);
#endif
#endif	/* LIBHD_TINY */

#if defined(__i386__) || defined(__alpha__) || defined(__PPC__)
  hd_scan_isapnp(hd_data);
#endif

#if defined(__i386__)
  hd_scan_isa(hd_data);
#endif

  hd_scan_pci(hd_data);
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
  hd_scan_usb(hd_data);
#if defined(__PPC__)
  hd_scan_adb(hd_data);
#endif
  hd_scan_kbd(hd_data);
#ifndef LIBHD_TINY
  hd_scan_modem(hd_data);	/* do it before hd_scan_mouse() */
#endif
  hd_scan_mouse(hd_data);

  /* keep these at the end of the list */
  hd_scan_cdrom(hd_data);
  hd_scan_net(hd_data);
  hd_scan_isdn(hd_data);

  /* we are done... */
  hd_data->module = mod_none;

  if(hd_data->debug) {
    sl0 = read_file(PROC_MODULES, 0, 0);
    ADD2LOG("----- /proc/modules -----\n");
    for(sl = sl0; sl; sl = sl->next) {
      ADD2LOG("  %s", sl->str);
    }
    ADD2LOG("----- /proc/modules end -----\n");
    free_str_list(sl0);
  }

  update_irq_usage(hd_data);

  if(hd_data->debug) {
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

  if(hd_data->debug) {
    i = hd_usb_support(hd_data);
    ADD2LOG("  usb support: %s\n", i == 2 ? "ohci" : i == 1 ? "uhci" : "none");
    ADD2LOG("  pcmcia support: %d\n", hd_has_pcmcia(hd_data));
    ADD2LOG("  special eide chipset: %d\n", hd_has_special_eide(hd_data));
    ADD2LOG("  apm status: %sabled\n", hd_apm_enabled(hd_data) ? "en" : "dis");
    ADD2LOG("  smp board: %s\n", hd_smp_support(hd_data) ? "yes" : "no");
    switch(hd_cpu_arch(hd_data)) {
      case arch_intel:
        s = "intel";
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
  free(m0);

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
  int last_len = 0;
  char b[1024];
  va_list args;

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

  last_buf = resize_mem(*buf, (last_len = offset + strlen(b)) + 1);
  strcpy(last_buf + offset, b);

  *buf = last_buf;
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
  while(*sl) sl = &(*sl)->next;

  *sl = new_mem(sizeof **sl);
  (*sl)->str = new_str(str);

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
  str_list_t *sl_start = NULL, *sl_end = NULL, *sl;

  if(!(f = fopen(file_name, "r"))) return NULL;

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

  fclose(f);

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

driver_info_t *kbd_driver(hd_data_t *hd_data, hd_t *hd)
{
  driver_info_t *di;
  driver_info_kbd_t *ki;
  int arch = hd_cpu_arch(hd_data);
  unsigned u;
  char *s1, *s2;

  if(hd->sub_class == sc_keyboard_console) return NULL;

  di = new_mem(sizeof *di);
  di->kbd.type = di_kbd;
  ki = &(di->kbd);

  ki->XkbRules = new_str(arch == arch_sparc || arch == arch_sparc64 ? "sun" : "xfree86");

  switch(arch) {
    case arch_intel:
    case arch_alpha:
      ki->XkbModel = new_str("pc104");
    case arch_ppc:
      ki->XkbModel = new_str(hd->bus == bus_ps2 ? "pc104" : "macintosh");
      break;
    case arch_sparc:
    case arch_sparc64:
      if(hd->vend == MAKE_ID(TAG_SPECIAL, 0x0202)) {
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
      break;
  }

  return di;
}


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
  driver_info_t *di, *di0 = NULL;
  ihw_card_info *ici;
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
  }

  if(!di0 && (hd->compat_vend || hd->compat_dev)) {
    di0 = device_driver(hd_data, hd->compat_vend, hd->compat_dev);
  }

  if((ici = get_isdn_info(hd))) {
    di0 = isdn_driver(hd_data, hd, ici);
    if(di0) return di0;
  }

  if(hd->base_class == bc_keyboard) {
    di0 = kbd_driver(hd_data, hd);
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
            di->x11.server = new_str(sl->str);
          }
          else {
            add_str_list(&di->x11.x3d, sl->str);
          }
#if 0
          else if(i == 2) {
            di->x11.colors.all = strtol(sl->str, NULL, 16);
            if(di->x11.colors.all & (1 << 0)) di->x11.colors.c8 = 1;
            if(di->x11.colors.all & (1 << 1)) di->x11.colors.c15 = 1;
            if(di->x11.colors.all & (1 << 2)) di->x11.colors.c16 = 1;
            if(di->x11.colors.all & (1 << 3)) di->x11.colors.c24 = 1;
            if(di->x11.colors.all & (1 << 4)) di->x11.colors.c32 = 1;
          }
          else if(i == 3) {
            di->x11.dacspeed = strtol(sl->str, NULL, 10);
          }
#endif
        }
        break;
    
      default:
        break; 
    }
  }

  return di0;
}


int hd_module_is_active(hd_data_t *hd_data, char *mod)
{
  str_list_t *sl, *sl0 = read_kmods(hd_data);

  for(sl = sl0; sl; sl = sl->next) {
    if(!strcmp(sl->str, mod)) break;
  }

  free_str_list(sl0);

  return sl ? 1 : 0;
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
int hd_has_special_eide(hd_data_t *hd_data)
{
  int i;
  hd_t *hd;
  static unsigned ids[][2] = {
    { PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C561 },
    { PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_1 },
    { PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20246 },
    { PCI_VENDOR_ID_PROMISE, 0x4d38 },		// PCI_DEVICE_ID_PROMISE_20262
    { PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_5513 },
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C621 },
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C558 },
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C825 },
    { PCI_VENDOR_ID_TEKRAM, PCI_DEVICE_ID_TEKRAM_DC290 },
    { PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_87410 },
    { PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_87415 }
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
  return detectSMP() > 0 ? 1 : 0;
}


int hd_mac_color(hd_data_t *hd_data)
{
#ifdef __PPC__
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

  return -1;
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

#ifdef __i386_
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
  return arch_unknown;
#endif
#endif
#endif
#endif
}


enum boot_arch hd_boot_arch(hd_data_t *hd_data)
{
  return hd_data->boot;
}


hd_t *hd_cd_list(hd_data_t *hd_data, int rescan)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  unsigned char probe_save[sizeof hd_data->probe];

  if(rescan) {
    memcpy(probe_save, hd_data->probe, sizeof probe_save);
    hd_clear_probe_feature(hd_data, pr_all);
    hd_set_probe_feature(hd_data, pr_cdrom);
    hd_set_probe_feature(hd_data, pr_cdrom_info);
    hd_scan(hd_data);
    memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_storage_device && hd->sub_class == sc_sdev_cdrom) {
      if(!((rescan == 2 || rescan == 3) && search_str_list(hd_data->cd_list, hd->unix_dev_name))) {
        if(rescan == 2) {
          add_str_list(&hd_data->cd_list, hd->unix_dev_name);
        }
        hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
        *hd1 = *hd;
        hd1->next = NULL;
      }
    }
  }

  return hd_list;
}


hd_t *hd_disk_list(hd_data_t *hd_data, int rescan)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  unsigned char probe_save[sizeof hd_data->probe];

  if(rescan) {
    memcpy(probe_save, hd_data->probe, sizeof probe_save);
    hd_clear_probe_feature(hd_data, pr_all);
    hd_set_probe_feature(hd_data, pr_ide);
    hd_set_probe_feature(hd_data, pr_scsi);
    hd_set_probe_feature(hd_data, pr_dac960);
    hd_set_probe_feature(hd_data, pr_smart);
    hd_scan(hd_data);
    memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_storage_device && hd->sub_class == sc_sdev_disk) {
      if(!((rescan == 2 || rescan == 3) && search_str_list(hd_data->disk_list, hd->unix_dev_name))) {
        if(rescan == 2) {
          add_str_list(&hd_data->disk_list, hd->unix_dev_name);
        }
        hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
        *hd1 = *hd;
        hd1->next = NULL;
      }
    }
  }

  return hd_list;
}


hd_t *hd_net_list(hd_data_t *hd_data, int rescan)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  unsigned char probe_save[sizeof hd_data->probe];

  if(rescan) {
    memcpy(probe_save, hd_data->probe, sizeof probe_save);
    hd_clear_probe_feature(hd_data, pr_all);
    hd_set_probe_feature(hd_data, pr_net);
    hd_scan(hd_data);
    memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_network_interface) {
      if(!((rescan == 2 || rescan == 3) && search_str_list(hd_data->net_list, hd->unix_dev_name))) {
        if(rescan == 2) {
          add_str_list(&hd_data->net_list, hd->unix_dev_name);
        }
        hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
        *hd1 = *hd;
        hd1->next = NULL;
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


/*
 * Return field 'field' (starting with 0) from the 'SuSE='
 * kernel cmd line parameter.
 */
char *get_cmd_param(int field)
{
  str_list_t *sl;
  char c_str[] = "SuSE=";
  char *s, *t;

  if(!(sl = read_file(PROC_CMDLINE, 0, 1))) return NULL;

  t = sl->str;
  while((s = strsep(&t, " "))) {
    if(!*s) continue;
    if(!strncmp(s, c_str, sizeof c_str - 1)) {
      s += sizeof c_str - 1;
      break;
    }
  }

  t = NULL;

  if(s) {
    for(; field; field--) {
      if(!(s = strchr(s, ','))) break;
      s++;
    }

    if(s && (t = strchr(s, ','))) *t = 0;
  }

  t = new_str(s);

  free_str_list(sl);

  return t;
}


typedef struct disk_s {
  struct disk_s *next;
  unsigned crc;
  unsigned crc_match:1;
  unsigned hd_idx;
  char *dev_name;
  unsigned data_len;
  unsigned char data[0x200];
} disk_t;

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

  if(!(s = get_cmd_param(2))) return 0;

  i = strlen(s);
  
  if(i > 0 && i <= 8) {
    crc = hex(s, i);
  }
  else {
    free_mem(s);
    return 0;
  }

  s = free_mem(s);

  if(hd_data->debug) {
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

  if(hd_data->debug) {
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

  if(i == 1 && dl1 && hd_data->debug) {
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
  int i;
  char *xcmd = NULL, *log = NULL;
  str_list_t *sl, *sl0;

  ADD2LOG("----- exec: \"%s\" -----\n", cmd);

  str_printf(&log, 0, "/tmp/tmplibhd.%d", getpid());
  str_printf(&xcmd, 0, "%s >%s 2>&1", cmd, log);

  i = system(xcmd);
  sl0 = read_file(log, 0, 0);
  unlink(log);

  for(sl = sl0; sl; sl = sl->next) ADD2LOG("  %s", sl->str);

  ADD2LOG("----- return code: 0x%x -----\n", i);

  free_mem(xcmd);
  free_mem(log);

  return i;
}

int load_module(hd_data_t *hd_data, char *module)
{
  char *cmd = NULL;

  if(hd_module_is_active(hd_data, module)) return 0;

  str_printf(&cmd, 0, "insmod %s", module);

  return run_cmd(hd_data, cmd);
}


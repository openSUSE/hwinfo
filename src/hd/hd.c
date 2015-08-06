#define _GNU_SOURCE		/* strcasestr() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <linux/pci.h>
#include <linux/hdreg.h>
#define _LINUX_AUDIT_H_
#define _LINUX_PRIO_TREE_H
#include <linux/fs.h>

/**
 * @defgroup libhdBUSint Bus scanning code
 * @ingroup libhdInternals
 */

/**
 * @defgroup libhdDEVint Device handling
 * @ingroup libhdInternals
 */

/**
 * @defgroup libhdINFOint Information gathering
 * @ingroup libhdInternals
 */

/**
 * @defgroup libhdInternals Implementation internals
 *
 * various functions commmon to all probing modules
 *
 * @{
 */

#define u64 uint64_t

#ifndef BLKSSZGET
#define BLKSSZGET _IO(0x12,104)		/* get block device sector size */
#endif

#include "hd.h"
#include "hddb.h"
#include "hd_int.h"
#include "smbios.h"
#include "memory.h"
#include "isapnp.h"
#include "monitor.h"
#include "cpu.h"
#include "misc.h"
#include "mouse.h"
#include "floppy.h"
#include "bios.h"
#include "serial.h"
#include "net.h"
#include "version.h"
#include "usb.h"
#include "modem.h"
#include "parallel.h"
#include "isa.h"
#include "isdn.h"
#include "kbd.h"
#include "prom.h"
#include "sbus.h"
#include "int.h"
#include "braille.h"
#include "sys.h"
#include "manual.h"
#include "fb.h"
#include "pppoe.h"
#include "pcmcia.h"
#include "s390.h"
#include "pci.h"
#include "block.h"
#include "edd.h"
#include "input.h"
#include "wlan.h"
#include "hal.h"
#include "klog.h"
#include "drm.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * various functions commmon to all probing modules
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifdef __i386__
#define HD_ARCH "ia32"
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

#ifdef __s390x__
#define HD_ARCH "s390x"
#else
#ifdef __s390__
#define HD_ARCH "s390"
#endif
#endif

#ifdef __arm__
#define HD_ARCH "arm"
#endif

#ifdef __aarch64__
#define HD_ARCH "aarch64"
#endif

#ifdef __mips__
#define HD_ARCH "mips"
#endif

#ifdef __x86_64__
#define HD_ARCH "x86-64"
#endif

#ifdef __hppa__
#define HD_ARCH "hppa"
#endif

#ifdef __m68k__
#define HD_ARCH "68k"
#endif

typedef struct disk_s {
  struct disk_s *next;
  unsigned crc;
  unsigned crc_match:1;
  unsigned hd_idx;
  char *dev_name;
  unsigned char *data;
} disk_t;

typedef struct {
  enum probe_feature val, parent;
  unsigned mask;	/* bit 0: default, bit 1: all, bit 2: max, bit 3: linuxrc */
  char *name;
  hal_prop_type_t type;
} pr_flags_t;

static pr_flags_t *pr_flags_by_name(char *name);
static pr_flags_t *pr_flags_by_id(enum probe_feature feature);
static int set_probe_val(hd_data_t *hd_data, enum probe_feature feature, char *val);
static void fix_probe_features(hd_data_t *hd_data);
static void set_probe_feature(hd_data_t *hd_data, enum probe_feature feature, unsigned val);
static void free_old_hd_entries(hd_data_t *hd_data);
static hd_t *free_hd_entry(hd_t *hd);
static hd_t *add_hd_entry2(hd_t **hd, hd_t *new_hd);
static void timeout_alarm_handler(int signal);
static void get_probe_env(hd_data_t *hd_data);
static void hd_scan_xtra(hd_data_t *hd_data);
static hd_t *hd_get_device_by_id(hd_data_t *hd_data, char *id);
static int has_item(hd_hw_item_t *items, hd_hw_item_t item);
static int has_hw_class(hd_t *hd, hd_hw_item_t *items);
static void hd_scan_with_hal(hd_data_t *hd_data);
static void hd_scan_no_hal(hd_data_t *hd_data);

static void test_read_block0_open(void *arg);
static void get_kernel_version(hd_data_t *hd_data);
static int is_modem(hd_data_t *hd_data, hd_t *hd);
static int is_audio(hd_data_t *hd_data, hd_t *hd);
static int is_pppoe(hd_data_t *hd_data, hd_t *hd);
static void assign_hw_class(hd_data_t *hd_data, hd_t *hd);
static void short_vendor(char *vendor);
static void create_model_name(hd_data_t *hd_data, hd_t *hd);

static void copy_log2shm(hd_data_t *hd_data);
static void sigchld_handler(int);
static void sigusr1_handler(int);
static pid_t child_id;
static volatile pid_t child;
static char *hd_shm_add_str(hd_data_t *hd_data, char *str);
static str_list_t *hd_shm_add_str_list(hd_data_t *hd_data, str_list_t *sl);

static hd_udevinfo_t *hd_free_udevinfo(hd_udevinfo_t *ui);
static hd_sysfsdrv_t *hd_free_sysfsdrv(hd_sysfsdrv_t *sf);

static hd_data_t *hd_data_sig;

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
  { mod_net, "net"},
  { mod_floppy, "floppy"},
  { mod_misc, "misc" },
  { mod_bios, "bios"},
  { mod_cpu, "cpu"},
  { mod_monitor, "monitor"},
  { mod_serial, "serial"},
  { mod_mouse, "mouse"},
  { mod_scsi, "scsi"},
  { mod_usb, "usb"},
  { mod_modem, "modem"},
  { mod_parallel, "parallel" },
  { mod_isa, "isa" },
  { mod_isdn, "isdn" },
  { mod_kbd, "kbd" },
  { mod_prom, "prom" },
  { mod_sbus, "sbus" },
  { mod_int, "int" },
  { mod_braille, "braille" },
  { mod_xtra, "hd" },
  { mod_sys, "sys" },
  { mod_manual, "manual" },
  { mod_fb, "fb" },
  { mod_pppoe, "pppoe" },
  { mod_pcmcia, "pcmcia" },
  { mod_s390, "s390" },
  { mod_sysfs, "sysfs" },
  { mod_dsl, "dsl" },
  { mod_block, "block" },
  { mod_edd, "edd" },
  { mod_input, "input" },
  { mod_hal, "hal" },
  { mod_wlan, "wlan" }
};

/*
 * Names for the probe flags. Used for debugging and command line parsing in
 * hw.c. Cf. enum probe_feature, hd_data_t.probe.
 */

static pr_flags_t pr_flags[] = {
  { pr_default,      -1,                  1, "default",      p_bool },
  { pr_all,          -1,                2  , "all",          p_bool },
  { pr_max,          -1,              4    , "max",          p_bool },
  { pr_lxrc,         -1,            8      , "lxrc",         p_bool },
  { pr_memory,        0,            8|4|2|1, "memory",       p_bool },
  { pr_pci,           0,            8|4|2|1, "pci",          p_bool },
  { pr_s390,          0,            8|4|2|1, "s390",         p_bool },
  { pr_s390disks,     0,                  0, "s390disks",    p_bool },
  { pr_isapnp,        0,              4|2|1, "isapnp",       p_bool },
  { pr_isapnp_old,    pr_isapnp,          0, "isapnp.old",   p_bool },
  { pr_isapnp_new,    pr_isapnp,          0, "isapnp.new",   p_bool },
  { pr_isapnp_mod,    0,              4    , "isapnp.mod",   p_bool },
  { pr_isapnp,        0,                  0, "pnpdump",      p_bool },	/* alias for isapnp */
  { pr_net,           0,            8|4|2|1, "net",          p_bool },
  { pr_net_eeprom,    0,                  0, "net.eeprom",   p_bool },
  { pr_floppy,        0,            8|4|2|1, "floppy",       p_bool },
  { pr_misc,          pr_bios,      8|4|2|1, "misc",         p_bool },	// ugly hack!
  { pr_misc_serial,   pr_misc,      8|4|2|1, "misc.serial",  p_bool },
  { pr_misc_par,      pr_misc,        4|2|1, "misc.par",     p_bool },
  { pr_misc_floppy,   pr_misc,      8|4|2|1, "misc.floppy",  p_bool },
  { pr_bios,          0,            8|4|2|1, "bios",         p_bool },
  { pr_bios_vesa,     pr_bios,        4|2|1, "bios.vesa",    p_bool },
  { pr_bios_ddc,      pr_bios_vesa,       0, "bios.ddc",     p_bool },
  { pr_bios_ddc_ports, pr_bios_ddc,       0, "bios.ddc.ports", p_int32 },
  { pr_bios_fb,       pr_bios_vesa,       0, "bios.fb",      p_bool },
  { pr_bios_mode,     pr_bios_vesa,       0, "bios.mode",    p_bool },
  { pr_bios_vbe,      pr_bios_mode,       0, "bios.vbe",     p_bool }, // just an alias
  { pr_bios_crc,      0,                  0, "bios.crc",     p_bool }, // require bios crc check to succeed
  { pr_bios_vram,     0,                  0, "bios.vram",    p_bool }, // map video bios ram
  { pr_bios_acpi,     0,                  0, "bios.acpi",    p_bool }, // dump acpi data
  { pr_cpu,           0,            8|4|2|1, "cpu",          p_bool },
  { pr_monitor,       0,            8|4|2|1, "monitor",      p_bool },
  { pr_serial,        0,              4|2|1, "serial",       p_bool },
  { pr_mouse,         0,              4|2|1, "mouse",        p_bool },
  { pr_scsi,          0,            8|4|2|1, "scsi",         p_bool },
  { pr_scsi_noserial, 0,                  0, "scsi.noserial", p_bool },
  { pr_usb,           0,            8|4|2|1, "usb",          p_bool },
  { pr_usb_mods,      0,              4    , "usb.mods",     p_bool },
  { pr_modem,         0,              4|2|1, "modem",        p_bool },
  { pr_modem_usb,     pr_modem,       4|2|1, "modem.usb",    p_bool },
  { pr_parallel,      0,              4|2|1, "parallel",     p_bool },
  { pr_parallel_lp,   pr_parallel,    4|2|1, "parallel.lp",  p_bool },
  { pr_parallel_zip,  pr_parallel,    4|2|1, "parallel.zip", p_bool },
  { pr_parallel_imm,  0,                  0, "parallel.imm", p_bool },
  { pr_isa,           0,                  0, "isa",          p_bool },
  { pr_isa_isdn,      pr_isa,             0, "isa.isdn",     p_bool },
  { pr_isdn,          0,              4|2|1, "isdn",         p_bool },
  { pr_kbd,           0,            8|4|2|1, "kbd",          p_bool },
  { pr_prom,          0,            8|4|2|1, "prom",         p_bool },
  { pr_sbus,          0,            8|4|2|1, "sbus",         p_bool },
  { pr_int,           0,            8|4|2|1, "int",          p_bool },
#if defined(__i386__) || defined (__x86_64__)
  { pr_braille,       0,              4|2|1, "braille",      p_bool },
  { pr_braille_alva,  pr_braille,     4|2|1, "braille.alva", p_bool },
  { pr_braille_fhp,   pr_braille,     4|2|1, "braille.fhp",  p_bool },
  { pr_braille_ht,    pr_braille,     4|2|1, "braille.ht",   p_bool },
  { pr_braille_baum,  pr_braille,     4|2|1, "braille.baum", p_bool },
#else
  { pr_braille,       0,              4|2  , "braille",      p_bool },
  { pr_braille_alva,  pr_braille,         0, "braille.alva", p_bool },
  { pr_braille_fhp,   pr_braille,     4|2  , "braille.fhp",  p_bool },
  { pr_braille_ht,    pr_braille,     4|2  , "braille.ht",   p_bool },
  { pr_braille_baum,  pr_braille,     4|2  , "braille.baum", p_bool },
#endif
  { pr_ignx11,        0,                  0, "ignx11",       p_bool },
  { pr_sys,           0,            8|4|2|1, "sys",          p_bool },
  { pr_manual,        0,                  0, "manual",       p_bool },
  { pr_fb,            0,            8|4|2|1, "fb",           p_bool },
  { pr_pppoe,         0,            8|4|2|1, "pppoe",        p_bool },
  /* dummy, used to turn off hwscan */
  { pr_scan,          0,                  0, "scan",         p_bool },
  { pr_pcmcia,        0,            8|4|2|1, "pcmcia",       p_bool },
  { pr_fork,          0,                  0, "fork",         p_bool },
  { pr_cpuemu,        0,                  0, "cpuemu",       p_bool },
  { pr_cpuemu_debug,  pr_cpuemu,          0, "cpuemu.debug", p_bool },
  { pr_sysfs,         0,                  0, "sysfs",        p_bool },
  { pr_udev,          0,            8|4|2|1, "udev",         p_bool },
  { pr_block,         0,            8|4|2|1, "block",        p_bool },
  { pr_block_cdrom,   pr_block,     8|4|2|1, "block.cdrom",  p_bool },
  { pr_block_part,    pr_block,     8|4|2|1, "block.part",   p_bool },
  { pr_block_mods,    pr_block,     8|4|2|1, "block.mods",   p_bool },
  { pr_edd,           0,            8|4|2|1, "edd",          p_bool },
  { pr_edd_mod,       pr_edd,       8|4|2|1, "edd.mod",      p_bool },
  { pr_input,         0,            8|4|2|1, "input",        p_bool },
  { pr_wlan,          0,            8|4|2|1, "wlan",         p_bool },
  { pr_hal,           0,                  0, "hal",          p_bool },
  { pr_modules_pata,  0,                  0, "modules.pata", p_bool },
  { pr_x86emu,        0,                  0, "x86emu",       p_list },
};


/*
 * Returns pointer to probe feature struct for name.
 * If name is not a valid probe feature, NULL is returned.
 *
 */
pr_flags_t *pr_flags_by_name(char *name)
{
  unsigned u;

  if(!name) return NULL;

  for(u = 0; u < sizeof pr_flags / sizeof *pr_flags; u++) {
    if(!strcmp(name, pr_flags[u].name)) return pr_flags + u;
  }

  return NULL;
}


pr_flags_t *pr_flags_by_id(enum probe_feature feature)
{
  unsigned u;

  for(u = 0; u < sizeof pr_flags / sizeof *pr_flags; u++) {
    if(feature == pr_flags[u].val) return pr_flags + u;
  }

  return NULL;
}


int get_probe_val_int(hd_data_t *hd_data, enum probe_feature feature)
{
  pr_flags_t *flags;
  hal_prop_t *prop;

  flags = pr_flags_by_id(feature);
  if(flags) {
    prop = hal_get_int32(hd_data->probe_val, flags->name);
    if(prop) return prop->val.int32;
  }

  return 0;
}


char *get_probe_val_str(hd_data_t *hd_data, enum probe_feature feature)
{
  pr_flags_t *flags;
  hal_prop_t *prop;

  flags = pr_flags_by_id(feature);
  if(flags) {
    prop = hal_get_str(hd_data->probe_val, flags->name);
    if(prop) return prop->val.str;
  }

  return NULL;
}


str_list_t *get_probe_val_list(hd_data_t *hd_data, enum probe_feature feature)
{
  pr_flags_t *flags;
  hal_prop_t *prop;

  flags = pr_flags_by_id(feature);
  if(flags) {
    prop = hal_get_list(hd_data->probe_val, flags->name);
    if(prop) return prop->val.list;
  }

  return NULL;
}


int set_probe_val(hd_data_t *hd_data, enum probe_feature feature, char *val)
{
  hal_prop_t *prop;
  pr_flags_t *flags;
  char *s;
  int i, is_set = 0;

  if(!val) val = "";

  flags = pr_flags_by_id(feature);
  if(flags && flags->type != p_bool) {
    prop = hal_get_any(hd_data->probe_val, flags->name);
    if(!prop) {
      prop = hal_add_new(&hd_data->probe_val);
    }
    else {
      hal_invalidate(prop);
    }
    switch(flags->type) {
      case p_int32:
        i = strtol(val, &s, 0);
        if(!*s) {
          prop->type = flags->type;
          prop->key = new_str(flags->name);
          prop->val.int32 = i;
          is_set = i ? 1 : 0;
        }
        break;

      case p_string:
        prop->type = flags->type;
        prop->key = new_str(flags->name);
        prop->val.str = new_str(val);
        is_set = *val ? 1 : 0;
        break;

      case p_list:
        prop->type = flags->type;
        prop->key = new_str(flags->name);
        prop->val.list = *val ? hd_split(':', val) : NULL;
        is_set = prop->val.list ? 1 : 0;
        break;

      default:
        break;
    }
  }

  return is_set;
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
  pr_flags_t *pr;

  if(!(pr = pr_flags_by_id(feature))) return;

  if(pr->parent == -1u) {
    mask = pr->mask;
    for(i = 0; (unsigned) i < sizeof pr_flags / sizeof *pr_flags; i++) {
      if(pr_flags[i].parent != -1u && (pr_flags[i].mask & mask))
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
  pr_flags_t *pr;

  if(!(pr = pr_flags_by_id(feature))) return;

  if(pr->parent == -1u) {
    mask = pr->mask;
    for(i = 0; i < sizeof pr_flags / sizeof *pr_flags; i++) {
      if(pr_flags[i].parent != -1u && (pr_flags[i].mask & mask)) {
        hd_set_probe_feature(hd_data, pr_flags[i].val);
      }
    }
  }
  else {
    ofs = feature >> 3; bit = feature & 7;
    if(ofs < sizeof hd_data->probe) {
      hd_data->probe[ofs] |= 1 << bit;
    }
    if(pr->parent) hd_set_probe_feature(hd_data, pr->parent);
  }

  fix_probe_features(hd_data);
}


void hd_clear_probe_feature(hd_data_t *hd_data, enum probe_feature feature)
{
  unsigned ofs, bit, mask;
  int i;
  pr_flags_t *pr;

  if(!(pr = pr_flags_by_id(feature))) return;

  if(pr->parent == -1u) {
    mask = pr->mask;
    for(i = 0; (unsigned) i < sizeof pr_flags / sizeof *pr_flags; i++) {
      if(pr_flags[i].parent != -1u && (pr_flags[i].mask & mask))
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


void hd_set_probe_feature_hw(hd_data_t *hd_data, hd_hw_item_t item)
{
  hd_set_probe_feature(hd_data, pr_int);
//  hd_set_probe_feature(hd_data, pr_manual);

/*
 * note: pr_serial needs pr_pci
 */

  switch(item) {
    case hw_cdrom:
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_block_mods);
      hd_set_probe_feature(hd_data, pr_scsi);
      if(!hd_data->flags.fast) {
        hd_set_probe_feature(hd_data, pr_block_cdrom);
      }
      break;

    case hw_floppy:
      hd_set_probe_feature(hd_data, pr_floppy);
      hd_set_probe_feature(hd_data, pr_misc_floppy);
      hd_set_probe_feature(hd_data, pr_prom);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_block);
      hd_set_probe_feature(hd_data, pr_block_mods);
      hd_set_probe_feature(hd_data, pr_scsi);
      break;

    case hw_partition:
      hd_set_probe_feature(hd_data, pr_block_part);

    case hw_disk:
      hd_set_probe_feature(hd_data, pr_s390disks);
      hd_set_probe_feature(hd_data, pr_bios);		// bios disk order
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_block);
      hd_set_probe_feature(hd_data, pr_block_mods);
      hd_set_probe_feature(hd_data, pr_edd_mod);
      hd_set_probe_feature(hd_data, pr_scsi);
      break;

    case hw_block:
      hd_set_probe_feature(hd_data, pr_prom);
      hd_set_probe_feature(hd_data, pr_s390disks);
      hd_set_probe_feature(hd_data, pr_bios);		// bios disk order
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_block);
      hd_set_probe_feature(hd_data, pr_block_mods);
      hd_set_probe_feature(hd_data, pr_edd_mod);
      hd_set_probe_feature(hd_data, pr_scsi);
      if(!hd_data->flags.fast) {
        hd_set_probe_feature(hd_data, pr_floppy);
        hd_set_probe_feature(hd_data, pr_misc_floppy);
        hd_set_probe_feature(hd_data, pr_block_cdrom);
      }
      hd_set_probe_feature(hd_data, pr_block_part);
      break;

    case hw_network:
      hd_set_probe_feature(hd_data, pr_net);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_prom);
      hd_set_probe_feature(hd_data, pr_usb);
      break;

    case hw_display:
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_sbus);
      hd_set_probe_feature(hd_data, pr_prom);
      hd_set_probe_feature(hd_data, pr_misc);		/* for isa cards */
      break;

    case hw_monitor:
      /* if KMS is not active the data need to be read from BIOS */
      if (!is_kms_active(hd_data)) {
        hd_set_probe_feature(hd_data, pr_misc);
        hd_set_probe_feature(hd_data, pr_prom);
        hd_set_probe_feature(hd_data, pr_bios_ddc);
        // hd_set_probe_feature(hd_data, pr_bios_fb);
        hd_set_probe_feature(hd_data, pr_fb);
      }
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_monitor);
      break;

    case hw_framebuffer:
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_prom);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_bios_fb);
      hd_set_probe_feature(hd_data, pr_fb);
      break;

    case hw_mouse:
      hd_set_probe_feature(hd_data, pr_misc);
      if(!hd_data->flags.fast) {
        hd_set_probe_feature(hd_data, pr_serial);
      }
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_kbd);
      hd_set_probe_feature(hd_data, pr_sys);
      hd_set_probe_feature(hd_data, pr_bios);
      hd_set_probe_feature(hd_data, pr_mouse);
      hd_set_probe_feature(hd_data, pr_input);
      hd_set_probe_feature(hd_data, pr_pci);
      break;

    case hw_joystick:
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_input);
      break;

    case hw_chipcard:
      hd_set_probe_feature(hd_data, pr_misc);
      if(!hd_data->flags.fast) {
        hd_set_probe_feature(hd_data, pr_serial);
      }
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_mouse);		/* we need the pnp code */
      hd_set_probe_feature(hd_data, pr_pci);
      break;

    case hw_camera:
      hd_set_probe_feature(hd_data, pr_usb);
      break;

    case hw_keyboard:
      hd_set_probe_feature(hd_data, pr_cpu);
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_kbd);
      hd_set_probe_feature(hd_data, pr_input);
#ifdef __PPC__
      hd_set_probe_feature(hd_data, pr_serial);
      hd_set_probe_feature(hd_data, pr_pci);
#endif
      break;

    case hw_sound:
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_isapnp);
      hd_set_probe_feature(hd_data, pr_isapnp_mod);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_sbus);
      hd_set_probe_feature(hd_data, pr_prom);
      break;

    case hw_isdn:
      hd_set_probe_feature(hd_data, pr_misc);		/* get basic i/o res */
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_pcmcia);
      hd_set_probe_feature(hd_data, pr_isapnp);
      hd_set_probe_feature(hd_data, pr_isapnp_mod);
      // hd_set_probe_feature(hd_data, pr_isa_isdn);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_isdn);
      break;

    case hw_modem:
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_serial);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_modem);
      hd_set_probe_feature(hd_data, pr_modem_usb);
      break;

    case hw_storage_ctrl:
      hd_set_probe_feature(hd_data, pr_floppy);
      hd_set_probe_feature(hd_data, pr_sys);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_sbus);
      if(!hd_data->flags.fast) {
        hd_set_probe_feature(hd_data, pr_misc_par);
        hd_set_probe_feature(hd_data, pr_parallel_zip);
      }
      hd_set_probe_feature(hd_data, pr_s390);
      hd_set_probe_feature(hd_data, pr_prom);
#ifdef __PPC__
      hd_set_probe_feature(hd_data, pr_misc);
#endif
      break;

    case hw_network_ctrl:
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_net);
      hd_set_probe_feature(hd_data, pr_pcmcia);
      hd_set_probe_feature(hd_data, pr_isapnp);
      hd_set_probe_feature(hd_data, pr_isapnp_mod);
      hd_set_probe_feature(hd_data, pr_sbus);
      hd_set_probe_feature(hd_data, pr_isdn);
      hd_set_probe_feature(hd_data, pr_prom);
      hd_set_probe_feature(hd_data, pr_s390);
      hd_set_probe_feature(hd_data, pr_wlan);
      break;

    case hw_printer:
      hd_set_probe_feature(hd_data, pr_sys);
      hd_set_probe_feature(hd_data, pr_bios);
      hd_set_probe_feature(hd_data, pr_misc_par);
      hd_set_probe_feature(hd_data, pr_parallel_lp);
      hd_set_probe_feature(hd_data, pr_usb);
      break;

    case hw_wlan:
      hd_set_probe_feature(hd_data, pr_pcmcia);
      hd_set_probe_feature(hd_data, pr_wlan);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_net);
      break;

    case hw_tv:
    case hw_dvb:
      hd_set_probe_feature(hd_data, pr_pci);
      break;

    case hw_scanner:
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_scsi);
      break;

    case hw_braille:
      hd_set_probe_feature(hd_data, pr_misc_serial);
      hd_set_probe_feature(hd_data, pr_serial);
      hd_set_probe_feature(hd_data, pr_braille_alva);
      hd_set_probe_feature(hd_data, pr_braille_fhp);
      hd_set_probe_feature(hd_data, pr_braille_ht);
      hd_set_probe_feature(hd_data, pr_braille_baum);
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_pci);
      break;

    case hw_sys:
      hd_set_probe_feature(hd_data, pr_bios);
      hd_set_probe_feature(hd_data, pr_prom);
      hd_set_probe_feature(hd_data, pr_s390);
      hd_set_probe_feature(hd_data, pr_sys);
      break;

    case hw_cpu:
      hd_set_probe_feature(hd_data, pr_cpu);
      break;

    case hw_bios:
      hd_set_probe_feature(hd_data, pr_bios);
      hd_set_probe_feature(hd_data, pr_edd_mod);
      break;

    case hw_vbe:
      hd_set_probe_feature(hd_data, pr_bios_ddc);
      hd_set_probe_feature(hd_data, pr_bios_fb);
      hd_set_probe_feature(hd_data, pr_bios_mode);
      hd_set_probe_feature(hd_data, pr_monitor);
      break;

    case hw_manual:
      hd_set_probe_feature(hd_data, pr_manual);
      break;

    case hw_usb_ctrl:
    case hw_pcmcia_ctrl:
    case hw_ieee1394_ctrl:
    case hw_hotplug_ctrl:
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_pci);
      break;

    case hw_usb:
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_input);
      hd_set_probe_feature(hd_data, pr_isdn);	// need pr_misc, too?
      hd_set_probe_feature(hd_data, pr_block);
      hd_set_probe_feature(hd_data, pr_block_mods);
      hd_set_probe_feature(hd_data, pr_scsi);
      hd_set_probe_feature(hd_data, pr_net);
      hd_data->flags.fast = 1;
      break;

    case hw_pci:
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_net);
      hd_set_probe_feature(hd_data, pr_isdn);
      hd_set_probe_feature(hd_data, pr_prom);
      break;

    case hw_isapnp:
      hd_set_probe_feature(hd_data, pr_isapnp);
      hd_set_probe_feature(hd_data, pr_isapnp_mod);
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_isdn);
      break;

    case hw_bridge:
      hd_set_probe_feature(hd_data, pr_misc);
      hd_set_probe_feature(hd_data, pr_pci);
      break;

    case hw_hub:
      hd_set_probe_feature(hd_data, pr_usb); 
      break;

    case hw_memory:
      hd_set_probe_feature(hd_data, pr_memory); 
      break;

    case hw_scsi:
    case hw_tape:
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_block);
      hd_set_probe_feature(hd_data, pr_block_mods);
      hd_set_probe_feature(hd_data, pr_scsi);
      hd_set_probe_feature(hd_data, pr_usb);
      break;

    case hw_ide:
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_block);
      hd_set_probe_feature(hd_data, pr_block_mods);
      break;

    case hw_pppoe:
      hd_set_probe_feature(hd_data, pr_net);
      hd_set_probe_feature(hd_data, pr_pppoe);
      break;

    case hw_dsl:
      hd_set_probe_feature(hd_data, pr_net);
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_pppoe);
      hd_set_probe_feature(hd_data, pr_usb);
      break;

    case hw_pcmcia:
      hd_set_probe_feature(hd_data, pr_pci);
      hd_set_probe_feature(hd_data, pr_pcmcia);
      hd_set_probe_feature(hd_data, pr_wlan);
      hd_set_probe_feature(hd_data, pr_net);
      hd_set_probe_feature(hd_data, pr_isdn);
      break;

    case hw_bluetooth:
      hd_set_probe_feature(hd_data, pr_usb);
      hd_set_probe_feature(hd_data, pr_isdn);	// need pr_misc, too?
      break;

    case hw_fingerprint:
      hd_set_probe_feature(hd_data, pr_usb);
      break;

    case hw_all:
      hd_set_probe_feature(hd_data, pr_default);
      break;
    
    case hw_redasd:
      hd_set_probe_feature(hd_data, pr_block);
      hd_set_probe_feature(hd_data, pr_block_mods);
      break;

    case hw_none:
    case hw_unknown:
    case hw_ieee1394:
    case hw_hotplug:
    case hw_zip:
      break;
  }
}


/*
 * Free all data associated with a hd_data_t struct. *Not* the struct itself.
 */
hd_data_t *hd_free_hd_data(hd_data_t *hd_data)
{
  modinfo_t *p;
  unsigned u;

  add_hd_entry2(&hd_data->old_hd, hd_data->hd); hd_data->hd = NULL;
  hd_data->log = free_mem(hd_data->log);
  free_old_hd_entries(hd_data);		/* hd_data->old_hd */
  /* hd_data->pci is always NULL */
  /* hd_data->isapnp->card is always NULL */
  hd_data->isapnp = free_mem(hd_data->isapnp);
  /* hd_data->cdrom is always NULL */
  hd_data->net = free_str_list(hd_data->net);
  hd_data->floppy = free_str_list(hd_data->floppy);
  hd_data->misc = free_misc(hd_data->misc);
  /* hd_data->serial is always NULL */
  /* hd_data->scsi is always NULL */
  /* hd_data->ser_mouse is always NULL */
  /* hd_data->ser_modem is always NULL */
  hd_data->cpu = free_str_list(hd_data->cpu);
  hd_data->klog = free_str_list(hd_data->klog);
  hd_data->klog_raw = free_str_list(hd_data->klog_raw);
  hd_data->proc_usb = free_str_list(hd_data->proc_usb);
  /* hd_data->usb is always NULL */

  if((p = hd_data->modinfo)) {
    for(; p->type; p++) {
      free_mem(p->module);
      free_mem(p->alias);
    }
  }
  hd_data->modinfo = free_mem(hd_data->modinfo);
  if((p = hd_data->modinfo_ext)) {
    for(; p->type; p++) free_mem(p->module);
  }
  hd_data->modinfo = free_mem(hd_data->modinfo_ext);

  if(hd_data->hddb2[0]) {
    free_mem(hd_data->hddb2[0]->list);
    free_mem(hd_data->hddb2[0]->ids); 
    free_mem(hd_data->hddb2[0]->strings);
    hd_data->hddb2[0] = free_mem(hd_data->hddb2[0]);
  }
  /* hddb2[1] is the static internal database; don't try to free it! */
  hd_data->hddb2[1] = NULL;

  hd_data->kmods = free_str_list(hd_data->kmods);
  hd_data->bios_rom.data = free_mem(hd_data->bios_rom.data);
  hd_data->bios_ram.data = free_mem(hd_data->bios_ram.data);
  hd_data->bios_ebda.data = free_mem(hd_data->bios_ebda.data);
  hd_data->cmd_line = free_mem(hd_data->cmd_line);
  hd_data->xtra_hd = free_str_list(hd_data->xtra_hd);
  hd_data->devtree = free_devtree(hd_data);

#if 0
  // always NULL -> manual.c
  for(hd = hd_data->manual; hd; hd = next) {
    next = hd->next;
    hd->next = NULL;
    hd_free_hd_list(hd);
  }
  hd_data->manual = NULL;
#endif

  hd_data->disks = free_str_list(hd_data->disks);
  hd_data->partitions = free_str_list(hd_data->partitions);
  hd_data->cdroms = free_str_list(hd_data->cdroms);

  hd_data->smbios = smbios_free(hd_data->smbios);

  hd_data->udevinfo = hd_free_udevinfo(hd_data->udevinfo);
  hd_data->sysfsdrv = hd_free_sysfsdrv(hd_data->sysfsdrv);

  hd_data->only = free_str_list(hd_data->only);
  hd_data->scanner_db = free_str_list(hd_data->scanner_db);

  for(u = 0; u < sizeof hd_data->edd / sizeof *hd_data->edd; u++) {
    hd_data->edd[u].sysfs_id = free_mem(hd_data->edd[u].sysfs_id);
  }

  hd_data->hal = hd_free_hal_devices(hd_data->hal);

  hd_data->lsscsi = free_str_list(hd_data->lsscsi);

  hd_data->probe_val = hd_free_hal_properties(hd_data->probe_val);

  hd_data->last_idx = 0;

  hd_shm_done(hd_data);

  memset(hd_data, 0, sizeof *hd_data);

  return NULL;
}


/*
 * Free HAL property data.
 */
hal_prop_t *hd_free_hal_properties(hal_prop_t *prop)
{
  hal_prop_t *next;

  for(; prop; prop = next) {
    next = prop->next;

    free_mem(prop->key);
    if(prop->type == p_string) free_mem(prop->val.str);
    if(prop->type == p_list) free_str_list(prop->val.list);
    free_mem(prop);
  }

  return NULL;
}


/*
 * Free HAL data.
 */
hal_device_t *hd_free_hal_devices(hal_device_t *dev)
{
  hal_device_t *next;

  for(; dev; dev = next) {
    next = dev->next;

    free_mem(dev->udi);
    hd_free_hal_properties(dev->prop);
    free_mem(dev);
  }

  return NULL;
}


/*
 * Free all data associated with a driver_info_t struct. Even the struct itself.
 */
driver_info_t *free_driver_info(driver_info_t *di)
{
  driver_info_t *next;

  for(; di; di = next) {
    next = di->next;

    switch(di->any.type) {
      case di_any:
      case di_display:
        break;

      case di_module:
        free_str_list(di->module.names);
        free_str_list(di->module.mod_args);
        free_mem(di->module.conf);
        break;

      case di_mouse:
        free_mem(di->mouse.xf86);
        free_mem(di->mouse.gpm);
        break;

      case di_x11:
        free_mem(di->x11.server);
        free_mem(di->x11.xf86_ver);
        free_str_list(di->x11.extensions);
        free_str_list(di->x11.options);
        free_str_list(di->x11.raw);
        free_mem(di->x11.script);
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

      case di_dsl:
        free_mem(di->dsl.name);
        free_mem(di->dsl.mode);
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


int exists_hd_entry(hd_data_t *hd_data, hd_t *old_hd, hd_t *hd_ex)
{
  hd_t *hd;

  if(!hd_ex) return 0;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd == hd_ex) return 1;
  }
  for(hd = old_hd; hd; hd = hd->next) {
    if(hd == hd_ex) return 1;
  }

  return 0;
}


/*!
 * \note This may not free it.
 */
hd_t *hd_free_hd_list(hd_t *hd)
{
  hd_t *h;

  /* Note: hd->next should better be NULL! */
  if(hd && hd->tag.freeit) {
    free_hd_entry(hd);
    return free_mem(hd);
  }

  /* do nothing unless the list holds only copies of hd_t entries */
  for(h = hd; h; h = h->next) if(!h->ref) return NULL;

  for(; hd; hd = (h = hd)->next, free_mem(h));

  return NULL;
}

hd_detail_t *free_hd_detail(hd_detail_t *d)
{
  if(!d) return NULL;

  switch(d->type) {
    case hd_detail_pci: {
        pci_t *p = d->pci.data;

        free_mem(p->log);
        free_mem(p->sysfs_id);
        free_mem(p->sysfs_bus_id);
        free_mem(p);
      }
      break;

    case hd_detail_usb:
      {
        usb_t *u = d->usb.data;

        if(!u->cloned) {
          free_str_list(u->c);
          free_str_list(u->e);
        }
        free_str_list(u->d);
        free_str_list(u->p);
        free_str_list(u->s);
        free_str_list(u->t);
        free_str_list(u->i);

        free_mem(u->manufact);
        free_mem(u->product);
        free_mem(u->serial);
        free_mem(u->driver);
        free_mem(u->raw_descr.data);

        free_mem(u);
      }
      break;

    case hd_detail_isapnp:
      {
        isapnp_dev_t *i = d->isapnp.data;
        int j;

        if(!i->ref) {
          free_mem(i->card->serial);
          free_mem(i->card->card_regs);
          free_mem(i->card->ldev_regs);
          for(j = 0; j < i->card->res_len; j++) {
            free_mem(i->card->res[j].data);
          }
          if(i->card->res) free_mem(i->card->res);
        }
        free_mem(i->card);
        free_mem(i);
      }
      break;

    case hd_detail_cdrom:
      {
        cdrom_info_t *c = d->cdrom.data;

        if(c) {
          free_mem(c->name);
          free_mem(c->iso9660.volume);
          free_mem(c->iso9660.publisher);
          free_mem(c->iso9660.preparer);
          free_mem(c->iso9660.application);
          free_mem(c->iso9660.creation_date);
          free_mem(c->el_torito.id_string);
          free_mem(c->el_torito.label);

          free_mem(c);
        }
      }
      break;

    case hd_detail_floppy:
      free_mem(d->floppy.data);
      break;

    case hd_detail_bios:
      {
        bios_info_t *b = d->bios.data;

        free_mem(b->vbe.oem_name);
        free_mem(b->vbe.vendor_name);
        free_mem(b->vbe.product_name);
        free_mem(b->vbe.product_revision);
        free_mem(b->vbe.mode);
        free_mem(b->lcd.vendor);
        free_mem(b->lcd.name);
        free_mem(b->mouse.vendor);
        free_mem(b->mouse.type);

        free_mem(b);
      }
      break;

    case hd_detail_cpu:
      {
        cpu_info_t *c = d->cpu.data;

        free_mem(c->vend_name);
        free_mem(c->model_name);
        free_mem(c->platform);
        free_str_list(c->features);
        free_mem(c);
      }
      break;

    case hd_detail_prom:
      free_mem(d->prom.data);
      break;

    case hd_detail_monitor:
      {
        monitor_info_t *m;
        hd_detail_monitor_t *mdetail, *next;

        for(mdetail = &d->monitor; mdetail; mdetail = next) {
          next = mdetail->next;
          m = mdetail->data;

          free_mem(m->vendor);
          free_mem(m->name);
          free_mem(m->serial);

          free_mem(m);

          if(mdetail != &d->monitor) free_mem(mdetail);
        }
      }
      break;

    case hd_detail_sys:
      {
        sys_info_t *s = d->sys.data;

        free_mem(s->system_type);
        free_mem(s->generation);
        free_mem(s->vendor);
        free_mem(s->model);
        free_mem(s->serial);
        free_mem(s->lang);

        free_mem(s);
      }
      break;

    case hd_detail_scsi:
      free_scsi(d->scsi.data, 1);
      break;

    case hd_detail_devtree:
      /* is freed with hd_data->dev_tree */
      break;

    case hd_detail_ccw:
      free_mem(d->ccw.data);
      break;

    case hd_detail_joystick:
      free_mem(d->joystick.data);
      break;
  }

  free_mem(d);

  return NULL;
}


hd_t *free_hd_entry(hd_t *hd)
{
  free_mem(hd->bus.name);
  free_mem(hd->base_class.name);
  free_mem(hd->sub_class.name);
  free_mem(hd->prog_if.name);
  free_mem(hd->vendor.name);
  free_mem(hd->device.name);
  free_mem(hd->sub_vendor.name);
  free_mem(hd->sub_device.name);
  free_mem(hd->revision.name);
  free_mem(hd->serial);
  free_mem(hd->compat_vendor.name);
  free_mem(hd->compat_device.name);
  free_mem(hd->model);
  free_mem(hd->sysfs_id);
  free_mem(hd->sysfs_bus_id);
  free_mem(hd->sysfs_device_link);
  free_str_list(hd->unix_dev_names);
  free_mem(hd->unix_dev_name);
  free_mem(hd->unix_dev_name2);
  free_mem(hd->rom_id);
  free_mem(hd->unique_id);
  free_mem(hd->udi);
  free_mem(hd->block0);
  free_mem(hd->driver);
  free_str_list(hd->drivers);
  free_str_list(hd->driver_modules);
  free_mem(hd->old_unique_id);
  free_mem(hd->unique_id1);
  free_mem(hd->usb_guid);
  free_mem(hd->parent_id);
  free_str_list(hd->child_ids);
  free_mem(hd->config_string);
  free_str_list(hd->extra_info);

  free_res_list(hd->res);

  free_hd_detail(hd->detail);

  free_driver_info(hd->driver_info);
  free_str_list(hd->requires);

  free_mem(hd->modalias);

  hd_free_hal_properties(hd->hal_prop);
  hd_free_hal_properties(hd->persistent_prop);

  memset(hd, 0, sizeof *hd);

  return NULL;
}

misc_t *free_misc(misc_t *m)
{
  int i, j;

  if(!m) return NULL;

  for(i = 0; (unsigned) i < m->io_len; i++) {
    free_mem(m->io[i].dev);
  }
  free_mem(m->io);

  for(i = 0; (unsigned) i < m->dma_len; i++) {
    free_mem(m->dma[i].dev);
  }
  free_mem(m->dma);

  for(i = 0; (unsigned) i < m->irq_len; i++) {
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
    free_mem(scsi->usb_guid);
    free_str_list(scsi->host_info);
    free_mem(scsi->controller_id);

    if(!free_all) {
      next = scsi->next;
      memset(scsi, 0, sizeof *scsi);
      scsi->next = next;
      break;
    }

    free_mem(scsi);
  }

  return NULL;
}


// FIXME: obsolete
hd_manual_t *hd_free_manual(hd_manual_t *manual)
{
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

    if(exists_hd_entry(hd_data, next, hd->ref) && hd->ref->ref_cnt) hd->ref->ref_cnt--;

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

  if(p) return p;

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

  t = strdup(s);

  if(t) return t;

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
	    break;
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
  hd_res_t *next;

  for(; res; res = next) {
    next = res->next;

    if(res->any.type == res_init_strings) {
      free_mem(res->init_strings.init1);
      free_mem(res->init_strings.init2);
    }

    if(res->any.type == res_pppd_option) {
      free_mem(res->pppd_option.option);
    }

    if(res->any.type == res_hwaddr) {
      free_mem(res->hwaddr.addr);
    }

    if(res->any.type == res_wlan) {
      free_str_list(res->wlan.channels);
      free_str_list(res->wlan.frequencies);
      free_str_list(res->wlan.bitrates);
      free_str_list(res->wlan.auth_modes);
      free_str_list(res->wlan.enc_modes);
    }

    if(res->any.type == res_fc) {
      free_mem(res->fc.controller_id);
    }

    free_mem(res);
  }

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
  hd_t *hd, *hd2;
  uint64_t irqs;
  str_list_t *sl, *sl0;
  pr_flags_t *pf;

  if(!hd_data->flags.internal) {
  /* log debug & probe flags */
    if(hd_data->debug) {
      ADD2LOG("libhd version %s%s (%s) [%ld]\n", HD_VERSION_STRING, getuid() ? "u" : "", HD_ARCH, sizeof (hd_data_t));
    }

    ADD2LOG("using %s\n", hd_get_hddb_dir());
  }

  get_kernel_version(hd_data);

  /* needed only on 1st call */
  if(hd_data->last_idx == 0) {
    get_probe_env(hd_data);
  }

  /* init driver info database */
  hddb_init(hd_data);

  /* only first time */
  if(hd_data->last_idx == 0) {
    hd_set_probe_feature(hd_data, pr_fork);
    if(!hd_probe_feature(hd_data, pr_fork)) hd_data->flags.nofork = 1;
//    hd_set_probe_feature(hd_data, pr_sysfs);
    if(!hd_probe_feature(hd_data, pr_sysfs)) hd_data->flags.nosysfs = 1;
    hd_set_probe_feature(hd_data, pr_cpuemu);
    if(hd_probe_feature(hd_data, pr_cpuemu)) hd_data->flags.cpuemu = 1;
    if(hd_probe_feature(hd_data, pr_udev)) hd_data->flags.udev = 1;
    if(!hd_probe_feature(hd_data, pr_bios_crc)) hd_data->flags.nobioscrc = 1;
    hd_set_probe_feature(hd_data, pr_bios_vram);
    if(hd_probe_feature(hd_data, pr_bios_vram)) hd_data->flags.biosvram = 1;
    hd_set_probe_feature(hd_data, pr_bios_acpi);
    hd_set_probe_feature(hd_data, pr_modules_pata);
    hd_data->flags.pata = hd_probe_feature(hd_data, pr_modules_pata) ? 1 : 0;
    hd_set_probe_feature(hd_data, pr_x86emu);
    if(!get_probe_val_list(hd_data, pr_x86emu)) {
      set_probe_val(hd_data, pr_x86emu, "dump");
    }
  }

  fix_probe_features(hd_data);

  if(hd_data->debug && !hd_data->flags.internal) {
    for(i = sizeof hd_data->probe - 1; i >= 0; i--) {
      str_printf(&s, -1, "%02x", hd_data->probe[i]);
    }
    ADD2LOG("debug = 0x%x\nprobe = 0x%s (", hd_data->debug, s);
    s = free_mem(s);

    for(i = 1; i < pr_default; i++) {		/* 1 because of pr_memory */
      pf = pr_flags_by_id(i);
      if(pf) {
        j = hd_probe_feature(hd_data, i);
        ADD2LOG("%s%c%s", i == 1 ? "" : " ", j ? '+' : '-', pf->name);
        switch(pf->type) {
          case p_int32:
            ADD2LOG("=%d", get_probe_val_int(hd_data, i));
            break;

          case p_string:
            ADD2LOG("=%s", get_probe_val_str(hd_data, i) ?: "");
            break;

          case p_list:
            s = hd_join(":", get_probe_val_list(hd_data, i));
            ADD2LOG("=%s", s ?: "");
            s = free_mem(s);
            break;

          default:
            break;
        }
      }
    }

    ADD2LOG(")\n");
  }

  /* get shm segment, if we didn't do it already */
  hd_shm_init(hd_data);

  if(!hd_data->shm.ok && !hd_data->flags.nofork) {
    hd_data->flags.nofork = 1;
    ADD2LOG("shm: failed to get shm segment; will not fork\n");
  }

  if(hd_data->only) {
    s = hd_join(", ", hd_data->only);
    ADD2LOG("only: %s\n", s);
    s = free_mem(s);
  }

  /*
   * There might be old 'manual' entries left from an earlier scan. Remove
   * them, they will confuse us.
   */
  hd_data->module = mod_manual;
  remove_hd_entries(hd_data);

  hd_scan_with_hal(hd_data);

  if(!hd_data->hal) {
    hd_scan_hal_basic(hd_data);
    hd_scan_no_hal(hd_data);
  }

  hd_scan_int(hd_data);

  /* and again... */
  for(hd = hd_data->hd; hd; hd = hd->next) hd_add_id(hd_data, hd);

  /* assign parent & child ids */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    hd->child_ids = free_str_list(hd->child_ids);
    if((hd2 = hd_get_device_by_idx(hd_data, hd->attached_to))) {
      free_mem(hd->parent_id);
      hd->parent_id = new_str(hd2->unique_id);
    }
    else if((hd2 = hd_get_device_by_id(hd_data, hd->parent_id))) {
      hd->attached_to = hd2->idx;
    }
    else {
      hd->attached_to = 0;
    }
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if((hd2 = hd_get_device_by_idx(hd_data, hd->attached_to))) {
      add_str_list(&hd2->child_ids, hd->unique_id);
    }
  }

  /* assign a hw_class & build a useful model string */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    assign_hw_class(hd_data, hd);

    /* create model name _after_ hw_class */
    create_model_name(hd_data, hd);
  }

#ifndef LIBHD_TINY
  /* must be _after_ we have valid hw_class entries */
  hd_scan_manual2(hd_data);
#endif

  /* we are done... */
  for(hd = hd_data->hd; hd; hd = hd->next) hd->tag.fixed = 1;

  hd_data->module = mod_none;

  if(hd_data->debug && !hd_data->flags.internal && hd_data->klog) {
    dump_klog(hd_data);
  }

  if(
    hd_data->debug &&
    !hd_data->flags.internal &&
    (
      hd_data->kmods ||
      hd_probe_feature(hd_data, pr_int /* arbitrary; just avoid /proc/modules for -pr_all */)
    )
  ) {
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
}


void hd_scan_with_hal(hd_data_t *hd_data)
{
  hd_t *hd;

  hd_data->hal = hd_free_hal_devices(hd_data->hal);

  hd_scan_hal(hd_data);

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(!hd->persistent_prop) hd->persistent_prop = hd_read_properties(hd->udi);
  }

}


void hd_scan_no_hal(hd_data_t *hd_data)
{
  hd_t *hd;

  /*
   * for various reasons, do it befor scan_misc()
   */
  hd_scan_floppy(hd_data);

  /*
   * to be able to read the right parport io,
   * we have to do this before scan_misc()
   */
#if defined(__i386__) || defined (__x86_64__) || defined (__ia64__)
  hd_scan_bios(hd_data);
#endif
  
  /* before hd_scan_misc(): we need some ppc info later */
  hd_scan_sys(hd_data);

  /* get basic system info */
  hd_scan_misc(hd_data);

  /* hd_scan_cpu() after hd_scan_misc(): klog needed */
  hd_scan_cpu(hd_data);
  hd_scan_memory(hd_data);

  hd_scan_sysfs_pci(hd_data);

  /* do it _after_ hd_scan_sysfs_pci() */
#if defined(__PPC__)
  hd_scan_prom(hd_data);
#endif

#if defined(__s390__) || defined(__s390x__)
  hd_scan_s390disks(hd_data);
  hd_scan_s390(hd_data);
#endif

  /* after hd_scan_prom() and hd_scan_bios() */
  hd_scan_monitor(hd_data);

#ifndef LIBHD_TINY
#if defined(__i386__) || defined(__alpha__)
  hd_scan_isapnp(hd_data);
#endif
#endif

#ifndef LIBHD_TINY
#if defined(__i386__)
  hd_scan_isa(hd_data);
#endif
#endif

  /* after pci & isa */
  hd_scan_pcmcia(hd_data);

  /* after pci */
  hd_scan_serial(hd_data);

  /* merge basic system info & the easy stuff */
  hd_scan_misc2(hd_data);

#ifndef LIBHD_TINY
  if(!hd_data->flags.no_parport) {
    hd_scan_parallel(hd_data);	/* after hd_scan_misc*() */
  }
#endif

  hd_scan_sysfs_block(hd_data);
  hd_scan_sysfs_scsi(hd_data);
  hd_scan_sysfs_usb(hd_data);
#if defined(__i386__) || defined(__x86_64__)
  hd_scan_sysfs_edd(hd_data);
#endif

#ifndef LIBHD_TINY
#if !defined(__sparc__)
  hd_scan_braille(hd_data);
#endif
  hd_scan_modem(hd_data);	/* do it before hd_scan_mouse() */
  hd_scan_mouse(hd_data);
#endif
  hd_scan_sbus(hd_data);

  hd_scan_input(hd_data);

#if !defined(__s390__) && !defined(__s390x__)
  hd_scan_kbd(hd_data);
#endif

  /* must be after hd_scan_monitor() */
  hd_scan_fb(hd_data);

  /* keep these at the end of the list */
  hd_scan_net(hd_data);

  hd_scan_pppoe(hd_data);

#ifndef LIBHD_TINY
  hd_scan_wlan(hd_data);
#endif

  for(hd = hd_data->hd; hd; hd = hd->next) hd_add_id(hd_data, hd);

  hd_scan_hal_assign_udi(hd_data);

#ifndef LIBHD_TINY
  hd_scan_manual(hd_data);
#endif

  /* add test entries */
  hd_scan_xtra(hd_data);

  /* some final fixup's */
#if WITH_ISDN
  hd_scan_isdn(hd_data);
  hd_scan_dsl(hd_data);
#endif

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


/*
 *  Must _not_ check that s is exactly 3 chars.
 */
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
  char *m2, *m1, *m0;
  int i;

  if(len < 0) len = 0;		/* just to be safe */

  m0 = new_mem(len + 1);

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
hd_t *hd_get_device_by_idx(hd_data_t *hd_data, unsigned idx)
{
  hd_t *hd;

  if(!idx) return NULL;		/* early out: idx is always != 0 */

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->idx == idx) return hd;
  }

  return NULL;
}


/*
 * find hardware entry with given unique id
 */
hd_t *hd_get_device_by_id(hd_data_t *hd_data, char *id)
{
  hd_t *hd;

  if(!id) return NULL;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->unique_id && !strcmp(hd->unique_id, id)) return hd;
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


void hd_log(hd_data_t *hd_data, char *buf, ssize_t len)
{
  if (!hd_data) return;
  ssize_t new_size;
  char *p;

  if(len <= 0 || !buf) return;

  if(hd_data->log_size + len + 1 > hd_data->log_max) {
    new_size = hd_data->log_max + len + (1 << 20);
    new_size += new_size / 2;
    p = realloc(hd_data->log, new_size);
    if(p) {
      hd_data->log = p;
      hd_data->log_max = new_size;
    }
  }

  if(hd_data->log) {
    memcpy(hd_data->log + hd_data->log_size, buf, len);
    hd_data->log_size += len;
    hd_data->log[hd_data->log_size] = 0;
  }
}


void hd_log_printf(hd_data_t *hd_data, char *format, ...)
{
  ssize_t l;
  char *s = NULL;
  va_list args;

  va_start(args, format);
  l = vasprintf(&s, format, args);
  va_end(args);

  hd_log(hd_data, s, l);

  free(s);
}


void hd_log_hex(hd_data_t *hd_data, int with_ascii, unsigned data_len, unsigned char *data)
{
  char *buf = NULL;

  hexdump(&buf, with_ascii, data_len, data);

  if(buf) hd_log(hd_data, buf, strlen(buf));

  free(buf);
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
  char b[0x10000];
  va_list args;

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


/** \relates s_str_list_t
 * Search a string list for a string.
 */
str_list_t *search_str_list(str_list_t *sl, char *str)
{
  if(!str) return NULL;

  for(; sl; sl = sl->next) if(sl->str && !strcmp(sl->str, str)) return sl;

  return NULL;
}


/** \relates s_str_list_t
 * Add a string to a string list.
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


/** \relates s_str_list_t
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


/** \relates s_str_list_t
 * Reverse string list.
 */
str_list_t *reverse_str_list(str_list_t *list)
{
  str_list_t *sl, *sl_new = NULL, *next;

  for(sl = list; sl; sl = next) {
    next = sl->next;
    sl->next = sl_new;
    sl_new = sl;
  }

  return sl_new;
}


/*
 * Read a file; return a linked list of lines.
 *
 * start_line is zero-based; lines == 0 -> all lines
 */
str_list_t *read_file(char *file_name, unsigned start_line, unsigned lines)
{
  FILE *f;
  char buf[0x10000];
  int pipe = 0;
  str_list_t *sl_start = NULL, *sl_end = NULL, *sl;

  if(*file_name == '|') {
    pipe = 1;
    file_name++;
    if(!(f = popen(file_name, "r"))) {
      return NULL;
    }
  }
  else {
    if(!(f = fopen(file_name, "r"))) {
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

  return sl_start;
}


/*
 * Read directory, return a list of entries with file type 'type'.
 */
str_list_t *read_dir(char *dir_name, int type)
{
  str_list_t *sl_start = NULL, *sl_end = NULL, *sl;
  DIR *dir;
  struct dirent *de;
  struct stat sbuf;
  char *s;
  int dir_type, link_allowed = 0;

  if(type == 'D') {
    type = 'd';
    link_allowed = 1;
  }

  if(dir_name && (dir = opendir(dir_name))) {
    while((de = readdir(dir))) {
      if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
      dir_type = 0;

      if(type) {
        s = NULL;
        str_printf(&s, 0, "%s/%s", dir_name, de->d_name);

        if(!lstat(s, &sbuf)) {
          if(S_ISDIR(sbuf.st_mode)) {
            dir_type = 'd';
          }
          else if(S_ISREG(sbuf.st_mode)) {
            dir_type = 'r';
          }
          else if(S_ISLNK(sbuf.st_mode)) {
            dir_type = 'l';
          }
        }

        s = free_mem(s);
      }

      if(dir_type == type || (link_allowed && dir_type == 'l')) {
        sl = new_mem(sizeof *sl);
        sl->str = new_str(de->d_name);
        if(sl_start)
          sl_end->next = sl;
        else
          sl_start = sl;
        sl_end = sl;
      }
    }
    closedir(dir);
  }

  return sl_start;
}


char *hd_read_sysfs_link(char *base_dir, char *link_name)
{
  char *s = NULL;
  static char *buf = NULL;

  if(!base_dir || !link_name) return NULL;

  str_printf(&s, 0, "%s/%s", base_dir, link_name);

  free_mem(buf);
  buf = realpath(s, NULL);

  free_mem(s);

  return buf;
}


/*
 * Log the hardware detection progress.
 */
void progress(hd_data_t *hd_data, unsigned pos, unsigned count, char *msg)
{
  if (!hd_data) return;

  char buf1[32], buf2[32], buf3[128], *fn;

  if(hd_data->shm.ok && hd_data->flags.forked) {
    ((hd_data_t *) (hd_data->shm.data))->shm.updated++;
  }

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
  pr_flags_t *flags;

  flags = pr_flags_by_name(name);

  return flags ? flags->val : 0;
}


/*
 * Coverts a enum probe_feature to a string.
 * If it fails, NULL is returned.
 */
char *hd_probe_feature_by_value(enum probe_feature feature)
{
  pr_flags_t *flags;

  flags = pr_flags_by_id(feature);

  return flags ? flags->name : NULL;
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


int hd_module_is_active(hd_data_t *hd_data, char *mod)
{
  str_list_t *sl, *sl0 = read_kmods(hd_data);
  int active = 0;
  char *s;
#ifdef __PPC__
  char *s1, *s2;
#endif

  mod = new_str(mod);

  /* convert '-' to '_' */
  for(s = mod; *s; s++) if(*s == '-') *s = '_';

  for(sl = sl0; sl; sl = sl->next) {
    if(!strcmp(sl->str, mod)) break;
  }

  free_str_list(sl0);
  active = sl ? 1 : 0;

  if(active) {
    free_mem(mod);

    return active;
  }

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

  free_mem(mod);

  return active;
}


int hd_has_pcmcia(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(is_pcmcia_ctrl(hd_data, hd)) return 1;
  }

  return 0;
}


int hd_apm_enabled(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_internal &&
      hd->sub_class.id == sc_int_bios &&
      hd->detail &&
      hd->detail->type == hd_detail_bios &&
      hd->detail->bios.data
    ) {
      return hd->detail->bios.data->apm_enabled;
    }
  }

  return 0;
}


int hd_usb_support(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class.id == bc_serial && hd->sub_class.id == sc_ser_usb) {
      for(res = hd->res; res; res = res->next) {
        if(res->any.type == res_irq)
          return hd->prog_if.id == pif_usb_ohci ? 2 : 1;	/* 2: ohci, 1: uhci */
      }
    }
  }

  return 0;
}


int hd_smp_support(hd_data_t *hd_data)
{
  int is_smp = 0;
  unsigned u;
  hd_t *hd, *hd0;
#if defined(__i386__) || defined (__x86_64__)
  unsigned cpu_threads = 0;
  cpu_info_t *ct;
#endif

  u = hd_data->flags.internal;
  hd_data->flags.internal = 1;
  hd = hd_list(hd_data, hw_cpu, 0, NULL);
  if(!hd) hd = hd_list(hd_data, hw_cpu, 1, NULL);
  hd_data->flags.internal = u;

  for(is_smp = 0, hd0 = hd; hd0; hd0 = hd0->next) is_smp++;
  if(is_smp == 1) is_smp = 0;

#if defined(__i386__) || defined (__x86_64__)
  if(
    hd &&
    hd->detail &&
    hd->detail->type == hd_detail_cpu &&
    (ct = hd->detail->cpu.data)
  ) {
    cpu_threads = ct->units;
  }
#endif

  hd = hd_free_hd_list(hd);

#if !defined(LIBHD_TINY) && (defined(__i386__) || defined (__x86_64__))
  if(is_smp < 2) {
    if(!hd_data->bios_ram.data) {
      hd_free_hd_list(hd_list(hd_data, hw_sys, 1, NULL));
    }
    is_smp = detect_smp_bios(hd_data);
    // at least 2 processors
    if(is_smp < 2) is_smp = 0;
    if(!is_smp && cpu_threads > 1) is_smp = 2;
  }
#endif

#ifdef __PPC__
  if(is_smp < 2) {
    if(!hd_data->devtree) {
      hd_free_hd_list(hd_list(hd_data, hw_sys, 1, NULL));
    }
    is_smp = detect_smp_prom(hd_data);
    if(is_smp < 0) is_smp = 0;
  }
#endif

#if defined(__s390__) || defined(__s390x__)
  if(!is_smp) is_smp = 1;
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
      hd->base_class.id == bc_internal && hd->sub_class.id == sc_int_prom &&
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
  unsigned disp, disp_sbus, disp_pci, disp_any, disp_di;
  unsigned disp_cnt, disp_any_cnt;

  /* if we know exactly where our primary display is, return it */
  if(hd_get_device_by_idx(hd_data, hd_data->display)) return hd_data->display;

  disp = disp_sbus = disp_pci = disp_any = disp_di = 0;
  disp_cnt = disp_any_cnt = 0;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class.id == bc_display) {
      disp_any_cnt++;
      if(!disp_any) disp_any = hd->idx;
      if(hd->sub_class.id == sc_dis_vga) {
        disp_cnt++;
        if(!disp) disp = hd->idx;
        if(hd->bus.id == bus_pci && !disp_pci) disp_pci = hd->idx;
        if(hd->bus.id == bus_sbus && !disp_sbus) disp_sbus = hd->idx;
      }
      if(!disp_di) {
        if(!(di = hd->driver_info)) {
          hddb_add_info(hd_data, hd);
          di = hd->driver_info;
        }
        if(di && di->any.type == di_x11 && di->x11.server) {
          disp_di = hd->idx;
        }
      }
    }
  }

  /* if there's only one display adapter, return it */
  if(disp_any_cnt == 1) return disp_any;

  /* if there's only one vga compatible adapter, return it */
  if(disp_cnt == 1) return disp;

  /* return 1st (vga compatible) sbus card */
  /* note: the sbus code enters display cards as 'vga compatible' */
  if(disp_sbus) return disp_sbus;

  /* return 1st display adapter that has some x11 info */
  if(disp_di) return disp_di;

  /* return 1st vga compatible pci card */
  if(disp_pci) return disp_pci;

  /* return 1st vga compatible card */
  if(disp) return disp;

  /* return 1st display adapter */
  if(disp_any) return disp_any;

  /* there were none... */
  return 0;
}


enum cpu_arch hd_cpu_arch(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_internal &&
      hd->sub_class.id == sc_int_cpu &&
      hd->detail &&
      hd->detail->type == hd_detail_cpu &&
      hd->detail->cpu.data
    ) {
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
#ifdef __s390x__
  return arch_s390x;
#else
#ifdef __s390__
  return arch_s390;
#else
#ifdef __ia64__
  return arch_ia64;
#else
#ifdef __x86_64__
  return arch_x86_64;
#else
#ifdef __arm__
  return arch_arm;
#else
#ifdef __aarch64__
  return arch_aarch64;
#elif defined __m68k__
  return arch_68k;
#else
  return arch_unknown;
#endif
#endif
#endif
#endif
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


int hd_is_uml(hd_data_t *hd_data)
{
  int is_uml = 0;
  hd_t *hd;
  cpu_info_t *ct;
  unsigned u;
  unsigned saved_mod = hd_data->module;
  unsigned char probe_save[sizeof hd_data->probe];

  u = hd_data->flags.internal;
  hd_data->flags.internal = 1;
  hd = hd_list(hd_data, hw_cpu, 0, NULL);
  if(!hd) {
    /* Do *not* run hd_list(,, 1,) here! */
    memcpy(probe_save, hd_data->probe, sizeof probe_save);
    hd_set_probe_feature(hd_data, pr_cpu);
    hd_scan_cpu(hd_data);
    memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->base_class.id == bc_internal && hd->sub_class.id == sc_int_cpu) break;
    }
  }
  hd_data->flags.internal = u;

  if(
    hd &&
    hd->detail &&
    hd->detail->type == hd_detail_cpu &&
    (ct = hd->detail->cpu.data) &&
    ct->model_name &&
    !strcmp(ct->model_name, "UML")
  ) {
    is_uml = 1;
  }

  hd = hd_free_hd_list(hd);

  hd_data->module = saved_mod;

  return is_uml;
}


int hd_is_sgi_altix(hd_data_t *hd_data)
{
  struct stat sbuf;

  return stat("/proc/sgi_sn", &sbuf) ? 0 : 1;
}




/*
 * check for xen hypervisor
 */
int hd_is_xen(hd_data_t *hd_data)
{
#if defined(__i386__) || defined(__x86_64__)

  char signature[13];
  unsigned u, foo;

  __asm__(
#ifdef __i386__
    "push %%ebx\n\t"
    "cpuid\n\t"
    "mov %%ebx,(%%esi)\n\t"
    "mov %%ecx,4(%%esi)\n\t"
    "mov %%edx,8(%%esi)\n\t"
    "pop %%ebx"
#else
    "push %%rbx\n\t"
    "cpuid\n\t"
    "mov %%ebx,(%%rsi)\n\t"
    "mov %%ecx,4(%%rsi)\n\t"
    "mov %%edx,8(%%rsi)\n\t"
    "pop %%rbx"
#endif
    : "=a" (u), "=c" (foo)
    : "a" (0x40000000), "c" (0), "S" (signature)
    : "%edx"
  );

  signature[12] = 0;

  return u < 0x40000002 || strcmp(signature, "XenVMMXenVMM") ? 0 : 1;

#else

  return 0;

#endif
}


/*
 * makes a (shallow) copy; does some magic fixes
 */
void hd_copy(hd_t *dst, hd_t *src)
{
  hd_t *tmp;
//  unsigned u;

  tmp = dst->next;
//  u = dst->idx;

  *dst = *src;
  src->ref_cnt++;
  dst->ref = src;

  dst->next = tmp;
//  dst->idx = u;

  /* needed to keep in sync with the real device tree */
  if(
    dst->detail &&
    dst->detail->type == hd_detail_devtree
  ) {
    dst->detail = NULL;		/* ??? was: free_mem(dst->detail); */
  }
}


hd_t *hd_list(hd_data_t *hd_data, hd_hw_item_t item, int rescan, hd_t *hd_old)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  unsigned char probe_save[sizeof hd_data->probe];
  unsigned fast_save;

  if(rescan) {
    memcpy(probe_save, hd_data->probe, sizeof probe_save);
    fast_save = hd_data->flags.fast;
    hd_clear_probe_feature(hd_data, pr_all);
#ifdef __powerpc__
    hd_set_probe_feature(hd_data, pr_sys);
    hd_scan(hd_data);
#endif
    hd_set_probe_feature_hw(hd_data, item);
    hd_scan(hd_data);
    memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);
    hd_data->flags.fast = fast_save;
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(!hd_report_this(hd_data, hd)) continue;

    if(
      (
        item == hw_manual || hd_is_hw_class(hd, item)
      )
#ifndef LIBHD_TINY
/* with LIBHD_TINY hd->status is not maintained (cf. manual.c) */
      && (
        hd_data->hal ||
        hd->status.available == status_yes ||
        hd->status.available == status_unknown ||
        item == hw_manual ||
        hd_data->flags.list_all
      )
#endif
    ) {
//      if(hd->is.softraiddisk) continue;		/* don't report them */

      /* don't report old entries again */
      for(hd1 = hd_old; hd1; hd1 = hd1->next) {
        if(!cmp_hd(hd1, hd)) break;
      }
      if(!hd1) {
        hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
        hd_copy(hd1, hd);
      }
    }
  }

  if(item == hw_manual) {
    for(hd = hd_list; hd; hd = hd->next) {
      hd->status.available = hd->status.available_orig;
    }
  }

  return hd_list;
}


hd_t *hd_list_with_status(hd_data_t *hd_data, hd_hw_item_t item, hd_status_t status)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  unsigned char probe_save[sizeof hd_data->probe];

  memcpy(probe_save, hd_data->probe, sizeof probe_save);
  hd_clear_probe_feature(hd_data, pr_all);
//  hd_set_probe_feature(hd_data, pr_manual);
  hd_scan(hd_data);
  memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd_is_hw_class(hd, item)) {
      if(
        (status.configured == 0 || status.configured == hd->status.configured) &&
        (status.available == 0 || status.available == hd->status.available) &&
        (status.needed == 0 || status.needed == hd->status.needed) &&
        (status.reconfig == 0 || status.reconfig == hd->status.reconfig)
      ) {
        hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
        hd_copy(hd1, hd);
      }
    }
  }

  return hd_list;
}


/* check if item is in items */
int has_item(hd_hw_item_t *items, hd_hw_item_t item)
{
  while(*items) if(*items++ == item) return 1;

  return 0;
}


/* check if one of items is in hw_class */
int has_hw_class(hd_t *hd, hd_hw_item_t *items)
{
  while(*items) if(hd_is_hw_class(hd, *items++)) return 1;

  return 0;
}


/*
 * items must be a 0 terminated list
 */
hd_t *hd_list2(hd_data_t *hd_data, hd_hw_item_t *items, int rescan)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  unsigned char probe_save[sizeof hd_data->probe];
  unsigned fast_save;
  hd_hw_item_t *item_ptr;
  int is_manual;

  if(!items) return NULL;

  is_manual = has_item(items, hw_manual);

  if(rescan) {
    memcpy(probe_save, hd_data->probe, sizeof probe_save);
    fast_save = hd_data->flags.fast;
    hd_clear_probe_feature(hd_data, pr_all);
#ifdef __powerpc__
    hd_set_probe_feature(hd_data, pr_sys);
    hd_scan(hd_data);
#endif
    for(item_ptr = items; *item_ptr; item_ptr++) {
      hd_set_probe_feature_hw(hd_data, *item_ptr);
    }
    hd_scan(hd_data);
    memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);
    hd_data->flags.fast = fast_save;
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(!hd_report_this(hd_data, hd)) continue;
    if(
      (
        (is_manual && hd->module == mod_manual) || has_hw_class(hd, items)
      )
#ifndef LIBHD_TINY
/* with LIBHD_TINY hd->status is not maintained (cf. manual.c) */
      && (
        hd_data->hal ||
        hd->status.available == status_yes ||
        hd->status.available == status_unknown ||
        is_manual ||
        hd_data->flags.list_all
      )
#endif
    ) {
//      if(hd->is.softraiddisk) continue;		/* don't report them */

      /* don't report old entries again */
      hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
      hd_copy(hd1, hd);
    }
  }

  if(is_manual) {
    for(hd = hd_list; hd; hd = hd->next) {
      if(hd->module == mod_manual) {
        hd->status.available = hd->status.available_orig;
      }
    }
  }

  return hd_list;
}


/*
 * items must be a 0 terminated list
 */
hd_t *hd_list_with_status2(hd_data_t *hd_data, hd_hw_item_t *items, hd_status_t status)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  unsigned char probe_save[sizeof hd_data->probe];

  if(!items) return NULL;

  memcpy(probe_save, hd_data->probe, sizeof probe_save);
  hd_clear_probe_feature(hd_data, pr_all);
//  hd_set_probe_feature(hd_data, pr_manual);
  hd_scan(hd_data);
  memcpy(hd_data->probe, probe_save, sizeof hd_data->probe);

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(has_hw_class(hd, items)) {
      if(
        (status.configured == 0 || status.configured == hd->status.configured) &&
        (status.available == 0 || status.available == hd->status.available) &&
        (status.needed == 0 || status.needed == hd->status.needed) &&
        (status.reconfig == 0 || status.reconfig == hd->status.reconfig)
      ) {
        hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
        hd_copy(hd1, hd);
      }
    }
  }

  return hd_list;
}


hd_t *hd_base_class_list(hd_data_t *hd_data, unsigned base_class)
{
  hd_t *hd, *hd1, *hd_list = NULL;
//  hd_t *bridge_hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    /* add multimedia/sc_multi_video to display */
    if(
      hd->base_class.id == base_class ||
      (
        base_class == bc_display &&
        hd->base_class.id == bc_multimedia &&
        hd->sub_class.id == sc_multi_video
      )
    ) {
      hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
      hd_copy(hd1, hd);
    }
  }

  return hd_list;
}

hd_t *hd_sub_class_list(hd_data_t *hd_data, unsigned base_class, unsigned sub_class)
{
  hd_t *hd, *hd1, *hd_list = NULL;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class.id == base_class && hd->sub_class.id == sub_class) {
      hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
      hd_copy(hd1, hd);
    }
  }

  return hd_list;
}

hd_t *hd_bus_list(hd_data_t *hd_data, unsigned bus)
{
  hd_t *hd, *hd1, *hd_list = NULL;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->bus.id == bus) {
      hd1 = add_hd_entry2(&hd_list, new_mem(sizeof *hd_list));
      hd_copy(hd1, hd);
    }
  }

  return hd_list;
}

/* Convert libhd bus IDs to hwcfg bus names */
const char* hd_busid_to_hwcfg(int busid)
{
	const char* const ids1[]={"none","isa","eisa","mc","pci","pcmcia","nubus","cardbus","other"};
	const char* const ids2[]={"ps2","serial","parallel","floppy","scsi","ide","usb","adb","raid","sbus","i2o","vio","ccw","iucv"};
	if(busid <9)
		return ids1[busid];
	else if(busid >=0x80 && busid <0x8e)
		return ids2[busid-0x80];
	else
		return 0;
}

/*
 * Check if the execution of (*func)() takes longer than timeout seconds.
 * This is useful to work around long kernel-timeouts as in the floppy
 * detection and ps/2 mouse detection.
 */
int hd_timeout(void(*func)(void *), void *arg, int timeout)
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
      _exit(0);
    }
    else {
      (*func)(arg);
      _exit(0);
    }
  }

  return status ? 1 : 0;
}

void timeout_alarm_handler(int signal)
{
  _exit(63);
}


/*
 * Return list of loaded modules. Converts '-' to '_'.
 */
str_list_t *read_kmods(hd_data_t *hd_data)
{
  str_list_t *sl, *sl0, *sl1 = NULL;
  char *s;

  if(!hd_data->kmods || hd_data->flags.keep_kmods != 2) {
    hd_data->kmods = free_str_list(hd_data->kmods);
    if(!(sl0 = read_file(PROC_MODULES, 0, 0))) return NULL;
    hd_data->kmods = sl0;
    if(hd_data->flags.keep_kmods == 1) hd_data->flags.keep_kmods = 2;
  }

  for(sl = hd_data->kmods; sl; sl = sl->next) {
    s = sl->str;
    add_str_list(&sl1, strsep(&s, " \t"));
  }

  for(sl = sl1; sl; sl = sl->next) {
    for(s = sl->str; *s; s++) if(*s == '-') *s = '_';
  }

  return sl1;
}


str_list_t *get_cmdline(hd_data_t *hd_data, char *key)
{
  str_list_t *sl0, *sl1, *cmd = NULL;
  char *s, *t, *t0, *lib_cmdline;
  int i, l = strlen(key);

  if(!hd_data->cmd_line) {
    sl0 = read_file(PROC_CMDLINE, 0, 1);
    sl1 = read_file((lib_cmdline = hd_get_hddb_path("cmdline")), 0, 1);

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
        ADD2LOG("----- %s -----\n", lib_cmdline);
        ADD2LOG("  %s\n", sl1->str);
        ADD2LOG("----- %s end -----\n", lib_cmdline);
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
      add_str_list(&cmd, s + l + 1);
    }
  }

  free_mem(t0);

  return cmd;
}


/*
 * Return field 'field' (starting with 0) from the 'SuSE='
 * kernel cmd line parameter.
 */
char *get_cmd_param(hd_data_t *hd_data, int field)
{
  char *s, *t;
  str_list_t *cmd;

  if(!(cmd = get_cmdline(hd_data, "SuSE"))) return NULL;

  s = cmd->str;

  t = NULL;

  if(s) {
    for(; field; field--) {
      if(!(s = strchr(s, ','))) break;
      s++;
    }

    if(s && (t = strchr(s, ','))) *t = 0;
  }

  t = new_str(s);

  free_str_list(cmd);

  return t;
}


unsigned get_disk_crc(unsigned char *data, unsigned len)
{
  unsigned i, crc;

  crc = -1;
  for(i = 0; i < len; i++) {
    crc += data[i];
    crc *= 57;
  }

  return crc;
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
  int i, j;
  disk_t *dl, *dl0 = NULL, *dl1 = NULL;

  if(matches) *matches = 0;

  if(!(s = get_cmd_param(hd_data, 2))) return 0;

  i = strlen(s);

  if(i >= 8) {
    crc = hex(s, 8);
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
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_disk &&
      hd->block0
    ) {
      if(dev_name_duplicate(dl0, hd->unix_dev_name)) continue;
      dl = add_disk_entry(&dl0, new_mem(sizeof *dl0));
      dl->dev_name = hd->unix_dev_name;
      dl->hd_idx = hd->idx;
      dl->crc = get_disk_crc(dl->data = hd->block0, 512);
    }
  }

  if(!dl0) return 0;

  if((hd_data->debug & HD_DEB_BOOT)) {
    for(dl = dl0; dl; dl = dl->next) {
      ADD2LOG("    crc %s 0x%08x\n", dl->dev_name, dl->crc);
    }
  }

  for(i = 0, dl = dl0; dl; dl = dl->next) {
    if(crc == dl->crc) {
      dl->crc_match = 1;
      dl1 = dl;
      if(!i++) hd_idx = dl->hd_idx;
    }
  }

  if(i == 1 && dl1 && (hd_data->debug & HD_DEB_BOOT)) {
    ADD2LOG("----- MBR -----\n");
    for(j = 0; j < 512; j += 0x10) {
      ADD2LOG("  %03x  ", j);
      hd_log_hex(hd_data, 1, 0x10, dl1->data + j);
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
    sl0 = free_str_list(sl0);
  }

  ADD2LOG("----- return code: ? -----\n");

  free_mem(xcmd);

  return 0;
}

int probe_module(hd_data_t *hd_data, char *module)
{
  char *cmd = NULL;
  int i;
  struct stat sbuf;

  if(hd_module_is_active(hd_data, module)) return 0;

  if(stat(PROG_MODPROBE, &sbuf)) return 127;

  str_printf(&cmd, 0, PROG_MODPROBE " %s", module);

  i = run_cmd(hd_data, cmd);

  free_mem(cmd);

  return i;
}

int load_module_with_params(hd_data_t *hd_data, char *module, char *params)
{
  char *cmd = NULL;
  int i;
  struct stat sbuf;

  if(hd_module_is_active(hd_data, module)) return 0;

  if(stat(PROG_MODPROBE, &sbuf)) return 127;

  str_printf(&cmd, 0, PROG_MODPROBE " %s %s", module, params ? params : "");

  i = run_cmd(hd_data, cmd);

  free_mem(cmd);

  return i;
}

int load_module(hd_data_t *hd_data, char *module)
{
  return load_module_with_params(hd_data, module, NULL);
}

int unload_module(hd_data_t *hd_data, char *module)
{
  char *cmd = NULL;
  int i;

  if(!hd_module_is_active(hd_data, module)) return 0;

  str_printf(&cmd, 0, PROG_RMMOD " %s", module);

  i = run_cmd(hd_data, cmd);

  free_mem(cmd);
  
  return i;
}

/*
 * Compare two hd entries and return 0 if they are identical.
 */
int cmp_hd(hd_t *hd1, hd_t *hd2)
{
  if(!hd1 || !hd2) return 1;

  if(
    hd1->bus.id != hd2->bus.id ||
    hd1->slot != hd2->slot ||
    hd1->func != hd2->func ||
    hd1->base_class.id != hd2->base_class.id ||
    hd1->sub_class.id != hd2->sub_class.id ||
    hd1->prog_if.id != hd2->prog_if.id ||
    hd1->device.id != hd2->device.id ||
    hd1->vendor.id != hd2->vendor.id ||
    hd1->sub_vendor.id != hd2->sub_vendor.id ||
    hd1->revision.id != hd2->revision.id ||
    hd1->compat_device.id != hd2->compat_device.id ||
    hd1->compat_vendor.id != hd2->compat_vendor.id ||

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
  char *s, *t, *env, *t2;
  str_list_t *cmd = NULL;
  int i, k;
  char buf[10];
  pr_flags_t *flags;

  env = getenv("hwprobe");
  if(!env) {
    cmd = get_cmdline(hd_data, "hwprobe");
    if(cmd) env = cmd->str;
  }
  s = env = new_str(env);

  free_str_list(cmd);

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
    }

    if((t2 = strchr(t, '=')) && t2 != t) *t2++ = 0;

    if((flags = pr_flags_by_name(t))) {
      if(flags->type == p_bool) {
        set_probe_feature(hd_data, flags->val, k ? 1 : 0);
      }
      else {
        i = set_probe_val(hd_data, flags->val, t2);
        set_probe_feature(hd_data, flags->val, k != 2 ? k : i);
      }
    }
    else if(sscanf(t, "%8[^:]:%8[^:]:%8[^:]", buf, buf, buf) == 3) {
      add_str_list(&hd_data->xtra_hd, t - (k == 2 ? 0 : 1));
    }
    else {
      if(*t) ADD2LOG("hwprobe: what is \"%s\"?\n", t);
      return;
    }
  }

  free_mem(env);
}

void hd_scan_xtra(hd_data_t *hd_data)
{
  str_list_t *sl;
  hd_t *hd, *hd_tmp;
  unsigned u0, u1, u2, u4 = 0, u5 = 0, tag;
  int i, err;
  char buf0[10], buf1[10], buf2[10], buf3[64], buf4[10], buf5[10], *s, k;

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
      (i = sscanf(s, "%8[^:]:%8[^:]:%8[^:]:%8[^:]:%8[^:]:%60s", buf0, buf1, buf2, buf4, buf5, buf3)) >= 5 ||
      (i = sscanf(s, "%8[^:]:%8[^:]:%8[^:]:%60s", buf0, buf1, buf2, buf3)) >= 3
    ) {
      if(i == 5 || i == 3) *buf3 = 0;
      if(i <= 4) *buf4 = *buf5 = 0;

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
          case 'P': tag = TAG_PCMCIA; s++; break;
        }
        u1 = strtoul(s, &s, 16);
        if(*s) err |= 2;
        u1 = MAKE_ID(tag, u1);
      }
      u2 = strtoul(buf2, &s, 16);
      if(*s) err |= 4;
      u2 = MAKE_ID(ID_TAG(u1), ID_VALUE(u2));
      if(i > 4) {
        u4 = strtoul(buf4, &s, 16);
        if(*s) err |= 8;
        u4 = MAKE_ID(ID_TAG(u1), ID_VALUE(u4));
        u5 = strtoul(buf5, &s, 16);
        if(*s) err |= 16;
        u5 = MAKE_ID(ID_TAG(u1), ID_VALUE(u5));
      }
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
      if((err & 8) && !strcmp(buf4, "*")) {
        u4 = 0;
        err &= ~8;
      }
      if((err & 16) && !strcmp(buf5, "*")) {
        u5 = 0;
        err &= ~16;
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
          hd->base_class.id = u0 >> 8;
          hd->sub_class.id = u0 & 0xff;
          hd->vendor.id = u1;
          hd->device.id = u2;
          hd->sub_vendor.id = u4;
          hd->sub_device.id = u5;
          if(ID_TAG(hd->vendor.id) == TAG_PCI) hd->bus.id = bus_pci;
          if(ID_TAG(hd->vendor.id) == TAG_USB) hd->bus.id = bus_usb;
          if(ID_TAG(hd->vendor.id) == TAG_PCMCIA) {
            hd->bus.id = bus_pcmcia;
            hd->hotplug = hp_pcmcia;
          }
          if(*buf3) hd->unix_dev_name = new_str(buf3);
          hd->status.available = status_yes;
          hd->status.configured = status_new;
          hd->status.needed = status_no;
        }
        else {
          for(hd = hd_data->hd; hd; hd = hd->next) {
            if(
                (u0 == -1u || (
                  hd->base_class.id == (u0 >> 8) &&
                  hd->sub_class.id == (u0 & 0xff)
                )) &&
                (u1 == 0 || hd->vendor.id == u1) &&
                (u2 == 0 || hd->device.id == u2) &&
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
  for(i = 0; len > 0 && i < (int) sizeof buf - 1; i++, len -= 6, id >>= 6) {
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
      u = '_';
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


// old method
void hd_add_old_id(hd_t *hd)
{
  uint64_t id0 = 0, id1 = 0;

  if(hd->unique_id) return;

  INT_CRC(id0, bus.id);
  INT_CRC(id0, slot);
  INT_CRC(id0, func);
  INT_CRC(id0, base_class.id);
  INT_CRC(id0, sub_class.id);
  INT_CRC(id0, prog_if.id);
  STR_CRC(id0, unix_dev_name);
  STR_CRC(id0, rom_id);

  INT_CRC(id1, base_class.id);
  INT_CRC(id1, sub_class.id);
  INT_CRC(id1, prog_if.id);
  INT_CRC(id1, device.id);
  INT_CRC(id1, vendor.id);
  INT_CRC(id1, sub_device.id);
  INT_CRC(id1, sub_vendor.id);
  INT_CRC(id1, revision.id);
  INT_CRC(id1, compat_device.id);
  INT_CRC(id1, compat_vendor.id);
  STR_CRC(id1, device.name);
  STR_CRC(id1, vendor.name);
  STR_CRC(id1, sub_device.name);
  STR_CRC(id1, sub_vendor.name);
  STR_CRC(id1, revision.name);
  STR_CRC(id1, serial);

  id0 += (id0 >> 32);
  str_printf(&hd->unique_id, 0, "%s", numid2str(id0, 24));
  str_printf(&hd->unique_id, -1, ".%s", numid2str(id1, 64));
}

void hd_add_id(hd_data_t *hd_data, hd_t *hd)
{
  uint64_t id0 = 0, id1 = 0;

  if(hd->unique_id) return;

  hd_add_old_id(hd);
  hd->old_unique_id = hd->unique_id;
  hd->unique_id = NULL;

  INT_CRC(id1, base_class.id);
  INT_CRC(id1, sub_class.id);
  INT_CRC(id1, prog_if.id);
  INT_CRC(id1, device.id);
  INT_CRC(id1, vendor.id);
  INT_CRC(id1, sub_device.id);
  INT_CRC(id1, sub_vendor.id);
  INT_CRC(id1, revision.id);
  if(
    hd->detail &&
    hd->detail->type == hd_detail_ccw &&
    hd->detail->ccw.data
  ) INT_CRC(id1, detail->ccw.data->cu_model);
  INT_CRC(id1, compat_device.id);
  INT_CRC(id1, compat_vendor.id);
  // make sure we get the same id even if, say, the pci name list changes
  if(!hd->device.id) STR_CRC(id1, device.name);
  if(!hd->vendor.id) STR_CRC(id1, vendor.name);
  if(!hd->sub_device.name) STR_CRC(id1, sub_device.name);
  if(!hd->sub_vendor.name) STR_CRC(id1, sub_vendor.name);
  if(!hd->revision.name) STR_CRC(id1, revision.name);
  STR_CRC(id1, serial);

  hd->unique_id1 = new_str(numid2str(id1, 64));

  INT_CRC(id0, bus.id);

  if(
    (hd->bus.id == bus_usb ||
    hd->bus.id == bus_ccw) &&
    hd->sysfs_bus_id
  ) {
    STR_CRC(id0, sysfs_bus_id);
  }
  else if(
    hd->bus.id != bus_usb &&
    hd->bus.id != bus_pci &&
    hd->sysfs_id
  ) {
    STR_CRC(id0, sysfs_id);
  }
  else if(hd->unix_dev_name) {
    STR_CRC(id0, unix_dev_name);
  }
  else {
    INT_CRC(id0, slot);
    INT_CRC(id0, func);
  }

  STR_CRC(id0, rom_id);

  id0 += (id0 >> 32);

  str_printf(&hd->unique_id, 0, "%s.%s", numid2str(id0, 24), hd->unique_id1);
}
#undef INT_CRC
#undef STR_CRC


devtree_t *free_devtree(hd_data_t *hd_data)
{
  hd_t *hd;
  devtree_t *dt, *next;

  /*
   * first, remove all references in the current device tree
   * (refs in hd_old can remain)
   */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->detail && hd->detail->type == hd_detail_devtree) {
      hd->detail = free_mem(hd->detail);
    }
  }

  for(dt = hd_data->devtree; dt; dt = next) {
    next = dt->next;

    free_mem(dt->path);
    free_mem(dt->filename);
    free_mem(dt->name);
    free_mem(dt->model);
    free_mem(dt->device_type);
    free_mem(dt->compatible);
    free_mem(dt->edid);
    free_mem(dt->ccin);
    free_mem(dt->fru_number);
    free_mem(dt->loc_code);
    free_mem(dt->serial_number);
    free_mem(dt->part_number);
    free_mem(dt->description);

    free_mem(dt);
  }

  return hd_data->devtree = NULL;
}


void test_read_block0_open(void *arg)
{
  open((char *) arg, O_RDONLY);
}

unsigned char *read_block0(hd_data_t *hd_data, char *dev, int *timeout)
{
  int fd, len, buf_size = 512, k, sel;
  unsigned char *buf = NULL;
  struct timeval to;
  fd_set set, set0;

  if(hd_timeout(test_read_block0_open, dev, *timeout) > 0) {
    ADD2LOG("  read_block0: open(%s) timed out\n", dev);
    *timeout = -1;
    fd = -2;
  }
  else {
    fd = open(dev, O_RDONLY);
    if(fd < 0) ADD2LOG("  read_block0: open(%s) failed\n", dev);
  }
  if(fd >= 0) {
    buf = new_mem(buf_size);
    len = k = 0;

    FD_ZERO(&set0);
    FD_SET(fd, &set0);

    to.tv_sec = *timeout; to.tv_usec = 0;
    for(;;) {
      set = set0;
      if((sel = select(fd + 1, &set, NULL, NULL, &to)) > 0) {
        if((k = read(fd, buf + len, buf_size - len)) > 0) len += k;
        ADD2LOG("  read_block0: %d bytes (%ds, %dus)\n", k, (int) to.tv_sec, (int) to.tv_usec);
        if(k <= 0 || buf_size == len) break;
      }
      if(sel == 0) {
        *timeout = -2; break;
      }
    }

    if(k < 0) {
      ADD2LOG("  read_block0: read error(%s, %d, %d): errno %d\n", dev, len, buf_size - len, errno);
      buf = free_mem(buf);
    }
    close(fd);
  }

  return buf;
}


void get_kernel_version(hd_data_t *hd_data)
{
  unsigned u1, u2;
  str_list_t *sl;

  if(hd_data->kernel_version) return;

  hd_data->kernel_version = KERNEL_24;

  sl = read_file(PROC_VERSION, 0, 1);

  if(!sl || !sl->str) return;

  if(sscanf(sl->str, "Linux version %u.%u.", &u1, &u2) == 2) {
    if(hd_data->debug) {
      ADD2LOG("kernel version is %u.%u\n", u1, u2);
    }
    u1 = (u1 << 16) + (u2 << 8);

    if(u1 <= KERNEL_22) {
      hd_data->kernel_version = KERNEL_22;
    }
    else if(u1 <= KERNEL_24) {
      hd_data->kernel_version = KERNEL_24;
    }
    else if(u1 <= KERNEL_26) {
      hd_data->kernel_version = KERNEL_26;
    }
  }

  free_str_list(sl);
}


char *vend_id2str(unsigned vend)
{
  static char buf[32];
  char *s;

  *(s = buf) = 0;

  if(ID_TAG(vend) == TAG_EISA) {
    strcpy(s, eisa_vendor_str(vend));
  }
  else {
    if(ID_TAG(vend) == TAG_USB) *s++ = 'u', *s = 0;
    if(ID_TAG(vend) == TAG_SPECIAL) *s++ = 's', *s = 0;
    if(ID_TAG(vend) == TAG_PCMCIA) *s++ = 'P', *s = 0;
    sprintf(s, "%04x", ID_VALUE(vend));
  }

  return buf;
}


int is_modem(hd_data_t *hd_data, hd_t *hd)
{
  if(
    hd->base_class.id == bc_modem ||
    (
      hd->base_class.id == bc_comm &&
      hd->sub_class.id == sc_com_modem
    )
  ) return 1;

  return 0;
}


int is_audio(hd_data_t *hd_data, hd_t *hd)
{
  /* ISA-PnP sound cards: just one entry per card */
  if(
    hd->bus.id == bus_isa &&
    hd->is.isapnp &&
    hd->func
  ) return 0;

  if(
    hd->base_class.id == bc_multimedia &&
    (
      hd->sub_class.id == sc_multi_audio ||
      hd->sub_class.id == 3
    )
  ) return 1;

  return 0;
}


int is_pppoe(hd_data_t *hd_data, hd_t *hd)
{
  return hd->base_class.id == bc_network_interface && hd->is.pppoe ? 1 : 0;
}


void assign_hw_class(hd_data_t *hd_data, hd_t *hd)
{
  int sc;		/* compare sub_class too */
  unsigned base_class, sub_class;
  hd_hw_item_t item;
  int (*test_func)(hd_data_t *, hd_t *);

  if(!hd) return;

  // ###### FIXME: maybe just return here?
  if(!hd->hw_class) {		/* skip if we've already done it */
    for(item = 1; item < hw_all; item++) {

      test_func = NULL;

      sc = 0;
      sub_class = 0;

      base_class = -1;
      switch(item) {
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

        case hw_joystick:
          base_class = bc_joystick;
          break;

        case hw_keyboard:
          base_class = bc_keyboard;
          break;

        case hw_camera:
          base_class = bc_camera;
          break;

        case hw_framebuffer:
          base_class = bc_framebuffer;
          break;

        case hw_chipcard:
          base_class = bc_chipcard;
          break;

        case hw_sound:
          test_func = is_audio;
          break;

        case hw_isdn:
          base_class = bc_isdn;
          break;

        case hw_dsl:
          base_class = bc_dsl;
          break;

        case hw_modem:
          test_func = is_modem;
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
          base_class = bc_tv;
          break;

        case hw_dvb:
          base_class = bc_dvb;
          break;

        case hw_scanner:
          base_class = bc_scanner;
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

        case hw_bios:
          base_class = bc_internal;
          sub_class = sc_int_bios;
          sc = 1;
          break;

        case hw_usb_ctrl:
          base_class = bc_serial;
          sub_class = sc_ser_usb;
          sc = 1;
          break;

        case hw_bridge:
          base_class = bc_bridge;
          break;

        case hw_hub:
          base_class = bc_hub;
          break;

        case hw_memory:
          base_class = bc_internal;
          sub_class = sc_int_main_mem;
          sc = 1;
          break;

        case hw_ieee1394_ctrl:
          base_class = bc_serial;
          sub_class = sc_ser_fire;
          sc = 1;
          break;

        case hw_pcmcia_ctrl:
          test_func = is_pcmcia_ctrl;
          break;

        case hw_pppoe:
          test_func = is_pppoe;
          break;

        case hw_partition:
          base_class = bc_partition;
          break;

        case hw_bluetooth:
          base_class = bc_bluetooth;
          break;

        case hw_fingerprint:
          base_class = bc_fingerprint;
          break;

        case hw_wlan:
        case hw_block:
        case hw_tape:
        case hw_vbe:
          break;

        case hw_none:
        case hw_unknown:
        case hw_all:
        case hw_manual:		/* special */

        /* bus types */
        case hw_usb:
        case hw_pci:
        case hw_isapnp:
        case hw_scsi:
        case hw_ide:

        case hw_pcmcia:		/* special */

        case hw_ieee1394:	/* not handled */
        case hw_hotplug:	/* not handled */
        case hw_hotplug_ctrl:	/* not handled */
        case hw_zip:		/* not handled */
        case hw_redasd:
          break;
      }

      if(test_func) {
        if(test_func(hd_data, hd)) {
          hd_set_hw_class(hd, item);
          if(!hd->hw_class) hd->hw_class = item;
        }
      }
      else if(
        (
          hd->base_class.id == base_class &&
          (sc == 0 || hd->sub_class.id == sub_class)
        )
        ||
        ( /* list other display adapters, too */
          base_class == bc_display &&
          hd->base_class.id == bc_multimedia &&
          hd->sub_class.id == sc_multi_video
        )
        ||
        ( /* make i2o storage controllers */
          item == hw_storage_ctrl &&
          hd->base_class.id == bc_i2o
        )
        ||
        ( /* add fibre channel to storage ctrl list */
          item == hw_storage_ctrl &&
          hd->base_class.id == bc_serial &&
          hd->sub_class.id == sc_ser_fiber
        )
        ||
        ( /* add infiniband to network ctrl list */
          item == hw_network_ctrl &&
          hd->base_class.id == bc_serial &&
          hd->sub_class.id == sc_ser_infiniband
        )
      ) {
        hd_set_hw_class(hd, item);
        if(!hd->hw_class) hd->hw_class = item;
      }
    }

    if(!hd->hw_class) hd->hw_class = hw_unknown;
  }

  hd_set_hw_class(hd, hd->hw_class);

  if(hd->bus.id == bus_usb) {
    hd_set_hw_class(hd, hw_usb);
  }
  else if(hd->bus.id == bus_pci) {
    hd_set_hw_class(hd, hw_pci);
  }
  else if(hd->bus.id == bus_scsi) {
    hd_set_hw_class(hd, hw_scsi);
  }
  else if(hd->bus.id == bus_ide) {
    hd_set_hw_class(hd, hw_ide);
  }
  else if(hd->bus.id == bus_isa && hd->is.isapnp) {
    hd_set_hw_class(hd, hw_isapnp);
  }

  if(hd->usb_guid) {
    hd_set_hw_class(hd, hw_usb);	// ###### maybe only if(hd->bus.id == bus_scsi)?
  }

  if(hd->hotplug == hp_pcmcia || hd->hotplug == hp_cardbus) {
    hd_set_hw_class(hd, hw_pcmcia);
  }

  if(hd->is.wlan) {
    hd_set_hw_class(hd, hw_wlan);
  }

  if(hd_is_hw_class(hd, hw_bios)) {
    hd_set_hw_class(hd, hw_vbe);
  }

  if(
    hd->base_class.id == bc_storage_device ||
    hd->base_class.id == bc_partition
  ) {
    hd_set_hw_class(hd, hw_block);
  }

  if(
    hd->base_class.id == bc_storage_device &&
    hd->sub_class.id == sc_sdev_tape
  ) {
    hd_set_hw_class(hd, hw_tape);
  }

  if(
    hd->base_class.id == bc_storage_device &&
    hd->sub_class.id == sc_sdev_disk
  ) {
    hd_set_hw_class(hd, hw_redasd);
  }
}


void short_vendor(char *vendor)
{
  static char *remove[] = {
    ".", ",", "-", "&", " inc", "corporation", " corp", " system",
    " systems", "technology", "technologies", "multimedia", "communications",
    "computer", " ltd", "(formerly ncr)", " group", " labs", "research",
    "equipment", " ag", "personal", " canada", "data", "products",
    " america", " co", " of", "solutions", " as", "publishing", "(old)",
    " usa", " gmbh", "electronic", "components", "(matsushita)", " ab",
    " pte", " north", " japan", "limited", "microcomputer", " kg",
    "incorporated", "semiconductor", "sem", "graphics"
  };
  int i, j;
  int len, len1, len2;

  if(!vendor) return;

  len2 = strlen(vendor);

  if(!len2) return;

  do {
    len = len2;
    for(i = 0; (unsigned) i < sizeof remove / sizeof *remove; i++) {
      len1 = strlen(remove[i]);
      if(len > len1 && !strcasecmp(vendor + len - len1, remove[i])) {
        vendor[j = len - len1] = 0;
        for(j--; j >= 0; vendor[j--] = 0) {
          if(!isspace(vendor[j])) break;
        }
      }
    }
    len2 = strlen(vendor);
  } while(len2 != len);

}


void create_model_name(hd_data_t *hd_data, hd_t *hd)
{
  char *vend, *dev;
  char *compat, *dev_class, *hw_class;
  char *part1, *part2;
  cpu_info_t *ct;

  /* early out */
  if(!hd || hd->model) return;

  part1 = part2 = NULL;

  vend = dev = compat = dev_class = hw_class = NULL;

  if(
    hd->hw_class == hw_cpu &&
    hd->detail &&
    hd->detail->type == hd_detail_cpu &&
    (ct = hd->detail->cpu.data) &&
    ct->model_name
  ) {
    /* cpu entry */

    part1 = new_str(ct->model_name);
    if(ct->clock) str_printf(&part1, -1, ", %u MHz", ct->clock);
  }
  else {
    /* normal entry */

    vend = new_str(hd->sub_vendor.name);

    dev = new_str(hd->sub_device.name);

    /* catch strange subdevice names */
    if(
      dev &&
      (
        (strcasestr(dev, "motherboard") && !strcasestr(dev, "on motherboard")) ||
        strcasestr(dev, "mainboard") ||
        strcasestr(dev, "primergy") ||
        strcasestr(dev, "poweredge")
      )
    ) {
      dev = free_mem(dev);
    }

    if(!vend || !dev) {
      vend = free_mem(vend);
      dev = free_mem(dev);
      vend = new_str(hd->vendor.name);
      dev = new_str(hd->device.name);
    }

    if(dev) {
      if(vend) {
        part1 = vend; part2 = dev;
      }
      else {
        part1 = dev;
      }
    }

    if(!part1 && !part2) {
      compat = new_str(hd->compat_device.name);

      dev_class = new_str(hd->sub_class.name ?: hd->base_class.name);

      hw_class = new_str(hd_hw_item_name(hd->hw_class));

      if(vend) {
        if(compat) {
          part1 = vend; part2 = compat;
        }
        else if(dev_class) {
          part1 = vend; part2 = dev_class;
        }
      }
      else {
        if(
          compat &&
          dev_class &&
          hd->base_class.id != bc_internal	/* avoid things like 'Notebook *System*' */
       ) {
          part1 = compat; part2 = dev_class;
        }
        else if(compat) {
          part1 = compat;
        }
        else if(dev_class) {
          part1 = dev_class;
          if(hw_class && !strchr(part1, ' ') && strchr(hw_class, ' ')) {
            part2 = hw_class;
          }
        }
      }
    }
  }

  if(part1 && part2) {
    short_vendor(part1);
  }

  if(part1 && !strcasecmp(part1, "unknown")) {
    part1 = part2;
    part2 = NULL;
  }

  if(part1 && part2) {
    /* maybe the vendor name is already part of the device name... */
    if(strstr(part2, part1) || !strcasecmp(part1, part2)) {
      part1 = part2;
      part2 = NULL;
    }
  }

  if(!part1 && !part2 && hw_class) {
    str_printf(&part1, 0, "unknown %s", hw_class);
    if(strchr(hw_class, ' ')) {
      str_printf(&part1, -1, " hardware");
    }
  }

  str_printf(&hd->model, 0, "%s%s%s", part1, part2 ? " " : "", part2 ? part2 : "");

  free_mem(vend);
  free_mem(dev);
  free_mem(compat);
  free_mem(dev_class);
  free_mem(hw_class);
}


#ifndef LIBHD_TINY
int hd_change_config_status(hd_data_t *hd_data, const char *id, hd_status_t status, const char *config_string)
{
  hd_t *hd;
  int i;

  hd = hd_read_config(hd_data, id);

  if(!hd || hd->status.invalid) return 1;

  if(status.configured) hd->status.configured = status.configured;
  if(status.available) hd->status.available = status.available;
  if(status.needed) hd->status.needed = status.needed;
  hd->status.invalid = status.invalid;

  if(hd->config_string) {
    free_mem(hd->config_string);
    hd->config_string = new_str(config_string);
  }

  i = hd_write_config(hd_data, hd);
  
  hd_free_hd_list(hd);

  return i;
}


/* wrapper for hd_change_config_status(); obsolete - do not use */
int hd_change_status(const char *id, hd_status_t status, const char *config_string)
{
  hd_data_t *hd_data;
  int i;

  hd_data = new_mem(sizeof *hd_data);

  i = hd_change_config_status(hd_data, id, status, config_string);

  hd_free_hd_data(hd_data);

  free_mem(hd_data);

  return i;
}

#endif		/* !defined(LIBHD_TINY) */


int hd_getdisksize(hd_data_t *hd_data, char *dev, int fd, hd_res_t **geo, hd_res_t **size)
{
  int status=0;
  hd_res_t *res;
  struct hd_geometry geo_s;
#ifdef HDIO_GETGEO_BIG
  struct hd_big_geometry big_geo_s;
#endif
  unsigned long secs32;
  uint64_t secs, secs0;
  unsigned sec_size;
  int close_fd = 0;
  int got_big = 0;

  *geo = *size = NULL;

  ADD2LOG("  dev = %s, fd = %d\n", dev, fd);

  if(fd < 0) {
    if(!dev) return 0;
    fd = open(dev, O_RDONLY | O_NONBLOCK);
    close_fd = 1;
    if(fd < 0) return 0;
  }

  ADD2LOG("  open ok, fd = %d\n", fd);

  secs0 = 0;
  res = NULL;

#ifdef HDIO_GETGEO_BIG
  if(!ioctl(fd, HDIO_GETGEO_BIG, &big_geo_s)) {
    if(dev) ADD2LOG("%s: ioctl(big geo) ok\n", dev);
    res = add_res_entry(geo, new_mem(sizeof *res));
    res->disk_geo.type = res_disk_geo;
    res->disk_geo.cyls = big_geo_s.cylinders;
    res->disk_geo.heads = big_geo_s.heads;
    res->disk_geo.sectors = big_geo_s.sectors;
    res->disk_geo.geotype = geo_logical;
    secs0 = (uint64_t) res->disk_geo.cyls * res->disk_geo.heads * res->disk_geo.sectors;
    got_big = 1;
  }
  else {
    ADD2LOG("  big geo failed: %s\n", strerror(errno));
#else
  {
#endif
    if(!ioctl(fd, HDIO_GETGEO, &geo_s)) {
      if(dev) ADD2LOG("%s: ioctl(geo) ok\n", dev);
      res = add_res_entry(geo, new_mem(sizeof *res));
      res->disk_geo.type = res_disk_geo;
      res->disk_geo.cyls = geo_s.cylinders;
      res->disk_geo.heads = geo_s.heads;
      res->disk_geo.sectors = geo_s.sectors;
      res->disk_geo.geotype = geo_logical;
      secs0 = (uint64_t) res->disk_geo.cyls * res->disk_geo.heads * res->disk_geo.sectors;
    }
    else {
      ADD2LOG("  geo failed: %s\n", strerror(errno));
    }
  }

  /* ##### maybe always BLKSZGET or always 0x200? */
  if(!ioctl(fd, BLKSSZGET, &sec_size)) {
    if(dev) ADD2LOG("%s: ioctl(block size) ok\n", dev);
    if(!sec_size) sec_size = 0x200;
  }
  else {
    sec_size = 0x200;
  }

  secs = 0;

#if defined(__s390__) || defined(__s390x__)
  if(res && res->disk_geo.sectors == 0)
  { /* This seems to be an unformatted DASD -> fake the formatted geometry */
    res->disk_geo.sectors=12;
    sec_size=4096;
    secs = (uint64_t) res->disk_geo.cyls * res->disk_geo.heads * res->disk_geo.sectors;
    status=1;
  }
  else
  {
#endif
    if(!ioctl(fd, BLKGETSIZE64, &secs)) {
      if(dev) ADD2LOG("%s: ioctl(disk size) ok\n", dev);
      secs /= sec_size;
    }
    else if(!ioctl(fd, BLKGETSIZE, &secs32)) {
      if(dev) ADD2LOG("%s: ioctl(disk size32) ok\n", dev);
      secs = secs32;
    }
    else {
      secs = secs0;
    }
#if defined(__s390__) || defined(__s390x__)
  }
#endif

  if(!got_big && secs0 && res) {
    /* fix cylinder value */
    res->disk_geo.cyls = secs / (res->disk_geo.heads * res->disk_geo.sectors);
  }

  if(secs) {
    res = add_res_entry(size, new_mem(sizeof *res));
    res->size.type = res_size;
    res->size.unit = size_unit_sectors;
    res->size.val1 = secs;
    res->size.val2 = sec_size;
  }

  // ADD2LOG("  geo = %p, size = %p\n", *geo, *size);

  if(close_fd) close(fd);
  
  return status;
}


str_list_t *hd_split(char del, const char *str)
{
  char *t, *s, *str0;
  str_list_t *sl = NULL;

  if(!str) return NULL;

  for(s = str0 = new_str(str); (t = strchr(s, del)); s = t + 1) {
    *t = 0;
    add_str_list(&sl, s);
  }
  add_str_list(&sl, s);

  free_mem(str0);

  return sl;
}


char *hd_join(char *del, str_list_t *str)
{
  char *s;
  str_list_t *str0;
  int len = 0, del_len = 0;

  if(del) del_len = strlen(del);

  for(str0 = str; str0; str0 = str0->next) {
    if(str0->str) len += strlen(str0->str);
    if(str0->next) len += del_len;
  }

  if(!len) return NULL;

  len++;

  s = new_mem(len);

  for(; str; str = str->next) {
    if(str->str) strcat(s, str->str);
    if(str->next && del) strcat(s, del);
  }

  return s;
}


/*
 * cf. pcmcia-cs-*:cardmgr/pcic_probe.c
 */
int is_pcmcia_ctrl(hd_data_t *hd_data, hd_t *hd)
{
  int i;
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
    { 0x104c, 0xac1a },
    { 0x104c, 0xac1e },
    { 0x104c, 0xac17 },
    { 0x104c, 0xac19 },
    { 0x104c, 0xac1c },
    { 0x104c, 0xac16 },
    { 0x104c, 0xac1d },
    { 0x104c, 0xac1f },
    { 0x104c, 0xac50 },
    { 0x104c, 0xac51 },
    { 0x104c, 0xac1b },
    { 0x104c, 0xac52 },
    { 0x104c, 0xac41 },
    { 0x104c, 0xac40 },
    { 0x104c, 0xac42 },
    { 0x1217, 0x6729 },
    { 0x1217, 0x673a },
    { 0x1217, 0x6832 },
    { 0x1217, 0x6836 },
    { 0x1217, 0x6872 },
    { 0x1217, 0x6925 },
    { 0x1217, 0x6933 },
    { 0x1217, 0x6972 },
    { 0x1179, 0x0603 },
    { 0x1179, 0x060a },
    { 0x1179, 0x060f },
    { 0x1179, 0x0617 },
    { 0x119b, 0x1221 },
    { 0x8086, 0x1221 }
  };

  if(!hd) return 0;

  if(
     hd->base_class.id == bc_bridge &&
    (hd->sub_class.id == sc_bridge_pcmcia || hd->sub_class.id == sc_bridge_cardbus)
  ) return 1;

  /* just in case... */
  if(hd->bus.id == bus_pci) {
    for(i = 0; (unsigned) i < sizeof ids / sizeof *ids; i++) {
      if(
        ID_VALUE(hd->vendor.id) == ids[i][0] &&
        ID_VALUE(hd->device.id) == ids[i][1]
      ) return 1;
    }
  }

  return 0;
}

void hd_set_hw_class(hd_t *hd, hd_hw_item_t hw_class)
{
  unsigned ofs, bit;

  ofs = (unsigned) hw_class >> 3;
  bit = (unsigned) hw_class & 7;

  if(ofs < sizeof hd->hw_class_list / sizeof *hd->hw_class_list) {
    hd->hw_class_list[ofs] |= 1 << bit;
  }
}


int hd_is_hw_class(hd_t *hd, hd_hw_item_t hw_class)
{
  unsigned ofs, bit;

  if(hw_class == hw_all) return 1;

  ofs = (unsigned) hw_class >> 3;
  bit = (unsigned) hw_class & 7;

  if(ofs < sizeof hd->hw_class_list / sizeof *hd->hw_class_list) {
    return hd->hw_class_list[ofs] & (1 << bit) ? 1 : 0;
  }

  return 0;
}


/*
 * Start subprocess for dangerous things.
 *
 * Stop it after total_timeout seconds or if nothing happens for
 * timeout seconds.
 */
void hd_fork(hd_data_t *hd_data, int timeout, int total_timeout)
{
  void (*old_sigchld_handler)(int);
  struct timespec wait_time;
  int i, j, sleep_intr = 1;
  hd_data_t *hd_data_shm;
  time_t stop_time;
  int updated, rem_time;
  sigset_t new_set, old_set;
  int kill_sig[] = { SIGUSR1, SIGKILL };

  if(hd_data->flags.forked) return;

  if(hd_data->flags.nofork) {
    hd_data->flags.forked = 1;
    return;
  }

  hd_data_shm = hd_data->shm.data;

  stop_time = time(NULL) + total_timeout;
  rem_time = total_timeout;

  child_id = child = 0;

  sigemptyset(&new_set);
  sigaddset(&new_set, SIGCHLD);
  sigprocmask(SIG_BLOCK, &new_set, &old_set);

  old_sigchld_handler = signal(SIGCHLD, sigchld_handler);

  wait_time.tv_sec = timeout;
  wait_time.tv_nsec = 0;

  updated = hd_data_shm->shm.updated;

  child = fork();

  sigprocmask(SIG_SETMASK, &old_set, NULL);

  if(child != -1) {
    if(child) {
      ADD2LOG(
        "******  started child process %d (%ds/%ds)  ******\n",
        (int) child, timeout, total_timeout
      );

      while(child_id != child && sleep_intr) {
        sleep_intr = nanosleep(&wait_time, &wait_time);
//        fprintf(stderr, "woke up %d\n", sleep_intr);
        rem_time = stop_time - time(NULL);
        if(updated != hd_data_shm->shm.updated && rem_time >= 0) {
          /* reset time if there was some progress and we've got some time left  */
          rem_time++;
          wait_time.tv_sec = rem_time > timeout ? timeout : rem_time;
          wait_time.tv_nsec = 0;

          sleep_intr = 1;
        }
        updated = hd_data_shm->shm.updated;
      }

      if(child_id != child) {
        ADD2LOG("******  killed child process %d (%ds)  ******\n", (int) child, rem_time);
        for(i = 0; i < sizeof kill_sig / sizeof *kill_sig; i++) {
          kill(child, kill_sig[i]);
          for(j = 10; j && !waitpid(child, NULL, WNOHANG); j--) {
            wait_time.tv_sec = 0;
            wait_time.tv_nsec = 10*1000000;
            nanosleep(&wait_time, NULL);
          }
        }
      }

      hd_log(hd_data, hd_data_shm->log, hd_data_shm->log_size);

      ADD2LOG("******  stopped child process %d (%ds)  ******\n", (int) child, rem_time);
    }
    else {
      hd_data->log = free_mem(hd_data->log);
      hd_data->log_size = hd_data->log_max = 0;

      hd_data->flags.forked = 1;

      hd_data_sig = hd_data;

      signal(SIGUSR1, sigusr1_handler);
    }
  }

  signal(SIGCHLD, old_sigchld_handler);
}


/*
 * Stop subprocess.
 */
void hd_fork_done(hd_data_t *hd_data)
{
  if(!hd_data->flags.forked || hd_data->flags.nofork) return;

  copy_log2shm(hd_data);

  _exit(0);
}


/*
 * Copy log to shm segment.
 */
void copy_log2shm(hd_data_t *hd_data)
{
  hd_data_t *hd_data_shm;

  hd_data_shm = hd_data->shm.data;

  if(hd_data->log) {
    hd_data_shm->log_size = hd_data->log_size;
    hd_data_shm->log_max = hd_data->log_size + 1;
    hd_data_shm->log = hd_shm_add(hd_data, hd_data->log, hd_data_shm->log_max);
  }
}


/*
 * SIGCHLD handler while we're waiting for our child.
 */
void sigchld_handler(int num)
{
  pid_t p = waitpid(child, NULL, WNOHANG);

  if(p && p == child) child_id = p;
}


/*
 * SIGUSR1 handler - copy log to shm, then exit
 */
void sigusr1_handler(int num)
{
  hd_data_t *hd_data = hd_data_sig;

  ADD2LOG("* got signal USR1 *\n");

  copy_log2shm(hd_data);

  _exit(0);
}


/*
 * Get a sufficiently large shm segment.
 */
void hd_shm_init(hd_data_t *hd_data)
{
  void *p;

  if(hd_data->shm.ok || hd_data->flags.nofork) return;

  memset(&hd_data->shm, 0, sizeof hd_data->shm);

  hd_data->shm.size = 256*1024;

  hd_data->shm.id = shmget(IPC_PRIVATE, hd_data->shm.size, IPC_CREAT | 0600);

  if(hd_data->shm.id == -1) {
    ADD2LOG("shm: shmget failed (errno %d)\n", errno);
    return;
  }

  p = shmat(hd_data->shm.id, NULL, 0);

  if(p == (void *) -1) {
    ADD2LOG("shm: shmat for segment %d failed (errno %d)\n", hd_data->shm.id, errno);
  }

  shmctl(hd_data->shm.id, IPC_RMID, NULL);

  if(p == (void *) -1) return;

  hd_data->shm.data = p;

  ADD2LOG("shm: attached segment %d at %p\n", hd_data->shm.id, hd_data->shm.data);

  hd_data->shm.ok = 1;

  hd_shm_clean(hd_data);
}


/*
 * Reset shm usage, remove references to shm area.
 */
void hd_shm_clean(hd_data_t *hd_data)
{
  hd_data_t *hd_data_shm;

  if(!hd_data->shm.ok) return;

  if(hd_is_shm_ptr(hd_data, hd_data->ser_mouse)) hd_data->ser_mouse = NULL;
  if(hd_is_shm_ptr(hd_data, hd_data->ser_modem)) hd_data->ser_modem = NULL;


  hd_data->shm.used = sizeof *hd_data;
  hd_data->shm.updated = 0;

  memcpy(hd_data_shm = hd_data->shm.data, hd_data, sizeof *hd_data);

  hd_data_shm->log = NULL;
}


/*
 * Release shm segment.
 */
void hd_shm_done(hd_data_t *hd_data)
{
  if(!hd_data->shm.ok) return;

  shmdt(hd_data->shm.data);

  hd_data->shm.ok = 0;
}


/*
 * Copy into shm area. If ptr is NULL return a shm area of size len.
 */
void *hd_shm_add(hd_data_t *hd_data, void *ptr, unsigned len)
{
  if(!hd_data->shm.ok || !len) return NULL;

  hd_data = hd_data->shm.data;

  if(hd_data->shm.size - hd_data->shm.used < len) return NULL;

  if(ptr) {
    ptr = memcpy(hd_data->shm.data + hd_data->shm.used, ptr, len);
  }
  else {
    ptr = memset(hd_data->shm.data + hd_data->shm.used, 0, len);
  }

  hd_data->shm.used += len;

  return ptr;
}


/*
 * Check if ptr points to a valid shm address.
 */
int hd_is_shm_ptr(hd_data_t *hd_data, void *ptr)
{
  if(!hd_data->shm.ok || !ptr) return 0;

  hd_data = hd_data->shm.data;

  if(
    ptr < hd_data->shm.data ||
    ptr >= hd_data->shm.data + hd_data->shm.used
  ) return 0;
  
  return 1;
}


char *hd_shm_add_str(hd_data_t *hd_data, char *str)
{
  return hd_shm_add(hd_data, str, str ? strlen(str) + 1 : 0);
}


str_list_t *hd_shm_add_str_list(hd_data_t *hd_data, str_list_t *sl)
{
  str_list_t *sl0 = NULL, **sl_shm;

  for(sl_shm = &sl0; sl; sl = sl->next, sl_shm = &(*sl_shm)->next) {
    *sl_shm = hd_shm_add(hd_data, NULL, sizeof **sl_shm);
    (*sl_shm)->str = hd_shm_add_str(hd_data, sl->str);
  }

  return sl0;
}


void hd_move_to_shm(hd_data_t *hd_data)
{
  hd_data_t *hd_data_shm;
  ser_device_t *ser, **ser_shm;
  struct {
    ser_device_t **src, **dst;
  } ser_dev[2];
  unsigned u;

  if(!hd_data->shm.ok) return;

  hd_data_shm = hd_data->shm.data;

  ser_dev[0].src = &hd_data->ser_mouse;
  ser_dev[0].dst = &hd_data_shm->ser_mouse;
  ser_dev[1].src = &hd_data->ser_modem;
  ser_dev[1].dst = &hd_data_shm->ser_modem;

  for(u = 0; u < sizeof ser_dev / sizeof *ser_dev; u++) {
    if(*ser_dev[u].src) {
      /* copy serial mouse data */
      for(
        ser = *ser_dev[u].src, ser_shm = ser_dev[u].dst;
        ser;
        ser = ser->next, ser_shm = &(*ser_shm)->next
      ) {
        *ser_shm = hd_shm_add(hd_data, ser, sizeof *ser);
      }

      for(ser = *ser_dev[u].dst; ser; ser = ser->next) {
        ser->dev_name = hd_shm_add_str(hd_data, ser->dev_name);
        ser->serial = hd_shm_add_str(hd_data, ser->serial);
        ser->class_name = hd_shm_add_str(hd_data, ser->class_name);
        ser->dev_id = hd_shm_add_str(hd_data, ser->dev_id);
        ser->user_name = hd_shm_add_str(hd_data, ser->user_name);
        ser->vend = hd_shm_add_str(hd_data, ser->vend);
        ser->init_string1 = hd_shm_add_str(hd_data, ser->init_string1);
        ser->init_string2 = hd_shm_add_str(hd_data, ser->init_string2);
        ser->pppd_option = hd_shm_add_str(hd_data, ser->pppd_option);

        ser->at_resp = hd_shm_add_str_list(hd_data, ser->at_resp);
      }
    }
  }

}


hd_udevinfo_t *hd_free_udevinfo(hd_udevinfo_t *ui)
{
  hd_udevinfo_t *next;

  for(; ui; ui = next) {
    next = ui->next;

    free_mem(ui->sysfs);
    free_mem(ui->name);
    free_str_list(ui->links);

    free_mem(ui);
  }

  return NULL;
}


void read_udevinfo(hd_data_t *hd_data)
{
  str_list_t *sl, *udevinfo;
  hd_udevinfo_t **uip, *ui;
  char *s = NULL, buf[256];

  udevinfo = read_file("| " PROG_UDEVADM " info -e 2>/dev/null", 0, 0);
  if(!udevinfo) udevinfo = read_file("| " PROG_UDEVINFO " -e 2>/dev/null", 0, 0);

  ADD2LOG("-----  udevinfo -----\n");
  for(sl = udevinfo; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("-----  udevinfo end -----\n");

  hd_data->udevinfo = hd_free_udevinfo(hd_data->udevinfo);

  uip = &hd_data->udevinfo;

  for(ui = NULL, sl = udevinfo; sl; sl = sl->next) {
    if(sscanf(sl->str, "P: %255s", buf) == 1) {
      ui = *uip = new_mem(sizeof **uip);
      uip = &(*uip)->next;
      ui->sysfs = new_str(buf);

      continue;
    }

    if(!ui) continue;

    if(sscanf(sl->str, "N: %255s", buf) == 1) {
      str_printf(&ui->name, 0, "/dev/%s", buf);

      continue;
    }

    if(sscanf(sl->str, "S: %255s", buf) == 1) {
      str_printf(&s, 0, "/dev/%s", buf);
      add_str_list(&ui->links, s);

      continue;
    }
  }

  s = free_mem(s);

  for(ui = hd_data->udevinfo; ui; ui = ui->next) {
    ADD2LOG("%s\n", ui->sysfs);
    if(ui->name) ADD2LOG("  name: %s\n", ui->name);
    if(ui->links) {
      s = hd_join(", ", ui->links);
      ADD2LOG("  links: %s\n", s);
      free_mem(s);
    }
  }

  free_str_list(udevinfo);
}


/*
 * Return libhd version.
 */
char *hd_version()
{
  return HD_VERSION_STRING;
}


hd_t *hd_find_sysfs_id(hd_data_t *hd_data, char *id)
{
  hd_t *hd;

  if(id && *id) {
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->sysfs_id && !strcmp(hd->sysfs_id, id)) return hd;
    }
  }

  return NULL;
}


hd_t *hd_find_sysfs_id_devname(hd_data_t *hd_data, char *id, char *devname)
{
  hd_t *hd;

  if(id && *id && devname) {
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->sysfs_id &&
        !strcmp(hd->sysfs_id, id) &&
        (
          !hd->unix_dev_name ||
          (
            hd->unix_dev_name &&
            !strcmp(hd->unix_dev_name, devname)
          )
        )
      ) return hd;
    }
  }

  return NULL;
}


hd_sysfsdrv_t *hd_free_sysfsdrv(hd_sysfsdrv_t *sf)
{
  hd_sysfsdrv_t *next;

  for(; sf; sf = next) {
    next = sf->next;

    free_mem(sf->driver);
    free_mem(sf->device);
    free_mem(sf->module);

    free_mem(sf);
  }

  return NULL;
}


void hd_sysfs_driver_list(hd_data_t *hd_data)
{
  hd_sysfsdrv_t **sfp, *sf;
  str_list_t *sl, *sl0;
  uint64_t id = 0;
  char *drv_dir = NULL, *drv = NULL, *module, *s;
  str_list_t *sf_bus, *sf_bus_e, *sf_drv, *sf_drv_e, *sf_drv2, *sf_drv2_e;

  for(sl = sl0 = read_file(PROC_MODULES, 0, 0); sl; sl = sl->next) {
    crc64(&id, sl->str, strlen(sl->str) + 1);
  }
  free_str_list(sl0);

  if(id != hd_data->sysfsdrv_id) {
    hd_data->sysfsdrv = hd_free_sysfsdrv(hd_data->sysfsdrv);
  }

  if(hd_data->sysfsdrv) return;

  hd_data->sysfsdrv_id = id;

  sfp = &hd_data->sysfsdrv;

  ADD2LOG("----- sysfs driver list (id 0x%016"PRIx64") -----\n", id);

  sf_bus = read_dir("/sys/bus", 'd');

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    str_printf(&drv_dir, 0, "/sys/bus/%s/drivers", sf_bus_e->str);
    sf_drv = read_dir(drv_dir, 'd');

    for(sf_drv_e = sf_drv; sf_drv_e; sf_drv_e = sf_drv_e->next) {
      str_printf(&drv, 0, "/sys/bus/%s/drivers/%s", sf_bus_e->str, sf_drv_e->str);

      sf_drv2 = read_dir(drv, 'l');

      for(sf_drv2_e = sf_drv2; sf_drv2_e; sf_drv2_e = sf_drv2_e->next) {
        if(!strcmp(sf_drv2_e->str, "module")) {
          s = hd_read_sysfs_link(drv, sf_drv2_e->str);
          module = s ? strrchr(s, '/') : NULL;
          if(module) {
            sf = *sfp = new_mem(sizeof **sfp);
            sfp = &(*sfp)->next;
            sf->driver = new_str(sf_drv_e->str);
            sf->module = new_str(module + 1);
            ADD2LOG("%16s: module = %s\n", sf->driver, sf->module);
          }
        }
        else {
          sf = *sfp = new_mem(sizeof **sfp);
          sfp = &(*sfp)->next;
          sf->driver = new_str(sf_drv_e->str);
          sf->device = new_str(hd_sysfs_id(hd_read_sysfs_link(drv, sf_drv2_e->str)));
          ADD2LOG("%16s: %s\n", sf->driver, sf->device);
        }
      }

      free_str_list(sf_drv2);

    }

    free_str_list(sf_drv);

  }

  free_str_list(sf_bus);

  drv = free_mem(drv);
  drv_dir = free_mem(drv_dir);

  ADD2LOG("----- sysfs driver list end -----\n");
}


int hd_report_this(hd_data_t *hd_data, hd_t *hd)
{
  if(!hd_data->only) return 1;

  if(search_str_list(hd_data->only, hd->sysfs_id)) return 1;

  return search_str_list(hd_data->only, hd->unix_dev_name) ? 1 : 0;
}


str_list_t *hd_module_list(hd_data_t *hd_data, unsigned id)
{
  hd_t *hd;
  str_list_t *drivers = NULL, *sl;
  driver_info_t *di;

  hd = new_mem(sizeof *hd);
  hd->tag.freeit = 1; 

  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0xf000);
  hd->device.id = MAKE_ID(TAG_SPECIAL, id);

  hddb_add_info(hd_data, hd);

  for(di = hd->driver_info; di; di = di->next) {
    if(di->any.type == di_module && di->module.modprobe) {
      for(sl = di->module.names; sl; sl = sl->next) {
        add_str_list(&drivers, sl->str);
      }
    }
  }

  hd_free_hd_list(hd);

  return drivers;
}


/*
 * Read using mmap().
 */
int hd_read_mmap(hd_data_t *hd_data, char *name, unsigned char *buf, off_t start, unsigned size)
{
  off_t map_start, xofs;
  int psize = getpagesize(), fd;
  unsigned map_size;
  void *p;
  struct stat sbuf;

  if(!size || !name) return 0;

  memset(buf, 0, size);

  map_start = start & -psize;
  xofs = start - map_start;

  map_size = (xofs + size + psize - 1) & -psize;

  fd = open(name, O_RDONLY);

  if(fd == -1) return 0;

  if(!fstat(fd, &sbuf) && S_ISREG(sbuf.st_mode)) {
    if(sbuf.st_size < start + size) {
      if(sbuf.st_size > start) {
        size = sbuf.st_size - start;
      }
      else {
        size = 0;
      }
    }
  }

  if(!size) {
    close(fd);
    return 0;
  }

  p = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, map_start);

  if(p == MAP_FAILED) {
    if(hd_data) ADD2LOG(
      "%s[0x%x, %u]: mmap(, %u,,,, 0x%x) failed: %s\n",
      name, (unsigned) start, size, map_size, (unsigned) map_start, strerror(errno)
    );
    close(fd);
    return 0;
  }
  if(hd_data) ADD2LOG(
    "%s[0x%x, %u]: mmap(, %u,,,, 0x%x) ok\n",
    name, (unsigned) start, size, map_size, (unsigned) map_start
  );

  memcpy(buf, p + xofs, size);

  munmap(p, map_size);

  close(fd);

  return 1;
}


/*
 * Get hardware data base dir (default: /var/lib/hardware).
 */
char *hd_get_hddb_dir()
{
  char *s = getenv("LIBHD_HDDB_DIR");

  return s && *s ? s : HARDWARE_DIR;
}


/*
 * Convenience function. Returns static buffer with full path.
 */
char *hd_get_hddb_path(char *sub)
{
  static char *dir = NULL;

  str_printf(&dir, 0, "%s/%s", hd_get_hddb_dir(), sub);

  return dir;
}


/*
 * Parse attribute and return integer value.
 */
int hd_attr_uint(char* attr, uint64_t* u, int base)
{
  char *s;
  uint64_t u2;
  int ok;
  
  if(!(s = attr)) return 0;
  u2 = strtoull(s, &s, base);
  ok = !*s || isspace(*s) ? 1 : 0;
  
  if(ok && u) *u = u2;
  
  return ok;
}

/*
 * Return attribute as string list.
 */
str_list_t *hd_attr_list(char *str)
{
  static str_list_t *sl = NULL;

  free_str_list(sl);

  return sl = hd_split('\n', str);
}


/*
 * Remove leading "/sys" from path.
 */
char *hd_sysfs_id(char *path)
{
  if(!path || !*path) return NULL;

  //fprintf(stderr, "hd_sysfs_id %s\n",path);
  return strchr(path + 1, '/');
}


/*
 * Convert '!' to '/'.
 */
char *hd_sysfs_name2_dev(char *str)
{
  static char *s = NULL;

  if(!str) return NULL;

  free_mem(s);
  s = str = new_str(str);

  while(*str) {
    if(*str == '!') *str = '/';
    str++;
  }

  return s;
}


/*
 * Convert '/' to '!'.
 */
char *hd_sysfs_dev2_name(char *str)
{
  static char *s = NULL;

  if(!str) return NULL;

  free_mem(s);
  s = str = new_str(str);

  while(*str) {
    if(*str == '/') *str = '!';
    str++;
  }

  return s;
}


char* get_sysfs_attr(const char* bus, const char* device, const char* attr)
{
  static char buf[256];
  FILE* fp;
  sprintf(buf, "/sys/bus/%s/devices/%s/%s", bus, device, attr);
  fp = fopen(buf, "r");
  if(!fp) return NULL;
  fgets(buf, 127, fp);
  fclose(fp);
  return buf;
}


/*
 * must be able to read more than one line
 */
char *get_sysfs_attr_by_path(const char *path, const char *attr)
{
  return get_sysfs_attr_by_path2(path, attr, NULL);
}  


/*
 * binary data version; return data length, too
 */
char *get_sysfs_attr_by_path2(const char *path, const char *attr, unsigned *len)
{
  static char buf[1024];
  int i, fd;

  if(len) *len = 0;

  sprintf(buf, "%s/%s", path, attr);
  fd = open(buf, O_RDONLY);
  if(fd >= 0) {
    i = read(fd, buf, sizeof buf - 1);
    close(fd);
    if(i >= 0) {
      if(len) *len = i;
      buf[i] = 0;
    }
    else {
      return NULL;
    }
  }
  else {
    return NULL;
  }

  return buf;
}  


/*
 * Compare module names.
 */
int hd_mod_cmp(char *str1, char *str2)
{
  char *s;
  int i;

  str1 = strdup(str1);
  str2 = strdup(str2);

  for(s = str1; *s; s++) if(*s == '-') *s = '_';
  for(s = str2; *s; s++) if(*s == '-') *s = '_';

  i = strcmp(str1, str2);  

  free(str1);
  free(str2);

  return i;
}


str_list_t *sort_str_list(str_list_t *sl0, int (*cmp_func)(const void *, const void *))
{
  int i, list_len;
  str_list_t *sl1 = NULL, *sl, **ssl;
  str_list_t **str_list_array;

  for(list_len = 0, sl = sl0; sl; sl = sl->next) list_len++;
  if(list_len < 2) return sl0;

  str_list_array = malloc(list_len * sizeof *str_list_array);

  for(i = 0, sl = sl0; sl; sl = sl->next) str_list_array[i++] = sl;

  qsort(str_list_array, list_len, sizeof *str_list_array, cmp_func);

  for(i = 0, ssl = &sl1; i < list_len; i++) {
    *ssl = str_list_array[i];
    ssl = &str_list_array[i]->next;
  }
  *ssl = NULL;

  free(str_list_array);

  return sl1;
}


/** @} */


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

static hd_t *add_hd_entry2(hd_t **hd, hd_t *new_hd);
static hd_res_t *get_res(hd_t *h, enum resource_types t, unsigned index);
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
  { mod_isa, "isa" }
};

/*
 * Names for the probe flags. Used for debugging and command line parsing in
 * hw.c. Cf. enum probe_feature, hd_data_t.probe.
 */
static struct s_pr_flags {
  enum probe_feature val;
  char *name;
} pr_flags[] = {
  { pr_default, "default"},			/* magic */
  { pr_all, "all" },				/* magic */
  { pr_memory, "memory" },
  { pr_pci, "pci" },
  { pr_pci_range, "pci.range" },
  { pr_pci_ext, "pci.ext" },
  { pr_isapnp, "isapnp" },
  { pr_isapnp, "pnpdump" },			/* alias for isapnp */
  { pr_cdrom, "cdrom" },
  { pr_cdrom_info, "cdrom.info" },
  { pr_net, "net" },
  { pr_floppy, "floppy" },
  { pr_misc, "misc" },
  { pr_misc_serial, "misc.serial" },
  { pr_misc_par, "misc.par" },
  { pr_misc_floppy, "misc.floppy" },
  { pr_bios, "bios" },
  { pr_cpu, "cpu" },
  { pr_monitor, "monitor" },
  { pr_serial, "serial" },
  { pr_mouse, "mouse" },
  { pr_ide, "ide" },
  { pr_scsi, "scsi" },
  { pr_usb, "usb" },
  { pr_adb, "adb" },
  { pr_modem, "modem" },
  { pr_modem_usb, "modem.usb" },
  { pr_parallel, "parallel" },
  { pr_isa, "isa" },
  { pr_isa_isdn, "isa.isdn" }
};

#define PR_OFS			2		/* skip 0, default */
#define ALL_PR_FEATURE		((1 << (pr_all - PR_OFS)) - 1)
#define DEFAULT_PR_FEATURE	(\
                                  ALL_PR_FEATURE\
                                  - (1 << (pr_pci_range - PR_OFS))\
                                  - (1 << (pr_pci_ext - PR_OFS))\
                                )

void hd_set_probe_feature(hd_data_t *hd_data, enum probe_feature feature)
{
  if(feature == pr_all)
    hd_data->probe |= ALL_PR_FEATURE;
  else if(feature == pr_default)
    hd_data->probe |= DEFAULT_PR_FEATURE;
  else if((feature -= PR_OFS) >= 0)	/* skip 0, default, all */
    hd_data->probe |= (1 << feature);
}

void hd_clear_probe_feature(hd_data_t *hd_data, enum probe_feature feature)
{
  if(feature == pr_all)
    hd_data->probe &= ~ALL_PR_FEATURE;
  else if(feature == pr_default)
    hd_data->probe &= ~DEFAULT_PR_FEATURE;
  else if((feature -= PR_OFS) >= 0)	/* skip 0, default, all */
    hd_data->probe &= ~(1 << feature);
}

int hd_probe_feature(hd_data_t *hd_data, enum probe_feature feature)
{
  feature -= PR_OFS;			/* skip 0, default, all */

  return feature >= 0 && (hd_data->probe & (1 << feature)) ? 1 : 0;
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
  void *p = calloc(size, 1);

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
  char *s;
  int i, j;
  hd_t *hd;

  /* log the debug & probe flags */
  if(hd_data->debug) {
    ADD2LOG(
      "libhd version %s (%s)\ndebug = 0x%x\nprobe = 0x%x (",
      HD_VERSION, HD_ARCH, hd_data->debug, hd_data->probe
    );

    for(j = PR_OFS, i = 0; j < pr_all; j++) {
      if((s = hd_probe_feature_by_value(j))) {
        ADD2LOG("%s%c%s", i++ ? " " : "", hd_probe_feature(hd_data, j) ? '+' : '-', s);
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
#endif	/* LIBHD_TINY */

#if defined(__i386__) || defined(__alpha__)
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
  hd_scan_usb(hd_data);
#if defined(__PPC__)
  hd_scan_adb(hd_data);
#endif
#ifndef LIBHD_TINY
  hd_scan_modem(hd_data);	/* do it before hd_scan_mouse() */
#endif
  hd_scan_mouse(hd_data);

  /* keep these at the end of the list */
  hd_scan_cdrom(hd_data);
  hd_scan_net(hd_data);

  /* we are done... */
  hd_data->module = mod_none;

  if(hd_data->debug) {
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




/*
 * Scan the hardware list for CD-ROMs with a given volume_id. Start
 * searching at the start'th entry.
 *
 * Returns a pointer to a hardware entry (hw_t *) or NULL on failure.
 */

// ####### replace or fix this!!!

#if 0
hw_t *find_cdrom_volume(const char *volume_id, int *start)
{
  int i;
  cdrom_info_t *ci;
  hw_t *h;

  for(i = *start; i < hw_len; i++) {
    h = hw + i;
    if(
      h->base_class == bc_storage_device &&
      h->sub_class == sc_sdev_cdrom /* &&
      (h->ext_flags & (1 << cdrom_flag_media_probed)) */
    ) {
      /* ok, found a CDROM device... */
      ci = h->ext;
      /* ... now compare the volume id */
      if(
        ci &&
        (!volume_id || !strncmp(ci->volume, volume_id, strlen(volume_id)))
      ) {
        *start = i + 1;
        return h;
      }
    }
  }



  return NULL;	/* CD not found :-( */
}
#endif


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
  str_list_t *sl;

  if(hd->sub_vend || hd->sub_dev) {
    di0 = sub_device_driver(hd_data, hd->vend, hd->dev, hd->sub_vend, hd->sub_dev);
  }

  if(!di0 && (hd->vend || hd->dev)) {
    di0 = device_driver(hd_data, hd->vend, hd->dev);
  }

  if(!di0 && (hd->compat_vend || hd->compat_dev)) {
    di0 = device_driver(hd_data, hd->compat_vend, hd->compat_dev);
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
          else if(i == 1) {
            di->x11.x3d = new_str(sl->str);
          }
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
  str_list_t *sl = read_kmods(hd_data);

  for(; sl; sl = sl->next) {
    if(!strcmp(sl->str, mod)) return 1;
  }

  return 0;
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
 * cf. pcmcia-cs-3.1.1:cardmgr/probe.c
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
    { 0x1179, 0x0603 },
    { 0x1179, 0x060a },
    { 0x1179, 0x060f },
    { 0x119b, 0x1221 },
    { 0x8086, 0x1221 }
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


int hd_smp_support(hd_data_t *hd_data)
{
  return detectSMP() > 0 ? 1 : 0;
}


enum cpu_arch hd_cpu_arch(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_internal && hd->sub_class == sc_int_cpu) {
      return hd->detail->cpu.data->architecture;
    }
  }

  return arch_unknown;
}


enum boot_arch hd_boot_arch(hd_data_t *hd_data)
{
  return hd_data->boot;
}


hd_t *hd_cd_list(hd_data_t *hd_data, int rescan)
{
  hd_t *hd, *hd1, *hd_list = NULL;
  unsigned probe_save;

  if(rescan) {
    probe_save = hd_data->probe;
    hd_clear_probe_feature(hd_data, pr_all);
    hd_set_probe_feature(hd_data, pr_cdrom);
    hd_set_probe_feature(hd_data, pr_cdrom_info);
    hd_scan(hd_data);
    hd_data->probe = probe_save;
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
  unsigned probe_save;

  if(rescan) {
    probe_save = hd_data->probe;
    hd_clear_probe_feature(hd_data, pr_all);
    hd_set_probe_feature(hd_data, pr_ide);
    hd_set_probe_feature(hd_data, pr_scsi);
    hd_scan(hd_data);
    hd_data->probe = probe_save;
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
  unsigned probe_save;

  if(rescan) {
    probe_save = hd_data->probe;
    hd_clear_probe_feature(hd_data, pr_all);
    hd_set_probe_feature(hd_data, pr_net);
    hd_scan(hd_data);
    hd_data->probe = probe_save;
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

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == base_class) {
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

  hd_data->kmods = free_str_list(hd_data->kmods);

  if(!(sl0 = read_file(PROC_MODULES, 0, 0))) return NULL;

  for(sl = sl0; sl; sl = sl->next) {
    add_str_list(&sl1, strsep(&sl->str, " \t"));
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



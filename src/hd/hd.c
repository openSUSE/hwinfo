#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "hd.h"
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

#ifdef __i386__
#define HD_ARCH "ix86"
#endif

#ifdef __alpha__
#define HD_ARCH "axp"
#endif

#ifdef __PPC__
#define HD_ARCH "ppc"
#endif


// For the moment, we need it outside this file... :-(
hw_t *hw = NULL;
unsigned hw_len = 0;

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
  { mod_mouse, "mod_mouse"},
  { mod_ide, "ide"},
  { mod_scsi, "scsi"}
};


/*
 * Names for the probe flags. Used for debugging and command line parsing in
 * hw.c. Cf. enum probe_feature, hd_data_t.probe.
 */
static struct s_pr_flags {
  unsigned val;
  char *name;
} pr_flags[] = {
  { pr_memory, "memory" },
  { pr_pci, "pci" },
  { pr_pci_range, "pci.range" },
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
  { pr_scsi, "scsi" }
};


/*
 * we need a functon to *delete* hw entries!!!
 */

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

void add_res(res_t *res, enum resource_types type, unsigned long r0, unsigned long r1, unsigned long r2)
{
  res->ent = add_mem(res->ent, sizeof *res->ent, res->n);

  res->ent[res->n].type = type;
  res->ent[res->n].r0 = r0;
  res->ent[res->n].r1 = r1;
  res->ent[res->n].r2 = r2;
  res->n++;
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



void free_all_res(res_t *res)
{
  res->ent = free_mem(res->ent);
  res->n = 0;
}


// ##### no category entry anymore...
hw_t *add_hw_entry(int cat)
{
  hw = add_mem(hw, sizeof *hw, hw_len);

  hw[hw_len].idx = hw_len + 1;

  return hw + hw_len++;
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
  hd_t **hd = &hd_data->hd;

  while(*hd) hd = &(*hd)->next;

  *hd = new_mem(sizeof **hd);

  (*hd)->idx = ++(hd_data->last_idx);
  (*hd)->module = hd_data->module;
  (*hd)->line = line;
  (*hd)->count = count;

  return *hd;
}


void hd_scan(hd_data_t *hd_data)
{
  char *s;
  int i;
  unsigned u;

  /* log the debug & probe flags */
  if(hd_data->debug) {
    ADD2LOG(
      "libhd version %s (%s)\ndebug = 0x%x\nprobe = 0x%x (",
      HD_VERSION, HD_ARCH, hd_data->debug, hd_data->probe
    );

    for(u = 0, i = 0; u < 8 * sizeof hd_data->probe; u++) {
      if((s = probe_flag2str(u))) {
        ADD2LOG("%s%c%s", i++ ? " " : "", hd_data->probe & (1 << u) ? '+' : '-', s);
      }
    }

    ADD2LOG(")\n");
  }

  /* for various reasons, do it befor scan_misc() */
  hd_scan_floppy(hd_data);

  /* get basic system info */
  hd_scan_misc(hd_data);

  /* start the detection  */
  hd_scan_cpu(hd_data);
  hd_scan_memory(hd_data);
  hd_scan_monitor(hd_data);

#if defined(__i386__)
  hd_scan_bios(hd_data);
#endif

#if defined(__i386__) || defined(__alpha__)
  hd_scan_isapnp(hd_data);
#endif

  hd_scan_pci(hd_data);
  hd_scan_serial(hd_data);

  /* merge basic system info & the easy stuff */
  hd_scan_misc2(hd_data);

  hd_scan_mouse(hd_data);
  hd_scan_ide(hd_data);
  hd_scan_scsi(hd_data);

  /* keep these at the end of the list */
  hd_scan_cdrom(hd_data);
  hd_scan_net(hd_data);

  /* we are done... */
  hd_data->module = mod_none;
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
    if(s[i] < 'A' - 1 || s[i] > 'A' - 1 + 0x1f) return MAKE_EISA_ID(0);
    u += s[i] - 'A' + 1;
  }

  return MAKE_EISA_ID(u);
}


/*
 * Create a 'canonical' version, i.e. no spaces at start and end.
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


void free_hw()
{
  int i;
  hw_t *h;

  if(hw) for(i = 0; i < hw_len; i++) {
    h = hw + i;

    free_mem(h->dev_name);
    free_mem(h->vend_name);
    free_mem(h->sub_dev_name);
    free_mem(h->sub_vend_name);
    free_mem(h->rev_name);
    free_mem(h->serial);

    free_mem(h->err_text1);
    free_mem(h->err_text2);

    free_mem(h->unix_dev_name);
    free_all_res(&h->res);

    // ###### ... h->ext ... 
  }


  hw = free_mem(hw);
  hw_len = 0;
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


/*
 * Get the current hardware list and its size.
 */
hw_t *hw_get_list(unsigned *list_len)
{
  *list_len = hw_len;

  return hw;
}


// n: decimals
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


// n: decimals
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
 * Find hardware entry with given index.
 */
hd_t *get_device_by_idx(hd_data_t *hd_data, int idx)  
{
  hd_t *hd;

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
 * Add a string to a string list; just for convenience.
 */
str_list_t *add_str_list(str_list_t **sl, char *str)
{
  while(*sl) sl = &(*sl)->next;

  *sl = new_mem(sizeof **sl);
  (*sl)->str = str;

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
  char buf1[32], buf2[32], *fn;

  if(!msg) msg = "";

  sprintf(buf1, "%u", hd_data->module);
  sprintf(buf2, ".%u", count);
  fn = mod_name_by_idx(hd_data->module);

  if((hd_data->debug & HD_DEB_PROGRESS))
    ADD2LOG(">> %s.%u%s: %s\n", *fn ? fn : buf1, pos, count ? buf2 : "", msg);

  if(hd_data->progress) hd_data->progress(hd_data->module, pos, count, msg);
}



/*
 * Returns a bitmask suitable for (hd_data_t).probe. If name is not a valid
 * probe feature, 0 is returned.
 *
 * 'all' stands magically for 'all features'.
 */
unsigned str2probe_flag(char *name)
{
  unsigned u;

  if(!strcmp(name, "all")) return -1;

  for(u = 0; u < sizeof pr_flags / sizeof *pr_flags; u++)
    if(!strcmp(name, pr_flags[u].name)) return 1 << pr_flags[u].val;

  return 0;
}


/*
 * Coverts a enum probe_feature to a string.
 * Note: flag is *not* a bitmask, so this function is *not* the inverse of
 * str2probe_flag()!
 */
char *probe_flag2str(unsigned flag)
{
  unsigned u;

  for(u = 0; u < sizeof pr_flags / sizeof *pr_flags; u++)
    if(flag == pr_flags[u].val) return pr_flags[u].name;

  return NULL;
}


/*
 * Removes all hd_data->hd entries created by the current module from the
 * list. The old entries are added to hd_data->old_hd.
 *
 * // ##### Returns a linked list of the removed entries.
 */
void remove_hd_entries(hd_data_t *hd_data)
{
  hd_t *hd, **prev, **h;

  for(hd = *(prev = &hd_data->hd); hd; /* hd = *(prev = &hd->next) */) {
    if(hd->module == hd_data->module) {
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


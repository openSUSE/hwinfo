#define _GNU_SOURCE		/* memmem */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

#include "../hd/hddb_int.h"

typedef enum hw_item {
  hw_none = 0, hw_sys, hw_cpu, hw_keyboard, hw_braille, hw_mouse,
  hw_joystick, hw_printer, hw_scanner, hw_chipcard, hw_monitor, hw_tv,
  hw_display, hw_framebuffer, hw_camera, hw_sound, hw_storage_ctrl,
  hw_network_ctrl, hw_isdn, hw_modem, hw_network, hw_disk, hw_partition,
  hw_cdrom, hw_floppy, hw_manual, hw_usb_ctrl, hw_usb, hw_bios, hw_pci,
  hw_isapnp, hw_bridge, hw_hub, hw_scsi, hw_ide, hw_memory, hw_dvb,
  hw_pcmcia, hw_pcmcia_ctrl, hw_ieee1394, hw_ieee1394_ctrl, hw_hotplug,
  hw_hotplug_ctrl, hw_zip, hw_pppoe, hw_wlan, hw_redasd, hw_dsl, hw_block,
  hw_tape, hw_vbe, hw_bluetooth, hw_nvme,
  /* append new entries here */
  hw_unknown, hw_all                                    /* hw_all must be last */
} hd_hw_item_t;

#include "../hd/hwclass_names.h"

#define TAG_PCI		1	/* pci ids */
#define TAG_EISA	2	/* eisa ids */
#define TAG_USB		3	/* usb ids */
#define TAG_SPECIAL	4	/* internally used ids */
#define TAG_PCMCIA	5	/* pcmcia ids */
#define TAG_SDIO	6	/* sdio ids */

#define ID_VALUE(id)		((id) & 0xffff)
#define ID_TAG(id)		(((id) >> 16) & 0xf)
#define MAKE_ID(tag, id_val)	((tag << 16) | (id_val))

typedef uint32_t hddb_entry_mask_t;

typedef enum {
  match_any, match_all
} match_t;

typedef enum {
  pref_empty, pref_new, pref_and, pref_or, pref_add
} prefix_t;

typedef struct line_s {
  prefix_t prefix;
  hddb_entry_t key;
  char *value;
} line_t;

typedef struct str_s {
  struct str_s *next;
  char *str;
} str_t;

typedef struct list_any_s {
  struct list_any_s *next;
} list_any_t;

typedef struct {
  void *first;
  void *last;
} list_t;

typedef struct {
  unsigned flag;
  unsigned remove;
} hid_any_t;

typedef struct {
  unsigned flag;
  unsigned remove;
  unsigned tag;
  unsigned id;
  unsigned range;
  unsigned mask;
  struct {
    unsigned range:1;
    unsigned mask:1;
  } has;
} hid_num_t;

typedef struct {
  unsigned flag;
  unsigned remove;
  list_t list;
} hid_str_t;

typedef union {
  hid_any_t any;
  hid_num_t num;
  hid_str_t str;
} hid_t;

typedef struct skey_s {
  struct skey_s *next;
  hid_t *hid[he_nomask];
} skey_t;

typedef struct item_s {
  struct item_s *next;
  unsigned remove:1;
  char *pos;
  list_t key;	/* skey_t */
  skey_t *value;
} item_t;


typedef struct hddb_list_s {   
  hddb_entry_mask_t key_mask;
  hddb_entry_mask_t value_mask;
  unsigned key;
  unsigned value;
} hddb_list_t;

typedef struct {
  unsigned list_len, list_max;
  hddb_list_t *list;
  unsigned ids_len, ids_max;
  unsigned *ids;
  unsigned strings_len, strings_max;
  char *strings;
} hddb_data_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#ifdef UCLIBC
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);       
#endif
void *new_mem(size_t size);
void *free_mem(void *ptr);
char *new_str(char *str);
void *add_list(list_t *list, void *entry);
void sort_list(list_t *list, int (*cmp_func)(const void *, const void *));
unsigned eisa_id(char *s);
char *eisa_str(unsigned id);
void write_stats(FILE *f);

void read_items(char *file);
line_t *parse_line(char *str);
hddb_entry_mask_t add_entry(skey_t *skey, hddb_entry_t idx, char *val);

void write_items(char *file, list_t *hd);
void write_item(FILE *f, item_t *item);
void write_skey(FILE *f, prefix_t pre, skey_t *skey);
void write_ent_name(FILE *f, hid_t *hid, char pre, hddb_entry_t ent);
void write_id(FILE *f, hddb_entry_t ent, hid_t *hid);
void write_drv(FILE *f, char pre, hid_t *hid);
void write_drv1(FILE *f, hid_t *hid, char pre, char *val);
void log_items(FILE *f, item_t *item0, item_t *item1);

int count_common_hids(skey_t *skey0, skey_t *skey1);
int strip_skey(skey_t *skey0, skey_t *skey1, int do_it);
void remove_deleted_hids(skey_t *skey);
void undelete_hids(skey_t *skey);
str_t *split(char del, char *s);
char *join(char del, str_t *str);

int cmp_driver_info(char *str0, char *str1);
int cmp_str_s(const void *p0, const void *p1);
int cmp_hid(hid_t *hid0, hid_t *hid1);
int cmp_skey(skey_t *skey0, skey_t *skey1);
int cmp_skey_s(const void *p0, const void *p1);
int cmp_item(item_t *item0, item_t *item1);
int cmp_item_s(const void *p0, const void *p1);

int match_hid(hid_t *hid0, hid_t *hid1, match_t match);
int match_skey(skey_t *skey0, skey_t *skey1, match_t match);
int match_item(item_t *item0, item_t *item1, match_t match);

int combine_keys(skey_t *skey0, skey_t *skey1);

str_t *clone_str(str_t *str);
hid_t *clone_hid(hid_t *hid);
skey_t *clone_skey(skey_t *skey);
item_t *clone_item(item_t *item);

str_t *free_str(str_t *str, int follow_next);
hid_t *free_hid(hid_t *hid);
skey_t *free_skey(skey_t *skey, int follow_next);
item_t *free_item(item_t *item, int follow_next);

unsigned driver_entry_types(hid_t *hid);

void remove_items(list_t *hd);
void remove_nops(list_t *hd);
void check_items(list_t *hd);
void split_items(list_t *hd);
void combine_driver(list_t *hd);
void combine_requires(list_t *hd);
void join_items_by_value(list_t *hd);
void join_items_by_key(list_t *hd);
void remove_unimportant_items(list_t *hd);

void write_cfile(FILE *f, list_t *hd);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
struct option options[] = {
  { "help", 0, NULL, 1 },
//  { "debug", 1, NULL, 2  },
  { "log", 1, NULL, 3 },
  { "mini", 0, NULL, 4 },
  { "sort", 0, NULL, 5 },
  { "reverse", 0, NULL, 6 },	/* for debugging */
  { "random", 0, NULL, 7 },	/* dto */
  { "check", 0, NULL, 8 },
  { "with-source", 0, NULL, 9 },
  { "out", 1, NULL, 10},
  { "split", 0, NULL, 11},
  { "cfile", 1, NULL, 12},
  { "no-compact", 0, NULL, 13},
  { "join-keys-first", 0, NULL, 14},
  { "combine", 0, NULL, 15},
  { "no-range", 0, NULL, 16},
  { }
};

list_t hd;

char *item_ind = NULL;
FILE *logfh = NULL;

struct {
  int debug;
  unsigned sort:1;
  unsigned reverse:1;
  unsigned random:1;
  unsigned check:1;
  unsigned with_source:1;
  unsigned mini:1;
  unsigned split:1;
  unsigned no_compact:1;
  unsigned join_keys_first:1;
  unsigned combine:1;		/* always combine driver info */
  unsigned no_range:1;		/* don't create entries with ranges */
  char *logfile;
  char *outfile;
  char *cfile;
} opt = {
  logfile: "hd.log",
  outfile: "hd.ids"
};

struct {
  unsigned items_in, items_out;
  unsigned diffs, errors, errors_res;
} stats;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
int main(int argc, char **argv)
{
  int i, close_log = 0, close_cfile = 0;
  item_t *item;
  FILE *cfile;

  for(opterr = 0; (i = getopt_long(argc, argv, "", options, NULL)) != -1; ) {
    switch(i) {
#if 0
      case 2:
        opt.debug = strtol(optarg, NULL, 0);
        break;
#endif

      case 3:
        opt.logfile = optarg;
        if(!*opt.logfile) opt.logfile = NULL;
        break;

      case 4:
        opt.mini = 1;
        break;

      case 5:
        opt.sort = 1;
        break;

      case 6:
        opt.reverse = 1;
        break;

      case 7:
        opt.random = 1;
        srand(time(NULL));
        break;

      case 8:
        opt.check = 1;
        break;

      case 9:
        opt.with_source = 1;
        break;

      case 10:
        opt.outfile = optarg;
        if(!*opt.outfile) opt.outfile = NULL;
        break;

      case 11:
        opt.split = 1;
        break;

      case 12:
        opt.cfile = optarg;
        if(!*opt.cfile) opt.cfile = NULL;
        break;

      case 13:
        opt.no_compact = 1;
        break;

      case 14:
        opt.join_keys_first = 1;
        break;

      case 15:
        opt.combine = 1;
        break;

      case 16:
        opt.no_range = 1;
        break;

      default:
        fprintf(stderr,
          "Usage: check_hd [options] files\n"
          "Try to put hardware data into a consistent form.\n"
          "  --check\t\tdo a lot of checks and remove unnecessary data\n"
          "  --sort\t\tsort data\n"
          "  --reverse\t\treverse sorting order\n"
          "  --split\t\twrite separate entries for each key\n"
          "  --with-source\t\tadd comment to each item indicating info source\n"
          "  --mini\t\tminimal data base (basically driver info only)\n"
          "  --join-keys-first\twhen combining similar items, join entries with\n"
          "  \t\t\tcommon keys first (default is common values first)\n"
          "  --cfile file\t\tcreate C file to be included in libhd\n"
          "  --no-compact\t\tdon't try to make C version as small as possible\n"
          "  --out file\t\twrite results to file, default is \"hd.ids\"\n"
          "  --log file\t\twrite log info to file, default is \"hd.log\"\n\n"
          "  Note: check_hd works with libhd/hwinfo internal format only;\n"
          "  to convert to other formats, use convert_hd\n"
        );
        return 1;
    }
  }

  if(opt.logfile && strcmp(opt.logfile, "-")) {
    logfh = fopen(opt.logfile, "w");
    if(!logfh) {
      perror(opt.logfile);
      return 3;
    }
    close_log = 1;
  }
  else {
    logfh = stdout;
  }

  for(argv += optind; *argv; argv++) {
    read_items(*argv);
  }

  for(item = hd.first; item; item = item->next) stats.items_in++;

  fprintf(logfh, "- removing useless entries\n");
  fflush(logfh);
  remove_nops(&hd);

  if(opt.mini) {
    fprintf(logfh, "- building mini version\n");
    fflush(logfh);
    remove_unimportant_items(&hd);
  }

  if(opt.check || opt.split) {
    fprintf(logfh, "- splitting entries\n");
    fflush(logfh);
    split_items(&hd);
  }

  if(opt.check) {
    fprintf(logfh, "- combining driver info\n");
    fflush(logfh);
    combine_driver(&hd);

    fprintf(logfh, "- combining requires info\n");
    fflush(logfh);
    combine_requires(&hd);

    fprintf(logfh, "- checking for consistency\n");
    fflush(logfh);
    check_items(&hd);

    fprintf(logfh, "- join items\n");
    fflush(logfh);
    if(opt.join_keys_first) {
      join_items_by_key(&hd);
      join_items_by_value(&hd);
    }
    else {
      join_items_by_value(&hd);
      join_items_by_key(&hd);
    }

    if(opt.split) split_items(&hd);
  }

  if(opt.sort) {
    fprintf(logfh, "- sorting\n");
    fflush(logfh);
    sort_list(&hd, cmp_item_s);
  }

  for(item = hd.first; item; item = item->next) stats.items_out++;

  write_items(opt.outfile, &hd);

  if(opt.cfile) {
    if(opt.cfile && strcmp(opt.cfile, "-")) {
      cfile = fopen(opt.cfile, "w");
      if(!cfile) {
        perror(opt.cfile);
        return 3;
      }
      close_cfile = 1;
    }
    else {
      cfile = stdout;
    }

    split_items(&hd);

    write_cfile(cfile, &hd);

    if(close_cfile) fclose(cfile);
  }

  fprintf(logfh, "- statistics\n");
  write_stats(logfh);
  if(logfh != stdout) {
    if(opt.outfile && strcmp(opt.outfile, "-")) {
      fprintf(stderr, "data written to \"%s\"\n", opt.outfile);
    }
    if(opt.logfile && strcmp(opt.logfile, "-")) {
      fprintf(stderr, "log written to \"%s\"\n", opt.logfile);
    }
    fprintf(stderr, "statistics:\n");
    write_stats(stderr);
  }

  free_item(hd.first, 1);

  if(close_log) fclose(logfh);

  return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void *new_mem(size_t size)
{
  if(size == 0) return NULL;

  return calloc(size, 1);
}


void *free_mem(void *ptr)
{
  if(ptr) free(ptr);

  return NULL;
}


char *new_str(char *str)
{
  if(!str) return NULL;

  return strdup(str);
}


void *add_list(list_t *list, void *entry)
{
  if(list->last) {
    ((list_any_t *) list->last)->next = entry;
  }
  list->last = entry;

  if(!list->first) {
    list->first = entry;
  }

  return entry;
}


void sort_list(list_t *list, int (*cmp_func)(const void *, const void *))
{
  int i, list_len = 0;
  list_any_t *list_entry;
  list_t new_list = {};
  list_any_t **list_array;

  for(list_entry = list->first; list_entry; list_entry = list_entry->next) list_len++;
  if(list_len < 2) return;

  list_array = new_mem(list_len * sizeof *list_array);
  for(i = 0, list_entry = list->first; list_entry; list_entry = list_entry->next) {
    list_array[i++] = list_entry;
  }

  qsort(list_array, list_len, sizeof *list_array, cmp_func);

  for(i = 0; i < list_len; i++) {
    add_list(&new_list, list_array[i]);
  }

  if(new_list.last) {
    ((list_any_t *) new_list.last)->next = NULL;
  }

  *list = new_list;

  free_mem(list_array);
}


unsigned eisa_id(char *s)
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


char *eisa_str(unsigned id)
{
  static char s[4];

  s[0] = ((id >> 10) & 0x1f) + 'A' - 1;
  s[1] = ((id >>  5) & 0x1f) + 'A' - 1;
  s[2] = ( id        & 0x1f) + 'A' - 1;
  s[3] = 0;

  return s;
}


void write_stats(FILE *f)
{
  fprintf(f, "  %u inconsistencies%s\n", stats.diffs, stats.diffs ? " fixed" : "");
  fprintf(f, "  %u errors", stats.errors + stats.errors_res);
  if(stats.errors_res) fprintf(f, ", %u resolved", stats.errors_res);
  fprintf(f, "\n");
  fprintf(f, "  %u items in\n", stats.items_in);
  fprintf(f, "  %u items out\n", stats.items_out);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void read_items(char *file)
{
  FILE *f;
  char buf[1024], fpos[256];
  unsigned u, state, l_nr;
  hddb_entry_mask_t entry_mask = 0;
  line_t *l;
  item_t *item;
  skey_t *skey;

  if(!(f = fopen(file, "r"))) {
    perror(file);
    return;
  }

  item = new_mem(sizeof *item);
  skey = new_mem(sizeof *skey);
  
  sprintf(fpos, "%s(1)", file);
  item->pos = new_str(fpos);

  for(l_nr = 1, state = 0; fgets(buf, sizeof buf, f); l_nr++) {
    l = parse_line(buf);
    if(!l) {
      fprintf(stderr, "%s: invalid line\n", fpos);
      state = 4;
      break;
    };
    if(l->prefix == pref_empty) continue;
    switch(l->prefix) {
      case pref_new:
        if(state == 1) {
          add_list(&item->key, skey);
          skey = new_mem(sizeof *skey);
        }
        else if(state == 2) {
          item->value = skey;
          skey = new_mem(sizeof *skey);
        }
        if(state == 2 || state == 1) {
          add_list(&hd, item);
          item = new_mem(sizeof *item);
          if(!item->pos) {
            sprintf(fpos, "%s(%d)", file, l_nr);
            item->pos = new_str(fpos);
          }
        }
        entry_mask = 0;
        state = 1;
        break;

      case pref_and:
        if(state != 1) {
          fprintf(stderr, "%s: must start item first\n", fpos);
          state = 4;
          break;
        }
        break;

      case pref_or:
        if(state != 1 || !entry_mask) {
          fprintf(stderr, "%s: must start item first\n", fpos);
          state = 4;
          break;
        }
        add_list(&item->key, skey);
        skey = new_mem(sizeof *skey);
        entry_mask = 0;
        break;

      case pref_add:
        if(state == 1 && !entry_mask) {
          fprintf(stderr, "%s: driver info not allowed\n", fpos);
          state = 4;
          break;
        }
        if(state == 1) {
          add_list(&item->key, skey);
          skey = new_mem(sizeof *skey);
          entry_mask = 0;
          state = 2;
        }
        if(state != 2) {
          fprintf(stderr, "%s: driver info not allowed\n", fpos);
          state = 4;
          break;
        }
        break;

      default:
        state = 4;
    }

    if(state != 4) {
      u = add_entry(skey, l->key, l->value);
      if(u) {
        entry_mask |= u;
      }
      else {
        fprintf(stderr, "%s: invalid info\n", fpos);
        state = 4;
      }
    }

    if(state == 4) break;	/* error */

  }

  /* finalize last item */
  if(entry_mask && (state == 1 || state == 2)) {
    if(state == 1) {
      add_list(&item->key, skey);
      skey = NULL;
    }
    else if(state == 2) {
      item->value = skey;
      skey = NULL;
    }
    add_list(&hd, item);
    item = NULL;
  }

  free_mem(skey);
  free_mem(item);

  fclose(f);
}


line_t *parse_line(char *str)
{
  static line_t l;
  char *s;
  int i;

  /* drop leading spaces */
  while(isspace(*str)) str++;

  /* skip emtpy lines and comments */
  if(!*str || *str == ';' || *str == '#') {
    l.prefix = pref_empty;
    return &l;
  }

  l.prefix = pref_new;

  switch(*str) {
    case '&':
      l.prefix = pref_and;
      str++;
      break;

    case '|':
      l.prefix = pref_or;
      str++;
      break;

    case '+':
      l.prefix = pref_add;
      str++;
      break;
  }

  /* skip spaces */
  while(isspace(*str)) str++;

  s = str;
  while(*str && !isspace(*str)) str++;
  if(*str) *str++ = 0;
  while(isspace(*str)) str++;

  for(i = 0; (unsigned) i < sizeof hddb_entry_strings / sizeof *hddb_entry_strings; i++) {
    if(!strcmp(s, hddb_entry_strings[i])) {
      l.key = i;
      break;
    }
  }

  if((unsigned) i >= sizeof hddb_entry_strings / sizeof *hddb_entry_strings) return NULL;

  l.value = str;

  /* drop trailing white space */
  i = strlen(str);
  while(i > 0) {
    if(isspace(str[i - 1]))
      str[--i] = 0;
    else
      break;
  }

  /* special case: drop leading and final double quotes, if any */
  i = strlen(l.value);
  if(i >= 2 && l.value[0] == '"' && l.value[i - 1] == '"') {
    l.value[i - 1] = 0;
    l.value++;
  }

  // fprintf(stderr, "pre = %d, key = %d, val = \"%s\"\n", l.prefix, l.key, l.value);

  return &l;
}


int parse_id(char *str, unsigned *id, unsigned *tag, unsigned *range, unsigned *mask)
{
  static unsigned id0, val;
  char c = 0, *s, *t = NULL;

  *id = *tag = *range = *mask = 0;

  if(!str || !*str) return 0;
  
  for(s = str; *str && !isspace(*str); str++);
  if(*str) {
    c = *(t = str);	/* remember for later */
    *str++ = 0;
  }
  while(isspace(*str)) str++;

  if(*s) {
    if(!strcmp(s, "pci")) *tag = TAG_PCI;
    else if(!strcmp(s, "usb")) *tag = TAG_USB;
    else if(!strcmp(s, "special")) *tag = TAG_SPECIAL;
    else if(!strcmp(s, "eisa")) *tag = TAG_EISA;
    else if(!strcmp(s, "isapnp")) *tag = TAG_EISA;
    else if(!strcmp(s, "pcmcia")) *tag = TAG_PCMCIA;
    else if(!strcmp(s, "sdio")) *tag = TAG_SDIO;
    else {
      str = s;
      if(t) *t = c;	/* restore */
    }
  }

  id0 = strtoul(str, &s, 0);

  if(s == str) {
    id0 = eisa_id(str);
    if(!id0) return 0;
    s = str + 3;
    id0 = ID_VALUE(id0);
    if(!*tag) *tag = TAG_EISA;
  }

  while(isspace(*s)) s++;
  if(*s && *s != '&' && *s != '+') return 0;

  *id = id0;

  if(!*s) return 1;

  c = *s++;

  while(isspace(*s)) s++;

  val = strtoul(s, &str, 0);

  if(s == str) return 0;

  while(isspace(*str)) str++;

  if(*str) return 0;

  if(c == '+') *range = val; else *mask = val;

  return c == '+' ? 2 : 3;
}


hddb_entry_mask_t add_entry(skey_t *skey, hddb_entry_t idx, char *val)
{
  hddb_entry_mask_t e_mask = 0;
  int i;
  unsigned id, tag, range, mask, u, u1;
  char *s, *s1, *s2, c;
  hid_t *hid;
  str_t *str, *str0;

  for(i = 0; (unsigned) i < sizeof hddb_is_numeric / sizeof *hddb_is_numeric; i++) {
    if(idx == hddb_is_numeric[i]) break;
  }

  // printf("i = %d, idx = %d, val = >%s<\n", i, idx, val);

  if((unsigned) i < sizeof hddb_is_numeric / sizeof *hddb_is_numeric) {
    /* numeric id */
    e_mask |= 1 << idx;

    /* special */
    if(idx == he_hwclass) {
      tag = id = 0;

      str0 = split('|', val);
      for(u1 = 0, str = str0; str && u1 <= 16; str = str->next) {
        u = hd_hw_item_type(str->str);
        if(u) {
          id += u << u1;
          u1 += 8;
        }
      }
      free_str(str0, 1);

      i = 1;
    }
    else {
      i = parse_id(val, &id, &tag, &range, &mask);
    }

    // printf("parse_id = %d\n", i);

    if(i) {
      skey->hid[idx] = hid = new_mem(sizeof *hid);
      hid->num.flag = FLAG_ID;
      hid->num.tag = tag;
      hid->num.id = id;
    }
    else {
      return 0;
    }

    switch(i) {
      case 1:
        break;

      case 2:
        hid->num.range = range;
        hid->num.has.range = 1;
        break;

      case 3:
        hid->num.mask = mask;
        hid->num.has.mask = 1;
        break;

      default:
        return 0;
    }
  }
  else {
    if(idx < he_nomask) {
      /* strings */

      e_mask |= 1 << idx;
      skey->hid[idx] = hid = new_mem(sizeof *hid);
      hid->str.flag = FLAG_STRING;
      str = add_list(&hid->str.list, new_mem(sizeof *str));
      str->str = new_str(val);
    }
    else {
      /* special */

      if(idx == he_class_id) {
        i = parse_id(val, &id, &tag, &range, &mask);
        if(i != 1) return 0;

        skey->hid[he_baseclass_id] = hid = new_mem(sizeof *hid);
        hid->num.flag = FLAG_ID;
        hid->num.tag = tag;
        hid->num.id = id >> 8;

        skey->hid[he_subclass_id] = hid = new_mem(sizeof *hid);
        hid->num.flag = FLAG_ID;
        hid->num.tag = tag;
        hid->num.id = id & 0xff;

        e_mask |= (1 << he_baseclass_id) + (1 << he_subclass_id) /* + (1 << he_progif_id) */;
      }
      else {
        switch(idx) {
          case he_driver_module_insmod:
            c = 'i';
            break;

          case he_driver_module_modprobe:
            c = 'm';
            break;

          case he_driver_module_config:
            c = 'M';
            break;

          case he_driver_xfree:
            c = 'x';
            break;

          case he_driver_xfree_config:
            c = 'X';
            break;

          case he_driver_mouse:
            c = 'p';
            break;

          case he_driver_display:
            c = 'd';
            break;

          case he_driver_any:
            c = 'a';
            break;

          default:
            c = 0;
            break;
        }
        if(c) {
          s = new_mem(strlen(val) + 3);
          s[0] = c;
          s[1] = '\t';
          strcpy(s + 2, val);
          hid = skey->hid[he_driver];
          if(!hid) {
            skey->hid[he_driver] = hid = new_mem(sizeof *hid);
            hid->str.flag = FLAG_STRING;
          }
          if(
            (c == 'X' || c == 'M') &&
            hid->str.list.last &&
            (s1 = ((str_t *) hid->str.list.last)->str)
          ) {
            s2 = new_mem(strlen(s1) + strlen(s) + 2);
            sprintf(s2, "%s\001%s", s1, s);
            free_mem(s1);
            ((str_t *) hid->str.list.last)->str = s2;
          }
          else {
            str = add_list(&hid->str.list, new_mem(sizeof *str));
            str->str = new_str(s);
          }
          e_mask |= (1 << he_driver);
          s = free_mem(s);
        }
      }
    }
  }

  return e_mask;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void write_items(char *file, list_t *hd)
{
  FILE *f;
  item_t *item;
  int close_it = 0;

  if(file && strcmp(file, "-")) {
    f = fopen(file, "w");
    if(!f) {
      perror(file);
      return;
    }
    close_it = 1;
  }
  else {
    f = stdout;
  }

  for(item = hd->first; item; item = item->next) {
    if(opt.with_source) fprintf(f, "# %s\n", item->pos);
    write_item(f, item);
    fputc('\n', f);
  }

  if(close_it) fclose(f);
}


void write_item(FILE *f, item_t *item)
{
  skey_t *skey;
  prefix_t pre;

  pre = pref_new;
  for(skey = item->key.first; skey; skey = skey->next) {
    write_skey(f, pre, skey);
    pre = pref_or;
  }
  write_skey(f, pref_add, item->value);
}


void write_skey(FILE *f, prefix_t pre, skey_t *skey)
{
  static char pref_char[5] = { ' ', ' ', '&', '|', '+' };
  int i;

  if(pre >= sizeof pref_char) {
    fprintf(stderr, "internal oops\n");
    exit(2);
  }

  if(!skey) return;

  for(i = 0; (unsigned) i < sizeof skey->hid / sizeof *skey->hid; i++) {
    if(skey->hid[i]) {
      if(i != he_driver) {
        write_ent_name(f, skey->hid[i], pref_char[pre], i);
        write_id(f, i, skey->hid[i]);
        fputc('\n', f);
      }
      else {
        write_drv(f, pref_char[pre], skey->hid[i]);
      }
      if(pre != pref_add) pre = pref_and;
    }
  }
}


void write_ent_name(FILE *f, hid_t *hid, char pre, hddb_entry_t ent)
{
  int len, tab_ind = 24;
  char c;

  if(ent >= sizeof hddb_entry_strings / sizeof *hddb_entry_strings) {
    fprintf(stderr, "internal oops\n");
    exit(2);
  }

  len = item_ind ? strlen(item_ind) : 0;

  if(!len) {
    fprintf(f, "%c%s\t", pre, hddb_entry_strings[ent]);
  }
  else {
    c = hid->any.remove ? '*' : ':';
    fprintf(f, "%s%c %c%s\t", item_ind, c, pre, hddb_entry_strings[ent]);
    len += 2;
    tab_ind += 8;
  }

  len += strlen(hddb_entry_strings[ent]) + 1;

  for(len = (len & ~7) + 8; len < tab_ind; len += 8) {
    fputc('\t', f);
  }
}


void write_id(FILE *f, hddb_entry_t ent, hid_t *hid)
{
  static char *tag_name[7] = { "", "pci ", "eisa ", "usb ", "special ", "pcmcia ", "sdio " };
  int tag;
  unsigned u;
  char c, *s;

  switch(hid->any.flag) {
    case FLAG_ID:
      tag = hid->num.tag;
      if((unsigned) tag >= sizeof tag_name / sizeof *tag_name) {
        fprintf(stderr, "internal oops\n");
        exit(2);
      }
      if(ent == he_hwclass) {
        /* is special */
        for(u = (hid->num.id & 0xffffff); u; u >>= 8) {
          s = hd_hw_item_name(u & 0xff);
          if(s) fprintf(f, "%s", s);
          if(u > 0x100) fprintf(f, "|");
        }
      }
      else if(tag == TAG_EISA && (ent == he_vendor_id || ent == he_subvendor_id)) {
        fprintf(f, "%s", eisa_str(hid->num.id));
      }
      else {
        u = 4;
        if(ent == he_bus_id || ent == he_subclass_id || ent == he_progif_id) {
          u = 2;
        }
        else if(ent == he_baseclass_id) {
          u = 3;
        }
        fprintf(f, "%s0x%0*x", tag_name[tag], u, hid->num.id);
      }
      if(hid->num.has.range || hid->num.has.mask) {
        if(hid->num.has.range) {
          u = hid->num.range;
          c = '+';
        }
        else {
          u = hid->num.mask;
          c = '&';
        }
        fprintf(f, "%c0x%04x", c, u);
      }
      break;

    case FLAG_STRING:
      if(	/* not exactly 1 string */
        !hid->str.list.first ||
        ((str_t *) hid->str.list.first)->next
      ) {
        fprintf(stderr, "internal oops\n");
        exit(2);
      }
      fprintf(f, "%s", ((str_t *) hid->str.list.first)->str);
      break;

    default:
      fprintf(stderr, "internal oops\n");
      exit(2);
      break;
  }
}


void write_drv(FILE *f, char pre, hid_t *hid)
{
  str_t *str;
  char *s, *t;

  if(hid->any.flag != FLAG_STRING) {
    fprintf(stderr, "internal oops\n");
    exit(2);
  }

  for(str = hid->str.list.first; str; str = str->next) {
    for(s = str->str; (t = strchr(s, '\001')); s = t + 1) {
      *t = 0;
      write_drv1(f, hid, pre, s);
      *t = '\001';
    }
    write_drv1(f, hid, pre, s);
  }
}


void write_drv1(FILE *f, hid_t *hid, char pre, char *val)
{
  char type;
  int ent;

  type = val[0];
  if(!type || val[1] != '\t') {
    fprintf(stderr, "internal oops\n");
    exit(2);
  }

  switch(type) {
    case 'x':
      ent = he_driver_xfree;
      break;

    case 'X':
      ent = he_driver_xfree_config;
      break;

    case 'i':
      ent = he_driver_module_insmod;
      break;

    case 'm':
      ent = he_driver_module_modprobe;
      break;

    case 'M':
      ent = he_driver_module_config;
      break;

    case 'p':
      ent = he_driver_mouse;
      break;

    case 'd':
      ent = he_driver_display;
      break;

    case 'a':
      ent = he_driver_any;
      break;

    default:
      fprintf(stderr, "internal oops\n");
      exit(2);
      break;
  }

  write_ent_name(f, hid, pre, ent);
  fprintf(f, "%s\n", val + 2);

}


void log_items(FILE *f, item_t *item0, item_t *item1)
{
  char *save_ind = item_ind;

  if(item0) {
    item_ind = "  0";
    write_item(f, item0);
  }

  if(item1) {
    item_ind = "  1";
    write_item(f, item1);
  }

  item_ind = save_ind;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* count common defined hid entries */
int count_common_hids(skey_t *skey0, skey_t *skey1)
{
  int i, cnt = 0;

  if(!skey0 || !skey1) return 0;

  for(i = 0; (unsigned) i < sizeof skey0->hid / sizeof *skey0->hid; i++) {
    if(skey0->hid[i] && skey1->hid[i]) cnt++;
  }

  return cnt;
}


/*
 * remove hid entries from skey0 that are defined in skey1
 *
 * do_it:
 *   0: don't remove anything, just count
 *   1: remove identical entries
 *   2: remove differing entries
 *   3: both of the above
 *
 * return
 *   bits  0- 7: identical entries
 *         8-15: different entries
 *        16-23: critical conflicts
 */
int strip_skey(skey_t *skey0, skey_t *skey1, int do_it)
{
  int i, cnt;

  for(i = cnt = 0; (unsigned) i < sizeof skey0->hid / sizeof *skey0->hid; i++) {
    if(!skey0->hid[i] || !skey1->hid[i]) continue;
    if(cmp_hid(skey0->hid[i], skey1->hid[i])) {
      cnt += 1 << 8;
      if(i == he_driver || i == he_requires) {
        cnt += 1 << 16;
      }
      if((do_it & 2)) skey0->hid[i]->any.remove = 1;
    }
    else {
      cnt++;
      if((do_it & 1)) skey0->hid[i]->any.remove = 1;
    }
  }

  return cnt;
}


/*
 * remove deleted hid entries from skey
 */
void remove_deleted_hids(skey_t *skey)
{
  int i;

  for(i = 0; (unsigned) i < sizeof skey->hid / sizeof *skey->hid; i++) {
    if(skey->hid[i] && skey->hid[i]->any.remove) {
      skey->hid[i] = free_hid(skey->hid[i]);
    }
  }
}


/*
 * undeleted hid entries from skey
 */
void undelete_hids(skey_t *skey)
{
  int i;

  for(i = 0; (unsigned) i < sizeof skey->hid / sizeof *skey->hid; i++) {
    if(skey->hid[i]) skey->hid[i]->any.remove = 0;
  }
}


str_t *split(char del, char *s)
{
  char *t, *s0;
  list_t list = {};
  str_t *str;

  if(!s) return NULL;

  for(s0 = s = new_str(s); (t = strchr(s, del)); s = t + 1) {
    *t = 0;
    str = add_list(&list, new_mem(sizeof *str));
    str->str = new_str(s);
  }
  str = add_list(&list, new_mem(sizeof *str));
  str->str = new_str(s);

  free_mem(s0);

  return list.first;
}


char *join(char del, str_t *str)
{
  char *s, t[2];
  str_t *str0;
  int len = 0;

  for(str0 = str; str0; str0 = str0->next) {
    len += strlen(str0->str) + 1;
  }

  if(!len) return NULL;

  s = new_mem(len);

  t[0] = del; t[1] = 0;

  for(; str; str = str->next) {
    strcat(s, str->str);
    if(str->next) strcat(s, t);
  }

  return s;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * str0 & str1 _must_ hold valid driver info
 */
int cmp_driver_info(char *str0, char *str1)
{
  char type0, type1;
  str_t *sl0, *sl1;
  int _3d0, _3d1, res;

  type0 = *str0;
  type1 = *str1;

  if(type0 == 'a' || type1 == 'a') {
    if(type0 == 'a' && type1 == 'a') return 0;
    if(type0 == 'a') return 1;
    return -1;
  }

  if(type0 != 'x' || type1 != 'x') return 0;

  str0 += 2;
  str1 += 2;

  sl0 = split('|', str0);
  sl1 = split('|', str1);

  res = 0;

  if(sl0 && sl1) {

    /* xfree v4 first, then xfree v3 */
    if(*sl0->str != *sl1->str) res = *sl0->str < *sl1->str ? 1 : -1;

    if(!res) {
      _3d0 = _3d1 = 0;

      if(sl0->next && sl0->next->next && *sl0->next->next->str) _3d0 = 1;
      if(sl1->next && sl1->next->next && *sl1->next->next->str) _3d1 = 1;

      /* entries without 3d support first */
      res = _3d0 - _3d1;
    }
  }

  free_str(sl0, 1);
  free_str(sl1, 1);

  return res;
}


/* wrapper for qsort */
int cmp_str_s(const void *p0, const void *p1)
{
  str_t **str0, **str1;

  str0 = (str_t **) p0;
  str1 = (str_t **) p1;

  return strcmp((*str0)->str, (*str1)->str);
}


int cmp_hid(hid_t *hid0, hid_t *hid1)
{
  int i = 0;
  str_t *str0, *str1;

  if(!hid0 && !hid1) return 0;
  if(!hid0) return -1;
  if(!hid1) return 1;

  if(hid0->any.flag != hid1->any.flag) {
    return hid0->any.flag < hid1->any.flag ? -1 : 1;
  }

  if(hid0->any.flag == FLAG_STRING) {
    str0 = hid0->str.list.first;
    str1 = hid1->str.list.first;
    for(; str0 && str1; str0 = str0->next, str1 = str1->next) {
      i = strcmp(str0->str, str1->str);
      if(i) {
        i = i > 0 ? 1 : -1;
        break;
      }
    }
    if(!i) {
      if(str0) i = 1; else if(str1) i = -1;
    }
  }
  else if(hid0->any.flag == FLAG_ID) {
    if(hid0->num.tag != hid1->num.tag) {
      i = hid0->num.tag < hid1->num.tag ? -1 : 1;
    }
    else if(hid0->num.id != hid1->num.id) {
      i = hid0->num.id < hid1->num.id ? -1 : 1;
    }
    else if(hid0->num.has.range || hid1->num.has.range) {
      if(!hid0->num.has.range) {
        i = -1;
      }
      else if(!hid1->num.has.range) {
        i = 1;
      }
      else if(hid0->num.range != hid1->num.range) {
        i = hid0->num.range < hid1->num.range ? -1 : 1;
      }
    }
    else if(hid0->num.has.mask || hid1->num.has.mask) {
      if(!hid0->num.has.mask) {
        i = -1;
      }
      else if(!hid1->num.has.mask) {
        i = 1;
      }
      else if(hid0->num.mask != hid1->num.mask) {
        i = hid0->num.mask < hid1->num.mask ? -1 : 1;
      }
    }
  }

  return i;
}


int cmp_skey(skey_t *skey0, skey_t *skey1)
{
  int i, j, len0, len1, len;

  if(!skey0 && !skey1) return 0;
  if(!skey0) return -1;
  if(!skey1) return 1;

  for(i = len0 = len1 = 0; (unsigned) i < sizeof skey0->hid / sizeof *skey0->hid; i++) {
    if(skey0->hid[i]) len0 = i;
    if(skey1->hid[i]) len1 = i;
  }
  len0++;
  len1++;

  // printf("len0 = %d, len1 = %d\n", len0, len1);

  len = len0 < len1 ? len0 : len1;

  for(i = j = 0; j < len; j++) {
    // printf("0: j = %d\n", j);

    if(!skey0->hid[j] && !skey1->hid[j]) continue;

    /* note: this looks reversed, but is intentional! */
    if(!skey0->hid[j]) { i = 1; break; }
    if(!skey1->hid[j]) { i = -1; break; }

    i = cmp_hid(skey0->hid[j], skey1->hid[j]);
    // printf("1: j = %d, i = %d\n", j, i);
    
    if(i) break;
  }

  if(!i && len0 != len1) {
    i = len0 > len1 ? 1 : -1;
  }

  return i;
}


/* wrapper for qsort */
int cmp_skey_s(const void *p0, const void *p1)
{
  skey_t **skey0, **skey1;

  skey0 = (skey_t **) p0;
  skey1 = (skey_t **) p1;

  return cmp_skey(*skey0, *skey1);
}


int cmp_item(item_t *item0, item_t *item1)
{
  int i;
  skey_t *skey0, *skey1;

  skey0 = item0->key.first;
  skey1 = item1->key.first;
  for(i = 0; skey0 && skey1; skey0 = skey0->next, skey1 = skey1->next) {
    if((i = cmp_skey(skey0, skey1))) break;
  }
  if(!i) i = cmp_skey(skey0, skey1);

  if(!i) i = 2 * cmp_skey(item0->value, item1->value);

  // printf("%s -- %s : %d\n", item0->pos, item1->pos, i);

  return i;
}


/* wrapper for qsort */
int cmp_item_s(const void *p0, const void *p1)
{
  int i;
  item_t **item0, **item1;

  item0 = (item_t **) p0;
  item1 = (item_t **) p1;

  if(opt.random) {
    i = ((rand() / 317) % 3) - 1;
  }
  else if(opt.reverse) {
    i = cmp_item(*item1, *item0);
  }
  else {
    i = cmp_item(*item0, *item1);
  }

  return i;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 * Does hid1 match if hid0 does?
 * > 0: yes, 0: no, < 0: maybe
 *
 * match:
 *   match_any: at least one common id in hid1 & hid0
 *   match_all: hid1 matches whenever hid0 does (hid0 is special case of hid1))
 */
int match_hid(hid_t *hid0, hid_t *hid1, match_t match)
{
  int i, m = -1;
  str_t *str0, *str1;

  if(!hid1) return 1;
  if(!hid0) return 0;

  if(hid0->any.flag != hid1->any.flag) return 0;

  if(hid0->any.flag == FLAG_STRING) {
    str0 = hid0->str.list.first;
    str1 = hid1->str.list.first;
    for(; str0 && str1; str0 = str0->next, str1 = str1->next) {
      i = strcmp(str0->str, str1->str);
      if(i) return 0;
    }
    m = str0 || str1 ? 0 : 1;
  }
  else if(hid0->any.flag == FLAG_ID) {
    if(hid0->num.tag != hid1->num.tag) return 0;

    if(match == match_any) {

      if(hid0->num.has.range) {
        if(hid1->num.has.range) {
          m =
            (
              hid1->num.id >= hid0->num.id &&
               hid1->num.id < hid0->num.id + hid0->num.range
            ) ||
            (
              hid0->num.id >= hid1->num.id &&
              hid0->num.id < hid1->num.id + hid1->num.range
            ) ? 1 : 0;

        }
        else if(hid1->num.has.mask) {

        }
        else {
          m =
            (
              hid1->num.id >= hid0->num.id &&
              hid1->num.id < hid0->num.id + hid0->num.range
            ) ? 1 : 0;
        }
      }
      else if(hid0->num.has.mask) {
        if(hid1->num.has.range) {

        }
        else if(hid1->num.has.mask) {

        }
        else {
          m = (hid1->num.id & ~hid0->num.mask) == hid0->num.id ? 1 : 0;
        }
      }
      else {
        if(hid1->num.has.range) {
          m =
            (
              hid0->num.id >= hid1->num.id &&
              hid0->num.id < hid1->num.id + hid1->num.range
            ) ? 1 : 0;
        }
        else if(hid1->num.has.mask) {
          m = (hid0->num.id & ~hid1->num.mask) == hid1->num.id ? 1 : 0;
        }
        else {
          m = hid0->num.id == hid1->num.id ? 1 : 0;
        }
      }

    }
    else {	/* match_all */

      if(hid0->num.has.range) {
        if(hid1->num.has.range) {
          m =
            (
              hid0->num.id >= hid1->num.id &&
               hid0->num.id + hid0->num.range <= hid1->num.id + hid1->num.range
            ) ? 1 : 0;
//          fprintf(logfh, "id0 = 0x%x, id1 = 0x%x, m = %d\n", hid0->num.id, hid1->num.id, m);
        }
        else if(hid1->num.has.mask) {

        }
        else {
          m = hid1->num.id == hid0->num.id && hid0->num.range == 1 ? 1 : 0;
        }
      }
      else if(hid0->num.has.mask) {
        if(hid1->num.has.range) {

        }
        else if(hid1->num.has.mask) {

        }
        else {
          m = (hid1->num.id & ~hid0->num.mask) == hid0->num.id && hid0->num.mask == 0 ? 1 : 0;
        }
      }
      else {
        if(hid1->num.has.range) {
          m =
            (
              hid0->num.id >= hid1->num.id &&
              hid0->num.id < hid1->num.id + hid1->num.range
            ) ? 1 : 0;
        }
        else if(hid1->num.has.mask) {
          m = (hid0->num.id & ~hid1->num.mask) == hid1->num.id ? 1 : 0;
        }
        else {
          m = hid0->num.id == hid1->num.id ? 1 : 0;
        }
      }

    }
  }

  return m;
}


/*
 * Does skey1 match if skey0 does?
 * > 0: yes, 0: no, < 0: maybe
 */
int match_skey(skey_t *skey0, skey_t *skey1, match_t match)
{
  int i, k, m = 1;

  for(i = k = 0; (unsigned) i < sizeof skey0->hid / sizeof *skey0->hid; i++) {
    k = match_hid(skey0->hid[i], skey1->hid[i], match);
    if(k > 0) continue;
    if(!k) return 0;
    m = k;
  }

  return m;
}


/*
 * Does item1 match if item0 does?
 * > 0: yes, 0: no, < 0: maybe
 */
int match_item(item_t *item0, item_t *item1, match_t match)
{
  int i, k = 0;
  skey_t *skey0, *skey1;

  skey0 = item0->key.first;
  skey1 = item1->key.first;

  for(skey0 = item0->key.first; skey0; skey0 = skey0->next) {
    for(skey1 = item1->key.first; skey1; skey1 = skey1->next) {
      i = match_skey(skey0, skey1, match);
      if(i > 0) return i;
      if(i) k = i;
    }
  }

  return k;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
int combine_keys(skey_t *skey0, skey_t *skey1)
{
  int i, ind;
  unsigned r0, r1;
  hid_t *hid0, *hid1;

  for(ind = -1, i = 0; (unsigned) i < sizeof skey0->hid / sizeof *skey0->hid; i++) {
    if(!skey0->hid[i] && !skey1->hid[i]) continue;
    if(!skey0->hid[i] || !skey1->hid[i]) return 0;
    if(!cmp_hid(skey0->hid[i], skey1->hid[i])) continue;
    if(ind >= 0) return 0;
    ind = i;
  }

  if(ind < 0) return 0;

  /* ok, exactly one hid differs */
  hid0 = skey0->hid[ind];
  hid1 = skey1->hid[ind];

  /* must be numerical */
  if(hid0->any.flag != FLAG_ID || hid1->any.flag != FLAG_ID) return 0;

  /* no mask value */
  if(hid0->num.has.mask || hid1->num.has.mask) return 0;

  if(opt.no_range) return 0;

  /* must be adjacent ranges, can overlap  */
  r0 = hid0->num.has.range ? hid0->num.range : 1;
  r1 = hid1->num.has.range ? hid1->num.range : 1;

  if(hid1->num.id >= hid0->num.id && hid1->num.id <= hid0->num.id + r0) {
    i = hid1->num.id + r1 - hid0->num.id;
    if((unsigned) i < r0) i = r0;
    if(i != 1) {
      hid0->num.range = i;
      hid0->num.has.range = 1;
    }
    else {
      hid0->num.range = 0;
      hid0->num.has.range = 0;
    }
  }
  else {
    return 0;
  }

  return 1;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
str_t *clone_str(str_t *str)
{
  str_t *n_str;

  if(!str) return NULL;

  n_str = new_mem(sizeof *n_str);
  n_str->str = new_str(str->str);

  return n_str;
}


hid_t *clone_hid(hid_t *hid)
{
  hid_t *new_hid;
  str_t *str;

  if(!hid) return NULL;

  new_hid = new_mem(sizeof *new_hid);

  *new_hid = *hid;

  if(hid->any.flag == FLAG_STRING) {
    memset(&new_hid->str.list, 0, sizeof new_hid->str.list);
    for(str = hid->str.list.first; str; str = str->next) {
      add_list(&new_hid->str.list, clone_str(str));
    }
  }

  return new_hid;
}


skey_t *clone_skey(skey_t *skey)
{
  int i;
  skey_t *new_skey;

  if(!skey) return NULL;

  new_skey = new_mem(sizeof *new_skey);

  for(i = 0; (unsigned) i < sizeof skey->hid / sizeof *skey->hid; i++) {
    new_skey->hid[i] = clone_hid(skey->hid[i]);
  }

  return new_skey;
}


item_t *clone_item(item_t *item)
{
  item_t *new_item;
  skey_t *skey;

  if(!item) return NULL;

  new_item = new_mem(sizeof *new_item);

  new_item->pos = new_str(item->pos);

  for(skey = item->key.first; skey; skey = skey->next) {
    add_list(&new_item->key, clone_skey(skey));
  }

  new_item->value = clone_skey(item->value);

  return new_item;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
str_t *free_str(str_t *str, int follow_next)
{
  str_t *next;

  for(; str; str = next) {
    next = str->next;

    free_mem(str->str);
    free_mem(str);

    if(!follow_next) break;
  }

  return NULL;
}


hid_t *free_hid(hid_t *hid)
{
  if(!hid) return NULL;

  if(hid->any.flag == FLAG_STRING) {
    free_str(hid->str.list.first, 1);
  }
  free_mem(hid);

  return NULL;
}


skey_t *free_skey(skey_t *skey, int follow_next)
{
  skey_t *next;
  int i;

  for(; skey; skey = next) {
    next = skey->next;

    for(i = 0; (unsigned) i < sizeof skey->hid / sizeof *skey->hid; i++) {
      free_hid(skey->hid[i]);
    }

    free_mem(skey);

    if(!follow_next) break;
  }

  return NULL;
}


item_t *free_item(item_t *item, int follow_next)
{
  item_t *next;

  for(; item; item = next) {
    next = item->next;

    free_mem(item->pos);

    free_skey(item->key.first, 1);
    free_skey(item->value, 0);

    free_mem(item);

    if(!follow_next) break;
  }

  return NULL;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
unsigned driver_entry_types(hid_t *hid)
{
  str_t *str;
  unsigned ent = 0;

  for(str = hid->str.list.first; str; str = str->next) {
    if(!str->str[0] || str->str[1] != '\t') break;
    switch(str->str[0]) {
      case 'a':
        ent |= 1;
        break;

      case 'x':
      case 'X':
        ent |= 2;
        break;

      default:
        ent |= 4;
    }
  }

  return ent;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void remove_items(list_t *hd)
{
  item_t *item, *next;
  list_t hd_new = {};

  for(item = hd->first; item; item = next) {
    next = item->next;
    if(item->remove) {
      free_item(item, 0);
    }
    else {
      add_list(&hd_new, item);
    }
  }

  if(hd_new.last) {
    ((list_any_t *) hd_new.last)->next = NULL;
  }

  *hd = hd_new;
}


void remove_nops(list_t *hd)
{
  item_t *item;
  int cnt = 0;

  for(item = hd->first; item; item = item->next) {
    if(!item->value || !item->key.first) {
      item->remove = 1;
      cnt++;
    }
  }

  if(cnt) remove_items(hd);
}


void split_items(list_t *hd)
{
  item_t *item, *new_item, *next_item;
  skey_t *skey, *next;
  list_t hd_new = {};
  int cnt, l;
  char buf[16];

  for(item = hd->first; item; item = next_item) {
    next_item = item->next;
    skey = item->key.first;
    if(skey && skey->next) {
      for(cnt = 0, skey = item->key.first; skey; skey = next) {
        next = skey->next;
        new_item = add_list(&hd_new, new_mem(sizeof *new_item));
        if(item->pos && (l = strlen(item->pos))) {
          sprintf(buf, ",%d)", cnt++);
          new_item->pos = new_mem(l - 1 + strlen(buf) + 1);
          strcpy(new_item->pos, item->pos);
          strcpy(new_item->pos + l - 1, buf);
        }
        new_item->value = clone_skey(item->value);
        add_list(&new_item->key, clone_skey(skey));
      }
      free_item(item, 0);
    }
    else {
      add_list(&hd_new, item);
    }
  }

  if(hd_new.last) {
    ((list_any_t *) hd_new.last)->next = NULL;
  }

  *hd = hd_new;
}


void check_items(list_t *hd)
{
  int i, j, k, m, mr, m_all, mr_all, c_ident, c_diff, c_crit;
  char *s;
  item_t *item0, *item1, *item_a, *item_b;
  unsigned *stat_cnt;

  for(item0 = hd->first; item0; item0 = item0->next) {
    if(item0->remove) continue;
    for(item1 = item0->next; item1 && !item0->remove; item1 = item1->next) {
      if(item1->remove) continue;

      item_a = item0; item_b = item1;

      m = match_item(item0, item1, match_any);
      mr = match_item(item1, item0, match_any);

      m_all = mr_all = 0;

      if(m && mr) {
        m_all = match_item(item0, item1, match_all);
        mr_all = match_item(item1, item0, match_all);
        if(mr_all) {
          item_a = item1; item_b = item0;
          i = m_all; m_all = mr_all; mr_all = i;
          i = m; m = mr; mr = i;
        }
      }
      else if(mr && !m) {
        item_a = item1; item_b = item0;
        m = mr; mr = 0;
      }

      if(m && !mr) {
        m_all = match_item(item_a, item_b, match_all);
        mr_all = match_item(item_b, item_a, match_all);
      }

      if(m) {
#if 0
        fprintf(
          logfh, "a = %s, b = %s, m = %d, mr = %d, m_all = %d, mr_all = %d\n",
          item_a->pos, item_b->pos,
          m, mr, m_all, mr_all
        );
#endif

        if(m_all) {
          /*
           * item_b matches (at least) everything that item_a does
           * (item_a is a special case of item_b)
           */

          i = cmp_item(item_a, item_b);		/* just informational */
          if(!i) {
            /* identical keys and values */
            fprintf(logfh,
              "%s: duplicate of %s, item removed\n",
              item_a->pos, item_b->pos
            );
            item_a->remove = 1;
          }
          else {
            /* matching keys, differing values */

            j = count_common_hids(item_a->key.first, item_b->key.first);
            k = (
              j == count_common_hids(item_b->key.first, item_b->key.first) &&
              j < count_common_hids(item_a->key.first, item_a->key.first)
            ) ? 1 : 0;

            if(k) {
              /*
               * item_a is a special case of item_b _and_ item_a has more hid fields
               * --> libhd can handle differing info in this case
               */
              j = strip_skey(item_a->value, item_b->value, 1);
              if(j) {
                c_ident = j & 0xff;
                c_diff = (j >> 8) & 0xff;
                if(c_diff && c_ident) {
                  fprintf(logfh,
                    "%s: some info identical to %s, identical info removed\n",
                    item_a->pos, item_b->pos
                  );
                  log_items(logfh, item_a, item_b);
                }
                else if(!c_diff) {
                  fprintf(logfh,
                    "%s: info is identical to %s, info removed\n",
                    item_a->pos, item_b->pos
                  );
                  log_items(logfh, item_a, item_b);
                }
                remove_deleted_hids(item_a->value);
              }
            }
            else {
              j = strip_skey(item_a->value, item_b->value, 3);
              if(j) {
                c_ident = j & 0xff;
                c_diff = (j >> 8) & 0xff;
                c_crit = (j >> 16) & 0xff;
                if(c_crit || cmp_skey(item_a->key.first, item_b->key.first)) {
                  s = "conflicts with";
                  stat_cnt = &stats.errors_res;
                }
                else {
                  s = "differs from";
                  stat_cnt = &stats.diffs;
                }
                /*
                 * if the keys are identical, make it a warning,
                 * else make it an error
                 */
                if(c_diff && !c_ident) {
                  (*stat_cnt)++;
                  fprintf(logfh,
                    "%s: info %s %s, info removed\n",
                    item_a->pos, s, item_b->pos
                  );
                }
                else if(c_diff && c_ident) {
                  (*stat_cnt)++;
                  fprintf(logfh,
                    "%s: info %s/is identical to %s, info removed\n",
                    item_a->pos, s, item_b->pos
                  );
                }
                else {
                  fprintf(logfh,
                    "%s: info is identical to %s, info removed\n",
                    item_a->pos, item_b->pos
                  );
                }
                log_items(logfh, item_a, item_b);
                remove_deleted_hids(item_a->value);
              }
            }

            if(!count_common_hids(item_a->value, item_a->value)) {
              /* remove if no values left */
              item_a->remove = 1;
              fprintf(logfh, "%s: no info left, item removed\n", item_a->pos);
            }

          }
        }
        else if(count_common_hids(item_a->value, item_b->value)) {
          /* different keys, potentially conflicting values */
          k = cmp_skey(item_a->value, item_b->value);
          if(k) {
            /* differing keys, differing values */
            j = strip_skey(item_b->value, item_a->value, 2);
            c_diff = (j >> 8) & 0xff;
            if(c_diff) {
              /* different keys, conflicting values --> error */
              stats.errors++;
              fprintf(logfh,
                "%s: info conflicts with %s\n",
                item_b->pos, item_a->pos
              );
              log_items(logfh, item_b, item_a);
            }
            undelete_hids(item_b->value);
          }
        }
      }
    }
  }

  remove_items(hd);
}


void combine_driver(list_t *hd)
{
  int i;
  item_t *item0, *item1, *item_a, *item_b;
  hid_t *hid0, *hid1, *new_hid, *hid_a, *hid_b;
  str_t *str0, *str1, *tmp_str, *last_str;
  unsigned type0, type1;

  for(item0 = hd->first; item0; item0 = item0->next) {
    if(
      item0->remove ||
      !item0->value ||
      !(hid0 = item0->value->hid[he_driver]) ||
      hid0->any.flag != FLAG_STRING
    ) continue;
    for(item1 = item0->next; item1 && !item0->remove; item1 = item1->next) {
      hid0 = item0->value->hid[he_driver];
      if(
        item1->remove ||
        !item1->value ||
        !(hid1 = item1->value->hid[he_driver]) ||
        hid1->any.flag != FLAG_STRING
      ) continue;

      i = cmp_item(item0, item1);

      /* remove duplicate entries */
      if(!i) {
        item1->remove = 1;
        continue;
      }

      /* work only on entries with identical keys */
      if(i == -1 || i == 1) continue;

      /* ensure these are proper driver entries */
      if(!(type0 = driver_entry_types(hid0))) continue;
      if(!(type1 = driver_entry_types(hid1))) continue;

      /*
       * Allow only (x11 + x11) & (!any + any)
       * unless --combine option was used.
       */
      if(!opt.combine && (((type0 & type1) & 5) || ((type0 | type1) & 6) == 6)) {
        fprintf(logfh,
          "%s: can't combine driver info with %s %d %d\n",
          item0->pos, item1->pos, type0, type1
        );
        log_items(logfh, item0, item1);
        continue;
      }

      item_a = item0;
      item_b = item1;
      hid_a = hid0;
      hid_b = hid1;

      if(type0 == 1) {
        item_a = item1;
        item_b = item0;
        hid_a = hid1;
        hid_b = hid0;
      }

      fprintf(logfh, "%s: combine with %s\n", item_a->pos, item_b->pos);
      log_items(logfh, item_a, item_b);

      new_hid = clone_hid(hid_a);

      for(str1 = hid_b->str.list.first; str1; str1 = str1->next) {
        last_str = NULL;
        for(str0 = new_hid->str.list.first; str0; last_str = str0, str0 = str0->next) {
          i = cmp_driver_info(str1->str, str0->str);
          if(i < 0) break;
        }
        if(last_str) {
          tmp_str = last_str->next;
          last_str->next = clone_str(str1);
          last_str->next->next = tmp_str;
          if(!tmp_str) {
            new_hid->str.list.last = last_str->next;
          }
        }
        else {
          /* smaller than first entry */
          tmp_str = clone_str(str1);
          tmp_str->next = new_hid->str.list.first;
          new_hid->str.list.first = tmp_str;
        }
      }

      free_hid(item_a->value->hid[he_driver]);
      item_a->value->hid[he_driver] = new_hid;
      item_b->value->hid[he_driver] = free_hid(item_b->value->hid[he_driver]);

      fprintf(logfh, "  --\n");
      log_items(logfh, item_a, item_b);

      if(!count_common_hids(item_b->value, item_b->value)) {
        /* remove if no values left */
        item_b->remove = 1;
        fprintf(logfh, "%s: no info left, item removed\n", item_b->pos);
      }

    }
  }

  remove_items(hd);
}


void combine_requires(list_t *hd)
{
  int i;
  item_t *item0, *item1;
  hid_t *hid0, *hid1;
  list_t slist = {};
  str_t *str, *str0, *str1;

  for(item0 = hd->first; item0; item0 = item0->next) {
    if(
      item0->remove ||
      !item0->value ||
      !(hid0 = item0->value->hid[he_requires]) ||
      hid0->any.flag != FLAG_STRING
    ) continue;
    for(item1 = item0->next; item1; item1 = item1->next) {
      if(
        item1->remove ||
        !item1->value ||
        !(hid1 = item1->value->hid[he_requires]) ||
        hid1->any.flag != FLAG_STRING
      ) continue;

      i = cmp_item(item0, item1);

      /* remove duplicate entries */
      if(!i) {
        item1->remove = 1;
        continue;
      }

      /* work only on entries with identical keys */
      if(i == -1 || i == 1) continue;

      if(!cmp_hid(hid0, hid1)) {
        hid1->any.remove = 1;
        fprintf(logfh,
          "%s: info is identical to %s, info removed\n",
          item1->pos, item0->pos
        );
        log_items(logfh, item1, item0);
        item1->value->hid[he_requires] = free_hid(item1->value->hid[he_requires]);
      }
      else {
        slist.first = split('|', ((str_t *) hid0->str.list.first)->str);

        /* add pointer to last element */
        for(str = slist.first; str; str = str->next) {
          if(!str->next) slist.last = str;
        }

        str1 = split('|', ((str_t *) hid1->str.list.first)->str);
        for(str = str1; str; str = str->next) {
          for(str0 = slist.first; str0; str0 = str0->next) {
            if(!strcmp(str->str, str0->str)) break;
          }
          if(!str0) add_list(&slist, clone_str(str));
        }
        free_str(str1, 1);

        sort_list(&slist, cmp_str_s);

        free_str(hid0->str.list.first, 1);
        hid0->str.list.last = NULL;
        hid0->str.list.first = add_list(&hid0->str.list, new_mem(sizeof (str_t)));
        ((str_t *) hid0->str.list.first)->str = join('|', slist.first);

        free_str(slist.first, 1);

        hid1->any.remove = 1;
        
        fprintf(logfh,
          "%s: combine with %s, info removed\n",
          item1->pos, item0->pos
        );
        log_items(logfh, item1, item0);
        item1->value->hid[he_requires] = free_hid(item1->value->hid[he_requires]);
      }

      if(!count_common_hids(item1->value, item1->value)) {
        /* remove if no values left */
        item1->remove = 1;
        fprintf(logfh, "%s: no info left, item removed\n", item1->pos);
      }
    }
  }

  remove_items(hd);
}


void join_items_by_value(list_t *hd)
{
  item_t *item0, *item1;
  skey_t *skey, *next;
  int i;

  for(item0 = hd->first; item0; item0 = item0->next) {
    if(item0->remove) continue;
    for(item1 = item0->next; item1; item1 = item1->next) {
      if(item1->remove) continue;

      if(!cmp_skey(item0->value, item1->value)) {
        for(skey = item1->key.first; skey; skey = next) {
          next = skey->next;
          add_list(&item0->key, skey);
        }
        memset(&item1->key, 0, sizeof item1->key);
        item1->remove = 1;
        fprintf(logfh, "%s: info added to %s, item removed\n", item1->pos, item0->pos);
      }
    }
  }

  remove_items(hd);

  for(item0 = hd->first; item0; item0 = item0->next) {

    /* sort key entries */
    sort_list(&item0->key, cmp_skey_s);

    /* try to join adjacent keys */
    for(skey = item0->key.first; skey && (next = skey->next); ) {
      i = combine_keys(skey, next);
      if(!i) {
        skey = next;
        continue;
      }
      if(!(skey->next = next->next)) {
        /* last element has changed */
        item0->key.last = skey;
      }
      free_skey(next, 0);
    }
  }
}


void join_items_by_key(list_t *hd)
{
  item_t *item0, *item1;
  skey_t *val0, *val1;
  int i;

  for(item0 = hd->first; item0; item0 = item0->next) {
    if(item0->remove) continue;
    val0 = item0->value;
    for(item1 = item0->next; item1; item1 = item1->next) {
      if(item1->remove) continue;

      i = cmp_item(item0, item1);

      if(i == 2 || i == -2) {
        /* identical keys, values differ */
        val1 = item1->value;
        if(!count_common_hids(val0, val1)) {
          /* move everything from item1 to item0 */
          
          for(i = 0; (unsigned) i < sizeof val1->hid / sizeof *val1->hid; i++) {
            if(val1->hid[i]) {
              val0->hid[i] = val1->hid[i];
              val1->hid[i] = NULL;
            }
          }
          item1->remove = 1;
          fprintf(logfh, "%s: info added to %s, item removed\n", item1->pos, item0->pos);
        }
      }
    }
  }

  remove_items(hd);
}


void remove_unimportant_items(list_t *hd)
{
  item_t *item;
  skey_t *val;
  str_t *str;
  int i, cnt;

  for(item = hd->first; item; item = item->next) {
    val = item->value;
    cnt = 0;
    if(val) {
      for(i = 0; (unsigned) i < sizeof val->hid / sizeof *val->hid; i++) {
        if(i == he_driver && val->hid[i]) {
          if(!(
            val->hid[i]->any.flag == FLAG_STRING &&
            (str = val->hid[i]->str.list.first) &&
            str->str &&
            (*str->str == 'i' || *str->str == 'm')
          )) {
            val->hid[i] = free_hid(val->hid[i]);
          }
        }
        else if(val->hid[i]) {
          if(val->hid[i]->any.flag != FLAG_ID) val->hid[i] = free_hid(val->hid[i]);
        }
        if(val->hid[i]) cnt++;
      }
    }
    /* no values left */
    if(!cnt) item->remove = 1;
  }

  remove_items(hd);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if 0
/* returns index in hddb2->ids */
unsigned store_entry(hddb2_data_t *x, tmp_entry_t *te)
{
  int i, j;
  unsigned ent = -1, u, v;

  for(i = 0; i < he_nomask; i++) {
    if(te[i].len) {
      for(j = 0; j < te[i].len; j++) {
        v = te[i].val[j] | (1 << 31);
        if(j == te[i].len - 1) v &= ~(1 << 31);
        u = store_value(x, v);
        if(ent == -1) ent = u;
      }
    }
  }

  return ent;
}

void add_value(tmp_entry_t *te, hddb_entry_t idx, unsigned val)
{
  if(idx >= he_nomask) return;
  te += idx;

  if(te->len >= sizeof te->val / sizeof *te->val) return;

  te->val[te->len++] = val;
}

#endif



unsigned hddb_store_string(hddb_data_t *hddb, char *str)
{
  unsigned l = strlen(str), u;
  char *s;

  if(!opt.no_compact) {
    /* maybe we already have it... */
    if(l && l < hddb->strings_len) {
      s = memmem(hddb->strings, hddb->strings_len, str, l + 1);
      if(s) return s - hddb->strings;
    }
  }

  if(hddb->strings_len + l >= hddb->strings_max) {
    hddb->strings_max += l + 0x1000;         /* >4k steps */
    hddb->strings = realloc(hddb->strings, hddb->strings_max * sizeof *hddb->strings);
  }

  /* make sure the 1st byte is 0 */
  if(hddb->strings_len == 0) {
    *hddb->strings = 0;	/* realloc does _not_ clear memory */
    hddb->strings_len = 1;
  }

  if(l == 0) return 0;		/* 1st byte is always 0 */

  strcpy(hddb->strings + (u = hddb->strings_len), str);
  hddb->strings_len += l + 1;

  return u;
}


unsigned hddb_store_value(hddb_data_t *hddb, unsigned val)
{
  if(hddb->ids_len == hddb->ids_max) {
    hddb->ids_max += 0x400;	/* 4k steps */
    hddb->ids = realloc(hddb->ids, hddb->ids_max * sizeof *hddb->ids);
  }

  hddb->ids[hddb->ids_len++] = val;

  return hddb->ids_len - 1;
}


unsigned hddb_store_hid(hddb_data_t *hddb, hid_t *hid, hddb_entry_t entry)
{
  unsigned u, idx = -1;
  str_t *str, *str0, *str1;

  if(!hid) return idx;

  if(hid->any.flag == FLAG_ID) {
    if(hid->num.has.range) {
      idx = hddb_store_value(hddb, MAKE_DATA(FLAG_RANGE, hid->num.range) | (1 << 31));
    }
    else if(hid->num.has.mask) {
      idx = hddb_store_value(hddb, MAKE_DATA(FLAG_MASK, hid->num.mask) | (1 << 31));
    }
    u = hddb_store_value(hddb, MAKE_DATA(FLAG_ID, hid->num.id + (hid->num.tag << 16)));
    if(idx == -1u) idx = u;
  }
  else if(hid->any.flag == FLAG_STRING) {
    if(entry == he_driver ) {
      for(str = hid->str.list.first; str; str = str->next) {
        str0 = split('\001', str->str);
        for(str1 = str0; str1; str1 = str1->next) {
          u = hddb_store_string(hddb, str1->str);
          if(str->next || str1->next) u |= 1 << 31;
          u = hddb_store_value(hddb, MAKE_DATA(FLAG_STRING, u));
          if(idx == -1u) idx = u;
        }
        free_str(str0, 1);
      }
    }
    else {
      u = hddb_store_string(hddb, ((str_t *) hid->str.list.first)->str);
      idx = hddb_store_value(hddb, MAKE_DATA(FLAG_STRING, u));
    }
  }

  return idx;
}


void hddb_store_skey(hddb_data_t *hddb, skey_t *skey, unsigned *mask, unsigned *idx)
{
  int i, j, end;
  unsigned ent;
  hddb_data_t save_db = *hddb;

  *mask = 0;
  *idx = 0;

  if(!skey) return;

  for(i = 0; (unsigned) i < sizeof skey->hid / sizeof *skey->hid; i++) {
    if(skey->hid[i]) {
      ent = hddb_store_hid(hddb, skey->hid[i], i);
      if(!*mask) *idx = ent;
      *mask |= (1 << i);
    }
  }

  if(!opt.no_compact) {
    /* maybe there was an identical skey before... */
    if(save_db.ids_len && hddb->ids_len > save_db.ids_len) {
      j = hddb->ids_len - save_db.ids_len;
      end = save_db.ids_len - j;
      /* this is pretty slow, but avoids memmem() alignment problems */
      for(i = 0; i < end; i++) {
        if(!memcmp(hddb->ids + i, hddb->ids + save_db.ids_len, j * sizeof *hddb->ids)) {
          /* remove new id entries and return existing entry */
          hddb->ids_len = save_db.ids_len;
          *idx = i;
          break;
        }
      }
    }
  }
}


void hddb_store_list(hddb_data_t *hddb, hddb_list_t *list)
{
  if(!list || !list->key_mask || !list->value_mask) return;

  if(hddb->list_len == hddb->list_max) {
    hddb->list_max += 0x100;	/* 4k steps */
    hddb->list = realloc(hddb->list, hddb->list_max * sizeof *hddb->list);
  }

  hddb->list[hddb->list_len++] = *list;
}


void hddb_init(hddb_data_t *hddb, list_t *hd)
{
  item_t *item;
  hddb_list_t db_list = {};
  unsigned item_cnt;

  for(item_cnt = 0, item = hd->first; item; item = item->next, item_cnt++) {

    hddb_store_skey(hddb, item->key.first, &db_list.key_mask, &db_list.key);
    hddb_store_skey(hddb, item->value, &db_list.value_mask, &db_list.value);

    hddb_store_list(hddb, &db_list);
  }
}


unsigned char *quote_string(unsigned char *str, int len)
{
  unsigned char *qstr;
  int i, j;

  if(!str || !len) return NULL;

  qstr = new_mem(4 * len + 1);	/* enough */

  for(i = j = 0; i < len; i++) {
    if(str[i] == '\\' || str[i] == '"' || str[i] == '?') {
      qstr[j++] = '\\';
      qstr[j++] = str[i];
    }
    else if(str[i] == '\n') {
      qstr[j++] = '\\';
      qstr[j++] = 'n';
    }
    else if(str[i] == '\t') {
      qstr[j++] = '\\';
      qstr[j++] = 't';
    }
    else if(str[i] < ' ') {
      qstr[j++] = '\\';
      qstr[j++] = (str[i] >> 6) + '0';
      qstr[j++] = ((str[i] >> 3) & 7) + '0';
      qstr[j++] = (str[i] & 7) + '0';
    }
    else {
      qstr[j++] = str[i];
    }
  }

  return qstr;
}


void write_cfile(FILE *f, list_t *hd)
{
  hddb_data_t hddb = {};
  char *qstr;
  unsigned u, qstr_len, len;

  fprintf(logfh, "- building C version\n");
  fflush(logfh);

  hddb_init(&hddb, hd);

  fprintf(logfh, "  db size: %u bytes\n",
    (unsigned) (sizeof hddb +
    hddb.strings_len +
    hddb.ids_len * sizeof *hddb.ids +
    hddb.list_len * sizeof *hddb.list)
  );

  fprintf(f,
    "static hddb_list_t hddb_internal_list[];\n"
    "static unsigned hddb_internal_ids[];\n"
    "static char hddb_internal_strings[];\n\n"
    "hddb2_data_t hddb_internal = {\n"
    "  %u, %u, hddb_internal_list,\n"
    "  %u, %u, hddb_internal_ids,\n"
    "  %u, %u, hddb_internal_strings\n"
    "};\n\n",
    hddb.list_len, hddb.list_len,
    hddb.ids_len, hddb.ids_len,
    hddb.strings_len, hddb.strings_len
  );

  fprintf(f, "static hddb_list_t hddb_internal_list[%u] = {\n", hddb.list_len);
  for(u = 0; u < hddb.list_len; u++) {
    fprintf(f,
    "  { 0x%08x, 0x%08x, 0x%08x, 0x%08x }%s\n",
    hddb.list[u].key_mask, hddb.list[u].value_mask,
    hddb.list[u].key, hddb.list[u].value,
    u + 1 == hddb.list_len ? "" : ","
    );
  }
  fprintf(f, "};\n\n");

  fprintf(f, "static unsigned hddb_internal_ids[%u] = {\n", hddb.ids_len);
  for(u = 0; u < hddb.ids_len; u++) {
    if((u % 6) == 0) fputc(' ', f);
    fprintf(f, " 0x%08x", hddb.ids[u]);
    if(u + 1 != hddb.ids_len) fputc(',', f);
    if(u % 6 == 6 - 1 || u + 1 == hddb.ids_len) fputc('\n', f);
  }
  fprintf(f, "};\n\n");

  qstr = quote_string(hddb.strings, hddb.strings_len);
  qstr_len = qstr ? strlen(qstr) : 0;
  fprintf(f, "static char hddb_internal_strings[%u] = \"\\\n", hddb.strings_len);
  for(u = 0; u < qstr_len; ) {
    len = qstr_len - u;
    if(len > 72) len = 72;
    while(len--) fputc(qstr[u++], f);
    fprintf(f, "\\\n");
  }
  fprintf(f, "\";\n\n");

  free_mem(qstr);

  free_mem(hddb.list);
  free_mem(hddb.ids);
  free_mem(hddb.strings);
}



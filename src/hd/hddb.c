#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/utsname.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"

extern hddb2_data_t hddb_internal;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
// #define HDDB_TRACE
// #define HDDB_TEST

#define DATA_VALUE(a)	((a) & ~(-1 << 28))
#define DATA_FLAG(a)	(((a) >> 28) & 0xf)
#define MAKE_DATA(a, b)	((a << 28) | (b))

#define FLAG_ID		0
#define FLAG_RANGE	1
#define FLAG_MASK	2
#define FLAG_STRING	3
#define FLAG_REGEXP	4
/* 5 - 7 reserved */
#define FLAG_CONT	8	/* bit mask, _must_ be bit 31 */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
typedef enum hddb_entry_e {
  he_other, he_bus_id, he_baseclass_id, he_subclass_id, he_progif_id,
  he_vendor_id, he_device_id, he_subvendor_id, he_subdevice_id, he_rev_id,
  he_bus_name, he_baseclass_name, he_subclass_name, he_progif_name,
  he_vendor_name, he_device_name, he_subvendor_name, he_subdevice_name,
  he_rev_name, he_serial, he_driver, he_requires /* 21 */,
  /* add new entries _here_! */
  he_nomask,
  he_class_id = he_nomask, he_driver_module_insmod, he_driver_module_modprobe,
  he_driver_module_config, he_driver_xfree, he_driver_xfree_config,
  he_driver_mouse, he_driver_display, he_driver_any
} hddb_entry_t;

static hddb_entry_t hddb_is_numeric[] = {
  he_bus_id, he_baseclass_id, he_subclass_id, he_progif_id, he_vendor_id,
  he_device_id, he_subvendor_id, he_subdevice_id, he_rev_id
};

static char *hddb_entry_strings[] = {
  "other", "bus.id", "baseclass.id", "subclass.id", "progif.id",
  "vendor.id", "device.id", "subvendor.id", "subdevice.id", "rev.id",
  "bus.name", "baseclass.name", "subclass.name", "progif.name",
  "vendor.name", "device.name", "subvendor.name", "subdevice.name",
  "rev.name", "serial", "driver", "requires",
  "class.id", "driver.module.insmod", "driver.module.modprobe",
  "driver.module.config", "driver.xfree", "driver.xfree.config",
  "driver.mouse", "driver.display", "driver.any"
};

static char *hid_tag_names[] = { "", "pci ", "eisa ", "usb ", "special " };

typedef enum {
  pref_empty, pref_new, pref_and, pref_or, pref_add
} prefix_t;

typedef struct line_s {
  prefix_t prefix;
  hddb_entry_t key;
  char *value;
} line_t;

typedef struct {
  int len;
  unsigned val[32];	/* arbitrary (approx. max. number of modules/xf86 config lines) */
} tmp_entry_t;

typedef struct {
  unsigned id;
  char *name;
} hd_id_t;

/* except for driver, all strings are static and _must not_ be freed */
typedef struct {
  hddb_entry_mask_t key;
  hddb_entry_mask_t value;
  hddb_entry_mask_t value_mask[he_nomask];
  hd_id_t bus;
  hd_id_t base_class;
  hd_id_t sub_class;
  hd_id_t prog_if;
  hd_id_t vendor;
  hd_id_t device;
  hd_id_t sub_vendor;
  hd_id_t sub_device;
  hd_id_t revision;
  char *serial;
  str_list_t *driver;
  char *requires;
} hddb_search_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void hddb_init_pci(hd_data_t *hd_data);
static void hddb_init_external(hd_data_t *hd_data);

static line_t *parse_line(char *str);
static unsigned store_string(hddb2_data_t *x, char *str);
static unsigned store_list(hddb2_data_t *x, hddb_list_t *list);
static unsigned store_value(hddb2_data_t *x, unsigned val);
static unsigned store_entry(hddb2_data_t *x, tmp_entry_t *te);
static void clear_entry(tmp_entry_t *te);
static void add_value(tmp_entry_t *te, hddb_entry_t idx, unsigned val);
static hddb_entry_mask_t add_entry(hddb2_data_t *hddb2, tmp_entry_t *te, hddb_entry_t idx, char *str);
static int compare_ids(hddb2_data_t *hddb, hddb_search_t *hs, hddb_entry_mask_t mask, unsigned key);
static void complete_ids(hddb2_data_t *hddb, hddb_search_t *hs, hddb_entry_mask_t key_mask, hddb_entry_mask_t mask, unsigned val_idx);
static int hddb_search(hd_data_t *hd_data, hddb_search_t *hs);
#ifdef HDDB_TEST
static void test_db(hd_data_t *hd_data);
#endif
static driver_info_t *hddb_to_device_driver(hd_data_t *hd_data, hddb_search_t *hs);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void hddb_init_pci(hd_data_t *hd_data)
{
  str_list_t *sl0 = NULL, *sl;
  char *s = NULL;
  struct utsname ubuf;
  int len;
  char buf[64];
  unsigned u0, u1, u2, u3, u4, u5;
  hddb_pci_t *p;

  if(hd_data->hddb_pci) return;

  /* during installation, the file is in /etc */
  sl0 = read_file("/etc/modules.pcimap", 1, 0);
  if(!sl0 && !uname(&ubuf)) {
    str_printf(&s, 0, "/lib/modules/%s/modules.pcimap", ubuf.release);
    sl0 = read_file(s, 1, 0);
    s = free_mem(s);
  }

  for(len = 1, sl = sl0; sl; sl = sl->next) len++;

  hd_data->hddb_pci = new_mem(len * sizeof *hd_data->hddb_pci);

  for(p = hd_data->hddb_pci, sl = sl0; sl; sl = sl->next) {
    if(sscanf(sl->str, "%63s %x %x %x %x %x %x", buf, &u0, &u1, &u2, &u3, &u4, &u5) == 7) {
      p->module = new_str(buf);
      p->vendor = u0;
      p->device = u1;
      p->subvendor = u2;
      p->subdevice = u3;
      p->pciclass = u4;
      p->classmask = u5;

      p++;
    }
  }

  free_str_list(sl0);

#if 0
  for(p = hd_data->hddb_pci; p->module; p++) {
    fprintf(stderr, "%s, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
      p->module, p->vendor, p->device, p->subvendor, p->subdevice,
      p->pciclass, p->classmask
    );
  }
#endif
}


driver_info_t *hd_pcidb(hd_data_t *hd_data, hd_t *hd)
{
  unsigned vendor, device, subvendor, subdevice, pciclass;
  driver_info_t *di = NULL, *di0 = NULL;
  hddb_pci_t *p;
  pci_t *pci;

  if(!(p = hd_data->hddb_pci)) return NULL;

  if(ID_TAG(hd->vend) != TAG_PCI) return NULL;

  vendor = ID_VALUE(hd->vend);
  device = ID_VALUE(hd->dev);
  subvendor = ID_VALUE(hd->sub_vend);
  subdevice = ID_VALUE(hd->sub_dev);
  pciclass = (hd->base_class << 16) + ((hd->sub_class & 0xff) << 8) + (hd->prog_if & 0xff);

  if(
    hd->detail &&
    hd->detail->type == hd_detail_pci &&
    (pci = hd->detail->pci.data)
  ) {
    pciclass = (pci->base_class << 16) + ((pci->sub_class & 0xff) << 8) + (pci->prog_if & 0xff);
  }

  for(; p->module; p++) {
    if(
      (p->vendor == 0xffffffff || p->vendor == vendor) &&
      (p->device == 0xffffffff || p->device == device) &&
      (p->subvendor == 0xffffffff || p->subvendor == subvendor) &&
      (p->subdevice == 0xffffffff || p->subdevice == subdevice) &&
      !((p->pciclass ^ pciclass) & p->classmask)
    ) {
      if(di) {
        di = di->next = new_mem(sizeof *di);
      }
      else {
        di = di0 = new_mem(sizeof *di);
      }
      di->any.type = di_module;
      di->module.modprobe = 1;
      add_str_list(&di->any.hddb0, p->module);
    }
  }

  return di0;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void hddb_init(hd_data_t *hd_data)
{
  hddb_init_pci(hd_data);
  hddb_init_external(hd_data);

  hd_data->hddb2[1] = &hddb_internal;

#ifdef HDDB_TEST
  test_db(hd_data);
#endif
}


void hddb_init_external(hd_data_t *hd_data)
{
  str_list_t *sl, *sl0;
  line_t *l;
  unsigned l_start, l_end /* end points _past_ last element */;
  unsigned u, ent, l_nr = 1;
  tmp_entry_t tmp_entry[he_nomask /* _must_ be he_nomask! */];
  hddb_entry_mask_t entry_mask = 0;
  int state;
  hddb_list_t dbl = {};
  hddb2_data_t *hddb2;

  if(hd_data->hddb2[0]) return;

  hddb2 = hd_data->hddb2[0] = new_mem(sizeof *hd_data->hddb2[0]);

  sl0 = read_file(ID_LIST, 0, 0);

  l_start = l_end = 0;
  state = 0;

  for(sl = sl0; sl; sl = sl->next, l_nr++) {
    l = parse_line(sl->str);
    if(!l) {
      fprintf(stderr, "hd.ids line %d: invalid line\n", l_nr);
      state = 4;
      break;
    };
    if(l->prefix == pref_empty) continue;
    switch(l->prefix) {
      case pref_new:
        if((state == 2 && !entry_mask) || state == 1) {
          fprintf(stderr, "hd.ids line %d: new item not allowed\n", l_nr);
          state = 4;
          break;
        }
        if(state == 2 && entry_mask) {
          ent = store_entry(hddb2, tmp_entry);
          if(ent == -1) {
            fprintf(stderr, "hd.ids line %d: internal hddb oops 1\n", l_nr);
            state = 4;
            break;
          }
          if(l_end && l_end > l_start) {
            for(u = l_start; u < l_end; u++) {
              hddb2->list[u].value_mask = entry_mask;
              hddb2->list[u].value = ent;
            }
          }
        }
        entry_mask = 0;
        clear_entry(tmp_entry);
        state = 1;
        l_start = store_list(hddb2, &dbl);
        l_end = l_start + 1;
        break;

      case pref_and:
        if(state != 1) {
          fprintf(stderr, "hd.ids line %d: must start item first\n", l_nr);
          state = 4;
          break;
        }
        break;

      case pref_or:
        if(state != 1 || !entry_mask || l_end <= l_start || l_end < 1) {
          fprintf(stderr, "hd.ids line %d: must start item first\n", l_nr);
          state = 4;
          break;
        }
        ent = store_entry(hddb2, tmp_entry);
        if(ent == -1) {
          fprintf(stderr, "hd.ids line %d: internal hddb oops 2\n", l_nr);
          state = 4;
          break;
        }
        hddb2->list[l_end - 1].key_mask = entry_mask;
        hddb2->list[l_end - 1].key = ent;
        entry_mask = 0;
        clear_entry(tmp_entry);
        u = store_list(hddb2, &dbl);
        if(u != l_end) {
          fprintf(stderr, "hd.ids line %d: internal hddb oops 2\n", l_nr);
          state = 4;
          break;
        }
        l_end++;
        break;

      case pref_add:
        if(state == 1 && !entry_mask) {
          fprintf(stderr, "hd.ids line %d: driver info not allowed\n", l_nr);
          state = 4;
          break;
        }
        if(state == 1 && l_end > l_start) {
          ent = store_entry(hddb2, tmp_entry);
          if(ent == -1) {
            fprintf(stderr, "hd.ids line %d: internal hddb oops 3\n", l_nr);
            state = 4;
            break;
          }
          hddb2->list[l_end - 1].key_mask = entry_mask;
          hddb2->list[l_end - 1].key = ent;
          entry_mask = 0;
          clear_entry(tmp_entry);
          state = 2;
        }
        if(state != 2 || l_end == 0) {
          fprintf(stderr, "hd.ids line %d: driver info not allowed\n", l_nr);
          state = 4;
          break;
        }
        break;

      default:
        state = 4;
    }

    if(state != 4) {
      u = add_entry(hddb2, tmp_entry, l->key, l->value);
      if(u) {
        entry_mask |= u;
      }
      else {
        fprintf(stderr, "hd.ids line %d: invalid info\n", l_nr);
        state = 4;
      }
    }

    if(state == 4) break;	/* error */
  }

  /* finalize last item */
  if(state == 2 && entry_mask) {
    ent = store_entry(hddb2, tmp_entry);
    if(ent == -1) {
      fprintf(stderr, "hd.ids line %d: internal hddb oops 4\n", l_nr);
      state = 4;
    }
    else if(l_end && l_end > l_start) {
      for(u = l_start; u < l_end; u++) {
        hddb2->list[u].value_mask = entry_mask;
        hddb2->list[u].value = ent;
      }
    }
  }

  sl0 = free_str_list(sl0);

  if(state == 4) {
    /* there was an error */

    free_mem(hddb2->list);
    free_mem(hddb2->ids);
    free_mem(hddb2->strings);
    hd_data->hddb2[0] = free_mem(hd_data->hddb2[0]);
  }
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

  for(i = 0; i < sizeof hddb_entry_strings / sizeof *hddb_entry_strings; i++) {
    if(!strcmp(s, hddb_entry_strings[i])) {
      l.key = i;
      break;
    }
  }

  if(i >= sizeof hddb_entry_strings / sizeof *hddb_entry_strings) return NULL;

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


unsigned store_string(hddb2_data_t *x, char *str)
{
  unsigned l = strlen(str), u;

  if(x->strings_len + l >= x->strings_max) {
    x->strings_max += l + 0x1000;         /* >4k steps */
    x->strings = resize_mem(x->strings, x->strings_max * sizeof *x->strings);
  }

  /* make sure the 1st byte is 0 */
  if(x->strings_len == 0) {
    *x->strings = 0;	/* resize_mem does _not_ clear memory */
    x->strings_len = 1;
  }

  if(l == 0) return 0;		/* 1st byte is always 0 */

  strcpy(x->strings + (u = x->strings_len), str);
  x->strings_len += l + 1;

  return u;
}


unsigned store_list(hddb2_data_t *x, hddb_list_t *list)
{
  if(x->list_len == x->list_max) {
    x->list_max += 0x100;	/* 4k steps */
    x->list = resize_mem(x->list, x->list_max * sizeof *x->list);
  }

  x->list[x->list_len++] = *list;

  return x->list_len - 1;
}


unsigned store_value(hddb2_data_t *x, unsigned val)
{
  if(x->ids_len == x->ids_max) {
    x->ids_max += 0x400;	/* 4k steps */
    x->ids = resize_mem(x->ids, x->ids_max * sizeof *x->ids);
  }

  x->ids[x->ids_len++] = val;

  return x->ids_len - 1;
}


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

void clear_entry(tmp_entry_t *te)
{
  memset(te, 0, he_nomask * sizeof *te);
}

void add_value(tmp_entry_t *te, hddb_entry_t idx, unsigned val)
{
  if(idx >= he_nomask) return;
  te += idx;

  if(te->len >= sizeof te->val / sizeof *te->val) return;

  te->val[te->len++] = val;
}

int parse_id(char *str, unsigned *id, unsigned *range, unsigned *mask)
{
  static unsigned id0, val;
  unsigned tag = 0;
  char c = 0, *s, *t = NULL;

  *id = *range = *mask = 0;

  if(!str || !*str) return 0;
  
  for(s = str; *str && !isspace(*str); str++);
  if(*str) {
    c = *(t = str);	/* remember for later */
    *str++ = 0;
  }
  while(isspace(*str)) str++;

  if(*s) {
    if(!strcmp(s, "pci")) tag = TAG_PCI;
    else if(!strcmp(s, "usb")) tag = TAG_USB;
    else if(!strcmp(s, "special")) tag = TAG_SPECIAL;
    else if(!strcmp(s, "eisa")) tag = TAG_EISA;
    else if(!strcmp(s, "isapnp")) tag = TAG_EISA;
    else {
      str = s;
      if(t) *t = c;	/* restore */
    }
  }

  id0 = strtoul(str, &s, 0);

  if(s == str) {
    id0 = name2eisa_id(str);
    if(!id0) return 0;
    s = str + 3;
    id0 = ID_VALUE(id0);
    if(!tag) tag = TAG_EISA;
  }

  while(isspace(*s)) s++;
  if(*s && *s != '&' && *s != '+') return 0;

  *id = MAKE_ID(tag, id0);

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


hddb_entry_mask_t add_entry(hddb2_data_t *hddb2, tmp_entry_t *te, hddb_entry_t idx, char *str)
{
  hddb_entry_mask_t mask = 0;
  int i;
  unsigned u, u0, u1, u2;
  char *s, c;

  for(i = 0; i < sizeof hddb_is_numeric / sizeof *hddb_is_numeric; i++) {
    if(idx == hddb_is_numeric[i]) break;
  }

  if(i < sizeof hddb_is_numeric / sizeof *hddb_is_numeric) {
    /* numeric id */
    mask |= 1 << idx;

    i = parse_id(str, &u0, &u1, &u2);

    switch(i) {
      case 1:
        add_value(te, idx, MAKE_DATA(FLAG_ID, u0));
        break;

      case 2:
        add_value(te, idx, MAKE_DATA(FLAG_RANGE, u1));
        add_value(te, idx, MAKE_DATA(FLAG_ID, u0));
        break;

      case 3:
        add_value(te, idx, MAKE_DATA(FLAG_MASK, u2));
        add_value(te, idx, MAKE_DATA(FLAG_ID, u0));
        break;

      default:
        return 0;
    }
  }
  else {
    if(idx < he_nomask) {
      /* strings */

      mask |= 1 << idx;
      u = store_string(hddb2, str);
      // fprintf(stderr, ">>> %s\n", str);
      add_value(te, idx, MAKE_DATA(FLAG_STRING, u));
    }
    else {
      /* special */

      if(idx == he_class_id) {
        i = parse_id(str, &u0, &u1, &u2);
        if(i != 1) return 0;
        u = ID_VALUE(u0) >> 8;
        add_value(te, he_baseclass_id, MAKE_DATA(FLAG_ID, u));
        u = u0 & 0xff;
        add_value(te, he_subclass_id, MAKE_DATA(FLAG_ID, u));
        /* add_value(te, he_progif_id, MAKE_DATA(FLAG_ID, 0)); */
        mask |= (1 << he_baseclass_id) + (1 << he_subclass_id) /* + (1 << he_progif_id) */;
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
          s = new_mem(strlen(str) + 3);
          s[0] = c;
          s[1] = '\t';
          strcpy(s + 2, str);
          mask |= add_entry(hddb2, te, he_driver, s);
          s = free_mem(s);
        }
      }
    }
  }

  return mask;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void hddb_dump_raw(hddb2_data_t *hddb, FILE *f)
{
  int i;
  unsigned u, fl, v, t, id;
  char *s;

  if(!hddb) return;

  fprintf(f, "=== strings 0x%05x/0x%05x ===\n", hddb->strings_len, hddb->strings_max);

  for(s = hddb->strings, i = 0, u = 0; u < hddb->strings_len; u++) {
    if(!hddb->strings[u]) {
      fprintf(f, "%4d (0x%05x): \"%s\"\n", i, (unsigned) (s - hddb->strings), s);
      i++;
      s = hddb->strings + u + 1;
    }
  }

  fprintf(f, "\n=== ids 0x%05x/0x%05x ===\n", hddb->ids_len, hddb->ids_max);

  for(u = 0; u < hddb->ids_len; u++) {
    fprintf(f, "0x%05x: 0x%08x  ", u, hddb->ids[u]);
    if(hddb->ids[u] & (1 << 31)) fprintf(f, "    ");
    fl = DATA_FLAG(hddb->ids[u]) & 0x7;
    v = DATA_VALUE(hddb->ids[u]);
    if(fl == FLAG_STRING && v < hddb->strings_len) {
      fprintf(f, "\"%s\"", hddb->strings + v);
    }
    else if(fl == FLAG_MASK) {
      fprintf(f, "&0x%04x", v);
    }
    else if(fl == FLAG_RANGE) {
      fprintf(f, "+0x%04x", v);
    }
    else if(fl == FLAG_ID) {
      t = ID_TAG(v);
      id = ID_VALUE(v);
      fprintf(f, "%s0x%04x",
        t < sizeof hid_tag_names / sizeof *hid_tag_names ? hid_tag_names[t] : "",
        id
      );
      if(t == TAG_EISA) {
        fprintf(f, " (%s)", eisa_vendor_str(id));
      }
    }
    fprintf(f, "\n");
  }

  fprintf(f, "\n===  search list 0x%05x/0x%05x ===\n", hddb->list_len, hddb->list_max);

  for(u = 0; u < hddb->list_len; u++) {
    fprintf(f,
      "%4d: 0x%08x 0x%08x 0x%05x 0x%05x\n",
      u, hddb->list[u].key_mask, hddb->list[u].value_mask,
      hddb->list[u].key, hddb->list[u].value
    );
  }
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void hddb_dump_ent_name(hddb2_data_t *hddb, FILE *f, char pre, hddb_entry_t ent)
{
  int len, tab_ind = 24;

  if(ent >= sizeof hddb_entry_strings / sizeof *hddb_entry_strings) return;

  fprintf(f, "%c%s\t", pre, hddb_entry_strings[ent]);

  len = strlen(hddb_entry_strings[ent]) + 1;

  for(len = (len & ~7) + 8; len < tab_ind; len += 8) {
    fputc('\t', f);
  }
}


void hddb_dump_skey(hddb2_data_t *hddb, FILE *f, prefix_t pre, hddb_entry_mask_t key_mask, unsigned key)
{
  static char pref_char[5] = { ' ', ' ', '&', '|', '+' };
  hddb_entry_t ent;
  unsigned rm_val = 0, r_or_m = 0;
  unsigned fl, val, *ids, id, tag, u;
  char *str_val;
  int i;

  if(pre >= sizeof pref_char) return;

  if(key >= hddb->ids_len) return;

  ids = hddb->ids + key;

  for(ent = 0; ent < he_nomask && key_mask; ent++, key_mask >>= 1) {
    if(!(key_mask & 1)) continue;

    fl = DATA_FLAG(*ids);
    val = DATA_VALUE(*ids);

    r_or_m = 0;

    while((fl & FLAG_CONT)) {
      if(fl == (FLAG_CONT | FLAG_RANGE)) {
        rm_val = val;
        r_or_m = 1;
      }
      else if(fl == (FLAG_CONT | FLAG_MASK)) {
        rm_val = val;
        r_or_m = 2;
      }
      else {
        break;
      }

      ids++;

      fl = DATA_FLAG(*ids);
      val = DATA_VALUE(*ids);
    }

    fl &= ~FLAG_CONT;

    if(ent != he_driver) {
      hddb_dump_ent_name(hddb, f, pref_char[pre], ent);

      if(fl == FLAG_ID) {
        tag = ID_TAG(val);
        id = ID_VALUE(val);
        if(tag == TAG_EISA && (ent == he_vendor_id || ent == he_subvendor_id)) {
          fprintf(f, "%s", eisa_vendor_str(id));
        }
        else {
          u = 4;
          if(ent == he_bus_id || ent == he_subclass_id || ent == he_progif_id) {
            u = 2;
          }
          else if(ent == he_baseclass_id) {
            u = 3;
          }
          fprintf(f, "%s0x%0*x",
            tag < sizeof hid_tag_names / sizeof *hid_tag_names ? hid_tag_names[tag] : "",
            u, id
          );
        }
        if(r_or_m) {
          fprintf(f, "%c0x%04x", r_or_m == 1 ? '+' : '&', rm_val);
        }
      }
      else if(fl == FLAG_STRING) {
        if(val < hddb->strings_len) {
          str_val = hddb->strings + val;
          fprintf(f, "%s", str_val);
        }
      }
      fputc('\n', f);
    }
    else {
      ids--;
      do {
        ids++;
        fl = DATA_FLAG(*ids) & ~FLAG_CONT;
        val = DATA_VALUE(*ids);
        if(fl != FLAG_STRING) break;
        str_val = NULL;
        if(val < hddb->strings_len) str_val = hddb->strings + val;
        if(!str_val) break;
        if(!*str_val && !str_val[1] == '\t') break;

        switch(*str_val) {
          case 'x':
             i = he_driver_xfree;
             break;

           case 'X':
             i = he_driver_xfree_config;
             break;

           case 'i':
             i = he_driver_module_insmod;
             break;

           case 'm':
             i = he_driver_module_modprobe;
             break;

           case 'M':
             i = he_driver_module_config;
             break;

           case 'p':
             i = he_driver_mouse;
             break;

           case 'd':
             i = he_driver_display;
             break;

           case 'a':
             i = he_driver_any;
             break;

           default:
             i = -1;
             break;
        }
        if(i == -1) break;

        hddb_dump_ent_name(hddb, f, pref_char[pre], i);
        fprintf(f, "%s\n", str_val + 2);
      }
      while((*ids & (1 << 31)));
    }

    /* at this point 'ids' must be the _current_ entry (_not_ the next) */

    /* skip potential garbage/unhandled entries */
    while((*ids & (1 << 31))) ids++;

    ids++;

    if(pre != pref_add) pre = pref_and;
  }
}


void hddb_dump(hddb2_data_t *hddb, FILE *f)
{
  unsigned u;

  if(!hddb) return;

  for(u = 0; u < hddb->list_len; u++) {
    hddb_dump_skey(hddb, f, pref_new, hddb->list[u].key_mask, hddb->list[u].key);
    hddb_dump_skey(hddb, f, pref_add, hddb->list[u].value_mask, hddb->list[u].value);
    fputc('\n', f);
  }
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
int compare_ids(hddb2_data_t *hddb, hddb_search_t *hs, hddb_entry_mask_t mask, unsigned key)
{
  hddb_entry_t ent;
  unsigned rm_val = 0, r_or_m = 0, res = 0;
  unsigned fl, val, ok, *ids, id;
  char *str, *str_val;

  if(key >= hddb->ids_len) return 1;

  ids = hddb->ids + key;

  for(ent = 0; ent < he_nomask && mask && !res; ent++, mask >>= 1) {
    if(!(mask & 1)) continue;

    fl = DATA_FLAG(*ids);
    val = DATA_VALUE(*ids);

    r_or_m = 0;

    while((fl & FLAG_CONT)) {
      if(fl == (FLAG_CONT | FLAG_RANGE)) {
        rm_val = val;
        r_or_m = 1;
      }
      else if(fl == (FLAG_CONT | FLAG_MASK)) {
        rm_val = val;
        r_or_m = 2;
      }
      else {
        break;
      }

      ids++;

      fl = DATA_FLAG(*ids);
      val = DATA_VALUE(*ids);
    }

    fl &= ~FLAG_CONT;

    id = 0;
    str = str_val = NULL;
    ok = 0;
    if(fl == FLAG_ID) {
      ok = 1;
      switch(ent) {
        case he_bus_id:
          id = hs->bus.id;
          break;

        case he_baseclass_id:
          id = hs->base_class.id;
          break;

        case he_subclass_id:
          id = hs->sub_class.id;
          break;

        case he_progif_id:
          id = hs->prog_if.id;
          break;

        case he_vendor_id:
          id = hs->vendor.id;
          break;

        case he_device_id:
          id = hs->device.id;
          break;

        case he_subvendor_id:
          id = hs->sub_vendor.id;
          break;

        case he_subdevice_id:
          id = hs->sub_device.id;
          break;

        case he_rev_id:
          id = hs->revision.id;
          break;

        default:
          ok = 0;
          break;
      }
    }
    else if(fl == FLAG_STRING) {
      if(val < hddb->strings_len) str_val = hddb->strings + val;
      ok = 2;
      switch(ent) {
        case he_bus_name:
          str = hs->bus.name;
          break;

        case he_baseclass_name:
          str = hs->base_class.name;
          break;

        case he_subclass_name:
          str = hs->sub_class.name;
          break;

        case he_progif_name:
          str = hs->prog_if.name;
          break;

        case he_vendor_name:
          str = hs->vendor.name;
          break;

        case he_device_name:
          str = hs->device.name;
          break;

        case he_subvendor_name:
          str = hs->sub_vendor.name;
          break;

        case he_subdevice_name:
          str = hs->sub_device.name;
          break;

        case he_rev_name:
          str = hs->revision.name;
          break;

        case he_serial:
          str = hs->serial;
          break;

        case he_requires:
          str = hs->requires;
          break;

        default:
          ok = 0;
      }
    }

    switch(ok) {
      case 1:
        switch(r_or_m) {
          case 1:
            if(id < val || id >= val + rm_val) res = 1;
            break;

          case 2:
            if((id & ~rm_val) != val) res = 1;
            break;

          default:
            if(id != val) res = 1;
        }
        break;

      case 2:
        if(str && str_val) {
          if(strcmp(str, str_val)) res = 1;
        }
        else {
          res = 1;
        }
        break;

      default:
        res = 1;
    }

#ifdef HDDB_TRACE
    switch(ok) {
      case 1:
        if(r_or_m) {
          printf(
            "cmp: 0x%05x: (ent = %2d, id = 0x%x, val = 0x%x%c0x%x) = %d\n",
            key, ent, id, val, r_or_m == 1 ? '+' : '&', rm_val, res
          );
        }
        else {
          printf(
            "cmp: 0x%05x: (ent = %2d, id = 0x%x, val = 0x%x) = %d\n",
            key, ent, id, val, res
          );
        }
        break;

      case 2:
        printf(
          "cmp: 0x%05x: (ent = %2d, id = \"%s\", val = \"%s\") = %d\n",
          key, ent, str, str_val, res
        );
        
        break;

      default:
        printf("cmp: 0x%05x: (ent = %2d, *** unhandled key ***) = %d\n", key, ent, res);
    }
#endif

    /* at this point 'ids' must be the _current_ entry (_not_ the next) */

    /* skip potential garbage/unhandled entries */
    while((*ids & (1 << 31))) ids++;

    ids++;
  }

  return res;
}

void complete_ids(
  hddb2_data_t *hddb, hddb_search_t *hs,
  hddb_entry_mask_t key_mask, hddb_entry_mask_t mask, unsigned val_idx
)
{
  hddb_entry_t ent;
  unsigned *ids, *id;
  unsigned fl, val, ok;
  char **str, *str_val;

  if(val_idx >= hddb->ids_len) return;

  ids = hddb->ids + val_idx;

  for(ent = 0; ent < he_nomask && mask; ent++, mask >>= 1) {
    if(!(mask & 1)) continue;

    fl = DATA_FLAG(*ids);
    val = DATA_VALUE(*ids);

    fl &= ~FLAG_CONT;

    id = NULL;
    str = NULL;
    str_val = NULL;
    ok = 0;
    if(fl == FLAG_ID) {
      ok = 1;
      switch(ent) {
        case he_bus_id:
          id = &hs->bus.id;
          break;

        case he_baseclass_id:
          id = &hs->base_class.id;
          break;

        case he_subclass_id:
          id = &hs->sub_class.id;
          break;

        case he_progif_id:
          id = &hs->prog_if.id;
          break;

        case he_vendor_id:
          id = &hs->vendor.id;
          break;

        case he_device_id:
          id = &hs->device.id;
          break;

        case he_subvendor_id:
          id = &hs->sub_vendor.id;
          break;

        case he_subdevice_id:
          id = &hs->sub_device.id;
          break;

        case he_rev_id:
          id = &hs->revision.id;
          break;

        default:
          ok = 0;
          break;
      }
    }
    else if(fl == FLAG_STRING) {
      if(val < hddb->strings_len) str_val = hddb->strings + val;
      ok = 2;
      switch(ent) {
        case he_bus_name:
          str = &hs->bus.name;
          break;

        case he_baseclass_name:
          str = &hs->base_class.name;
          break;

        case he_subclass_name:
          str = &hs->sub_class.name;
          break;

        case he_progif_name:
          str = &hs->prog_if.name;
          break;

        case he_vendor_name:
          str = &hs->vendor.name;
          break;

        case he_device_name:
          str = &hs->device.name;
          break;

        case he_subvendor_name:
          str = &hs->sub_vendor.name;
          break;

        case he_subdevice_name:
          str = &hs->sub_device.name;
          break;

        case he_rev_name:
          str = &hs->revision.name;
          break;

        case he_serial:
          str = &hs->serial;
          break;

        case he_driver:
          ok = 3;
          break;

        case he_requires:
          str = &hs->requires;
          break;

        default:
          ok = 0;
      }
    }

    if(ok) {
      if(
        (hs->value_mask[ent] & key_mask) == hs->value_mask[ent] &&
        key_mask != hs->value_mask[ent]
      ) {
        hs->value_mask[ent] = key_mask;
        hs->value |= 1 << ent;
      }
      else {
        /* don't change if already set */
        ok = 4;
      }

#if 0
      if((hs->value & (1 << ent))) {
        /* don't change if already set */
        ok = 4;
      }
      else if(ent != he_driver) {
        hs->value |= 1 << ent;
      }
#endif
    }

    switch(ok) {
      case 1:
        *id = val;
#ifdef HDDB_TRACE
        printf("add: 0x%05x: (ent = %2d, val = 0x%08x)\n", val_idx, ent, val);
#endif
        break;

      case 2:
        *str = str_val;
#ifdef HDDB_TRACE
        printf("add: 0x%05x: (ent = %2d, val = \"%s\")\n", val_idx, ent, str_val);
#endif
        break;

      case 3:
        ids--;
        do {
          ids++;
          fl = DATA_FLAG(*ids) & ~FLAG_CONT;
          val = DATA_VALUE(*ids);
          if(fl != FLAG_STRING) break;
          str_val = NULL;
          if(val < hddb->strings_len) str_val = hddb->strings + val;
          if(!str_val) break;
#ifdef HDDB_TRACE
          printf("add: 0x%05x: (ent = %2d, val = \"%s\")\n", val_idx, ent, str_val);
#endif
          add_str_list(&hs->driver, str_val);
        }
        while((*ids & (1 << 31)));
        break;

      case 4:
        break;

#ifdef HDDB_TRACE
      default:
        printf("add: 0x%05x: (ent = %2d, *** unhandled value ***)\n", val_idx, ent);
#endif
    }

    /* at this point 'ids' must be the _current_ entry (_not_ the next) */

    /* skip potential garbage/unhandled entries */
    while((*ids & (1 << 31))) ids++;

    ids++;
  }
}

int hddb_search(hd_data_t *hd_data, hddb_search_t *hs)
{
  unsigned u;
  int i;
  hddb2_data_t *hddb;
  int db_idx;

  if(!hs) return 0;

  for(db_idx = 0; db_idx < sizeof hd_data->hddb2 / sizeof *hd_data->hddb2; db_idx++) {
    if(!(hddb = hd_data->hddb2[db_idx])) continue;

    for(u = 0; u < hddb->list_len; u++) {
      if(
        (hs->key & hddb->list[u].key_mask) == hddb->list[u].key_mask
        /* && (hs->value & hddb->list[u].value_mask) != hddb->list[u].value_mask */
      ) {
        i = compare_ids(hddb, hs, hddb->list[u].key_mask, hddb->list[u].key);
        if(!i) {
          complete_ids(hddb, hs,
            hddb->list[u].key_mask,
            hddb->list[u].value_mask, hddb->list[u].value
          );
        }
      }
    }
  }

  return 1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#ifdef HDDB_TEST
void test_db(hd_data_t *hd_data)
{
  hddb_search_t hs = {};
  int i;

  hs.bus.id = 4;
  hs.key |= (1 << he_bus_id) + (1 << he_serial);

  hs.serial = "ser 0123";

  i = hddb_search(hd_data, &hs);

  printf("%d, >%s<\n", i, hs.bus.name);
}
#endif


str_list_t *get_hddb_packages(hd_data_t *hd_data)
{
  return NULL;
}

int hd_find_device_by_name(hd_data_t *hd_data, unsigned base_class, char *vendor, char *device, unsigned *vendor_id, unsigned *device_id)
{
  return 0;
}


char *hd_bus_name(hd_data_t *hd_data, unsigned bus)
{
  hddb_search_t hs = {};

  hs.bus.id = bus;
  hs.key |= 1 << he_bus_id;

  hddb_search(hd_data, &hs);

  return hs.bus.name;
}

char *hd_class_name(hd_data_t *hd_data, int level, unsigned base_class, unsigned sub_class, unsigned prog_if)
{
  hddb_search_t hs = {};
  static char *name = NULL;
  char *s;

  hs.base_class.id = base_class;
  hs.sub_class.id = sub_class;
  hs.prog_if.id = prog_if;
  hs.key |= 1 << he_baseclass_id;
  if(level > 1) hs.key |= 1 << he_subclass_id;
  if(level > 2) hs.key |= 1 << he_progif_id;

  hddb_search(hd_data, &hs);

  if(name) name = free_mem(name);

  s = hs.base_class.name;
  switch(level) {
    case 2:
      if(hs.sub_class.name) s = hs.sub_class.name;
      break;

    case 3:
      if(hs.sub_class.name) s = hs.sub_class.name;
      if(hs.prog_if.name) {
        str_printf(&name, 0, "%s (%s)", s, hs.prog_if.name);
        s = name;
      }
      break;
  }

  return s;
}

char *hd_vendor_name(hd_data_t *hd_data, unsigned vendor)
{
  hddb_search_t hs = {};

  hs.vendor.id = vendor;
  hs.key |= 1 << he_vendor_id;

  hddb_search(hd_data, &hs);

  return hs.vendor.name;
}


char *hd_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  hddb_search_t hs = {};

  hs.vendor.id = vendor;
  hs.device.id = device;
  hs.key |= (1 << he_vendor_id) + (1 << he_device_id);

  hddb_search(hd_data, &hs);

  return hs.device.name;
}


char *hd_sub_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  hddb_search_t hs = {};

  hs.vendor.id = vendor;
  hs.device.id = device;
  hs.sub_vendor.id = sub_vendor;
  hs.sub_device.id = sub_device;
  hs.key |= (1 << he_vendor_id) + (1 << he_device_id) + (1 << he_subvendor_id) + (1 << he_subdevice_id);

  hddb_search(hd_data, &hs);

  return hs.sub_device.name;
}


unsigned device_class(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  hddb_search_t hs = {};

  hs.vendor.id = vendor;
  hs.device.id = device;
  hs.key |= (1 << he_vendor_id) + (1 << he_device_id);

  hddb_search(hd_data, &hs);

  if(
    (hs.value & ((1 << he_baseclass_id) + (1 << he_subclass_id))) ==
    ((1 << he_baseclass_id) + (1 << he_subclass_id))
  ) {
    return (hs.base_class.id << 8) + (hs.sub_class.id & 0xff);
  }

  return 0;
}


unsigned sub_device_class(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  hddb_search_t hs = {};

  hs.vendor.id = vendor;
  hs.device.id = device;
  hs.sub_vendor.id = sub_vendor;
  hs.sub_device.id = sub_device;
  hs.key |= (1 << he_vendor_id) + (1 << he_device_id) + (1 << he_subvendor_id) + (1 << he_subdevice_id);

  hddb_search(hd_data, &hs);

  if(
    (hs.value & ((1 << he_baseclass_id) + (1 << he_subclass_id))) ==
    ((1 << he_baseclass_id) + (1 << he_subclass_id))
  ) {
    return (hs.base_class.id << 8) + (hs.sub_class.id & 0xff);
  }

  return 0;
}


driver_info_t *device_driver(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  hddb_search_t hs = {};

  hs.vendor.id = vendor;
  hs.device.id = device;
  hs.key |= (1 << he_vendor_id) + (1 << he_device_id);

  hddb_search(hd_data, &hs);

  if((hs.value & (1 << he_driver))) {
    return hddb_to_device_driver(hd_data, &hs);
  }

  return NULL;
}


driver_info_t *sub_device_driver(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  hddb_search_t hs = {};

  hs.vendor.id = vendor;
  hs.device.id = device;
  hs.sub_vendor.id = sub_vendor;
  hs.sub_device.id = sub_device;
  hs.key |= (1 << he_vendor_id) + (1 << he_device_id) + (1 << he_subvendor_id) + (1 << he_subdevice_id);

  hddb_search(hd_data, &hs);

  if((hs.value & (1 << he_driver))) {
    return hddb_to_device_driver(hd_data, &hs);
  }

  return NULL;
}


driver_info_t *hddb_to_device_driver(hd_data_t *hd_data, hddb_search_t *hs)
{
  char *s, *t, *t0;
  driver_info_t *di = NULL, *di0 = NULL;
  str_list_t *sl;

  for(sl = hs->driver; sl; sl = sl->next) {
    if(!sl->str || !*sl->str || sl->str[1] != '\t') return NULL;

    if(di && (*sl->str == 'M' || *sl->str == 'X')) {
      add_str_list(&di->any.hddb1, sl->str + 2);
      continue;
    }

    if(di)
      di = di->next = new_mem(sizeof *di);
    else
      di = di0 = new_mem(sizeof *di);

    switch(*sl->str) {
      case 'd':
        di->any.type = di_display;
        break;

      case 'm':
        di->module.modprobe = 1;
      case 'i':
        di->any.type = di_module;
        break;

      case 'p':
        di->any.type = di_mouse;
        break;

      case 'x':
        di->any.type = di_x11;
        break;

      default:
        di->any.type = di_any;
    }

    s = new_str(sl->str + 2);
    for(t0 = s; (t = strsep(&t0, "|")); ) {
      add_str_list(&di->any.hddb0, t);
    }
    free_mem(s);
  }

  return di0;
}



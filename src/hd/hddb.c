#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"

/* activate special data base debug code */
#undef DEBUG_HDDB

#define HDDB_DEV	hddb_dev
#define HDDB_DRV	hddb_drv

extern hddb_data_t HDDB_DEV;
extern hddb_data_t HDDB_DRV;

#define FL_RANGE	4
#define FL_VAL0		5
#define FL_VAL1		6
#define FL_RES		7

#define DATA_VALUE(a)	((a) & ~(-1 << 29))
#define DATA_FLAG(a)	(((a) >> 29) & 0x7)
#define MAKE_DATA(a, b)	((a << 29) | (b))

typedef struct {
  unsigned ok:1, tag_ok:1, val_ok:1, range_ok:1, xtra_ok:1;
  unsigned tag, val, range, xtra;
} hwid_t;

static unsigned find_entry2(hddb_data_t *x, unsigned flag, unsigned level, unsigned *ids);
static unsigned find_entry(hddb_data_t **x, unsigned flag, unsigned level, unsigned *ids);
static unsigned find_entry2_by_name(hddb_data_t *x, unsigned tag, unsigned level, unsigned base_class, unsigned *ids, char **names);
static unsigned find_entry_by_name(hddb_data_t **xx, unsigned tag, unsigned level, unsigned base_class, unsigned *ids, char **names);
static char *name_ind(hddb_data_t *x, unsigned ind);
static unsigned device_class_ind(hddb_data_t *x, unsigned ind);
static driver_info_t *device_driver_ind(hddb_data_t *x, unsigned ind);

static int is_space(int c);
static int is_delim(int c);
static void skip_spaces(char **str);
static hwid_t read_id(char **s);
static char *read_str(char *s);
static void store_data(hddb_data_t *x, unsigned val);
static unsigned store_name(hddb_data_t *x, char *name);
static void store_id(hddb_data_t *x, hwid_t *id, unsigned tag, unsigned level, char *name);

#ifdef DEBUG_HDDB
static void dump_hddb_data(hd_data_t *hd_data, hddb_data_t *x, char *name);
#endif

/*
 * Returns 0 if no entry was found.
 */
unsigned find_entry2(hddb_data_t *x, unsigned flag, unsigned level, unsigned *ids)
{
  unsigned u, u0, u1, v;
  unsigned cur_ids[4], cur_ranges[4], cur_level;

  if(level > 3) return 0;
  memset(cur_ids, 0, sizeof cur_ids);
  for(u = 0; u < sizeof cur_ranges / sizeof *cur_ranges; u++) { cur_ranges[u] = 1; }
  cur_level = 0;

  for(u = 0; u < x->data_len; u++) {
    u0 = DATA_FLAG(x->data[u]);
    u1 = DATA_VALUE(x->data[u]);
    if(u0 < 4) {
      for(v = u0 + 1; v <= level; v++) {
        cur_ids[v] = 0;
        cur_ranges[v] = 1;
      }
      cur_ids[cur_level = u0] = u1;
      cur_ranges[cur_level] = 1;
      if(u + 1 < x->data_len && DATA_FLAG(x->data[u + 1]) == FL_RANGE) continue;
    }
    else if(u0 == 4) {
      cur_ranges[cur_level] = u1;
    }
    else {
      continue;
    }

    /* check if we found an id */
    if(level != cur_level) continue;	/* must match */

    /* this one is tricky... */
    if(
      u < x->data_len &&
      DATA_FLAG(x->data[u + 1]) < 4 &&
      DATA_FLAG(x->data[u + 1]) > cur_level
    ) {
      continue;
    }

    for(v = 0; v <= level; v++) {
      if(ids[v] - cur_ids[v] >= cur_ranges[v]) break;
    }

    if(v > level) {	/* ok */
      if(flag) {
        /* skip all id entries */
        for(u++; u < x->data_len; u++) {
          if(DATA_FLAG(x->data[u]) >= FL_VAL0) break;
        }
        return u < x->data_len ? u : 0;
      }
      if(++u >= x->data_len) return 0;
      return DATA_FLAG(x->data[u]) >= FL_VAL0 ? u : 0;
    }
  }

  return 0;
}

/*
 * Check loaded entries first, then check to the static ones.
 */
unsigned find_entry(hddb_data_t **xx, unsigned flag, unsigned level, unsigned *ids)
{
  unsigned u;

  if((u = find_entry2(*xx, flag, level, ids))) return u;

  *xx = flag ? &HDDB_DRV : &HDDB_DEV;
  return find_entry2(*xx, flag, level, ids);
}


/*
 * Returns 0 if no entry was found.
 */
unsigned find_entry2_by_name(hddb_data_t *x, unsigned tag, unsigned level, unsigned base_class, unsigned *ids, char **names)
{
  unsigned u, u0, u1, v;
  unsigned cur_ids[4], matched, final, cur_level, cur_class, cur_tag;
  char *s;

  if(level > 3) return 0;
  memset(cur_ids, 0, sizeof cur_ids);
  cur_level = matched = final = cur_class = cur_tag = 0;

  /* bitmask */
  final = (1 << (level + 1)) - 1;

  for(u = 0; u < x->data_len; u++) {
    u0 = DATA_FLAG(x->data[u]);
    u1 = DATA_VALUE(x->data[u]);
    if(u0 < 4) {
      cur_class = 0;
      cur_tag = ID_TAG(u1);
      for(v = u0 + 1; v <= level; v++) {
        cur_ids[v] = 0;
        matched &= ~(1 << v);
      }
      cur_ids[cur_level = u0] = u1;
      if(u + 1 < x->data_len && DATA_FLAG(x->data[u + 1]) == FL_RANGE) continue;
    }
    else if(u0 == FL_VAL1) {
      cur_class = u1 >> 8;	/* only base class */
    }
    else if(u0 == FL_VAL0 && cur_level <= level && cur_tag == tag) {
      s = x->names + u1;
//      fprintf(stderr, ">>%d/%d: class 0x%x: \"%s\" <-> \"%s\"\n", cur_level, level, cur_class, s, names[cur_level]);
      if(!strcmp(s, names[cur_level])) matched |= 1 << cur_level;

//      fprintf(stderr, ">>> %x/%x\n", matched, final);

      if(matched == final && level == cur_level && cur_class == base_class) {
//        fprintf(stderr, "FOUND!!!, 0x%04x/0x%04x\n", cur_ids[0], cur_ids[1]);
        for(v = 0; v <= level; v++) {
          ids[v] = cur_ids[v];
        }
        return u;
      }
    }
  }

  return 0;
}

/*
 * Check loaded entries first, then check to the static ones.
 */
unsigned find_entry_by_name(hddb_data_t **xx, unsigned tag, unsigned level, unsigned base_class, unsigned *ids, char **names)
{
  unsigned u;

  if((u = find_entry2_by_name(*xx, tag, level, base_class, ids, names))) return u;

  return find_entry2_by_name(&HDDB_DEV, tag, level, base_class, ids, names);
}


char *name_ind(hddb_data_t *x, unsigned ind)
{
  unsigned u0, u1;

  if(!ind) return NULL;

  u0 = DATA_FLAG(x->data[ind]);
  u1 = DATA_VALUE(x->data[ind]);

  if(u0 == FL_VAL1 && ind + 1 < x->data_len) {
    ind++;
    u0 = DATA_FLAG(x->data[ind]);
    u1 = DATA_VALUE(x->data[ind]);
  }

  if(u0 != FL_VAL0) return NULL;

  return x->names + u1;
}

unsigned device_class_ind(hddb_data_t *x, unsigned ind)
{
  unsigned u0, u1;

  if(!ind) return 0;

  u0 = DATA_FLAG(x->data[ind]);
  u1 = DATA_VALUE(x->data[ind]);

  return u0 == FL_VAL1 ? u1 : 0;
}

driver_info_t *device_driver_ind(hddb_data_t *x, unsigned ind)
{
  unsigned u0, u1, u2, u3;
  char *s, *t, *t0;
  driver_info_t *di = NULL, *di0 = NULL;

  while(1) {
    if(!ind || ind + 1 >= x->data_len) return di0;

    u0 = DATA_FLAG(x->data[ind]);
    u1 = DATA_VALUE(x->data[ind]);

    ind++;
    u2 = DATA_FLAG(x->data[ind]);
    u3 = DATA_VALUE(x->data[ind]);

    if(u0 != FL_VAL1 || u2 != FL_VAL0) return di0;

    if(di)
      di = di->next = new_mem(sizeof *di);
    else
      di = di0 = new_mem(sizeof *di);

    switch(u1) {
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

    s = new_str(x->names + u3);
    for(t0 = s; (t = strsep(&t0, "|")); ) {
      add_str_list(&di->any.hddb0, t);
    }
    free_mem(s);

    for(ind++; ind < x->data_len; ind++) {
      u0 = DATA_FLAG(x->data[ind]);
      u1 = DATA_VALUE(x->data[ind]);
      if(u0 != FL_VAL0) break;
      add_str_list(&di->any.hddb1, x->names + u1);
    }
  }
}


char *hd_bus_name(hd_data_t *hd_data, unsigned bus)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;
  unsigned ids[4] = { MAKE_ID(TAG_BUS, ID_VALUE(bus)), };

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log)
      fprintf(libhd_log, "; %s\t%p\t%p\n", __FUNCTION__, CALLED_FROM(hd_bus_name, hd_data), hd_data);
  }
#endif

  u = find_entry(&x, 0, 0, ids);
  return name_ind(x, u);
}

/*
 * level is 1, 2 or 3 indicating if the base_class, sub_class or prog_if values are valid.
 */
char *hd_class_name(hd_data_t *hd_data, int level, unsigned base_class, unsigned sub_class, unsigned prog_if)
{
  static char *name = NULL;
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;
  unsigned ids[4] = { MAKE_ID(TAG_CLASS, ID_VALUE(base_class)), 0, 0, 0 };
  char *s = NULL, *t;

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log)
      fprintf(libhd_log, "; %s\t%p\t%p\n", __FUNCTION__, CALLED_FROM(hd_class_name, hd_data), hd_data);
  }
#endif

  if(name) name = free_mem(name);

  if(level > 1) ids[1] = MAKE_ID(TAG_CLASS, ID_VALUE(sub_class));
  if(level > 2) ids[2] = MAKE_ID(TAG_CLASS, ID_VALUE(prog_if));

  if(level == 3) {
    u = find_entry(&x, 0, --level, ids);
    if(u) s = name_ind(x, u);
    x = hd_data->hddb_dev;
  }

  do {
    u = find_entry(&x, 0, --level, ids);
  }
  while(u == 0 && level > 0);

  t = name_ind(x, u);

  if(s && t && *t) {
    str_printf(&name, 0, "%s (%s)", t, s);
    t = name;
  }

  return t;
}

char *hd_vendor_name(hd_data_t *hd_data, unsigned vendor)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;
  unsigned ids[4] = { vendor, };

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log)
      fprintf(libhd_log, "; %s\t%p\t%p\n", __FUNCTION__, CALLED_FROM(hd_vendor_name, hd_data), hd_data);
  }
#endif

  u = find_entry(&x, 0, 0, ids);
  return name_ind(x, u);
}

char *hd_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;
  unsigned ids[4] = { vendor, device, };

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log)
      fprintf(libhd_log, "; %s\t%p\t%p\n", __FUNCTION__, CALLED_FROM(hd_device_name, hd_data), hd_data);
  }
#endif

  u = find_entry(&x, 0, 1, ids);
  return name_ind(x, u);
}

char *hd_sub_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;
  unsigned ids[4] = { vendor, device, sub_vendor, sub_device };

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log)
      fprintf(libhd_log, "; %s\t%p\t%p\n", __FUNCTION__, CALLED_FROM(hd_sub_device_name, hd_data), hd_data);
  }
#endif

  u = find_entry(&x, 0, 3, ids);
  return name_ind(x, u);
}

unsigned device_class(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;
  unsigned ids[4] = { vendor, device, };

  u = find_entry(&x, 0, 1, ids);
  return device_class_ind(x, u);
}

unsigned sub_device_class(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;
  unsigned ids[4] = { vendor, device, sub_vendor, sub_device };

  u = find_entry(&x, 0, 3, ids);
  return device_class_ind(x, u);
}

driver_info_t *device_driver(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  hddb_data_t *x = hd_data->hddb_drv;
  unsigned u;
  unsigned ids[4] = { vendor, device, };

  u = find_entry(&x, 1, 1, ids);
  return device_driver_ind(x, u);
}

driver_info_t *sub_device_driver(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  hddb_data_t *x = hd_data->hddb_drv;
  unsigned u;
  unsigned ids[4] = { vendor, device, sub_vendor, sub_device };

  u = find_entry(&x, 1, 3, ids);
  return device_driver_ind(x, u);
}

int is_space(int c)
{
  return c == ' ' || c == '\t' || c == '\n' ? 1 : 0;
}

int is_delim(int c)
{
  return c == '+' || c == '.' || c == 0 ? 1 : is_space(c);
}

void skip_spaces(char **str)
{
  while(is_space(**str)) (*str)++;
}

hwid_t read_id(char **s)
{
  char *t, *t1;
  int l;
  unsigned u;
  hwid_t id;

  memset(&id, 0, sizeof id);

  skip_spaces(s);
  t = *s;
  l = strlen(t);

  if(l >= 3 && is_delim(t[3]) && (u = name2eisa_id(t))) {
    id.tag_ok = id.val_ok = 1;
    id.tag = TAG_EISA;
    id.val = ID_VALUE(u);
    t += 3; l -= 3;
  }
  else {
    switch(*t) {
      case 'p':
        id.tag_ok = 1;
        id.tag = TAG_PCI;
        t++; l--;
        break;

      case 'u':
        id.tag_ok = 1;
        id.tag = TAG_USB;
        t++; l--;
        break;

      case 's':
        id.tag_ok = 1;
        id.tag = TAG_SPECIAL;
        t++; l--;
        break;
    }

    if(l < 2) return id;

    u = strtoul(t, &t1, 16);

    if(t1 - t > 4 || t1 - t < 2) return id;
    l -= t1 - t; t = t1;

    id.val = u;
    id.val_ok = 1;
  }

  if(*t == '+') {
    t++;
    u = strtoul(t, &t1, 16);
    if(t1 == t) return id;
    t = t1;
    id.range = u;
    id.range_ok = 1;
    if(id.range > 0x10000 || id.val + id.range > 0x10000) {
      id.range = 0x10000 - id.val;
    }
  }

  if(*t == '.') {
    t++;
    u = strtoul(t, &t1, 16);
    if(t1 == t) return id;
    t = t1;
    id.xtra = u;
    id.xtra_ok = 1;
  }

  if(is_delim(*t)) {
    *s = t;
    id.ok = 1;
  }

  return id;
}

char *read_str(char *s)
{
  static char *buf = NULL;

  if(buf) buf = free_mem(buf);

  skip_spaces(&s);

  buf = new_str(s);
  s = buf + strlen(buf);

  while(s > buf && is_space(s[-1])) *--s = 0;

  return buf;
}

void store_data(hddb_data_t *x, unsigned val)
{
  if(x->data_len == x->data_max) {
    x->data_max += 0x400;	/* 4k steps */
    x->data = resize_mem(x->data, x->data_max * sizeof *x->data);
  }

  x->data[x->data_len++] = val;
}

unsigned store_name(hddb_data_t *x, char *name)
{
  unsigned l = strlen(name), u;

  if(x->names_len + l >= x->names_max) {
    x->names_max += l + 0x1000;		/* >4k steps */
    x->names = resize_mem(x->names, x->names_max * sizeof *x->names);
  }

  /* make sure that the 1st byte is 0 */
  if(x->names_len == 0) x->names_len = 1;

  if(l == 0) return 0;		/* 1st byte is always 0 */

  strcpy(x->names + (u = x->names_len), name);
  x->names_len += l + 1;

  return u;
}

void store_id(hddb_data_t *x, hwid_t *id, unsigned tag, unsigned level, char *name)
{
  unsigned u;

  store_data(x, MAKE_DATA(level, MAKE_ID(tag, id->val)));

  if(id->range_ok) {
    store_data(x, MAKE_DATA(FL_RANGE, id->range));
  }

  if(id->xtra_ok) {
    store_data(x, MAKE_DATA(FL_VAL1, id->xtra));
  }

  if(name) {
    u = store_name(x, read_str(name));
    store_data(x, MAKE_DATA(FL_VAL0, u));
  }
}

void init_hddb(hd_data_t *hd_data)
{
  char *s;
  int line, no_init = -1;
  str_list_t *sl, *sl0;
  hwid_t id_0, id_1, id_2, id_3;
  unsigned u, tag, last_ids;
  unsigned id0, id1, id2;

  if(hd_data->hddb_dev && hd_data->hddb_drv) return;

  hd_data->hddb_dev = new_mem(sizeof *hd_data->hddb_dev);
  hd_data->hddb_drv = new_mem(sizeof *hd_data->hddb_drv);

  /* read the device names */

  sl0 = read_file(NAME_LIST, 0, 0);

  for(sl = sl0, line = tag = 0; sl; sl = sl->next) {
    s = sl->str;
    line++;

    /* skip empty lines & comments */
    if(*s == '\n' || *s == '#' || *s == ';') continue;

    /* sub-entries */
    if(*s == '\t') {
      if(s[1] == '\t') {	/* level 2 & 3 entries */

        id_0 = read_id(&s);
        if(id_0.ok) {
          if(tag == TAG_CLASS) {	/* only level 2 (prog-if) */
            store_id(hd_data->hddb_dev, &id_0, tag, 2, s);
            continue;
          }
          else {
            store_id(hd_data->hddb_dev, &id_0, tag, 2, NULL);
            id_0 = read_id(&s);
            if(id_0.ok) {
              store_id(hd_data->hddb_dev, &id_0, tag, 3, s);
              continue;
            }
          }
        }

        ADD2LOG("invalid sub-sub id (tag %u) at line %d\n", tag, line);
        no_init = -2;
        break;
      }

      /* level 1 entries */
      id_0 = read_id(&s);
      if(id_0.ok) {
        store_id(hd_data->hddb_dev, &id_0, tag, 1, s);
        continue;
      }

      ADD2LOG("invalid sub id (tag %u) at line %d\n", tag, line);
      no_init = -3;
      break;
    }

    /* level 0 entries */

    tag = TAG_PCI;

    if(*s == 'B' && is_space(s[1])) {
      s++;
      tag = TAG_BUS;
    }
    else if(*s == 'C' && is_space(s[1])) {
      s++;
      tag = TAG_CLASS;
    }

    id_0 = read_id(&s);
    if(id_0.ok) {
      if(id_0.tag_ok) tag = id_0.tag;
      store_id(hd_data->hddb_dev, &id_0, tag, 0, s);
      continue;
    }

    ADD2LOG("invalid id (tag %u) at line %d\n", tag, line);
    no_init = -4;
    break;
  }

  free_str_list(sl0);

  if(no_init < -1) return;

  /* read the driver info */

  sl0 = read_file(DRIVER_LIST, 0, 0);

  id0 = id1 = id2 = 0;
  last_ids = 0;

  for(sl = sl0, line = tag = 0; sl; sl = sl->next) {
    s = sl->str;
    line++;

    /* skip empty lines & comments */
    if(*s == '\n' || *s == '#' || *s == ';') continue;

    /* driver info */
    if(*s == '\t') {
      last_ids = 0;

      if(s[1] == '\t') {	/* extra driver info */

        /* remove new-line char at end of line */
        u = strlen(s);
        if(u && s[u - 1] == '\n') s[u - 1] = 0;

        u = store_name(hd_data->hddb_drv, s + 2);
        store_data(hd_data->hddb_drv, MAKE_DATA(FL_VAL0, u));

        continue;
      }

      if(s[1] >= 'a' && s[1] <= 'z' && is_space(s[2])) {
         /* beware of chars > 127! */
         store_data(hd_data->hddb_drv, MAKE_DATA(FL_VAL1, s[1]));

         u = store_name(hd_data->hddb_drv, read_str(s + 2));
         store_data(hd_data->hddb_drv, MAKE_DATA(FL_VAL0, u));
         
         continue;
      }

      ADD2LOG("invalid driver info at line %d\n", line);
      break;
    }

    /* device ids */

    tag = TAG_PCI;

    id_1.ok = id_2.ok = id_3.ok = 0;
    id_0 = read_id(&s);
    if(id_0.ok) id_1 = read_id(&s);
    if(id_1.ok) id_2 = read_id(&s);
    if(id_2.ok) id_3 = read_id(&s);

    if(id_0.ok && (id_1.ok || id_3.ok)) {
      if(id_0.tag_ok) tag = id_0.tag;

      if(id0 != id_0.val || id_0.range_ok) {
        id0 = id_0.val;
        store_id(hd_data->hddb_drv, &id_0, tag, 0, NULL);
      }
      if(id_1.ok && (
        id1 != id_1.val ||
        id_1.range_ok ||
        (id_3.ok && last_ids < 4) ||
        (!id_3.ok && last_ids > 2)
      )) {
        id1 = id_1.val;
        store_id(hd_data->hddb_drv, &id_1, tag, 1, NULL);
      }
      if(id_2.ok && (id2 != id_2.val || id_2.range_ok)) {
        id2 = id_2.val;
        store_id(hd_data->hddb_drv, &id_2, tag, 2, NULL);
      }
      if(id_3.ok) {
        store_id(hd_data->hddb_drv, &id_3, tag, 3, NULL);
      }

      last_ids = 1;
      if(id_1.ok) last_ids++;
      if(id_2.ok) last_ids++;
      if(id_3.ok) last_ids++;

      continue;
    }

    ADD2LOG("invalid id spec (tag %u) at line %d\n", tag, line);
    no_init = -10;
    break;
  }

  free_str_list(sl0);

  if(no_init < -1) return;

  no_init = 0;

#ifdef DEBUG_HDDB
  if((hd_data->debug & HD_DEB_HDDB)) {
    dump_hddb_data(hd_data, &HDDB_DEV, "hddb_dev, static");
    dump_hddb_data(hd_data, hd_data->hddb_dev, "hddb_dev, loaded");
    dump_hddb_data(hd_data, &HDDB_DRV, "hddb_drv, static");
    dump_hddb_data(hd_data, hd_data->hddb_drv, "hddb_drv, loaded");
  }
#endif

}


/*
 * Should we remove a potentially existing old entry? At the moment the new
 * entry is ignored in that case.
 */
void add_vendor_name(hd_data_t *hd_data, unsigned vendor, char *name)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;

  if(!x) return;

  store_data(x, MAKE_DATA(0, DATA_VALUE(vendor)));

  if(name) {
    u = store_name(x, name);
    store_data(x, MAKE_DATA(FL_VAL0, u));
  }
}

void add_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device, char *name)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;

  if(!x) return;

  store_data(x, MAKE_DATA(0, DATA_VALUE(vendor)));
  store_data(x, MAKE_DATA(1, DATA_VALUE(device)));

  if(name) {
    u = store_name(x, name);
    store_data(x, MAKE_DATA(FL_VAL0, u));
  }
}

void add_sub_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device, char *name)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;

  if(!x) return;

  store_data(x, MAKE_DATA(0, DATA_VALUE(vendor)));
  store_data(x, MAKE_DATA(1, DATA_VALUE(device)));
  store_data(x, MAKE_DATA(2, DATA_VALUE(sub_vendor)));
  store_data(x, MAKE_DATA(3, DATA_VALUE(sub_device)));

  if(name) {
    u = store_name(x, name);
    store_data(x, MAKE_DATA(FL_VAL0, u));
  }
}

int hd_find_device_by_name(hd_data_t *hd_data, unsigned base_class, char *vendor, char *device, unsigned *vendor_id, unsigned *device_id)
{
  hddb_data_t *x = hd_data->hddb_dev;
  unsigned u;
  unsigned ids[4];
  char *names[4];

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log)
      fprintf(libhd_log, "; %s\t%p\t%p\n", __FUNCTION__, CALLED_FROM(hd_find_device_by_name, hd_data), hd_data);
  }
#endif

  names[0] = vendor;
  names[1] = device;

  u = find_entry_by_name(&x, TAG_SPECIAL, 1, base_class, ids, names);

  if(u) {
    *vendor_id = ids[0];
    *device_id = ids[1];
  }

  return u;
}


#ifdef DEBUG_HDDB
static char *id2str(unsigned id, int vend);

char *id2str(unsigned id, int vend)
{
  static char buf[32];
  char *s;

  *(s = buf) = 0;

  if(vend && ID_TAG(id) == TAG_EISA) {
    strcpy(s, eisa_vendor_str(id));
  }
  else {
    if(ID_TAG(id) == TAG_PCI) *s++ = 'p', *s = 0;
    if(ID_TAG(id) == TAG_EISA) *s++ = 'i', *s = 0;
    if(ID_TAG(id) == TAG_USB) *s++ = 'u', *s = 0;
    if(ID_TAG(id) == TAG_SPECIAL) *s++ = 's', *s = 0;
    if(ID_TAG(id) == TAG_BUS) *s++ = 'b', *s = 0;
    if(ID_TAG(id) == TAG_CLASS) *s++ = 'c', *s = 0;
    sprintf(s, "%04x", ID_VALUE(id));
  }

  return buf;
}


void dump_hddb_data(hd_data_t *hd_data, hddb_data_t *x, char *name)
{
  unsigned u, u0, u1;
  static char *flags[8] = {
    "#0   ", "#1   ", "#2   ", "#3   ", "range", "   v0", "   v1", "  res"
  };

  ADD2LOG(
    "%s: data 0x%x/0x%x, names 0x%x/0x%x\n",
    name, x->data_len, x->data_max, x->names_len, x->names_max
  );
  
  for(u = 0; u < x->data_len; u++) {
    u0 = DATA_FLAG(x->data[u]);
    u1 = DATA_VALUE(x->data[u]);
    ADD2LOG("%3d\t%x:%05x\t%s:", u, u0, u1, flags[u0]);
    if(u0 < 4) {
      ADD2LOG("\t%-5s", id2str(u1, 1 ^ (u0 & 1)));
    }
    else {
    }
    if(u0 == FL_RANGE) ADD2LOG("  +0x%04x", u1);
    if(u0 == FL_VAL0) ADD2LOG("  \"%s\"", x->names + u1);
    if(u0 == FL_VAL1) ADD2LOG("  '%c'", u1 & 0xff);
    ADD2LOG("\n");
  }
  ADD2LOG("----\n");
}
#endif

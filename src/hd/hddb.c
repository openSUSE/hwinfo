#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"

#ifdef SHORT_LIST
#define ID_LIST id_list_r
#else
#define ID_LIST id_list
#endif

unsigned hddb_dev_data[] = { };
char hddb_dev_names[] = "";

hddb_data_t hddb_dev = {
  0, 0, hddb_dev_data,
  0, 0, hddb_dev_names
};

unsigned hddb_drv_data[] = { };
char hddb_drv_names[] = "";

hddb_data_t hddb_drv = {
  0, 0, hddb_drv_data,
  0, 0, hddb_drv_names
};

#define HDDB_DEV	hddb_dev
#define HDDB_DRV	hddb_drv

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
static unsigned find_entry(hddb_data_t *x, unsigned flag, unsigned level, unsigned *ids);
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

static void dump_hddb_data(hd_data_t *hd_data, hddb_data_t *x, char *name);

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
unsigned find_entry(hddb_data_t *x, unsigned flag, unsigned level, unsigned *ids)
{
  unsigned u;

  if((u = find_entry2(x, flag, level, ids))) return u;

  return find_entry2(flag ? &HDDB_DRV : &HDDB_DEV, flag, level, ids);
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
      add_str_list(&di->any.hddb0, new_str(t));
    }
    free_mem(s);

    for(ind++; ind < x->data_len; ind++) {
      u0 = DATA_FLAG(x->data[ind]);
      u1 = DATA_VALUE(x->data[ind]);
      if(u0 != FL_VAL0) break;
      add_str_list(&di->any.hddb1, new_str(x->names + u1));
    }
  }
}


char *hd_bus_name(hd_data_t *hd_data, unsigned bus)
{
  unsigned ids[4] = { MAKE_ID(TAG_BUS, ID_VALUE(bus)), };

  return name_ind(hd_data->hddb_dev, find_entry(hd_data->hddb_dev, 0, 0, ids));
}

/*
 * level is 1, 2 or 3 indicating if the base_class, sub_class or prog_if values are valid.
 */
char *hd_class_name(hd_data_t *hd_data, int level, unsigned base_class, unsigned sub_class, unsigned prog_if)
{
  unsigned ids[4] = { MAKE_ID(TAG_CLASS, ID_VALUE(base_class)), 0, 0, 0 };
  unsigned u;

  if(level > 1) ids[1] = MAKE_ID(TAG_CLASS, ID_VALUE(sub_class));
  if(level > 2) ids[2] = MAKE_ID(TAG_CLASS, ID_VALUE(prog_if));

  do {
    u = find_entry(hd_data->hddb_dev, 0, --level, ids);
  }
  while(u == 0 && level > 0);

  return name_ind(hd_data->hddb_dev, u);
}

char *hd_vendor_name(hd_data_t *hd_data, unsigned vendor)
{
  unsigned ids[4] = { vendor, };

  return name_ind(hd_data->hddb_dev, find_entry(hd_data->hddb_dev, 0, 0, ids));
}

char *hd_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  unsigned ids[4] = { vendor, device, };

  return name_ind(hd_data->hddb_dev, find_entry(hd_data->hddb_dev, 0, 1, ids));
}

char *hd_sub_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  unsigned ids[4] = { vendor, device, sub_vendor, sub_device };

  return name_ind(hd_data->hddb_dev, find_entry(hd_data->hddb_dev, 0, 3, ids));
}

unsigned device_class(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  unsigned ids[4] = { vendor, device, };

  return device_class_ind(hd_data->hddb_dev, find_entry(hd_data->hddb_dev, 0, 1, ids));
}

unsigned sub_device_class(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  unsigned ids[4] = { vendor, device, sub_vendor, sub_device };

  return device_class_ind(hd_data->hddb_dev, find_entry(hd_data->hddb_dev, 0, 3, ids));
}

driver_info_t *device_driver(hd_data_t *hd_data, unsigned vendor, unsigned device)
{
  unsigned ids[4] = { vendor, device, };

  return device_driver_ind(hd_data->hddb_drv, find_entry(hd_data->hddb_drv, 1, 1, ids));
}

driver_info_t *sub_device_driver(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device)
{
  unsigned ids[4] = { vendor, device, sub_vendor, sub_device };

  return device_driver_ind(hd_data->hddb_drv, find_entry(hd_data->hddb_drv, 1, 3, ids));
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
    store_data(x, MAKE_DATA(FL_RANGE, MAKE_ID(tag, id->range)));
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
  hwid_t id;
  unsigned u, tag;
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

        id = read_id(&s);
        if(id.ok) {
          if(tag == TAG_CLASS) {	/* only level 2 (prog-if) */
            store_id(hd_data->hddb_dev, &id, tag, 2, s);
            continue;
          }
          else {
            store_id(hd_data->hddb_dev, &id, tag, 2, NULL);
            id = read_id(&s);
            if(id.ok) {
              store_id(hd_data->hddb_dev, &id, tag, 3, s);
              continue;
            }
          }
        }

        ADD2LOG("invalid sub-sub id (tag %u) at line %d\n", tag, line);
        no_init = -2;
        break;
      }

      /* level 1 entries */
      id = read_id(&s);
      if(id.ok) {
        store_id(hd_data->hddb_dev, &id, tag, 1, s);
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

    id = read_id(&s);
    if(id.ok) {
      if(id.tag_ok) tag = id.tag;
      store_id(hd_data->hddb_dev, &id, tag, 0, s);
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

  for(sl = sl0, line = tag = 0; sl; sl = sl->next) {
    s = sl->str;
    line++;

    /* skip empty lines & comments */
    if(*s == '\n' || *s == '#' || *s == ';') continue;

    /* driver info */
    if(*s == '\t') {
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

    id = read_id(&s);
    if(id.ok) {
      if(id.tag_ok) tag = id.tag;
      if(id0 != id.val || id.range_ok) {
        id0 = id.val;
        store_id(hd_data->hddb_drv, &id, tag, 0, NULL);
      }
      id = read_id(&s);
      if(id.ok) {
        if(id1 != id.val || id.range_ok) {
          id1 = id.val;
          store_id(hd_data->hddb_drv, &id, tag, 1, NULL);
        }
        id = read_id(&s);
        if(id.ok) {
          if(id2 != id.val || id.range_ok) {
            id2 = id.val;
            store_id(hd_data->hddb_drv, &id, tag, 2, NULL);
          }
          id = read_id(&s);
          if(id.ok) {
            store_id(hd_data->hddb_drv, &id, tag, 3, NULL);
            continue;
          }
        }
        continue;
      }
    }

    ADD2LOG("invalid id spec (tag %u) at line %d\n", tag, line);
    no_init = -10;
    break;
  }

  free_str_list(sl0);

  if(no_init < -1) return;

  no_init = 0;

  if(hd_data->debug) {
    dump_hddb_data(hd_data, &HDDB_DEV, "hddb_dev, static");
    dump_hddb_data(hd_data, hd_data->hddb_dev, "hddb_dev, loaded");
    dump_hddb_data(hd_data, &HDDB_DRV, "hddb_drv, static");
    dump_hddb_data(hd_data, hd_data->hddb_drv, "hddb_drv, loaded");
  }

#if 0
  {
    unsigned ids[4] = { 0x10de, 0x20, 0x1043, 0x200 };
    driver_info_t *di;
    str_list_t *sl;

    u = find_entry(hd_data->hddb_dev, 0, 3, ids);
    ADD2LOG(">>%d\n", u);

    di = device_driver(hd_data, 0x10de, 0x20);

    for(; di; di = di->next) {
      ADD2LOG("type %d\n", di->any.type);

      for(sl = di->any.hddb0; sl; sl = sl->next) {
        ADD2LOG("  >%s<\n", sl->str);
      }

      for(sl = di->any.hddb1; sl; sl = sl->next) {
        ADD2LOG("  \"%s\"\n", sl->str);
      }
    }
  }
#endif

}


/*
 * Should we remove a potentially existing old entry? At the moment the new
 * entry is ignored in that case.
 */
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

void dump_hddb_data(hd_data_t *hd_data, hddb_data_t *x, char *name)
{
  unsigned u, u0, u1;

  ADD2LOG(
    "%s: data 0x%x/0x%x, names 0x%x/0x%x\n",
    name, x->data_len, x->data_max, x->names_len, x->names_max
  );
  
  for(u = 0; u < x->data_len; u++) {
    u0 = DATA_FLAG(x->data[u]);
    u1 = DATA_VALUE(x->data[u]);
    ADD2LOG("%3d\t%x:%05x", u, u0, u1);
    if(u0 == FL_VAL0) ADD2LOG("  \"%s\"", x->names + u1);
    ADD2LOG("\n");
  }
  ADD2LOG("----\n");
}


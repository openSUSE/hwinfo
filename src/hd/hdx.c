#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hdx.h"

#ifdef SHORT_LIST
#define ID_LIST id_list_r
#else
#define ID_LIST id_list
#endif
extern char *ID_LIST[];

// we are somewhat slower if we define this
#undef NO_DUP_ENTRIES

typedef struct {
  unsigned val0;
  char *name;
} val1_name_t;

typedef struct {
  unsigned val0, val1;
  char *name;
} val2_name_t;

typedef struct {
  unsigned val0, val1, val2;
  char *drv;
  char *name;
} val3_name_t;

typedef struct {
  unsigned val0, val1, val2, val3, val4;
  char *drv;
  char *name;
} val5_name_t;


static int no_init = 1;

static val1_name_t* bus_name_lst = NULL;
static unsigned bus_names = 0;

static val1_name_t* class_name_lst = NULL;
static unsigned class_names = 0;

static val2_name_t* sub_class_name_lst = NULL;
static unsigned sub_class_names = 0;

static val1_name_t* vendor_name_lst = NULL;
static unsigned vendor_names = 0;

static val3_name_t* device_name_lst = NULL;
static unsigned device_names = 0;

static val5_name_t* sub_device_name_lst = NULL;
static unsigned sub_device_names = 0;

static void init_hdx(void);

static char *next_line(char *buf, int buf_size, FILE *f, int line);

char *hd_bus_name(unsigned bus)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < bus_names; i++) {
    if(bus_name_lst[i].val0 == bus) return bus_name_lst[i].name;
  }

  return NULL;
}

int hd_bus_number(char *bus_name)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < bus_names; i++) {
    if(!strcmp(bus_name, bus_name_lst[i].name)) return bus_name_lst[i].val0;
  }

  return -1;
}

char *hd_base_class_name(unsigned bc)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < class_names; i++) {
    if(class_name_lst[i].val0 == bc) return class_name_lst[i].name;
  }

  return NULL;
}

int hd_base_class_number(char *bc_name)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < class_names; i++) {
    if(!strcmp(bc_name, class_name_lst[i].name)) return class_name_lst[i].val0;
  }

  return -1;
}

char *hd_sub_class_name(unsigned bc, unsigned sc)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < sub_class_names; i++) {
    if(
      sub_class_name_lst[i].val0 == sc &&
      sub_class_name_lst[i].val1 == bc
    ) return sub_class_name_lst[i].name;
  }

  return hd_base_class_name(bc);
}

char *hd_vendor_name(unsigned v)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < vendor_names; i++) {
    if(vendor_name_lst[i].val0 == v) return vendor_name_lst[i].name;
  }

  return NULL;
}

char *hd_device_name(unsigned v, unsigned d)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < device_names; i++) {
    if(
      device_name_lst[i].val0 == d &&
      device_name_lst[i].val1 == v
    ) return device_name_lst[i].name;
  }

  return NULL;
}

unsigned device_class(unsigned v, unsigned d)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < device_names; i++) {
    if(
      device_name_lst[i].val0 == d &&
      device_name_lst[i].val1 == v
    ) return device_name_lst[i].val2;
  }

  return 0;
}

char *hd_device_drv_name(unsigned v, unsigned d)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < device_names; i++) {
    if(
      device_name_lst[i].val0 == d &&
      device_name_lst[i].val1 == v
    ) return device_name_lst[i].drv;
  }

  return NULL;
}

char *hd_sub_device_name(unsigned v, unsigned d, unsigned sv, unsigned sd)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < sub_device_names; i++) {
    if(
      sub_device_name_lst[i].val0 == sd &&
      sub_device_name_lst[i].val1 == sv &&
      sub_device_name_lst[i].val2 == d &&
      sub_device_name_lst[i].val3 == v
    ) return sub_device_name_lst[i].name;
  }

  /*
   * If nothing was found, try dedicated subdevice entries.
   */
  for(i = 0; i < sub_device_names; i++) {
    if(
      sub_device_name_lst[i].val0 == sd &&
      sub_device_name_lst[i].val1 == sv &&
      sub_device_name_lst[i].val2 == 0 &&
      sub_device_name_lst[i].val3 == 0
    ) return sub_device_name_lst[i].name;
  }

  return NULL;		// or try device_name(sv, sd) ???
}

unsigned sub_device_class(unsigned v, unsigned d, unsigned sv, unsigned sd)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < sub_device_names; i++) {
    if(
      sub_device_name_lst[i].val0 == sd &&
      sub_device_name_lst[i].val1 == sv &&
      sub_device_name_lst[i].val2 == d &&
      sub_device_name_lst[i].val3 == v
    ) return sub_device_name_lst[i].val4;
  }

  /*
   * If nothing was found, try dedicated subdevice entries.
   */
  for(i = 0; i < sub_device_names; i++) {
    if(
      sub_device_name_lst[i].val0 == sd &&
      sub_device_name_lst[i].val1 == sv &&
      sub_device_name_lst[i].val2 == 0 &&
      sub_device_name_lst[i].val3 == 0
    ) return sub_device_name_lst[i].val4;
  }

  return 0;
}

char *hd_sub_device_drv_name(unsigned v, unsigned d, unsigned sv, unsigned sd)
{
  int i;

  if(no_init) init_hdx();

  for(i = 0; i < sub_device_names; i++) {
    if(
      sub_device_name_lst[i].val0 == sd &&
      sub_device_name_lst[i].val1 == sv &&
      sub_device_name_lst[i].val2 == d &&
      sub_device_name_lst[i].val3 == v
    ) return sub_device_name_lst[i].drv;
  }

  /*
   * If nothing was found, try dedicated subdevice entries.
   */
  for(i = 0; i < sub_device_names; i++) {
    if(
      sub_device_name_lst[i].val0 == sd &&
      sub_device_name_lst[i].val1 == sv &&
      sub_device_name_lst[i].val2 == 0 &&
      sub_device_name_lst[i].val3 == 0
    ) return sub_device_name_lst[i].drv;
  }

  return NULL;		// or try device_drv_name(sv, sd) ???
}

char *next_line(char *buf, int buf_size, FILE *f, int line)
{
  if(f) return fgets(buf, buf_size, f);
  if(ID_LIST[line]) {
    strncpy(buf, ID_LIST[line], buf_size);
    buf[buf_size - 1] = 0;
    return buf;
  }

  return NULL;
}

void init_hdx()
{
  FILE *f;
  char buf[256], buf2[256], buf3[10], cur_ent_type = 0;
  unsigned u, u2, u3;
  unsigned cur_ent_val = 0, cur_ent_val1 = 0;
  int line = 0;

  if(no_init < 0) return;
  no_init = -1;

  f = NULL;
  if(
    !(f = fopen(ID_LIST_NAME, "r")) &&
    !(f = fopen(ID_LIST_NAME_FALLBACK, "r"))
  );

  while(next_line(buf, sizeof buf, f, line)) {
    line++;

    if(*buf == '\n' || *buf == '#' || *buf == ';') continue;

    // device or subdevice entries
    if(*buf == '\t') {
      // subdevice entries
      if(buf[1] == '\t') {
        // EISA subdevice entries with class & subclass value
        if(
          cur_ent_type == 'v' &&
          sscanf(buf, " %3s%x.%x %200[^\n]", buf3, &u, &u2, buf2) == 4
        ) {
          u3 = name2eisa_id(buf3);
          add_sub_device_name(cur_ent_val, cur_ent_val1, u3, MAKE_EISA_ID(u), u2, buf2);
          continue;
        }

        // EISA subdevice entries
        if(
          cur_ent_type == 'v' &&
          sscanf(buf, " %3s%x %200[^\n]", buf3, &u, buf2) == 3
        ) {
          u3 = name2eisa_id(buf3);
          add_sub_device_name(cur_ent_val, cur_ent_val1, u3, MAKE_EISA_ID(u), 0, buf2);
          continue;
        }

        // subdevice entries
        if(
          cur_ent_type == 'V' &&
          sscanf(buf, " %4x%4x %200[^\n]", &u, &u2, buf2) == 3
        ) {
          add_sub_device_name(cur_ent_val, cur_ent_val1, u2, u, 0, buf2);
          continue;
        }

        fprintf(stderr, "oops3 at line %d\n", line);
      }

      // EISA device entries with class & subclass value
      if(cur_ent_type == 'v' && sscanf(buf, " %x.%x %200[^\n]", &u, &u2, buf2) == 3) {
        cur_ent_val1 = MAKE_EISA_ID(u);
        add_device_name(cur_ent_val, cur_ent_val1, u2, buf2);
        continue;
      }

      // EISA dedicated subdevice entries with class & subclass value
      if(cur_ent_type == 's' && sscanf(buf, " %x.%x %200[^\n]", &u, &u2, buf2) == 3) {
        add_sub_device_name(0, 0, cur_ent_val, MAKE_EISA_ID(u), u2, buf2);
        continue;
      }

      // subclass, device or subdevice entries
      if(sscanf(buf, " %x %200[^\n]", &u, buf2) == 2) {
        switch(cur_ent_type) {
          case 'C':
            add_sub_class_name(cur_ent_val, u, buf2);
            break;

          case 'V':
            cur_ent_val1 = u;
            add_device_name(cur_ent_val, cur_ent_val1, 0, buf2);
            break;

          case 'S':
            add_sub_device_name(0, 0, cur_ent_val, u, 0, buf2);
            break;

          case 'v':
            cur_ent_val1 = MAKE_EISA_ID(u);
            add_device_name(cur_ent_val, cur_ent_val1, 0, buf2);
            break;

          case 's':
            add_sub_device_name(0, 0, cur_ent_val, MAKE_EISA_ID(u), 0, buf2);
            break;

          default:
            fprintf(stderr, "oops2 at line %d\n", line);
        }
        continue;
      }
    }

    // bus entries
    if(sscanf(buf, "B %x %200[^\n]", &u, buf2) == 2) {
      cur_ent_val = cur_ent_val1 = 0;
      cur_ent_type = 'B';
      add_bus_name(u, buf2);
      continue;
    }

    // class entries
    if(sscanf(buf, "C %x %200[^\n]", &u, buf2) == 2) {
      cur_ent_val = cur_ent_val1 = 0;
      cur_ent_type = 'C';
      add_class_name(u, buf2);
      cur_ent_val = u;
      continue;
    }

    // EISA vendor entries
    if((buf[3] == ' ' || buf[3] == '\t') && sscanf(buf, "%3s %200[^\n]", buf3, buf2) == 2) {
      cur_ent_val = cur_ent_val1 = 0;
      cur_ent_type = 'v';
      u = name2eisa_id(buf3);
      add_vendor_name(u, buf2);
      cur_ent_val = u;
      continue;
    }

    // PCI vendor entries
    if(sscanf(buf, "%x %200[^\n]", &u, buf2) == 2) {
      cur_ent_val = cur_ent_val1 = 0;
      cur_ent_type = 'V';
      add_vendor_name(u, buf2);
      cur_ent_val = u;
      continue;
    }

    // EISA dedicated subdevice entries
    if((buf[5] == ' ' || buf[5] == '\t') && sscanf(buf, "S %3s %200[^\n]", buf3, buf2) == 2) {
      cur_ent_val = cur_ent_val1 = 0;
      cur_ent_type = 's';
      u = name2eisa_id(buf3);
      add_vendor_name(u, buf2);
      cur_ent_val = u;
      continue;
    }

    // PCI dedicated subdevice entries
    if(sscanf(buf, "S %x %200[^\n]", &u, buf2) == 2) {
      cur_ent_val = cur_ent_val1 = 0;
      cur_ent_type = 'S';
      add_vendor_name(u, buf2);
      cur_ent_val = u;
      continue;
    }

    fprintf(stderr, "oops at line %d\n", line);
  }

  if(f) fclose(f);  

  no_init = 0;
}


/*
 * b = bus
 * c = class
 * d = device
 * v = vendor
 * sc, sd, sv = sub...
 */

void add_bus_name(unsigned b, char *s)
{
#ifdef NO_DUP_ENTRIES
  if(bus_name(b)) return;
#endif

  bus_name_lst = add_mem(bus_name_lst, sizeof *bus_name_lst, bus_names);
  bus_name_lst[bus_names].val0 = b;
  bus_name_lst[bus_names].name = new_str(s);
  bus_names++;
}

void add_class_name(unsigned c, char *s)
{
#ifdef NO_DUP_ENTRIES
  if(base_class_name(c)) return;
#endif

  class_name_lst = add_mem(class_name_lst, sizeof *class_name_lst, class_names);
  class_name_lst[class_names].val0 = c;
  class_name_lst[class_names].name = new_str(s);
  class_names++;
}

void add_sub_class_name(unsigned c, unsigned sc, char *s)
{
#ifdef NO_DUP_ENTRIES
  if(sub_class_name(c, sc)) return;
#endif

  sub_class_name_lst = add_mem(sub_class_name_lst, sizeof *sub_class_name_lst, sub_class_names);
  sub_class_name_lst[sub_class_names].val0 = sc;
  sub_class_name_lst[sub_class_names].val1 = c;
  sub_class_name_lst[sub_class_names].name = new_str(s);
  sub_class_names++;
}

void add_vendor_name(unsigned v, char *s)
{
  // always block duplicate entries
  if(hd_vendor_name(v)) return;

  vendor_name_lst = add_mem(vendor_name_lst, sizeof *vendor_name_lst, vendor_names);
  vendor_name_lst[vendor_names].val0 = v;
  vendor_name_lst[vendor_names].name = new_str(s);
  vendor_names++;
}

void add_device_name(unsigned v, unsigned d, unsigned c, char *s)
{
  char *t, *t1;

#ifdef NO_DUP_ENTRIES
  if(device_name(v, d)) return;
#endif

  device_name_lst = add_mem(device_name_lst, sizeof *device_name_lst, device_names);
  device_name_lst[device_names].val0 = d;
  device_name_lst[device_names].val1 = v;
  device_name_lst[device_names].val2 = c;

  if(*s == '[' && (t = index(s + 1, ']'))) {
    if(t > s + 1) {
      t1 =  new_mem(t - s);
      memcpy(t1, s + 1, t - s - 1);
      device_name_lst[device_names].drv = t1;
    }
    while(t[1] == ' ' || t[1] == '\t') t++;
    s = t + 1;
  }

  device_name_lst[device_names].name = new_str(s);
  device_names++;
}

void add_sub_device_name(unsigned v, unsigned d, unsigned sv, unsigned sd, unsigned c, char *s)
{
  char *t, *t1;

#ifdef NO_DUP_ENTRIES
  if(sub_device_name(v, d, sv, sd)) return;
#endif

  sub_device_name_lst = add_mem(sub_device_name_lst, sizeof *sub_device_name_lst, sub_device_names);
  sub_device_name_lst[sub_device_names].val0 = sd;
  sub_device_name_lst[sub_device_names].val1 = sv;
  sub_device_name_lst[sub_device_names].val2 = d;
  sub_device_name_lst[sub_device_names].val3 = v;
  sub_device_name_lst[sub_device_names].val4 = c;

  if(*s == '[' && (t = index(s + 1, ']'))) {
    if(t > s + 1) {
      t1 =  new_mem(t - s);
      memcpy(t1, s + 1, t - s - 1);
      sub_device_name_lst[sub_device_names].drv = t1;
    }
    while(t[1] == ' ' || t[1] == '\t') t++;
    s = t + 1;
  }

  sub_device_name_lst[sub_device_names].name = new_str(s);
  sub_device_names++;
}


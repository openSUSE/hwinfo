#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#ifndef DBUS_API_SUBJECT_TO_CHANGE
  #define DBUS_API_SUBJECT_TO_CHANGE 1
#endif

#include <dbus/dbus.h>
#include <hal/libhal.h>

#include "hd.h"
#include "hd_int.h"
#include "hal.h"

/**
 * @defgroup HALint Hardware abstraction (HAL) information
 * @ingroup  libhdInternals
 *
 * @{
 */

static void read_hal(hd_data_t *hd_data);
static void add_pci(hd_data_t *hd_data);
static void link_hal_tree(hd_data_t *hd_data);

static int hal_match_str(hal_prop_t *prop, const char *key, const char *val);

static int check_udi(const char *udi);
static FILE *hd_open_properties(const char *udi, const char *mode);
static char *skip_space(char *s);
static char *skip_non_eq_or_space(char *s);
static char *skip_nonquote(char *s);
static void parse_property(hal_prop_t *prop, char *str);

static void find_udi(hd_data_t *hd_data, hd_t *hd, int match);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * read hal data
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void hd_scan_hal(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_hal)) return;

  hd_data->module = mod_hal;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->hal = hd_free_hal_devices(hd_data->hal);

  PROGRESS(1, 0, "read hal data");

  read_hal(hd_data);

  if(!hd_data->hal) return;

  link_hal_tree(hd_data);

  PROGRESS(1, 0, "pci sysfs");

  hd_pci_read_data(hd_data);

  PROGRESS(2, 0, "pci devices");

  add_pci(hd_data);

}


void hd_scan_hal_basic(hd_data_t *hd_data)
{
  hd_data->module = mod_hal;

  /* some clean-up */
  hd_data->hal = hd_free_hal_devices(hd_data->hal);

  PROGRESS(1, 0, "read hal data");

  read_hal(hd_data);
}


void read_hal(hd_data_t *hd_data)
{
  DBusError error;
  DBusConnection *conn;
  LibHalContext *hal_ctx;
  LibHalPropertySet *props;
  LibHalPropertySetIterator it;
  char **device_names, **slist, *s;
  int i, num_devices, type;
  hal_device_t *dev;
  hal_prop_t *prop;

  dbus_error_init(&error);

  if(!(conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error))) {
    ADD2LOG("  hal: dbus_bus_get: %s: %s\n", error.name, error.message);
    return;
  }

  ADD2LOG("  hal: connected to: %s\n", dbus_bus_get_unique_name(conn));

  if(!(hal_ctx = libhal_ctx_new())) return;

  if(!libhal_ctx_set_dbus_connection(hal_ctx, conn)) return;

  if(!libhal_ctx_init(hal_ctx, &error)) {
    ADD2LOG("  hal: libhal_ctx_init: %s: %s\n", error.name, error.message);
    return;
  }


  dbus_error_init(&error);

  if((device_names = libhal_get_all_devices(hal_ctx, &num_devices, &error))) {
    ADD2LOG("----- hal device list -----\n");
    for(i = 0; i < num_devices; i++) {
      if(!(props = libhal_device_get_all_properties(hal_ctx, device_names[i], &error))) {
        ADD2LOG("  hal: %s: %s\n", error.name, error.message);
        dbus_error_init(&error);
        continue;
      }

      dev = new_mem(sizeof *dev);
      dev->udi = new_str(device_names[i]);
      dev->next = hd_data->hal;
      hd_data->hal = dev;

      ADD2LOG("  %d: udi = '%s'\n", i, dev->udi);

      for(libhal_psi_init(&it, props); libhal_psi_has_more(&it); libhal_psi_next(&it)) {
        type = libhal_psi_get_type(&it);

        prop = new_mem(sizeof *prop);
        prop->next = dev->prop;
        dev->prop = prop;

        switch(type) {
          case LIBHAL_PROPERTY_TYPE_STRING:
            prop->type = p_string;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.str = new_str(libhal_psi_get_string(&it));
            break;

          case LIBHAL_PROPERTY_TYPE_INT32:
            prop->type = p_int32;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.int32 = libhal_psi_get_int(&it);
            break;

          case LIBHAL_PROPERTY_TYPE_UINT64:
            prop->type = p_uint64;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.uint64 = libhal_psi_get_uint64(&it);
            break;

          case LIBHAL_PROPERTY_TYPE_DOUBLE:
            prop->type = p_double;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.d = libhal_psi_get_double(&it);
            break;

          case LIBHAL_PROPERTY_TYPE_BOOLEAN:
            prop->type = p_bool;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.b = libhal_psi_get_bool(&it);
            break;

          case LIBHAL_PROPERTY_TYPE_STRLIST:
            prop->type = p_list;
            prop->key = new_str(libhal_psi_get_key(&it));
            for(slist = libhal_psi_get_strlist(&it); *slist; slist++) {
              add_str_list(&prop->val.list, *slist);
            }
            break;

          default:
            prop->type = p_invalid;
        }

        if((s = hd_hal_print_prop(prop))) {
          ADD2LOG("  %s\n", s);
        }
      }

      libhal_free_property_set(props);
      if(i != num_devices - 1) ADD2LOG("\n");

    }

    ADD2LOG("----- hal device list end -----\n");

    libhal_free_string_array(device_names);

    dbus_error_free(&error);
  }
  else {
    ADD2LOG("  hal: empty device list\n");
  }


  libhal_ctx_shutdown(hal_ctx, &error);

  libhal_ctx_free(hal_ctx);

  dbus_connection_close(conn);
  dbus_connection_unref(conn);

  dbus_error_free(&error);
}


void link_hal_tree(hd_data_t *hd_data)
{
  hal_device_t *dev;
  hal_prop_t *prop;

  for(dev = hd_data->hal; dev; dev = dev->next) {
    prop = hal_get_str(dev->prop, "info.parent");
    if(prop) {
      dev->parent = hal_find_device(hd_data, prop->val.str);
    }
  }
}


hal_device_t *hal_find_device(hd_data_t *hd_data, char *udi)
{
  hal_device_t *dev;

  if(udi) {
    for(dev = hd_data->hal; dev; dev = dev->next) {
      if(!strcmp(dev->udi, udi)) return dev;
    }
  }

  return NULL;
}


void hal_invalidate(hal_prop_t *prop)
{
  if(prop->type == p_string) free_mem(prop->val.str);
  if(prop->type == p_list) free_str_list(prop->val.list);
  prop->type = p_invalid;
  memset(&prop->val, 0, sizeof prop->val);
}


void hal_invalidate_all(hal_prop_t *prop, const char *key)
{
  for(; (prop = hal_get_any(prop, key)); prop = prop->next) {
    hal_invalidate(prop);
  }
}


hal_prop_t *hal_get_any(hal_prop_t *prop, const char *key)
{
  for(; prop; prop = prop->next) {
    if(!strcmp(prop->key, key)) return prop;
  }

  return NULL;
}


hal_prop_t *hal_get_bool(hal_prop_t *prop, const char *key)
{
  for(; prop; prop = prop->next) {
    if(prop->type == p_bool && !strcmp(prop->key, key)) return prop;
  }

  return NULL;
}


hal_prop_t *hal_get_int32(hal_prop_t *prop, const char *key)
{
  for(; prop; prop = prop->next) {
    if(prop->type == p_int32 && !strcmp(prop->key, key)) return prop;
  }

  return NULL;
}


hal_prop_t *hal_get_str(hal_prop_t *prop, const char *key)
{
  for(; prop; prop = prop->next) {
    if(prop->type == p_string && !strcmp(prop->key, key)) return prop;
  }

  return NULL;
}


char *hal_get_useful_str(hal_prop_t *prop, const char *key)
{
  for(; prop; prop = prop->next) {
    if(prop->type == p_string && !strcmp(prop->key, key)) {
      if(prop->val.str && strncmp(prop->val.str, "Unknown", sizeof "Unknown" - 1)) return prop->val.str;
      break;
    }
  }

  return NULL;
}


int hal_match_str(hal_prop_t *prop, const char *key, const char *val)
{
  return val && (prop = hal_get_str(prop, key)) && !strcmp(prop->val.str, val);
}


hal_prop_t *hal_get_list(hal_prop_t *prop, const char *key)
{
  for(; prop; prop = prop->next) {
    if(prop->type == p_list && !strcmp(prop->key, key)) return prop;
  }

  return NULL;
}


void add_pci(hd_data_t *hd_data)
{
  hd_t *hd;
  hal_prop_t *prop;
  int i, j;
  char *s;
  hal_device_t *dev;
  pci_t *pci;

  for(dev = hd_data->hal ; dev; dev = dev->next) {
    if(dev->used) continue;
    if(!hal_match_str(dev->prop, "info.bus", "pci")) continue;
    dev->used = 1;

    hd = add_hd_entry(hd_data, __LINE__, 0);

    if((prop = hal_get_str(dev->prop, "linux.sysfs_path"))) hd->sysfs_id = new_str(hd_sysfs_id(prop->val.str));

    for(pci = hd_data->pci; pci; pci = pci->next) {
      if(!strcmp(hd_sysfs_id(pci->sysfs_id), hd->sysfs_id)) {
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_pci;
        hd->detail->pci.data = pci;

        break;
      }
    }

    hd_pci_complete_data(hd);

    hd->udi = new_str(dev->udi);
    if(dev->parent) hd->parent_udi = new_str(dev->parent->udi);

    if((prop = hal_get_int32(dev->prop, "pci.device_protocol"))) hd->prog_if.id = prop->val.int32;
    if((prop = hal_get_int32(dev->prop, "pci.device_subclass"))) hd->sub_class.id = prop->val.int32;
    if((prop = hal_get_int32(dev->prop, "pci.device_class"))) hd->base_class.id = prop->val.int32;

    i = (prop = hal_get_int32(dev->prop, "pci.vendor_id")) ? prop->val.int32 : 0;
    j = (prop = hal_get_int32(dev->prop, "pci.product_id")) ? prop->val.int32 : 0;

    if(i || j) {
      hd->vendor.id = MAKE_ID(TAG_PCI, i);
      hd->device.id = MAKE_ID(TAG_PCI, j);
    }

    if((s = hal_get_useful_str(dev->prop, "pci.vendor"))) hd->vendor.name = new_str(s);
    if((s = hal_get_useful_str(dev->prop, "pci.product"))) hd->device.name = new_str(s);

    i = (prop = hal_get_int32(dev->prop, "pci.subsys_vendor_id")) ? prop->val.int32 : 0;
    j = (prop = hal_get_int32(dev->prop, "pci.subsys_product_id")) ? prop->val.int32 : 0;

    if(i || j) {
      hd->sub_vendor.id = MAKE_ID(TAG_PCI, i);
      hd->sub_device.id = MAKE_ID(TAG_PCI, j);
    }

    if((s = hal_get_useful_str(dev->prop, "pci.subsys_vendor"))) hd->sub_vendor.name = new_str(s);
    if((s = hal_get_useful_str(dev->prop, "pci.subsys_product"))) hd->sub_device.name = new_str(s);

    if((prop = hal_get_str(dev->prop, "linux.sysfs_path"))) hd->sysfs_id = new_str(hd_sysfs_id(prop->val.str));

    if((prop = hal_get_str(dev->prop, "info.linux.driver"))) add_str_list(&hd->drivers, prop->val.str);

    hd->hal_prop = dev->prop;
    dev->prop = NULL;
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      !hd->detail ||
      hd->detail->type != hd_detail_pci ||
      !(pci = hd->detail->pci.data)
    ) continue;

    pci->next = NULL;
  }

  hd_data->pci = NULL;
}


char *hd_hal_print_prop(hal_prop_t *prop)
{
  static char *s = NULL;
  str_list_t *sl;

  switch(prop->type) {
    case p_string:
      str_printf(&s, 0, "%s = '%s'", prop->key, prop->val.str);
      break;

    case p_int32:
      str_printf(&s, 0, "%s = %d (0x%x)", prop->key, prop->val.int32, prop->val.int32);
      break;

    case p_uint64:
      str_printf(&s, 0, "%s = %"PRIu64"ull (0x%"PRIx64"ull)", prop->key, prop->val.uint64, prop->val.uint64);
      break;

    case p_double:
      str_printf(&s, 0, "%s = %#g", prop->key, prop->val.d);
      break;

    case p_bool:
      str_printf(&s, 0, "%s = %s", prop->key, prop->val.b ? "true" : "false");
      break;

    case p_list:
      str_printf(&s, 0, "%s = { ", prop->key);
      for(sl = prop->val.list; sl; sl = sl->next) {
        str_printf(&s, -1, "'%s'%s", sl->str, sl->next ? ", " : "");
      }
      str_printf(&s, -1, " }");
      break;

    case p_invalid:
      str_printf(&s, 0, "%s", prop->key);
      break;
  }

  return s;
}


/*
 * Ensure that udi is a sane path name.
 *
 * return:
 *   0/1: fail/ok
 */
int check_udi(const char *udi)
{
  if(
    !udi ||
    !strncmp(udi, "../", sizeof "../" - 1) ||
    strstr(udi, "/../") ||
    strstr(udi, "//")
  ) return 0;

  return 1;
}


int hd_write_properties(const char *udi, hal_prop_t *prop)
{
  FILE *f;
  char *s;

  f = hd_open_properties(udi, "w");

  if(!f) return 1;

  for(; prop; prop = prop->next) {
    if(prop->type == p_invalid) continue;
    s = hd_hal_print_prop(prop);
    if(s) fprintf(f, "%s\n", s);
  }

  fclose(f);

  return 0;
}


hal_prop_t *hd_read_properties(const char *udi)
{
  char *path = NULL;
  str_list_t *sl0, *sl;
  hal_prop_t *prop_list = NULL, *prop_list_e = NULL, prop, *p;

  if(!udi) return NULL;

  while(*udi == '/') udi++;

  if(!check_udi(udi)) return NULL;

  str_printf(&path, 0, "%s/%s", hd_get_hddb_path("udi"), udi);

  sl0 = read_file(path, 0, 0);

  free_mem(path);

  for(sl = sl0; sl; sl = sl->next) {
    parse_property(&prop, sl->str);
    if(prop.type != p_invalid) {
      p = new_mem(sizeof *p);
      *p = prop;
      if(prop_list) {
        prop_list_e->next = p;
        prop_list_e = prop_list_e->next;
      }
      else {
        prop_list = prop_list_e = p;
      }
    }
    else {
      prop.key = free_mem(prop.key);
    }
  }

  free_str_list(sl0);

  return prop_list;
}


FILE *hd_open_properties(const char *udi, const char *mode)
{
  str_list_t *path, *sl;
  struct stat sbuf;
  char *dir = NULL;
  int err, i;
  FILE *f = NULL;

  if(!udi) return f;
  while(*udi == '/') udi++;

  if(!check_udi(udi)) return f;

  path = hd_split('/', udi);

  if(!path) return f;

  dir = new_str(hd_get_hddb_path("udi"));

  for(err = 0, sl = path; sl->next; sl = sl->next) {
    str_printf(&dir, -1, "/%s", sl->str);
    i = lstat(dir, &sbuf);
    if(i == -1 && errno == ENOENT) {
      mkdir(dir, 0755);
      i = lstat(dir, &sbuf);
    }
    if(i || !S_ISDIR(sbuf.st_mode)) {
      err = 1;
      break;
    }
  }

  if(!err) {
    str_printf(&dir, -1, "/%s", sl->str);
    f = fopen(dir, mode);
  }

  free_mem(dir);

  return f;
}


char *skip_space(char *s)
{
  while(isspace(*s)) s++;

  return s;
}


char *skip_non_eq_or_space(char *s)
{
  while(*s && *s != '=' && !isspace(*s)) s++;

  return s;
}


char *skip_nonquote(char *s)
{
  while(*s && *s != '\'') s++;

  return s;
}


void parse_property(hal_prop_t *prop, char *str)
{
  char *s, *s1, *key, *s_val;
  int l;

  memset(prop, 0, sizeof *prop);
  prop->type = p_invalid;

  s = skip_space(str);
  s = skip_non_eq_or_space(key = s);

  *s++ = 0;
  if(!*key) return;

  s = skip_space(s);
  if(*s == '=') s++;
  s = skip_space(s);

  prop->key = new_str(key);

  if(!*s) return;

  if(*s == '\'') {
    s_val = s + 1;
    s = strrchr(s_val, '\'');
    *(s ?: s_val) = 0;
    prop->type = p_string;
    prop->val.str = strdup(s_val);
  }
  else if(*s == '{') {
    s_val = skip_space(s + 1);
    s1 = strrchr(s_val, '}');
    if(s1) *s1 = 0;
    prop->type = p_list;
    while(*s_val++ == '\'') {
      s = skip_nonquote(s_val);
      if(*s) *s++ = 0;
      add_str_list(&prop->val.list, s_val);
      s_val = skip_nonquote(s);
    }
  }
  else if(!strncmp(s, "true", 4)) {
    s += 4;
    prop->type = p_bool;
    prop->val.b = 1;
  }
  else if(!strncmp(s, "false", 5)) {
    s += 5;
    prop->type = p_bool;
    prop->val.b = 0;
  }
  else if(isdigit(*s) || *s == '+' || *s == '-' || *s == '.') {
    *skip_non_eq_or_space(s) = 0;
    if(strchr(s, '.')) {
      prop->type = p_double;
      prop->val.d = strtod(s, NULL);
    }
    else {
      l = strlen(s);
      if(l >= 2 && s[l - 2] == 'l' && s[l - 1] == 'l') {
        prop->type = p_uint64;
        s[l -= 2] = 0;
      }
      else {
        prop->type = p_int32;
      }
      if(l >= 1 && s[l - 1] == 'u') s[--l] = 0;

      if(prop->type == p_int32) {
        prop->val.int32 = strtol(s, NULL, 0);
      }
      else {
        prop->val.uint64 = strtoull(s, NULL, 0);
      }
    }
  }
}


void hd_scan_hal_assign_udi(hd_data_t *hd_data)
{
  hd_t *hd;
  int i;

  if(!hd_data->hal) return;

  PROGRESS(2, 0, "assign udi");

  for(i = 0; i < 3; i++) {
    for(hd = hd_data->hd; hd; hd = hd->next) find_udi(hd_data, hd, i);
  }
}


void find_udi(hd_data_t *hd_data, hd_t *hd, int match)
{
  hal_device_t *dev;
  char *h_sysfsid, *h_devname;

  if(hd->udi) return;

  dev = NULL;

  /* device file first, thanks to usb devices */

  /* based on device file */
  if(
    !dev &&
    (
      (match == 0 && hd->unix_dev_name) ||
      (match == 1 && hd->unix_dev_name2) ||
      (match == 2 && hd->unix_dev_names)
    )
  ) for(dev = hd_data->hal; dev; dev = dev->next) {
    h_devname = hal_get_useful_str(dev->prop, "linux.device_file");
    if(!h_devname) h_devname = hal_get_useful_str(dev->prop, "block.device");
    if(h_devname) {
      if(match == 0 && hd->unix_dev_name && !strcmp(hd->unix_dev_name, h_devname)) break;
      if(match == 1 && hd->unix_dev_name2 && !strcmp(hd->unix_dev_name2, h_devname)) break;
      if(match == 2 && search_str_list(hd->unix_dev_names, h_devname)) break;
    }
  }

  /* based on sysfs id, only once for match == 0 */
  if(!dev && !match && hd->sysfs_id) for(dev = hd_data->hal; dev; dev = dev->next) {
    h_sysfsid = hd_sysfs_id(hal_get_useful_str(dev->prop, "linux.sysfs_path"));
    if(h_sysfsid && !strcmp(hd->sysfs_id, h_sysfsid)) break;
  }

  if(dev) {
    hd->udi = new_str(dev->udi);
    hd->hal_prop = dev->prop;
    dev->prop = NULL;
  }
}

/** @} */


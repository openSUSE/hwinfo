#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

static void read_hal(hd_data_t *hd_data);
static void add_pci(hd_data_t *hd_data);
static void link_hal_tree(hd_data_t *hd_data);
static hal_device_t *hal_find_device(hd_data_t *hd_data, char *udi);

static hal_prop_t *hal_get_int32(hal_prop_t *prop, char *key);
static hal_prop_t *hal_get_str(hal_prop_t *prop, char *key);
static char *hal_get_useful_str(hal_prop_t *prop, char *key);
static int hal_match_str(hal_prop_t *prop, char *key, char *val);


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


void read_hal(hd_data_t *hd_data)
{
  DBusError error;
  DBusConnection *conn;
  LibHalContext *hal_ctx;
  LibHalPropertySet *props;
  LibHalPropertySetIterator it;
  char **device_names, **slist;
  int i, num_devices, type;
  hal_device_t *dev;
  hal_prop_t *prop;
  str_list_t *sl;

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
            ADD2LOG("  %s = '%s' (string)\n", prop->key, prop->val.str);
            break;

          case LIBHAL_PROPERTY_TYPE_INT32:
            prop->type = p_int32;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.int32 = libhal_psi_get_int(&it);
            ADD2LOG("  %s = %d (0x%x) (int)\n", prop->key, prop->val.int32, prop->val.int32);
            break;

          case LIBHAL_PROPERTY_TYPE_UINT64:
            prop->type = p_uint64;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.uint64 = libhal_psi_get_uint64(&it);
            ADD2LOG("  %s = %"PRId64" (0x%"PRIx64") (uint64)\n", prop->key, prop->val.uint64, prop->val.uint64);
            break;

          case LIBHAL_PROPERTY_TYPE_DOUBLE:
            prop->type = p_double;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.d = libhal_psi_get_double(&it);
            ADD2LOG("  %s = %g (double)\n", prop->key, prop->val.d);
            break;

          case LIBHAL_PROPERTY_TYPE_BOOLEAN:
            prop->type = p_bool;
            prop->key = new_str(libhal_psi_get_key(&it));
            prop->val.b = libhal_psi_get_bool(&it);
            ADD2LOG("  %s = %s (bool)\n", prop->key, prop->val.b ? "true" : "false");
            break;

          case LIBHAL_PROPERTY_TYPE_STRLIST:
            prop->type = p_list;
            prop->key = new_str(libhal_psi_get_key(&it));
            for(slist = libhal_psi_get_strlist(&it); *slist; slist++) {
              add_str_list(&prop->val.list, *slist);
            }
            ADD2LOG("  %s = { ", prop->key);
            for(sl = prop->val.list; sl; sl = sl->next) {
              ADD2LOG("'%s'%s", sl->str, sl->next ? ", " : "");
            }
            ADD2LOG(" } (string list)\n");
            break;

          default:
            prop->type = p_invalid;
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

  dbus_connection_disconnect(conn);
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


hal_prop_t *hal_get_int32(hal_prop_t *prop, char *key)
{
  for(; prop; prop = prop->next) {
    if(prop->type == p_int32 && !strcmp(prop->key, key)) return prop;
  }

  return NULL;
}


hal_prop_t *hal_get_str(hal_prop_t *prop, char *key)
{
  for(; prop; prop = prop->next) {
    if(prop->type == p_string && !strcmp(prop->key, key)) return prop;
  }

  return NULL;
}


char *hal_get_useful_str(hal_prop_t *prop, char *key)
{
  for(; prop; prop = prop->next) {
    if(prop->type == p_string && !strcmp(prop->key, key)) {
      if(strncmp(prop->val.str, "Unknown", sizeof "Unknown" - 1)) return prop->val.str;
      break;
    }
  }

  return NULL;
}


int hal_match_str(hal_prop_t *prop, char *key, char *val)
{
  return val && (prop = hal_get_str(prop, key)) && !strcmp(prop->val.str, val);
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



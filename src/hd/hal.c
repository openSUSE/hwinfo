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
            prop->val.bool = libhal_psi_get_bool(&it);
            ADD2LOG("  %s = %s (bool)\n", prop->key, prop->val.bool ? "true" : "false");
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



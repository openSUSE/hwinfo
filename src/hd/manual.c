#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "manual.h"
#include "hddb.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * hardware in /var/lib/hardware/
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#include <hwclass_names.h>

/* corresponds to hd_status_value_t */
static hash_t status_names[] = {
  { status_no,      "no"      },
  { status_yes,     "yes"     },
  { status_unknown, "unknown" },
  { status_new,     "new"     },
  { 0,              NULL      }
};

typedef enum {
  hw_id_unique = 1, hw_id_parent, hw_id_child, hw_id_hwclass, hw_id_model,
  hw_id_configured, hw_id_available, hw_id_needed, hw_id_cfgstring, hw_id_active
} hw_id_t;

#ifndef LIBHD_TINY

#define MAN_SECT_GENERAL	"General"
#define MAN_SECT_STATUS		"Status"
#define MAN_SECT_HARDWARE	"Hardware"

/* structure elements from hd_t */
typedef enum {
  hwdi_bus = 1, hwdi_slot, hwdi_func, hwdi_base_class, hwdi_sub_class,
  hwdi_prog_if, hwdi_dev, hwdi_vend, hwdi_sub_dev, hwdi_sub_vend, hwdi_rev,
  hwdi_compat_dev, hwdi_compat_vend, hwdi_dev_name, hwdi_vend_name,
  hwdi_sub_dev_name, hwdi_sub_vend_name, hwdi_rev_name, hwdi_serial,
  hwdi_unix_dev_name, hwdi_unix_dev_name2, hwdi_unix_dev_names, hwdi_rom_id,
  hwdi_broken, hwdi_usb_guid, hwdi_res_mem, hwdi_res_phys_mem, hwdi_res_io,
  hwdi_res_irq, hwdi_res_dma, hwdi_res_size, hwdi_res_baud, hwdi_res_cache,
  hwdi_res_disk_geo, hwdi_res_monitor, hwdi_res_framebuffer, hwdi_features,
  hwdi_hotplug, hwdi_class_list, hwdi_drivers, hwdi_sysfs_id,
  hwdi_sysfs_busid, hwdi_sysfs_link
} hw_hd_items_t;

#if 0
static hash_t hw_ids_hd_items[] = {
  { hwdi_bus,             "Bus"              },
  { hwdi_slot,            "Slot"             },
  { hwdi_func,            "Function"         },
  { hwdi_base_class,      "BaseClass"        },
  { hwdi_sub_class,       "SubClass"         },
  { hwdi_prog_if,         "ProgIF"           },
  { hwdi_dev,             "DeviceID"         },
  { hwdi_vend,            "VendorID"         },
  { hwdi_sub_dev,         "SubDeviceID"      },
  { hwdi_sub_vend,        "SubVendorID"      },
  { hwdi_rev,             "RevisionID"       },
  { hwdi_compat_dev,      "CompatDeviceID"   },
  { hwdi_compat_vend,     "CompatVendorID"   },
  { hwdi_dev_name,        "DeviceName"       },
  { hwdi_vend_name,       "VendorName"       },
  { hwdi_sub_dev_name,    "SubDeviceName"    },
  { hwdi_sub_vend_name,   "SubVendorName"    },
  { hwdi_rev_name,        "RevisionName"     },
  { hwdi_serial,          "Serial"           },
  { hwdi_unix_dev_name,   "UnixDevice"       },
  { hwdi_unix_dev_name2,  "UnixDeviceAlt"    },
  { hwdi_unix_dev_names,  "UnixDeviceList"   },
  { hwdi_rom_id,          "ROMID"            },
  { hwdi_broken,          "Broken"           },
  { hwdi_usb_guid,        "USBGUID"          },
  { hwdi_res_phys_mem,    "Res.PhysMemory"   },
  { hwdi_res_mem,         "Res.Memory"       },
  { hwdi_res_io,          "Res.IO"           },
  { hwdi_res_irq,         "Res.Interrupts"   },
  { hwdi_res_dma,         "Res.DMA"          },
  { hwdi_res_size,        "Res.Size"         },
  { hwdi_res_baud,        "Res.Baud"         },
  { hwdi_res_cache,       "Res.Cache"        },
  { hwdi_res_disk_geo,    "Res.DiskGeometry" },
  { hwdi_res_monitor,     "Res.Monitor"      },
  { hwdi_res_framebuffer, "Res.Framebuffer"  },
  { hwdi_features,        "Features"         },
  { hwdi_hotplug,         "Hotplug"          },
  { hwdi_class_list,      "HWClassList"      },
  { hwdi_drivers,         "Drivers"          },
  { hwdi_sysfs_id,        "SysfsID"          },
  { hwdi_sysfs_busid,     "SysfsBusID"       },
  { hwdi_sysfs_link,      "SysfsLink"        },
  { 0,                    NULL               }
};
#endif

#endif	/* LIBHD_TINY */

#ifndef LIBHD_TINY

// static unsigned str2id(char *str);

static hal_prop_t *hal_get_new(hal_prop_t **list, const char *key);
static void hd2prop_add_int32(hal_prop_t **list, const char *key, int32_t i);
static void hd2prop_add_str(hal_prop_t **list, const char *key, const char *str);
static void hd2prop_add_list(hal_prop_t **list, const char *key, str_list_t *sl);


void hd_scan_manual(hd_data_t *hd_data)
{
  DIR *dir;
  struct dirent *de;
  int i;
  hd_t *hd, *hd1, *next, *hdm, **next2;

  if(!hd_probe_feature(hd_data, pr_manual)) return;

  hd_data->module = mod_manual;

  /* some clean-up */
  remove_hd_entries(hd_data);

  for(hd = hd_data->manual; hd; hd = next) {
    next = hd->next; 
    hd->next = NULL;
    hd_free_hd_list(hd);
  }
  hd_data->manual = NULL;

  next2 = &hd_data->manual;

  if((dir = opendir(HARDWARE_UDI))) {
    i = 0;
    while((de = readdir(dir))) {
      if(*de->d_name == '.') continue;
      PROGRESS(1, ++i, "read");
      if((hd = hd_read_config(hd_data, de->d_name))) {
        ADD2LOG("  got %s\n", hd->unique_id);
        *next2 = hd;
        next2 = &hd->next;
      }
    }
    closedir(dir);
  }

  // FIXME!
  if((dir = opendir(HARDWARE_UNIQUE_KEYS))) {
    i = 0;
    while((de = readdir(dir))) {
      if(*de->d_name == '.') continue;
      PROGRESS(2, ++i, "read");
      // evil kludge
      for(hd = hd_data->manual; hd; hd = hd->next) {
        if(hd->unique_id && !strcmp(hd->unique_id, de->d_name)) break;
      }
      if(hd) continue;
      if((hd = hd_read_config(hd_data, de->d_name))) {
        ADD2LOG("  got %s\n", hd->unique_id);
        *next2 = hd;
        next2 = &hd->next;
      }
    }
    closedir(dir);
  }

  /* initialize some useful status value */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      !hd->status.configured &&
      !hd->status.available &&
      !hd->status.needed &&
      !hd->status.active &&
      !hd->status.invalid
    ) {
      hd->status.configured = status_new;
      hd->status.available = hd->module == mod_manual ? status_unknown : status_yes;
      hd->status.needed = status_no;
      hd->status.active = status_unknown;
    }
  }

  hd_data->flags.keep_kmods = 1;
  for(hdm = hd_data->manual; hdm; hdm = next) {
    next = hdm->next;

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->unique_id && hdm->unique_id && !strcmp(hd->unique_id, hdm->unique_id)) break;
    }

    if(hd) {
      /* just update config status */
      hd->status = hdm->status;
      hd->status.available = status_yes;

      hd->config_string = new_str(hdm->config_string);

      hd->persistent_prop = hdm->persistent_prop;
      hdm->persistent_prop = NULL;
    }
    else {
      /* add new entry */
      hd = add_hd_entry(hd_data, __LINE__, 0);
      *hd = *hdm;
      hd->next = NULL;
      hd->tag.freeit = 0;

      hdm->tag.remove = 1;

      if(hd->status.available != status_unknown) hd->status.available = status_no;

      // FIXME: do it really here?
      if(hd->parent_id) {
        for(hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
          if(hd1->unique_id && !strcmp(hd1->unique_id, hd->parent_id)) {
            hd->attached_to = hd1->idx;
            break;
          }
        }
      }
    }
  }
  hd_data->flags.keep_kmods = 0;

  for(hd = hd_data->manual; hd; hd = next) {
    next = hd->next;
    hd->next = NULL;
    if(!hd->tag.remove) {
      hd_free_hd_list(hd);
    }
    else {
      free_mem(hd);
    }
  }
  hd_data->manual = NULL;


}


void hd_scan_manual2(hd_data_t *hd_data)
{
  hd_t *hd, *hd1;

  /* check if it's necessary to reconfigure this hardware */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    hd->status.reconfig = status_no;

    if(hd->status.needed != status_yes) continue;

    if(hd->status.available == status_no) {
      hd->status.reconfig = status_yes;
      continue;
    }

    if(hd->status.available != status_unknown) continue;

    for(hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
      if(hd1 == hd) continue;

      if(
        hd1->hw_class == hd->hw_class &&
        hd1->status.configured == status_new &&
        hd1->status.available == status_yes
      ) break;
    }

    if(hd1) hd->status.reconfig = status_yes;
  }
}

#endif

#ifndef LIBHD_TINY

char *hd_status_value_name(hd_status_value_t status)
{
  return key2value(status_names, status);
}


/*
 * read an entry - obsolete
 */
hd_manual_t *hd_manual_read_entry(hd_data_t *hd_data, const char *id)
{
  return NULL;
}


/*
 * read an entry
 */
hal_prop_t *hd_manual_read_entry_old(hd_data_t *hd_data, const char *id)
{
  char path[PATH_MAX];
  int line;
  str_list_t *sl, *sl0;
  char *s, *s1, *s2;
  hal_prop_t *prop_list = NULL, *prop = NULL;

  snprintf(path, sizeof path, "%s/%s", HARDWARE_UNIQUE_KEYS, id);

  if(!(sl0 = read_file(path, 0, 0))) return prop_list;

  for(line = 1, sl = sl0; sl; sl = sl->next, line++) {
    s = sl->str;
    while(isspace(*s)) s++;
    if(!*s || *s == '#' || *s == ';') continue;	/* empty lines & comments */

    s2 = s;
    s1 = strsep(&s2, "=");

    if(!s2 && *s == '[') continue;

    if(!s2) {
      ADD2LOG("  %s: invalid line %d\n", id, line);
      break;
    }

    if(s1) {
      if(prop) {
        prop->next = new_mem(sizeof *prop);
        prop = prop->next;
      }
      else {
        prop_list = prop = new_mem(sizeof *prop);
      }

      prop->type = p_string;
      for(s = s1; *s; s++) if(*s >= 'A' && *s <= 'Z') *s += 'a' - 'A';
      str_printf(&prop->key, 0, "hwinfo.%s", s1);
      prop->val.str = canon_str(s2, strlen(s2));
    }
  }

  free_str_list(sl0);

  return prop_list;
}


/*
 * write an entry
 */

int hd_manual_write_entry(hd_data_t *hd_data, hd_manual_t *entry)
{
#if 1

  return 0;

#else

  /* remove old file */
  if(!error) {
    snprintf(path, sizeof path, "%s/%s", HARDWARE_DIR, entry->unique_id);
    unlink(path);
  }

#endif
}


#if 0
unsigned str2id(char *str)
{
  unsigned id;
  unsigned tag = 0;

  if(strlen(str) == 3) return name2eisa_id(str);

  switch(*str) {
    case 'p':
      tag = TAG_PCI; str++; break;

    case 'r':
      str++; break;

    case 'u':
      tag = TAG_USB; str++; break;

    case 's':
      tag = TAG_SPECIAL; str++; break;

    case 'P':
      tag = TAG_PCMCIA; str++; break;

  }

  id = strtoul(str, &str, 16);
  if(*str) return 0;

  return MAKE_ID(tag, ID_VALUE(id));
}
#endif

void prop2hd(hd_data_t *hd_data, hd_t *hd)
{
  hal_prop_t *prop, *list;

  list = hd->persistent_prop;

  if((prop = hal_get_str(list, "hwinfo.uniqueid"))) {
    hd->unique_id = new_str(prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.parentid"))) {
    hd->parent_id = new_str(prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.childids"))) {
    hd->child_ids = hd_split(',', prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.model"))) {
    hd->model = new_str(prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.hwclass"))) {
    hd->hw_class = value2key(hw_items, prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.configstring"))) {
    hd->config_string = new_str(prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.configured"))) {
    hd->status.configured = value2key(status_names, prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.available"))) {
    hd->status.available_orig =
    hd->status.available = value2key(status_names, prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.needed"))) {
    hd->status.needed = value2key(status_names, prop->val.str);
  }

  if((prop = hal_get_str(list, "hwinfo.active"))) {
    hd->status.active = value2key(status_names, prop->val.str);
  }

  /*
   * if the status info is completely missing, fake some:
   * new hardware, not autodetectable, not needed
   */
  if(
    !hd->status.configured &&
    !hd->status.available &&
    !hd->status.needed &&
    !hd->status.invalid
  ) {
    hd->status.configured = status_new;
    hd->status.available = status_unknown;
    hd->status.needed = status_no;
  }
  if(!hd->status.active) hd->status.active = status_unknown;

}


hal_prop_t *hal_get_new(hal_prop_t **list, const char *key)
{
  hal_prop_t *prop;

  prop = hal_get_any(*list, key);
  if(!prop) {
    prop = new_mem(sizeof *prop);
    prop->next = *list;
    *list = prop;
    prop->key = new_str(key);
  }
  else {
    hal_invalidate_all(prop, key);
  }

  return prop;
}


void hd2prop_add_int32(hal_prop_t **list, const char *key, int32_t i)
{
  hal_prop_t *prop;

  if(i) {
    prop = hal_get_new(list, key);
    prop->type = p_int32;
    prop->val.int32 = i;
  }
  else {
    hal_invalidate_all(*list, key);
  }
}


void hd2prop_add_str(hal_prop_t **list, const char *key, const char *str)
{
  hal_prop_t *prop;

  if(str) {
    prop = hal_get_new(list, key);
    prop->type = p_string;
    prop->val.str = new_str(str);
  }
  else {
    hal_invalidate_all(*list, key);
  }
}


void hd2prop_add_list(hal_prop_t **list, const char *key, str_list_t *sl)
{
  hal_prop_t *prop;

  if(sl) {
    prop = hal_get_new(list, key);
    prop->type = p_list;
    for(; sl; sl = sl->next) {
      add_str_list(&prop->val.list, sl->str);
    }
  }
  else {
    hal_invalidate_all(*list, key);
  }
}


void hd2prop(hd_data_t *hd_data, hd_t *hd)
{
  hal_prop_t **list;
  char *s = NULL, *key;
  unsigned u;
  str_list_t *sl;
  hd_res_t *res;

  list = &hd->persistent_prop;

  hd2prop_add_str(list, "hwinfo.uniqueid", hd->unique_id);
  hd2prop_add_str(list, "hwinfo.parentid", hd->parent_id);

  hd2prop_add_list(list, "hwinfo.childids", hd->child_ids);

  hd2prop_add_str(list, "hwinfo.model", hd->model);
  hd2prop_add_str(list, "hwinfo.configstring", hd->config_string);
  hd2prop_add_str(list, "hwinfo.hwclass", key2value(hw_items, hd->hw_class));
  hd2prop_add_str(list, "hwinfo.configured", key2value(status_names, hd->status.configured));
  hd2prop_add_str(list, "hwinfo.available", key2value(status_names, hd->status.available));
  hd2prop_add_str(list, "hwinfo.needed", key2value(status_names, hd->status.needed));
  hd2prop_add_str(list, "hwinfo.active", key2value(status_names, hd->status.active));

  hd2prop_add_int32(list, "hwinfo.broken", hd->broken);
  hd2prop_add_int32(list, "hwinfo.bus", hd->bus.id);
  hd2prop_add_int32(list, "hwinfo.slot", hd->slot);

  hd2prop_add_int32(list, "hwinfo.func", hd->func);
  hd2prop_add_int32(list, "hwinfo.baseclass", hd->base_class.id);
  hd2prop_add_int32(list, "hwinfo.subclass", hd->sub_class.id);
  hd2prop_add_int32(list, "hwinfo.progif", hd->prog_if.id);

  hd2prop_add_int32(list, "hwinfo.revisionid", hd->revision.id);
  hd2prop_add_str(list, "hwinfo.revisionname", hd->revision.name);

  hd2prop_add_int32(list, "hwinfo.vendorid", hd->vendor.id);
  hd2prop_add_str(list, "hwinfo.vendorname", hd->vendor.name);

  hd2prop_add_int32(list, "hwinfo.deviceid", hd->device.id);
  hd2prop_add_str(list, "hwinfo.devicename", hd->device.name);

  hd2prop_add_int32(list, "hwinfo.subvendorid", hd->sub_vendor.id);
  hd2prop_add_str(list, "hwinfo.subvendorname", hd->sub_vendor.name);

  hd2prop_add_int32(list, "hwinfo.subdeviceid", hd->sub_device.id);
  hd2prop_add_str(list, "hwinfo.subdevicename", hd->sub_device.name);

  hd2prop_add_int32(list, "hwinfo.compatvendorid", hd->compat_vendor.id);
  hd2prop_add_int32(list, "hwinfo.compatdeviceid", hd->compat_device.id);

  hd2prop_add_str(list, "hwinfo.serial", hd->serial);
  hd2prop_add_str(list, "hwinfo.unixdevice", hd->unix_dev_name);
  hd2prop_add_str(list, "hwinfo.unixdevicealt", hd->unix_dev_name2);

  hd2prop_add_list(list, "hwinfo.unixdevicelist", hd->unix_dev_names);
  hd2prop_add_list(list, "hwinfo.drivers", hd->drivers);

  hd2prop_add_str(list, "hwinfo.sysfsid", hd->sysfs_id);
  hd2prop_add_str(list, "hwinfo.sysfsbusid", hd->sysfs_bus_id);
  hd2prop_add_str(list, "hwinfo.sysfslink", hd->sysfs_device_link);
  hd2prop_add_str(list, "hwinfo.romid", hd->rom_id);
  hd2prop_add_str(list, "hwinfo.usbguid", hd->usb_guid);
  hd2prop_add_int32(list, "hwinfo.hotplug", hd->hotplug);

  for(u = 0; u < sizeof hd->hw_class_list / sizeof *hd->hw_class_list; u++) {
    str_printf(&s, -1, "%02x", hd->hw_class_list[u]);
  }
  hd2prop_add_str(list, "hwinfo.hwclasslist", s);
  s = free_mem(s);

  u = 0;
  if(hd->is.agp)          u |= 1 << 0;
  if(hd->is.isapnp)       u |= 1 << 1;
  if(hd->is.softraiddisk) u |= 1 << 2;
  if(hd->is.zip)          u |= 1 << 3;
  if(hd->is.cdr)          u |= 1 << 4;
  if(hd->is.cdrw)         u |= 1 << 5;
  if(hd->is.dvd)          u |= 1 << 6;
  if(hd->is.dvdr)         u |= 1 << 7;
  if(hd->is.dvdram)       u |= 1 << 8;
  if(hd->is.pppoe)        u |= 1 << 9;
  if(hd->is.wlan)         u |= 1 << 10;

  hd2prop_add_int32(list, "hwinfo.features", u);
  
  for(res = hd->res; res; res = res->next) {
    sl = NULL;
    key = NULL;
    switch(res->any.type) {
      case res_mem:
        key = "hwinfo.res.memory";
        str_printf(&s, 0, "0x%"PRIx64"", res->mem.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%"PRIx64"", res->mem.range);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.enabled);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.access);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.prefetch);
        add_str_list(&sl, s);
        break;

      case res_phys_mem:
        key = "hwinfo.res.physmemory";
        str_printf(&s, 0, "0x%"PRIx64"", res->phys_mem.range);
        add_str_list(&sl, s);
        break;

      case res_io:
        key = "hwinfo.res.io";
        str_printf(&s, 0, "0x%"PRIx64"", res->io.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%"PRIx64"", res->io.range);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->io.enabled);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->io.access);
        add_str_list(&sl, s);
        break;

      case res_irq:
        key = "hwinfo.res.interrupts";
        str_printf(&s, 0, "%u", res->irq.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->irq.triggered);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->irq.enabled);
        add_str_list(&sl, s);
        break;

      case res_dma:
        key = "hwinfo.res.dma";
        str_printf(&s, 0, "%u", res->dma.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->dma.enabled);
        add_str_list(&sl, s);
        break;

      case res_size:
        key = "hwinfo.res.size";
        str_printf(&s, 0, "%u", res->size.unit);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%"PRIu64, res->size.val1);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%"PRIu64, res->size.val2);
        add_str_list(&sl, s);
        break;

      case res_baud:
        key = "hwinfo.res.baud";
        str_printf(&s, 0, "%u", res->baud.speed);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->baud.bits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->baud.stopbits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%02x", (unsigned) res->baud.parity);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%02x", (unsigned) res->baud.handshake);
        add_str_list(&sl, s);
        break;

      case res_cache:
        key = "hwinfo.res.cache";
        str_printf(&s, 0, "%u", res->cache.size);
        add_str_list(&sl, s);
        break;

      case res_disk_geo:
        key = "hwinfo.res.diskgeometry";
        str_printf(&s, 0, "%u", res->disk_geo.cyls);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.heads);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.sectors);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.geotype);
        add_str_list(&sl, s);
        break;

      case res_monitor:
        key = "hwinfo.res.monitor";
        str_printf(&s, 0, "%u", res->monitor.width);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.height);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.vfreq);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.interlaced);
        add_str_list(&sl, s);
        break;

      case res_framebuffer:
        key = "hwinfo.res.framebuffer";
        str_printf(&s, 0, "%u", res->framebuffer.width);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.height);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.bytes_p_line);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.colorbits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.mode);
        add_str_list(&sl, s);
        break;

      default:
        break;
    }

    if(key) hd2prop_add_list(list, key, sl);

    free_str_list(sl);
  }

}


#if 0
/*
 * move info from hd_manual_t to hd_t
 */
void manual2hd(hd_data_t *hd_data, hd_manual_t *entry, hd_t *hd)
{
  str_list_t *sl1, *sl2;
  hw_hd_items_t item;
  unsigned tag, u0, u1, u2, u3, u4;
  hd_res_t *res;
  uint64_t u64_0, u64_1;
  char *s;
  int i;

  if(!hd || !entry) return;

  hd->unique_id = new_str(entry->unique_id);
  hd->parent_id = new_str(entry->parent_id);
  hd->child_ids = hd_split(',', entry->child_ids);
  hd->model = new_str(entry->model);
  hd->hw_class = entry->hw_class;

  hd->config_string = new_str(entry->config_string);

  hd->status = entry->status;

  for(
    sl1 = entry->key, sl2 = entry->value;
    sl1 && sl2;
    sl1 = sl1->next, sl2 = sl2->next
  ) {
    switch(item = value2key(hw_ids_hd_items, sl1->str)) {
      case hwdi_bus:
        hd->bus.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_slot:
        hd->slot = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_func:
        hd->func = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_base_class:
        hd->base_class.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_sub_class:
        hd->sub_class.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_prog_if:
        hd->prog_if.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_dev:
        hd->device.id = str2id(sl2->str);
        break;

      case hwdi_vend:
        hd->vendor.id = str2id(sl2->str);
        break;

      case hwdi_sub_dev:
        hd->sub_device.id = str2id(sl2->str);
        break;

      case hwdi_sub_vend:
        hd->sub_vendor.id = str2id(sl2->str);
        break;

      case hwdi_rev:
        hd->revision.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_compat_dev:
        hd->compat_device.id = str2id(sl2->str);
        break;

      case hwdi_compat_vend:
        hd->compat_vendor.id = str2id(sl2->str);
        break;

      case hwdi_dev_name:
        hd->device.name = new_str(sl2->str);
        break;

      case hwdi_vend_name:
        hd->vendor.name = new_str(sl2->str);
        break;

      case hwdi_sub_dev_name:
        hd->sub_device.name = new_str(sl2->str);
        break;

      case hwdi_sub_vend_name:
        hd->sub_vendor.name = new_str(sl2->str);
        break;

      case hwdi_rev_name:
        hd->revision.name = new_str(sl2->str);
        break;

      case hwdi_serial:
        hd->serial = new_str(sl2->str);
        break;

      case hwdi_unix_dev_name:
        hd->unix_dev_name = new_str(sl2->str);
        break;

      case hwdi_unix_dev_name2:
        hd->unix_dev_name2 = new_str(sl2->str);
        break;

      case hwdi_unix_dev_names:
        hd->unix_dev_names = hd_split(' ', sl2->str);
        break;

      case hwdi_drivers:
        hd->drivers = hd_split('|', sl2->str);
        break;

      case hwdi_sysfs_id:
        hd->sysfs_id = new_str(sl2->str);
        break;

      case hwdi_sysfs_busid:
        hd->sysfs_bus_id = new_str(sl2->str);
        break;

      case hwdi_sysfs_link:
        hd->sysfs_device_link = new_str(sl2->str);
        break;

      case hwdi_rom_id:
        hd->rom_id = new_str(sl2->str);
        break;

      case hwdi_broken:
        hd->broken = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_usb_guid:
        hd->usb_guid = new_str(sl2->str);
        break;

      case hwdi_hotplug:
        hd->hotplug = strtol(sl2->str, NULL, 0);
        break;

      case hwdi_class_list:
        for(
          u0 = 0, s = sl2->str;
          u0 < sizeof hd->hw_class_list / sizeof *hd->hw_class_list;
          u0++
        ) {
          if(*s && s[1] && (i = hex(s, 2)) >= 0) {
            hd->hw_class_list[u0] = i;
            s += 2;
          }
          else {
            break;
          }
        }
        break;

      case hwdi_res_mem:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_mem;
        if(sscanf(sl2->str, "0x%"SCNx64",0x%"SCNx64",%u,%u,%u", &u64_0, &u64_1, &u0, &u1, &u2) == 5) {
          res->mem.base = u64_0;
          res->mem.range = u64_1;
          res->mem.enabled = u0;
          res->mem.access = u1;
          res->mem.prefetch = u2;
        }
        break;

      case hwdi_res_phys_mem:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_phys_mem;
        if(sscanf(sl2->str, "0x%"SCNx64"", &u64_0) == 1) {
          res->phys_mem.range = u64_0;
        }
        break;

      case hwdi_res_io:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_io;
        if(sscanf(sl2->str, "0x%"SCNx64",0x%"SCNx64",%u,%u", &u64_0, &u64_1, &u0, &u1) == 4) {
          res->io.base = u64_0;
          res->io.range = u64_1;
          res->io.enabled = u0;
          res->io.access = u1;
        }
        break;

      case hwdi_res_irq:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_irq;
        if(sscanf(sl2->str, "%u,%u,%u", &u0, &u1, &u2) == 3) {
          res->irq.base = u0;
          res->irq.triggered = u1;
          res->irq.enabled = u2;
        }
        break;

      case hwdi_res_dma:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_dma;
        if(sscanf(sl2->str, "%u,%u", &u0, &u1) == 2) {
          res->dma.base = u0;
          res->dma.enabled = u1;
        }
        break;

      case hwdi_res_size:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_size;
        if(sscanf(sl2->str, "%u,%u,%u", &u0, &u1, &u2) == 3) {
          res->size.unit = u0;
          res->size.val1 = u1;
          res->size.val2 = u2;
        }
        break;

      case hwdi_res_baud:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_baud;
        if(sscanf(sl2->str, "%u,%u,%u,%u,%u", &u0, &u1, &u2, &u3, &u4) == 5) {
          res->baud.speed = u0;
          res->baud.bits = u1;
          res->baud.stopbits = u2;
          res->baud.parity = (char) u3;
          res->baud.handshake = (char) u4;
        }
        break;

      case hwdi_res_cache:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_cache;
        if(sscanf(sl2->str, "%u", &u0) == 1) {
          res->cache.size = u0;
        }
        break;

      case hwdi_res_disk_geo:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_disk_geo;
        if(sscanf(sl2->str, "%u,%u,%u,%u", &u0, &u1, &u2, &u3) == 4) {
          res->disk_geo.cyls = u0;
          res->disk_geo.heads = u1;
          res->disk_geo.sectors = u2;
          res->disk_geo.geotype = u3;
        }
        break;

      case hwdi_res_monitor:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_monitor;
        if(sscanf(sl2->str, "%u,%u,%u,%u", &u0, &u1, &u2, &u3) == 4) {
          res->monitor.width = u0;
          res->monitor.height = u1;
          res->monitor.vfreq = u2;
          res->monitor.interlaced = u3;
        }
        break;

      case hwdi_res_framebuffer:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_framebuffer;
        if(sscanf(sl2->str, "%u,%u,%u,%u,%u", &u0, &u1, &u2, &u3, &u4) == 5) {
          res->framebuffer.width = u0;
          res->framebuffer.height = u1;
          res->framebuffer.bytes_p_line = u2;
          res->framebuffer.colorbits = u3;
          res->framebuffer.mode = u4;
        }
        break;

      case hwdi_features:
        u0 = strtoul(sl2->str, NULL, 0);
        if(u0 & (1 << 0)) hd->is.agp = 1;
        if(u0 & (1 << 1)) hd->is.isapnp = 1;
        if(u0 & (1 << 2)) hd->is.softraiddisk = 1;
        if(u0 & (1 << 3)) hd->is.zip = 1;
        if(u0 & (1 << 4)) hd->is.cdr = 1;
        if(u0 & (1 << 5)) hd->is.cdrw = 1;
        if(u0 & (1 << 6)) hd->is.dvd = 1;
        if(u0 & (1 << 7)) hd->is.dvdr = 1;
        if(u0 & (1 << 8)) hd->is.dvdram = 1;
        if(u0 & (1 << 9)) hd->is.pppoe = 1;
        if(u0 & (1 << 10)) hd->is.wlan = 1;
        break;
    }
  }

  if(hd->device.id || hd->vendor.id) {
    tag = ID_TAG(hd->device.id);
    tag = tag ? tag : ID_TAG(hd->vendor.id);
    tag = tag ? tag : TAG_PCI;
    hd->device.id = MAKE_ID(tag, ID_VALUE(hd->device.id));
    hd->vendor.id = MAKE_ID(tag, ID_VALUE(hd->vendor.id));
  }

  if(hd->sub_device.id || hd->sub_vendor.id) {
    tag = ID_TAG(hd->sub_device.id);
    tag = tag ? tag : ID_TAG(hd->sub_vendor.id);
    tag = tag ? tag : TAG_PCI;
    hd->sub_device.id = MAKE_ID(tag, ID_VALUE(hd->sub_device.id));
    hd->sub_vendor.id = MAKE_ID(tag, ID_VALUE(hd->sub_vendor.id));
  }

  if(hd->compat_device.id || hd->compat_vendor.id) {
    tag = ID_TAG(hd->compat_device.id);
    tag = tag ? tag : ID_TAG(hd->compat_vendor.id);
    tag = tag ? tag : TAG_PCI;
    hd->compat_device.id = MAKE_ID(tag, ID_VALUE(hd->compat_device.id));
    hd->compat_vendor.id = MAKE_ID(tag, ID_VALUE(hd->compat_vendor.id));
  }

  if(hd->status.available == status_unknown) hd->is.manual = 1;

  /* create some entries, if missing */

  if(!hd->device.id && !hd->vendor.id && !hd->device.name) {
    hd->device.name = new_str(hd->model);
  }

  if(hd->hw_class && !hd->base_class.id) {
    switch(hd->hw_class) {
      case hw_cdrom:
        hd->base_class.id = bc_storage_device;
        hd->sub_class.id = sc_sdev_cdrom;
        break;

      case hw_mouse:
        hd->base_class.id = bc_mouse;
        hd->sub_class.id = sc_mou_other;
        break;

      default:
	break;
    }
  }

  hddb_add_info(hd_data, hd);
}


void hd2manual(hd_t *hd, hd_manual_t *entry)
{
  char *s, *t;
  hd_res_t *res;
  str_list_t *sl;
  unsigned u;

  if(!hd || !entry) return;

  entry->unique_id = new_str(hd->unique_id);
  entry->parent_id = new_str(hd->parent_id);
  entry->child_ids = hd_join(",", hd->child_ids);
  entry->model = new_str(hd->model);
  entry->hw_class = hd->hw_class;

  entry->config_string = new_str(hd->config_string);

  entry->status = hd->status;

  if(
    !entry->status.configured &&
    !entry->status.available &&
    !entry->status.needed &&
    !entry->status.active &&
    !entry->status.invalid
  ) {
    entry->status.configured = status_new;
    entry->status.available = hd->module == mod_manual ? status_unknown : status_yes;
    entry->status.needed = status_no;
    entry->status.active = status_unknown;
  }

  s = NULL;

  if(hd->broken) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_broken));
    str_printf(&s, 0, "0x%x", hd->broken);
    add_str_list(&entry->value, s);
  }

  if(hd->bus.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_bus));
    str_printf(&s, 0, "0x%x", hd->bus.id);
    add_str_list(&entry->value, s);
  }

  if(hd->slot) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_slot));
    str_printf(&s, 0, "0x%x", hd->slot);
    add_str_list(&entry->value, s);
  }

  if(hd->func) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_func));
    str_printf(&s, 0, "0x%x", hd->func);
    add_str_list(&entry->value, s);
  }

  if(hd->base_class.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_base_class));
    str_printf(&s, 0, "0x%x", hd->base_class.id);
    add_str_list(&entry->value, s);
  }

  if(hd->sub_class.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_class));
    str_printf(&s, 0, "0x%x", hd->sub_class.id);
    add_str_list(&entry->value, s);
  }

  if(hd->prog_if.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_prog_if));
    str_printf(&s, 0, "0x%x", hd->prog_if.id);
    add_str_list(&entry->value, s);
  }

  if(hd->device.id || hd->vendor.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_vend));
    add_str_list(&entry->value, vend_id2str(hd->vendor.id));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->device.id));
    add_str_list(&entry->value, s);
  }

  if(hd->sub_device.id || hd->sub_vendor.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_vend));
    add_str_list(&entry->value, vend_id2str(hd->sub_vendor.id));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->sub_device.id));
    add_str_list(&entry->value, s);
  }

  if(hd->revision.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rev));
    str_printf(&s, 0, "0x%x", hd->revision.id);
    add_str_list(&entry->value, s);
  }

  if(hd->compat_device.id || hd->compat_vendor.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_compat_vend));
    add_str_list(&entry->value, vend_id2str(hd->compat_vendor.id));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_compat_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->compat_device.id));
    add_str_list(&entry->value, s);
  }

  if(hd->device.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_dev_name));
    add_str_list(&entry->value, hd->device.name);
  }

  if(hd->vendor.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_vend_name));
    add_str_list(&entry->value, hd->vendor.name);
  }

  if(hd->sub_device.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_dev_name));
    add_str_list(&entry->value, hd->sub_device.name);
  }

  if(hd->sub_vendor.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_vend_name));
    add_str_list(&entry->value, hd->sub_vendor.name);
  }

  if(hd->revision.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rev_name));
    add_str_list(&entry->value, hd->revision.name);
  }

  if(hd->serial) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_serial));
    add_str_list(&entry->value, hd->serial);
  }

  if(hd->unix_dev_name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_unix_dev_name));
    add_str_list(&entry->value, hd->unix_dev_name);
  }

  if(hd->unix_dev_name2) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_unix_dev_name2));
    add_str_list(&entry->value, hd->unix_dev_name2);
  }

  if(hd->unix_dev_names) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_unix_dev_names));
    s = free_mem(s);
    s = hd_join(" ", hd->unix_dev_names);
    add_str_list(&entry->value, s);
  }

  if(hd->drivers) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_drivers));
    s = free_mem(s);
    s = hd_join("|", hd->drivers);
    add_str_list(&entry->value, s);
  }

  if(hd->sysfs_id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sysfs_id));
    add_str_list(&entry->value, hd->sysfs_id);
  }

  if(hd->sysfs_bus_id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sysfs_busid));
    add_str_list(&entry->value, hd->sysfs_bus_id);
  }

  if(hd->sysfs_device_link) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sysfs_link));
    add_str_list(&entry->value, hd->sysfs_device_link);
  }

  if(hd->rom_id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rom_id));
    add_str_list(&entry->value, hd->rom_id);
  }

  if(hd->usb_guid) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_usb_guid));
    add_str_list(&entry->value, hd->usb_guid);
  }

  if(hd->hotplug) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_hotplug));
    str_printf(&s, 0, "%d", hd->hotplug);
    add_str_list(&entry->value, s);
  }

  s = free_mem(s);
  for(u = 0; u < sizeof hd->hw_class_list / sizeof *hd->hw_class_list; u++) {
    str_printf(&s, -1, "%02x", hd->hw_class_list[u]);
  }
  add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_class_list));
  add_str_list(&entry->value, s);

  u = 0;
  if(hd->is.agp)          u |= 1 << 0;
  if(hd->is.isapnp)       u |= 1 << 1;
  if(hd->is.softraiddisk) u |= 1 << 2;
  if(hd->is.zip)          u |= 1 << 3;
  if(hd->is.cdr)          u |= 1 << 4;
  if(hd->is.cdrw)         u |= 1 << 5;
  if(hd->is.dvd)          u |= 1 << 6;
  if(hd->is.dvdr)         u |= 1 << 7;
  if(hd->is.dvdram)       u |= 1 << 8;
  if(hd->is.pppoe)        u |= 1 << 9;
  if(hd->is.wlan)         u |= 1 << 10;
  
  if(u) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_features));
    str_printf(&s, 0, "0x%x", u);
    add_str_list(&entry->value, s);
  }

  for(res = hd->res; res; res = res->next) {
    sl = NULL;
    switch(res->any.type) {
      case res_mem:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_mem));
        str_printf(&s, 0, "0x%"PRIx64"", res->mem.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%"PRIx64"", res->mem.range);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.enabled);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.access);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.prefetch);
        add_str_list(&sl, s);
        break;

      case res_phys_mem:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_phys_mem));
        str_printf(&s, 0, "0x%"PRIx64"", res->phys_mem.range);
        add_str_list(&sl, s);
        break;

      case res_io:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_io));
        str_printf(&s, 0, "0x%"PRIx64"", res->io.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%"PRIx64"", res->io.range);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->io.enabled);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->io.access);
        add_str_list(&sl, s);
        break;

      case res_irq:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_irq));
        str_printf(&s, 0, "%u", res->irq.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->irq.triggered);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->irq.enabled);
        add_str_list(&sl, s);
        break;

      case res_dma:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_dma));
        str_printf(&s, 0, "%u", res->dma.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->dma.enabled);
        add_str_list(&sl, s);
        break;

      case res_size:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_size));
        str_printf(&s, 0, "%u", res->size.unit);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%"PRIu64, res->size.val1);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%"PRIu64, res->size.val2);
        add_str_list(&sl, s);
        break;

      case res_baud:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_baud));
        str_printf(&s, 0, "%u", res->baud.speed);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->baud.bits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->baud.stopbits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%02x", (unsigned) res->baud.parity);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%02x", (unsigned) res->baud.handshake);
        add_str_list(&sl, s);
        break;

      case res_cache:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_cache));
        str_printf(&s, 0, "%u", res->cache.size);
        add_str_list(&sl, s);
        break;

      case res_disk_geo:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_disk_geo));
        str_printf(&s, 0, "%u", res->disk_geo.cyls);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.heads);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.sectors);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.geotype);
        add_str_list(&sl, s);
        break;

      case res_monitor:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_monitor));
        str_printf(&s, 0, "%u", res->monitor.width);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.height);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.vfreq);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.interlaced);
        add_str_list(&sl, s);
        break;

      case res_framebuffer:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_framebuffer));
        str_printf(&s, 0, "%u", res->framebuffer.width);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.height);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.bytes_p_line);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.colorbits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.mode);
        add_str_list(&sl, s);
        break;

      default:
        break;
    }
    /* keep entry->key & entry->value symmetrical! */
    if(sl) {
      t = hd_join(",", sl);
      add_str_list(&entry->value, t);
      free_mem(t);
      free_str_list(sl);
    }
  }

  free_mem(s);
}
#endif


hd_t *hd_read_config(hd_data_t *hd_data, const char *id)
{
  hd_t *hd = NULL;
  hal_prop_t *prop = NULL;
  const char *udi = NULL;

  /* only of we didn't already (check internal db pointer) */
  if(!hd_data->hddb2[1]) hddb_init(hd_data);

  if(*id != '/') {
    /* try to find udi entry */
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->udi && hd->unique_id && !strcmp(id, hd->unique_id)) {
        udi = hd->udi;
        break;
      }
    }
  }

  if(udi) prop = hd_read_properties(udi);
  if(!prop) prop = hd_read_properties(id);
  if(!prop) prop = hd_manual_read_entry_old(hd_data, id);

  if(prop) {
    hd = new_mem(sizeof *hd);
    hd->idx = ++(hd_data->last_idx);
    hd->module = hd_data->module;
    hd->line = __LINE__;
    hd->tag.freeit = 1;		/* make it a 'stand alone' entry */
    hd->persistent_prop = prop;
    prop2hd(hd_data, hd);
  }

  return hd;
}


int hd_write_config(hd_data_t *hd_data, hd_t *hd)
{
  char *udi;

  if(!hd_report_this(hd_data, hd)) return 0;

  hd2prop(hd_data, hd);

  udi = hd->unique_id;
  if(hd->udi) udi = hd->udi;

  if(!udi) return 5;

  return hd_write_properties(udi, hd->persistent_prop);
}


#endif	/* LIBHD_TINY */


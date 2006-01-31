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
 * hardware in /var/lib/hardware/udi/
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

#ifndef LIBHD_TINY

static void prop2hd(hd_data_t *hd_data, hd_t *hd, int status_only);
static hal_prop_t *hal_get_new(hal_prop_t **list, const char *key);
static void hd2prop_add_int32(hal_prop_t **list, const char *key, int32_t i);
static void hd2prop_add_str(hal_prop_t **list, const char *key, const char *str);
static void hd2prop_add_list(hal_prop_t **list, const char *key, str_list_t *sl);
static void hd2prop_append_list(hal_prop_t **list, const char *key, char *str);
static void hd2prop(hd_data_t *hd_data, hd_t *hd);

static hal_prop_t *hd_manual_read_entry_old(const char *id);
static hal_prop_t *read_properties(hd_data_t *hd_data, const char *udi, const char *id);


void hd_scan_manual(hd_data_t *hd_data)
{
  DIR *dir;
  struct dirent *de;
  int i;
  hd_t *hd, *hd1, *next, *hdm, **next2;
  char *s;

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

  if((dir = opendir(hd_get_hddb_path("udi/org/freedesktop/Hal/devices")))) {
    i = 0;
    s = NULL;
    while((de = readdir(dir))) {
      if(*de->d_name == '.') continue;
      PROGRESS(1, ++i, "read");
      str_printf(&s, 0, "/org/freedesktop/Hal/devices/%s", de->d_name);
      if((hd = hd_read_config(hd_data, s))) {
        if(hd->status.available != status_unknown) hd->status.available = status_no;
        ADD2LOG("  got %s\n", hd->unique_id);
        *next2 = hd;
        next2 = &hd->next;
      }
    }
    s = free_mem(s);
    closedir(dir);
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
      if(hd->status.available != status_unknown) hd->status.available = status_yes;

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

  /* add persistent properties */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->persistent_prop) continue;
    hd->persistent_prop = read_properties(hd_data, hd->udi, hd->unique_id);
    prop2hd(hd_data, hd, 1);
    if(hd->status.available != status_unknown) hd->status.available = status_yes;
  }

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
hal_prop_t *hd_manual_read_entry_old(const char *id)
{
  char path[PATH_MAX];
  int line;
  str_list_t *sl, *sl0;
  char *s, *s1, *s2;
  hal_prop_t *prop_list = NULL, *prop = NULL;

  if(!id) return NULL;

  snprintf(path, sizeof path, "%s/%s", hd_get_hddb_path("unique-keys"), id);

  if(!(sl0 = read_file(path, 0, 0))) return prop_list;

  for(line = 1, sl = sl0; sl; sl = sl->next, line++) {
    s = sl->str;
    while(isspace(*s)) s++;
    if(!*s || *s == '#' || *s == ';') continue;	/* empty lines & comments */

    s2 = s;
    s1 = strsep(&s2, "=");

    if(!s2 && *s == '[') continue;

    if(!s2) break;

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
  return 0;
}


char *prop2hd_str(hal_prop_t *prop, const char *id)
{
  return (prop = hal_get_str(prop, id)) ? new_str(prop->val.str) : NULL;
}


int32_t prop2hd_int32(hal_prop_t *prop, const char *id)
{
  return (prop = hal_get_int32(prop, id)) ? prop->val.int32 : 0;
}


str_list_t *prop2hd_list(hal_prop_t *prop, const char *id)
{
  str_list_t *sl0 = NULL, *sl;

  prop = hal_get_list(prop, id);

  if(prop) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      add_str_list(&sl0, sl->str);
    }
  }

  return sl0;
}


void prop2hd(hd_data_t *hd_data, hd_t *hd, int status_only)
{
  hal_prop_t *prop, *list;
  hd_res_t *res;
  int i;
  unsigned u, u0, u1, u2, u3, u4;
  char *s;
  uint64_t u64_0, u64_1;
  str_list_t *sl;

  list = hd->persistent_prop;

  hd->config_string = prop2hd_str(list, "hwinfo.configstring");

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
   * new hardware, autodetectable, not needed
   */
  if(
    !hd->status.configured &&
    !hd->status.available &&
    !hd->status.needed &&
    !hd->status.invalid
  ) {
    hd->status.configured = status_new;
    hd->status.available = status_yes;
    hd->status.needed = status_no;
  }
  if(!hd->status.active) hd->status.active = status_unknown;

  if(status_only || !list) return;

  hd->udi = prop2hd_str(list, "info.udi");
  hd->unique_id = prop2hd_str(list, "hwinfo.uniqueid");
  hd->parent_id = prop2hd_str(list, "hwinfo.parentid");
  hd->child_ids = prop2hd_list(list, "hwinfo.childids");
  hd->model = prop2hd_str(list, "hwinfo.model");

  if((prop = hal_get_str(list, "hwinfo.hwclass"))) {
    hd->hw_class = value2key(hw_items, prop->val.str);
  }

  hd->broken = prop2hd_int32(list, "hwinfo.broken");

  hd->bus.id = prop2hd_int32(list, "hwinfo.busid");
  hd->slot = prop2hd_int32(list, "hwinfo.slot");
  hd->func = prop2hd_int32(list, "hwinfo.func");

  hd->base_class.id = prop2hd_int32(list, "hwinfo.baseclass");
  hd->sub_class.id = prop2hd_int32(list, "hwinfo.subclass");
  hd->prog_if.id = prop2hd_int32(list, "hwinfo.progif");

  hd->revision.id = prop2hd_int32(list, "hwinfo.revisionid");
  hd->revision.name = prop2hd_str(list, "hwinfo.revisionname");

  hd->vendor.id = prop2hd_int32(list, "hwinfo.vendorid");
  hd->vendor.name = prop2hd_str(list, "hwinfo.vendorname");

  hd->device.id = prop2hd_int32(list, "hwinfo.deviceid");
  hd->device.name = prop2hd_str(list, "hwinfo.devicename");

  hd->sub_vendor.id = prop2hd_int32(list, "hwinfo.subvendorid");
  hd->sub_vendor.name = prop2hd_str(list, "hwinfo.subvendorname");

  hd->sub_device.id = prop2hd_int32(list, "hwinfo.subdeviceid");
  hd->sub_device.name = prop2hd_str(list, "hwinfo.subdevicename");

  hd->compat_device.id = prop2hd_int32(list, "hwinfo.compatdeviceid");
  hd->compat_device.name = prop2hd_str(list, "hwinfo.compatdevicename");

  hd->serial = prop2hd_str(list, "hwinfo.serial");
  hd->unix_dev_name = prop2hd_str(list, "hwinfo.unixdevice");
  hd->unix_dev_name2 = prop2hd_str(list, "hwinfo.unixdevicealt");

  hd->unix_dev_names = prop2hd_list(list, "hwinfo.unixdevicelist");
  hd->drivers = prop2hd_list(list, "hwinfo.drivers");

  hd->sysfs_id = prop2hd_str(list, "hwinfo.sysfsid");
  hd->sysfs_bus_id = prop2hd_str(list, "hwinfo.sysfsbusid");
  hd->sysfs_device_link = prop2hd_str(list, "hwinfo.sysfslink");
  hd->rom_id = prop2hd_str(list, "hwinfo.romid");
  hd->usb_guid = prop2hd_str(list, "hwinfo.usbguid");
  hd->hotplug = prop2hd_int32(list, "hwinfo.hotplug");

  if((s = hal_get_useful_str(list, "hwinfo.hwclasslist"))) {
    for(u = 0; u < sizeof hd->hw_class_list / sizeof *hd->hw_class_list; u++) {
      if(*s && s[1] && (i = hex(s, 2)) >= 0) {
        hd->hw_class_list[u] = i;
        s += 2;
      }
      else {
        break;
      }
    }
  }

  u = prop2hd_int32(list, "hwinfo.features");
  if(u & (1 << 0)) hd->is.agp = 1;
  if(u & (1 << 1)) hd->is.isapnp = 1;
  if(u & (1 << 2)) hd->is.softraiddisk = 1;
  if(u & (1 << 3)) hd->is.zip = 1;
  if(u & (1 << 4)) hd->is.cdr = 1;
  if(u & (1 << 5)) hd->is.cdrw = 1;
  if(u & (1 << 6)) hd->is.dvd = 1;
  if(u & (1 << 7)) hd->is.dvdr = 1;
  if(u & (1 << 8)) hd->is.dvdram = 1;
  if(u & (1 << 9)) hd->is.pppoe = 1;
  if(u & (1 << 10)) hd->is.wlan = 1;


  if((prop = hal_get_list(list, "hwinfo.res.memory"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "0x%"SCNx64",0x%"SCNx64",%u,%u,%u", &u64_0, &u64_1, &u0, &u1, &u2) == 5) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_mem;
        res->mem.base = u64_0;
        res->mem.range = u64_1;
        res->mem.enabled = u0;
        res->mem.access = u1;
        res->mem.prefetch = u2;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.physmemory"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "0x%"SCNx64"", &u64_0) == 1) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_phys_mem;
        res->phys_mem.range = u64_0;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.io"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "0x%"SCNx64",0x%"SCNx64",%u,%u", &u64_0, &u64_1, &u0, &u1) == 4) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_io;
        res->io.base = u64_0;
        res->io.range = u64_1;
        res->io.enabled = u0;
        res->io.access = u1;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.interrupts"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "%u,%u,%u", &u0, &u1, &u2) == 3) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_irq;
        res->irq.base = u0;
        res->irq.triggered = u1;
        res->irq.enabled = u2;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.dma"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "%u,%u", &u0, &u1) == 2) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_dma;
        res->dma.base = u0;
        res->dma.enabled = u1;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.size"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "%u,%u,%u", &u0, &u1, &u2) == 3) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_size;
        res->size.unit = u0;
        res->size.val1 = u1;
        res->size.val2 = u2;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.baud"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "%u,%u,%u,%u,%u", &u0, &u1, &u2, &u3, &u4) == 5) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_baud;
        res->baud.speed = u0;
        res->baud.bits = u1;
        res->baud.stopbits = u2;
        res->baud.parity = (char) u3;
        res->baud.handshake = (char) u4;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.cache"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "%u", &u0) == 1) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_cache;
        res->cache.size = u0;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.diskgeometry"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "%u,%u,%u,%u", &u0, &u1, &u2, &u3) == 4) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_disk_geo;
        res->disk_geo.cyls = u0;
        res->disk_geo.heads = u1;
        res->disk_geo.sectors = u2;
        res->disk_geo.geotype = u3;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.monitor"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "%u,%u,%u,%u", &u0, &u1, &u2, &u3) == 4) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_monitor;
        res->monitor.width = u0;
        res->monitor.height = u1;
        res->monitor.vfreq = u2;
        res->monitor.interlaced = u3;
      }
    }
  }

  if((prop = hal_get_list(list, "hwinfo.res.framebuffer"))) {
    for(sl = prop->val.list; sl; sl = sl->next) {
      if(sscanf(sl->str, "%u,%u,%u,%u,%u", &u0, &u1, &u2, &u3, &u4) == 5) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_framebuffer;
        res->framebuffer.width = u0;
        res->framebuffer.height = u1;
        res->framebuffer.bytes_p_line = u2;
        res->framebuffer.colorbits = u3;
        res->framebuffer.mode = u4;
      }
    }
  }

  hddb_add_info(hd_data, hd);

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


void hd2prop_append_list(hal_prop_t **list, const char *key, char *str)
{
  hal_prop_t *prop;
  str_list_t *sl = NULL;

  if(!str) return;

  prop = hal_get_list(*list, key);

  if(!prop) {
    add_str_list(&sl, str);
    hd2prop_add_list(list, key, sl);
    return;
  }

  add_str_list(&prop->val.list, str);
}


void hd2prop(hd_data_t *hd_data, hd_t *hd)
{
  hal_prop_t **list;
  char *s = NULL;
  unsigned u;
  hd_res_t *res;

  list = &hd->persistent_prop;

  hd2prop_add_str(list, "info.udi", hd->udi);

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

  hal_invalidate_all(*list, "hwinfo.res.memory");
  hal_invalidate_all(*list, "hwinfo.res.physmemory");
  hal_invalidate_all(*list, "hwinfo.res.io");
  hal_invalidate_all(*list, "hwinfo.res.interrupts");
  hal_invalidate_all(*list, "hwinfo.res.dma");
  hal_invalidate_all(*list, "hwinfo.res.size");
  hal_invalidate_all(*list, "hwinfo.res.baud");
  hal_invalidate_all(*list, "hwinfo.res.cache");
  hal_invalidate_all(*list, "hwinfo.res.diskgeometry");
  hal_invalidate_all(*list, "hwinfo.res.monitor");
  hal_invalidate_all(*list, "hwinfo.res.framebuffer");
  
  for(res = hd->res; res; res = res->next) {
    switch(res->any.type) {
      case res_mem:
        str_printf(&s, 0,
          "0x%"PRIx64",0x%"PRIx64",%u,%u,%u",
          res->mem.base, res->mem.range, res->mem.enabled, res->mem.access, res->mem.prefetch
        );
        hd2prop_append_list(list, "hwinfo.res.memory", s);
        break;

      case res_phys_mem:
        str_printf(&s, 0,
          "0x%"PRIx64,
          res->phys_mem.range
        );
        hd2prop_append_list(list, "hwinfo.res.physmemory", s);
        break;

      case res_io:
        str_printf(&s, 0,
          "0x%"PRIx64",0x%"PRIx64",%u,%u",
          res->io.base, res->io.range, res->io.enabled, res->io.access
        );
        hd2prop_append_list(list, "hwinfo.res.io", s);
        break;

      case res_irq:
        str_printf(&s, 0,
          "%u,%u,%u",
          res->irq.base, res->irq.triggered, res->irq.enabled
        );
        hd2prop_append_list(list, "hwinfo.res.interrupts", s);
        break;

      case res_dma:
        str_printf(&s, 0,
          "%u,%u",
          res->dma.base, res->dma.enabled
        );
        hd2prop_append_list(list, "hwinfo.res.dma", s);
        break;

      case res_size:
        str_printf(&s, 0,
          "%u,%"PRIu64",%"PRIu64,
          res->size.unit, res->size.val1, res->size.val2
        );
        hd2prop_append_list(list, "hwinfo.res.size", s);
        break;

      case res_baud:
        str_printf(&s, 0,
          "%u,%u,%u,0x%02x,0x%02x",
          res->baud.speed, res->baud.bits, res->baud.stopbits,
          (unsigned) res->baud.parity, (unsigned) res->baud.handshake
        );
        hd2prop_append_list(list, "hwinfo.res.baud", s);
        break;

      case res_cache:
        str_printf(&s, 0,
          "%u",
          res->cache.size
        );
        hd2prop_append_list(list, "hwinfo.res.cache", s);
        break;

      case res_disk_geo:
        str_printf(&s, 0,
          "%u,%u,%u,%u",
          res->disk_geo.cyls, res->disk_geo.heads, res->disk_geo.sectors, res->disk_geo.geotype
        );
        hd2prop_append_list(list, "hwinfo.res.diskgeometry", s);
        break;

      case res_monitor:
        str_printf(&s, 0,
          "%u,%u,%u,%u",
          res->monitor.width, res->monitor.height, res->monitor.vfreq, res->monitor.interlaced
        );
        hd2prop_append_list(list, "hwinfo.res.monitor", s);
        break;

      case res_framebuffer:
        str_printf(&s, 0,
          "%u,%u,%u,%u,%u",
          res->framebuffer.width, res->framebuffer.height, res->framebuffer.bytes_p_line,
          res->framebuffer.colorbits, res->framebuffer.mode
        );
        hd2prop_append_list(list, "hwinfo.res.framebuffer", s);
        break;

      default:
        break;
    }
  }

  s = free_mem(s);

}


hal_prop_t *read_properties(hd_data_t *hd_data, const char *udi, const char *id)
{
  hd_t *hd;
  hal_prop_t *prop = NULL;

  if(udi) {
    prop = hd_read_properties(udi);
    ADD2LOG("  prop read: %s (%s)\n", udi, prop ? "ok" : "failed");
  }

  if(prop) return prop;

  if(id && !udi) {
    /* try to find udi entry */
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->udi && hd->unique_id && !strcmp(id, hd->unique_id)) {
        udi = hd->udi;
        break;
      }
    }

    if(udi) {
      prop = hd_read_properties(udi);
      ADD2LOG("  prop read: %s (%s)\n", udi, prop ? "ok" : "failed");
    }
  }

  if(!prop) {
    prop = hd_read_properties(id);
    ADD2LOG("  prop read: %s (%s)\n", id, prop ? "ok" : "failed");
  }
  if(!prop) {
    prop = hd_manual_read_entry_old(id);
    ADD2LOG("  old prop read: %s (%s)\n", id, prop ? "ok" : "failed");
  }

  return prop;
}


hd_t *hd_read_config(hd_data_t *hd_data, const char *id)
{
  hd_t *hd = NULL;
  hal_prop_t *prop = NULL;
  const char *udi = NULL;

  /* only of we didn't already (check internal db pointer) */
  /* prop2hd() makes db lookups */
  if(!hd_data->hddb2[1]) hddb_init(hd_data);

  if(id && *id == '/') {
    udi = id;
    id = NULL;
  }

  prop = read_properties(hd_data, udi, id);

  if(prop) {
    hd = new_mem(sizeof *hd);
    hd->idx = ++(hd_data->last_idx);
    hd->module = hd_data->module;
    hd->line = __LINE__;
    hd->tag.freeit = 1;		/* make it a 'stand alone' entry */
    hd->persistent_prop = prop;
    prop2hd(hd_data, hd, 0);
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


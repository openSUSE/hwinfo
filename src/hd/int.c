#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hd.h"
#include "hd_int.h"
#include "int.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * internal things
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void int_hotplug(hd_data_t *hd_data);
static void int_cdrom(hd_data_t *hd_data);
#if defined(__i386__) || defined (__x86_64__)
static int set_bios_id(hd_data_t *hd_data, char *dev_name, int bios_id);
static void int_bios(hd_data_t *hd_data);
#endif
static void int_media_check(hd_data_t *hd_data);
static void int_floppy(hd_data_t *hd_data);
static void int_fix_ide_scsi(hd_data_t *hd_data);
static void int_fix_usb_scsi(hd_data_t *hd_data);
static void int_mouse(hd_data_t *hd_data);
static void new_id(hd_data_t *hd_data, hd_t *hd);

void hd_scan_int(hd_data_t *hd_data)
{
  hd_t *hd;

  if(!hd_probe_feature(hd_data, pr_int)) return;

  hd_data->module = mod_int;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "idescsi");
  int_fix_ide_scsi(hd_data);

  PROGRESS(2, 0, "cdrom");
  int_cdrom(hd_data);

  PROGRESS(3, 0, "media");
  int_media_check(hd_data);

  PROGRESS(4, 0, "floppy");
  int_floppy(hd_data);

#if defined(__i386__) || defined (__x86_64__)
  PROGRESS(5, 0, "bios");
  int_bios(hd_data);
#endif

  PROGRESS(6, 0, "mouse");
  int_mouse(hd_data);

  PROGRESS(7, 0, "usbscsi");
  int_fix_usb_scsi(hd_data);

  PROGRESS(8, 0, "hdb");
  for(hd = hd_data->hd; hd; hd = hd->next) {
    hddb_add_info(hd_data, hd);
  }

  PROGRESS(9, 0, "hotplug");
  int_hotplug(hd_data);
}

/*
 * Identify hotpluggable devices.
 */
void int_hotplug(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_t *bridge_hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->bus.id == bus_usb || hd->usb_guid) {
      hd->hotplug = hp_usb;
    }
    if((bridge_hd = hd_get_device_by_idx(hd_data, hd->attached_to))) {
      if(
        bridge_hd->base_class.id == bc_bridge &&
        bridge_hd->sub_class.id == sc_bridge_cardbus
      ) {
        hd->is.cardbus = 1;
        hd->hotplug = hp_cardbus;
      }
     else if(
        bridge_hd->base_class.id == bc_bridge &&
        bridge_hd->sub_class.id == sc_bridge_pcmcia
      ) {
        hd->is.pcmcia = 1;
        hd->hotplug = hp_pcmcia;
      }
    }
  }
}

/*
 * Add more info to CDROM entries.
 */
void int_cdrom(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_cdrom &&
      !hd->prog_if.id
    ) {
      if(hd->device.name && strstr(hd->device.name, "DVD")) {
        hd->prog_if.id = 3;
      }
    }
  }
}

#if defined(__i386__) || defined (__x86_64__)

int set_bios_id(hd_data_t *hd_data, char *dev_name, int bios_id)
{
  int found = 0;
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_disk &&
      hd->unix_dev_name &&
      !strcmp(hd->unix_dev_name, dev_name)
    ) {
      str_printf(&hd->rom_id, 0, "0x%02x", bios_id);
      found = 1;
    }
  }

  return found;
}

void int_bios(hd_data_t *hd_data)
{
  hd_t *hd, *hd_boot;
  int i, start, bios = 0x80;
  int ide_1st;
  char ide_name[] = "/dev/hda";
  char scsi_name[] = "/dev/sda";
  char *s;

  hd_boot = hd_get_device_by_idx(hd_data, hd_boot_disk(hd_data, &i));

  if(hd_boot) {
    free_mem(hd_boot->rom_id);
    hd_boot->rom_id = new_str("0x80");
  }

  if(!hd_boot || i != 1) return;

  if(strstr(hd_boot->unix_dev_name, "/dev/sd") == hd_boot->unix_dev_name) {
    ide_1st = 0;
    start = hd_boot->unix_dev_name[sizeof "/dev/sd" - 1] - 'a';
  }
  else if(strstr(hd_boot->unix_dev_name, "/dev/hd") == hd_boot->unix_dev_name) {
    ide_1st = 1;
    start = hd_boot->unix_dev_name[sizeof "/dev/hd" - 1] - 'a';
  }
  else {
    return;
  }

  if(start < 0) return;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_disk
    ) {
      hd->rom_id = free_mem(hd->rom_id);
    }
  }

  s = ide_1st ? ide_name : scsi_name;

  for(i = start; i < 26; i++) {
    s[strlen(s) - 1] = 'a' + i;
    bios += set_bios_id(hd_data, s, bios);
  }

  for(i = 0; i < start; i++) {
    s[strlen(s) - 1] = 'a' + i;
    bios += set_bios_id(hd_data, s, bios);
  }

  s = ide_1st ? scsi_name : ide_name;

  for(i = 0; i < 26; i++) {
    s[strlen(s) - 1] = 'a' + i;
    bios += set_bios_id(hd_data, s, bios);
  }

}
#endif	/* defined(__i386__) || defined (__x86_64__) */

/*
 * Try to read block 0 for block devices.
 */
void int_media_check(hd_data_t *hd_data)
{
  hd_t *hd;
  int i, j = 0;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      (
        /* hd->sub_class.id == sc_sdev_cdrom || */ /* cf. cdrom.c */
        hd->sub_class.id == sc_sdev_disk ||
        hd->sub_class.id == sc_sdev_floppy
      ) &&
      hd->unix_dev_name &&
      !hd->block0 &&
      !hd->is.notready &&
      hd->status.available != status_no
    ) {
      i = 5;
      PROGRESS(4, ++j, hd->unix_dev_name);
      hd->block0 = read_block0(hd_data, hd->unix_dev_name, &i);
      hd->is.notready = hd->block0 ? 0 : 1;
    }
  }
}


/*
 * Turn some Zip drives into floppies.
 */
void int_floppy(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_disk
    ) {
      if(
        (
          (
            (hd->vendor.name && !strcasecmp(hd->vendor.name, "iomega")) ||
            (hd->sub_vendor.name && !strcasecmp(hd->sub_vendor.name, "iomega"))
          ) &&
          (
            (hd->device.name && strstr(hd->device.name, "ZIP")) ||
            (hd->sub_device.name && strstr(hd->sub_device.name, "Zip"))
          )
        )
      ) {
        hd->sub_class.id = sc_sdev_floppy;
        new_id(hd_data, hd);
      }
    }
  }
}


#define COPY_ENTRY(a) if(hd_ide->a) { free_mem(hd_scsi->a); hd_scsi->a = new_str(hd_ide->a); }
/*
 * Remove ide entries that are handled by ide-scsi.
 */
void int_fix_ide_scsi(hd_data_t *hd_data)
{
  hd_t *hd_scsi, *hd_ide;

  for(hd_scsi = hd_ide = hd_data->hd; hd_scsi; hd_scsi = hd_scsi->next) {
    if(
      hd_scsi->bus.id == bus_scsi &&
      hd_scsi->driver &&
      !strcmp(hd_scsi->driver, "ide-scsi")
    ) {
      for(; hd_ide ; hd_ide = hd_ide->next) {
        if(
          hd_ide->bus.id == bus_ide &&
          hd_ide->driver &&
          !strcmp(hd_ide->driver, "ide-scsi")
        ) {
          // FIXME: we need a proper solution for this!
          if(hd_ide->status.configured != status_no) {
            hd_ide->tag.remove = 1;
          }
          hd_ide->status.available = status_no;

          COPY_ENTRY(vendor.name);
          COPY_ENTRY(device.name);
          COPY_ENTRY(sub_device.name);
          COPY_ENTRY(sub_vendor.name);
          COPY_ENTRY(revision.name);
          COPY_ENTRY(serial);

          new_id(hd_data, hd_scsi);
        }
      }
    }
  }

  remove_tagged_hd_entries(hd_data);
}
#undef COPY_ENTRY


#define COPY_ENTRY(a) if(hd_scsi->a) { free_mem(hd_usb->a); hd_usb->a = new_str(hd_scsi->a); }
/*
 * Remove usb entries that are handled by usb-storage.
 */
void int_fix_usb_scsi(hd_data_t *hd_data)
{
  hd_t *hd_scsi, *hd_usb;

  for(hd_scsi = hd_data->hd; hd_scsi; hd_scsi = hd_scsi->next) {
    if(
      hd_scsi->bus.id == bus_scsi &&
      hd_scsi->driver &&
      hd_scsi->usb_guid &&
      !strcmp(hd_scsi->driver, "usb-storage")
    ) {
      for(hd_usb = hd_data->hd; hd_usb ; hd_usb = hd_usb->next) {
        if(
          hd_usb->bus.id == bus_usb &&
          hd_usb->usb_guid &&
          !hd_usb->tag.remove &&
          !strcmp(hd_usb->usb_guid, hd_scsi->usb_guid)
        ) {
          hd_scsi->tag.remove = 1;

          /* join usb & scsi info */
          hd_usb->bus.id = hd_scsi->bus.id;
          COPY_ENTRY(bus.name);
          hd_usb->base_class.id = hd_scsi->base_class.id;
          COPY_ENTRY(base_class.name);
          hd_usb->sub_class.id = hd_scsi->sub_class.id;
          COPY_ENTRY(sub_class.name);
          hd_usb->prog_if.id = hd_scsi->prog_if.id;
          COPY_ENTRY(prog_if.name);
          COPY_ENTRY(unix_dev_name);
          COPY_ENTRY(model);
          COPY_ENTRY(driver);

          hd_usb->vendor.id = hd_scsi->vendor.id;
          COPY_ENTRY(vendor.name);
          hd_usb->device.id = hd_scsi->device.id;
          COPY_ENTRY(device.name);
          hd_usb->sub_vendor.name = free_mem(hd_usb->sub_vendor.name);
          COPY_ENTRY(sub_vendor.name);
          hd_usb->sub_device.name = free_mem(hd_usb->sub_device.name);
          COPY_ENTRY(sub_device.name);
          COPY_ENTRY(revision.name);
          COPY_ENTRY(serial);

          hd_usb->is.notready = hd_scsi->is.notready;
          if(hd_usb->block0) free_mem(hd_usb->block0);
          hd_usb->block0 = hd_scsi->block0;
          hd_scsi->block0 = NULL;
          add_res_entry(&hd_usb->res, hd_scsi->res);
          hd_scsi->res = NULL;

          new_id(hd_data, hd_usb);
        }
      }
    }
  }

  remove_tagged_hd_entries(hd_data);
}
#undef COPY_ENTRY


/*
 * Improve mouse info.
 */
void int_mouse(hd_data_t *hd_data)
{
  hd_t *hd;
  bios_info_t *bt = NULL;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->detail &&
      hd->detail->type == hd_detail_bios &&
      (bt = hd->detail->bios.data) &&
      bt->mouse.type
    ) break;
  }

  if(!bt) return;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_mouse &&
      hd->sub_class.id == sc_mou_ps2 &&
      hd->bus.id == bt->mouse.bus &&
      hd->vendor.id == MAKE_ID(TAG_SPECIAL, 0x0200) &&
      hd->device.id == MAKE_ID(TAG_SPECIAL, 0x0002)
    ) {
      hd->vendor.name = free_mem(hd->vendor.name);
      hd->device.name = free_mem(hd->device.name);
      hd->vendor.id = hd->device.id = 0;
#if 1
      hd->vendor.id = bt->mouse.compat_vend;
      hd->device.id = bt->mouse.compat_dev;
#else
      hd->vend_name = new_str(bt->mouse.vendor);
      hd->device.name = new_str(bt->mouse.type);
      hd->compat_vendor.id = bt->mouse.compat_vend;
      hd->compat_device.id = bt->mouse.compat_dev;
#endif
      new_id(hd_data, hd);
    }
  }
}


void new_id(hd_data_t *hd_data, hd_t *hd)
{
#if 0
  hd->unique_id = free_mem(hd->unique_id);
  hd->unique_id1 = free_mem(hd->unique_id1);
  hd->old_unique_id = free_mem(hd->old_unique_id);
  hd_add_id(hd_data, hd);
#endif
}
 

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

static void int_pcmcia(hd_data_t *hd_data);
static void int_cdrom(hd_data_t *hd_data);
#if defined(__i386__)
static int set_bios_id(hd_data_t *hd_data, char *dev_name, int bios_id);
static void int_bios(hd_data_t *hd_data);
#endif
static void int_media_check(hd_data_t *hd_data);
static void int_floppy(hd_data_t *hd_data);
static void int_fix_ide_scsi(hd_data_t *hd_data);

void hd_scan_int(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_int)) return;

  hd_data->module = mod_int;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "idescsi");
  int_fix_ide_scsi(hd_data);

  PROGRESS(2, 0, "pcmcia");
  int_pcmcia(hd_data);

  PROGRESS(3, 0, "cdrom");
  int_cdrom(hd_data);

  PROGRESS(4, 0, "media");
  int_media_check(hd_data);

  PROGRESS(5, 0, "floppy");
  int_floppy(hd_data);

#if defined(__i386__)
  PROGRESS(6, 0, "bios");
  int_bios(hd_data);
#endif
}

/*
 * Identify cardbus cards.
 */
void int_pcmcia(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_t *bridge_hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if((bridge_hd = hd_get_device_by_idx(hd_data, hd->attached_to))) {
      if(
        bridge_hd->base_class == bc_bridge &&
        bridge_hd->sub_class == sc_bridge_cardbus
      ) hd->is.cardbus = 1;
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
      hd->base_class == bc_storage_device &&
      hd->sub_class == sc_sdev_cdrom &&
      !hd->prog_if
    ) {
      if(hd->dev_name && strstr(hd->dev_name, "DVD")) {
        hd->prog_if = 3;
      }
    }
  }
}

#if defined(__i386__)

int set_bios_id(hd_data_t *hd_data, char *dev_name, int bios_id)
{
  int found = 0;
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_storage_device &&
      hd->sub_class == sc_sdev_disk &&
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
      hd->base_class == bc_storage_device &&
      hd->sub_class == sc_sdev_disk
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
#endif	/* defined(__i386__) */

/*
 * Try to read block 0 for block devices.
 */
void int_media_check(hd_data_t *hd_data)
{
  hd_t *hd;
  int i, j = 0;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_storage_device &&
      (
        /* hd->sub_class == sc_sdev_cdrom || */ /* cf. cdrom.c */
        hd->sub_class == sc_sdev_disk ||
        hd->sub_class == sc_sdev_floppy
      ) &&
      hd->unix_dev_name &&
      !hd->block0 &&
      !hd->is.notready
    ) {
      i = 5;
      PROGRESS(4, ++j, hd->unix_dev_name);
      hd->block0 = read_block0(hd_data, hd->unix_dev_name, &i);
      hd->is.notready = hd->block0 ? 0 : 1;
    }
  }
}


/*
 * Turn some Zip drives into flppies.
 */
void int_floppy(hd_data_t *hd_data)
{
  hd_t *hd;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_storage_device &&
      hd->sub_class == sc_sdev_disk
    ) {
      if(
        (
          (
            (hd->vend_name && !strcasecmp(hd->vend_name, "iomega")) ||
            (hd->sub_vend_name && !strcasecmp(hd->sub_vend_name, "iomega"))
          ) &&
          (
            (hd->dev_name && strstr(hd->dev_name, "ZIP")) ||
            (hd->sub_dev_name && strstr(hd->sub_dev_name, "Zip"))
          )
        )
      ) {
        hd->sub_class = sc_sdev_floppy;
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
      hd_scsi->bus == bus_scsi &&
      hd_scsi->driver &&
      !strcmp(hd_scsi->driver, "ide-scsi")
    ) {
      for(; hd_ide ; hd_ide = hd_ide->next) {
        if(
          hd_ide->bus == bus_ide &&
          hd_ide->driver &&
          !strcmp(hd_ide->driver, "ide-scsi")
        ) {
          hd_ide->tag.remove = 1;

          /* ide info is more accurate, take it instead */
          hd_scsi->base_class = hd_ide->base_class;
          hd_scsi->sub_class = hd_ide->sub_class;
          hd_scsi->prog_if = hd_ide->prog_if;
          
          COPY_ENTRY(dev_name);
          COPY_ENTRY(vend_name);
          COPY_ENTRY(sub_dev_name);
          COPY_ENTRY(sub_vend_name);
          COPY_ENTRY(rev_name);
          COPY_ENTRY(serial);

          hd_scsi->unique_id = free_mem(hd_scsi->unique_id);
          hd_scsi->unique_id1 = free_mem(hd_scsi->unique_id1);
          hd_scsi->old_unique_id = free_mem(hd_scsi->old_unique_id);
          hd_add_id(hd_data, hd_scsi);
        }
      }
    }
  }

  remove_tagged_hd_entries(hd_data);
}
#undef COPY_ENTRY



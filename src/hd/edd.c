#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "edd.h"

/**
 * @defgroup EDDint EDD partition information
 * @ingroup  libhdINFOint
 * @brief EDD disks layout / partition functions
 *
 * @{
 */

static void get_edd_info(hd_data_t *hd_data);

void hd_scan_sysfs_edd(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_edd)) return;

  hd_data->module = mod_edd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  hd_data->flags.edd_used = 0;

  if(hd_probe_feature(hd_data, pr_edd_mod)) {
    PROGRESS(1, 0, "edd mod");
    load_module(hd_data, "edd");
  }

  PROGRESS(2, 0, "edd info");

  get_edd_info(hd_data);
}


void get_edd_info(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res;
  unsigned u, u1, u2, edd_cnt = 0, lba;
  uint64_t ul0;
  str_list_t *sl;
  bios_info_t *bt;
  edd_info_t *ei;

  struct sysfs_directory *sf_dir;
  struct sysfs_directory *sf_dir_2;
  struct sysfs_link *sf_link;

  for(u = 0; u < sizeof hd_data->edd / sizeof *hd_data->edd; u++) {
    free_mem(hd_data->edd[u].sysfs_id);
  }

  memset(hd_data->edd, 0, sizeof hd_data->edd);

  sf_dir = sysfs_open_directory("/sys/firmware/edd");

  if(sf_dir) {
    if(!sysfs_read_all_subdirs(sf_dir)) {
      if(sf_dir->subdirs) {
        dlist_for_each_data(sf_dir->subdirs, sf_dir_2, struct sysfs_directory) {

          if(
            sscanf(sf_dir_2->name, "int13_dev%02x", &u) == 1 &&
            u >= 0x80 &&
            u <= 0xff
          ) {
            edd_cnt++;

            u -= 0x80;

            ei = hd_data->edd + u;

            if(hd_attr_uint(sysfs_get_directory_attribute(sf_dir_2, "sectors"), &ul0, 0)) {
              ei->sectors = ul0;
            }

            if(hd_attr_uint(sysfs_get_directory_attribute(sf_dir_2, "default_cylinders"), &ul0, 0)) {
              ei->edd.cyls = ul0;
            }

            if(hd_attr_uint(sysfs_get_directory_attribute(sf_dir_2, "default_heads"), &ul0, 0)) {
              ei->edd.heads = ul0;
            }

            if(hd_attr_uint(sysfs_get_directory_attribute(sf_dir_2, "default_sectors_per_track"), &ul0, 0)) {
              ei->edd.sectors = ul0;
            }

            if(hd_attr_uint(sysfs_get_directory_attribute(sf_dir_2, "legacy_max_cylinder"), &ul0, 0)) {
              ei->legacy.cyls = ul0 + 1;
            }

            if(hd_attr_uint(sysfs_get_directory_attribute(sf_dir_2, "legacy_max_head"), &ul0, 0)) {
              ei->legacy.heads = ul0 + 1;
            }

            if(hd_attr_uint(sysfs_get_directory_attribute(sf_dir_2, "legacy_sectors_per_track"), &ul0, 0)) {
              ei->legacy.sectors = ul0;
            }

            if(ei->sectors && ei->edd.heads && ei->edd.sectors) {
              ei->edd.cyls = ei->sectors / (ei->edd.heads * ei->edd.sectors);
            }

            sf_link = sysfs_get_directory_link(sf_dir_2, "pci_dev");
            if(sf_link) {
              hd_data->edd[u].sysfs_id = new_str(hd_sysfs_id(sf_link->target));
              if((hd = hd_find_sysfs_id(hd_data, hd_data->edd[u].sysfs_id))) {
                hd_data->edd[u].hd_idx = hd->idx;
              }
            }

            sl = hd_attr_list(sysfs_get_directory_attribute(sf_dir_2, "extensions"));
            if(search_str_list(sl, "Fixed disk access")) hd_data->edd[u].ext_fixed_disk = 1;
            if(search_str_list(sl, "Device locking and ejecting")) hd_data->edd[u].ext_lock_eject = 1;
            if(search_str_list(sl, "Enhanced Disk Drive support")) hd_data->edd[u].ext_edd = 1;
            if(search_str_list(sl, "64-bit extensions")) hd_data->edd[u].ext_64bit = 1;

            ADD2LOG(
              "edd: 0x%02x\n  size: %"PRIu64"\n  chs default: %u/%u/%u\n  chs legacy: %u/%u/%u\n  caps: %s%s%s%s\n  attached: #%u %s\n",
              u + 0x80,
              ei->sectors,
              ei->edd.cyls,
              ei->edd.heads,
              ei->edd.sectors,
              ei->legacy.cyls,
              ei->legacy.heads,
              ei->legacy.sectors,
              ei->ext_fixed_disk ? "fixed " : "",
              ei->ext_lock_eject ? "lock " : "",
              ei->ext_edd ? "edd " : "",
              ei->ext_64bit ? "64bit " : "",
              ei->hd_idx,
              ei->sysfs_id ?: ""
            );
          }
        }
      }
    }
  }

  sysfs_close_directory(sf_dir);

  if(!edd_cnt) return;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_disk
    ) {
      hd->rom_id = free_mem(hd->rom_id);
    }
  }

  /* add BIOS drive ids to disks */

  /* first, check sysfs link */
  for(u = 0; u < sizeof hd_data->edd / sizeof *hd_data->edd; u++) {
    if(!hd_data->edd[u].hd_idx) continue;
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_storage_device &&
        hd->sub_class.id == sc_sdev_disk &&
        hd->attached_to == hd_data->edd[u].hd_idx &&
        !hd->rom_id
      ) {
        str_printf(&hd->rom_id, 0, "0x%02x", u + 0x80);
        hd_data->flags.edd_used = 1;
        hd_data->edd[u].assigned = 1;
        break;
      }
    }
  }

  /* try based on disk size */
  for(u = 0; u < sizeof hd_data->edd / sizeof *hd_data->edd; u++) {
    if(hd_data->edd[u].assigned) continue;
    if(!(ul0 = hd_data->edd[u].sectors)) continue;
    for(u1 = u2 = 0; u1 < sizeof hd_data->edd / sizeof *hd_data->edd; u1++) {
      if(ul0 == hd_data->edd[u1].sectors) u2++;
    }

    /* more than one disk with this size */
    if(u2 != 1) continue;

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_storage_device &&
        hd->sub_class.id == sc_sdev_disk &&
        !hd->rom_id
      ) {
        for(res = hd->res; res; res = res->next) {
          if(
            res->any.type == res_size &&
            res->size.unit == size_unit_sectors &&
            res->size.val1 == ul0
          ) break;
        }

        if(!res) continue;

        str_printf(&hd->rom_id, 0, "0x%02x", u + 0x80);
        hd_data->flags.edd_used = 1;
        hd_data->edd[u].assigned = 1;
        break;
      }
    }
  }

  /* set lba support flag in BIOS data */
  for(lba = u = 0; u < sizeof hd_data->edd / sizeof *hd_data->edd; u++) {
    if(hd_data->edd[u].ext_fixed_disk) {
      lba = 1;
      break;
    }
  }

  if(lba) {
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_internal &&
        hd->sub_class.id == sc_int_bios &&
        hd->detail &&
        hd->detail->type == hd_detail_bios &&
        (bt = hd->detail->bios.data)
      ) {
        bt->lba_support = lba;
      }
    }
  }

}

/** @} */


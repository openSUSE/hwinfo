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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * edd
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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
  unsigned u, edd_cnt = 0, lba;
  uint64_t ul0;
  str_list_t *sl;
  bios_info_t *bt;

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

            if(hd_attr_uint(sysfs_get_directory_attribute(sf_dir_2, "sectors"), &ul0, 0)) {
              hd_data->edd[u].sectors = ul0;
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
              "edd: 0x%02x %12"PRIu64" %s%s%s%s#%3u %s\n",
              u + 0x80,
              hd_data->edd[u].sectors,
              hd_data->edd[u].ext_fixed_disk ? "fixed " : "",
              hd_data->edd[u].ext_lock_eject ? "lock " : "",
              hd_data->edd[u].ext_edd ? "edd " : "",
              hd_data->edd[u].ext_64bit ? "64bit " : "",
              hd_data->edd[u].hd_idx,
              hd_data->edd[u].sysfs_id
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


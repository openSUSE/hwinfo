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
  unsigned u, edd_cnt = 0;
  uint64_t ul0;
  struct {
    uint64_t sectors;
    char *sysfs_id;
    unsigned hd_idx;
  } id_list[0x80] = {};

  struct sysfs_directory *sf_dir;
  struct sysfs_directory *sf_dir_2;
  struct sysfs_link *sf_link;

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
              id_list[u].sectors = ul0;
            }

            sf_link = sysfs_get_directory_link(sf_dir_2, "pci_dev");
            if(sf_link) {
              id_list[u].sysfs_id = new_str(hd_sysfs_id(sf_link->target));
              if((hd = hd_find_sysfs_id(hd_data, id_list[u].sysfs_id))) {
                id_list[u].hd_idx = hd->idx;
              }
            }

            ADD2LOG(
              "edd: 0x%02x %12"PRIu64" #%3u %s\n",
              u + 0x80,
              id_list[u].sectors,
              id_list[u].hd_idx,
              id_list[u].sysfs_id
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

  for(u = 0; u < sizeof id_list / sizeof *id_list; u++) {
    if(!id_list[u].hd_idx) continue;
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_storage_device &&
        hd->sub_class.id == sc_sdev_disk &&
        hd->attached_to == id_list[u].hd_idx &&
        !hd->rom_id
      ) {
        str_printf(&hd->rom_id, 0, "0x%02x", u + 0x80);
        break;
      }
    }
  }

  for(u = 0; u < sizeof id_list / sizeof *id_list; u++) {
    free_mem(id_list[u].sysfs_id);
  }

}


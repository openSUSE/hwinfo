#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hd.h"
#include "hd_int.h"
#include "ataraid.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * fix disk list: remove ide devs used by ata soft raid
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


void hd_scan_ataraid(hd_data_t *hd_data)
{
  hd_t *hd;
  driver_info_t *di;
  str_list_t *sl;
  int found, ok, i;
  hd_t *raid_ctrl[16];
  int raid_cnt = 0;
  hd_t *raid_array[16];
  int raid_array_cnt = 0;

  if(!hd_probe_feature(hd_data, pr_ataraid)) return;

  hd_data->module = mod_ataraid;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "fix disk list");

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage
    ) {
      if(!hd->driver_info) hddb_add_info(hd_data, hd);
      for(di = hd->driver_info; di; di = di->next) {
        if(
          di->any.type == di_module &&
          di->module.names
        ) {
          found = 0;
          ok = 1;
          for(sl = di->module.names; sl; sl = sl->next) {
            if(!strcmp(sl->str, "ataraid")) {
              if(!di->module.active) ok = 0;
              found = 1;
            }
          }
          if(found && ok) {
            if((unsigned) raid_cnt < sizeof raid_ctrl / sizeof *raid_ctrl) {
              raid_ctrl[raid_cnt++] = hd;
            }
            break;
          }
        }
      }
    }
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_disk &&
      hd->unix_dev_name
    ) {
      if(strstr(hd->unix_dev_name, "/dev/ataraid") == hd->unix_dev_name) {
        if((unsigned) raid_array_cnt < sizeof raid_array / sizeof *raid_array) {
          raid_array[raid_array_cnt++] = hd;
        }
      }
    }
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_disk
    ) {
      for(i = 0; i < raid_cnt; i++) {
        if(hd->attached_to == raid_ctrl[i]->idx) {
          hd->is.softraiddisk = 1;
          break;
        }
      }
    }

  }

}



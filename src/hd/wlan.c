#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hd.h"
#include "hd_int.h"
#include "wlan.h"

void hd_scan_wlan(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res;

  // nothing to do yet
  return;

  if(!hd_probe_feature(hd_data, pr_wlan)) return;

  hd_data->module = mod_wlan;

  PROGRESS(1, 0, "detecting wlan features");

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_network &&
      hd->unix_dev_name &&
      1 /* hd->is.wlan */
    ) {
      // do something

      ADD2LOG("*** wlan features for %s ***\n", hd->unix_dev_name);

      res = new_mem(sizeof *res);
      res->any.type = res_wlan;
      res->wlan.auth_modes = new_str("mode1 mode2");
      // or: add_str_list(&res->wlan.auth_modes, "mode1");
      add_res_entry(&hd->res, res);

    }
  }

}


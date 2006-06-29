#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hd.h"
#include "hd_int.h"
#include "veth.h"

/**
 * @defgroup VETHint iSeries veth devices
 * @ingroup libhdDEVint
 * @brief iSeries veth device functions
 *
 * @{
 */

#if defined(__PPC__)

void hd_scan_veth(hd_data_t *hd_data)
{
  unsigned u;
  hd_t *hd;
  DIR *dir;
  struct dirent *de;

  if(!hd_probe_feature(hd_data, pr_veth)) return;

  hd_data->module = mod_veth;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "read data");

  if((dir = opendir(PROC_ISERIES_VETH))) {
    while((de = readdir(dir))) {
      if(sscanf(de->d_name, "veth%u", &u) == 1) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_network;
        hd->slot = u;
        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);	// IBM
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1000);
        str_printf(&hd->device.name, 0, "Virtual Ethernet card %d", hd->slot);
      }
    }
    closedir(dir);
    return;
  }
  if((dir = opendir(PROC_ISERIES))) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_network;
    hd->slot = 0;
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);	// IBM
    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1000);
    str_printf(&hd->device.name, 0, "Virtual Ethernet card %d", hd->slot);
  }

}

#endif	/* __PPC__ */

/** @} */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "cciss.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Compaq RAID
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void hd_scan_cciss(hd_data_t *hd_data)
{
  hd_t *hd;
  unsigned u0, u1;
  hd_res_t *geo, *size;
  int fd;
  char *s = NULL;

  if(!hd_probe_feature(hd_data, pr_cciss)) return;

  hd_data->module = mod_cciss;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "read info");

  for(u0 = 0; u0 < 4; u0++) {
    for(u1 = 0; u1 < 16; u1++) {
      str_printf(&s, 0, DEV_CCISS "/c%ud%u", u0, u1);
      fd = open(s, O_RDONLY | O_NONBLOCK);
      if(fd >= 0) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_storage_device;
        hd->bus.id = bus_none;
        hd->slot = (u0 << 8) + u1;

        hd->sub_class.id = sc_sdev_disk;
        hd->unix_dev_name = s; s = NULL;

        str_printf(&hd->device3.name, 0, "CCISS disk %u/%u", u0, u1);

        PROGRESS(2, u1, "ioctl");

        hd_getdisksize(hd_data, hd->unix_dev_name, fd, &geo, &size);
      
        if(geo) add_res_entry(&hd->res, geo);
        if(size) add_res_entry(&hd->res, size);

        close(fd);
      }
    }
  }

  free_mem(s);
}


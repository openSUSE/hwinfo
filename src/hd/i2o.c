#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <linux/hdreg.h>

#include "hd.h"
#include "hd_int.h"
#include "i2o.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Intelligent I/O
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void hd_scan_i2o(hd_data_t *hd_data)
{
  hd_t *hd;
  unsigned u;
  hd_res_t *res;
  struct hd_geometry geo;
  int fd;
  unsigned long secs;
  char *s = NULL;

  if(!hd_probe_feature(hd_data, pr_i2o)) return;

  hd_data->module = mod_i2o;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "read info");

  for(u = 0; u < 16; u++) {
    str_printf(&s, 0, DEV_I2O "/hd%c", 'a' + u);
    fd = open(s, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_storage_device;
      hd->bus = bus_none;
      hd->slot = u;

      hd->sub_class = sc_sdev_disk;
      hd->unix_dev_name = s; s = NULL;

      str_printf(&hd->dev_name, 0, "I2O disk %u", u);

      PROGRESS(2, u, "ioctl");
      if(!ioctl(fd, HDIO_GETGEO, &geo)) {
        ADD2LOG("i2o ioctl(geo) ok\n");
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->disk_geo.type = res_disk_geo;
        res->disk_geo.cyls = geo.cylinders;
        res->disk_geo.heads = geo.heads;
        res->disk_geo.sectors = geo.sectors;
        res->disk_geo.logical = 1;
      }

      if(!ioctl(fd, BLKGETSIZE, &secs)) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->size.type = res_size;
        res->size.unit = size_unit_sectors;
        res->size.val1 = secs;
        res->size.val2 = 512;
      }

      close(fd);
    }
  }
}


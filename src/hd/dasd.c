#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>

#include "hd.h"
#include "hd_int.h"
#include "dasd.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * s390 disk info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__s390__) || defined(__s390x__)

void hd_scan_dasd(hd_data_t *hd_data)
{
  hd_t *hd;
  char c;
  unsigned u0, u2, u3;
  str_list_t *sl, *sl0;
  hd_res_t *res;
  struct hd_geometry geo;
  int i, fd;

  if(!hd_probe_feature(hd_data, pr_dasd)) return;

  hd_data->module = mod_dasd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "read info");

  sl0 = read_file(PROC_DASD "/devices", 0, 0);

  i = 1;
  for(sl = sl0; sl; sl = sl->next) {
    if(sscanf(sl->str, "%x%*s at (%*u:%*u) is dasd%c:active at blocksize: %u, %u blocks", &u0, &c, &u2, &u3) == 4) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_storage_device;
      hd->bus.id = bus_none;
      hd->slot = (u0 >> 8) & 0xff;
      hd->func = u0 & 0xff;

      hd->sub_class.id = sc_sdev_disk;

      hd->device.name = new_str("S390 Disk");
      str_printf(&hd->unix_dev_name, 0, "/dev/dasd%c", c);
      str_printf(&hd->rom_id, 0, "%04X", u0);

      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->size.type = res_size;
      res->size.unit = size_unit_sectors;
      res->size.val1 = u3;
      res->size.val2 = u2;

      fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK);
      if(fd >= 0) {
        PROGRESS(2, i++, "ioctl");
        if(!ioctl(fd, HDIO_GETGEO, &geo)) {
          ADD2LOG("dasd ioctl(geo) ok\n");
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->disk_geo.type = res_disk_geo;
          res->disk_geo.cyls = geo.cylinders;
          res->disk_geo.heads = geo.heads;
          res->disk_geo.sectors = geo.sectors;
          res->disk_geo.logical = 1;
        }
        close(fd);
      }
    }
  }
  free_str_list(sl0);

  if(i > 1) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_storage;
    hd->sub_class.id = sc_sto_other;
    hd->vendor.name = new_str("IBM");
    hd->device.name = new_str("VIO DASD");
  }
}

#endif	/* defined(__s390__) || defined(__s390x__) */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "disk.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * get some generic disk info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void hd_scan_disk(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *geo, *size;
  int fd;
  char *s = NULL;
  str_list_t *sl;
  unsigned u0, u1;
  char c;

  if(!hd_probe_feature(hd_data, pr_disk)) return;

  hd_data->module = mod_disk;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "get info");

  for(sl = hd_data->disks; sl; sl = sl->next) {
    str_printf(&s, 0, "/dev/%s", sl->str);

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->base_class != bc_storage_device || !hd->unix_dev_name) continue;
      if(!strcmp(hd->unix_dev_name, s)) break;
    }

    if(hd) continue;

    fd = open(s, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_storage_device;
      hd->bus = bus_none;

      hd->sub_class = sc_sdev_disk;
      hd->unix_dev_name = s; s = NULL;

      str_printf(&hd->dev_name, 0, "Disk");
      if(sscanf(sl->str, "ataraid/d%u", &u0) == 1) {
        hd->slot = u0;
        str_printf(&hd->dev_name, 0, "IDE RAID Array %u", u0);
      }
      else if(sscanf(sl->str, "cciss/d%uc%u", &u0, &u1) == 2) {
        hd->slot = (u0 << 8) + u1;
        str_printf(&hd->dev_name, 0, "CCISS disk %u/%u", u0, u1);
      }
      else if(sscanf(sl->str, "hd%c", &c) == 1) {
        u0 = c - 'a';
        hd->slot = u0;
        str_printf(&hd->dev_name, 0, "IDE Disk %u", u0);
      }
      else if(sscanf(sl->str, "sd%c", &c) == 1) {
        u0 = c - 'a';
        hd->slot = u0;
        str_printf(&hd->dev_name, 0, "SCSI Disk %u", u0);
      }

      hd_getdisksize(hd_data, hd->unix_dev_name, fd, &geo, &size);

      if(geo) add_res_entry(&hd->res, geo);
      if(size) add_res_entry(&hd->res, size);

      close(fd);
    }
  }
  s = free_mem(s);
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/hdreg.h>

#include "hd.h"
#include "hd_int.h"
#include "smart.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Compaq SMART2 RAID controller info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static void dump_smart_data(hd_data_t *hd_data, char *fname, str_list_t *sl);

void hd_scan_smart(hd_data_t *hd_data)
{
  hd_t *hd;
  char *fname = NULL;
  str_list_t *sl, *sl0;
  int i, j, fd;
  unsigned u0, u1, u2, u3;
  unsigned long secs;
  hd_res_t *res;
  struct hd_geometry geo;
  char *cpqarray;
  DIR *dir;

  if(!hd_probe_feature(hd_data, pr_smart)) return;

  hd_data->module = mod_smart;

  /* some clean-up */
  remove_hd_entries(hd_data);

  cpqarray = hd_data->kernel_version != KERNEL_22 ? PROC_SMART_24 : PROC_SMART_22;

  if((dir = opendir(cpqarray))) {
    closedir(dir);
  }
  else {
    cpqarray = PROC_SMART_24_NEW;
    if((dir = opendir(cpqarray))) {
      closedir(dir);
    }
    else {
      return;
    }
  }

  for(i = 0; i < 8; i++) {
    PROGRESS(1, 1 + i, "read info");

    str_printf(&fname, 0, "%s/ida%i", cpqarray, i);
    if(!(sl0 = read_file(fname, 0, 0))) continue;

    if(hd_data->debug) dump_smart_data(hd_data, fname, sl0);

    for(sl = sl0; sl; sl = sl->next) {
      if(strstr(sl->str, "Logical Drive Info:")) {
        sl = sl->next;
        break;
      }
    }

    for(j = 0; sl; sl = sl->next, j++) {
      if(sscanf(sl->str, " ida/c%ud%u: blksz=%u nr_blks=%u", &u0, &u1, &u2, &u3) == 4) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_storage_device;
        hd->sub_class.id = sc_sdev_disk;
        hd->bus.id = bus_raid;
        hd->slot = u0;
        hd->func = u1;
        str_printf(&hd->unix_dev_name, 0, DEV_SMART "/c%ud%u", hd->slot, hd->func);

        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->disk_geo.type = res_disk_geo;
        res->disk_geo.logical = 1;

        secs = 0; geo.cylinders = 0; fd = -1;
        if((fd = open(hd->unix_dev_name, O_RDONLY)) >= 0) {
          if(!ioctl(fd, BLKGETSIZE, &secs)) {
            ADD2LOG("smart ioctl(secs) ok\n");
          }

          if(!ioctl(fd, HDIO_GETGEO, &geo)) {
            ADD2LOG("smart ioctl(geo) ok\n");
            res->disk_geo.cyls = geo.cylinders;
            res->disk_geo.heads = geo.heads;
            res->disk_geo.sectors = geo.sectors;
          }
        }

        ADD2LOG("smart open %s\n", fd >= 0 ? "ok" : "failed");
        if(fd >= 0) close(fd);

        if(!geo.cylinders) {
          res->disk_geo.heads = 255;
          res->disk_geo.sectors = 63;
          res->disk_geo.cyls = u3 / (63 * 255);
        }

        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->size.type = res_size;
        res->size.unit = size_unit_sectors;
        if(secs) {
          res->size.val1 = secs;
          res->size.val2 = 512;
        }
        else {
          res->size.val1 = u3;
          res->size.val2 = u2;
        }
      }
      else {
        break;
      }
    }

    sl0 = free_str_list(sl0);
  }

  free_mem(fname);
}

void dump_smart_data(hd_data_t *hd_data, char *fname, str_list_t *sl)
{
  ADD2LOG("----- %s -----\n", fname);
  for(; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- %s end -----\n", fname);
}


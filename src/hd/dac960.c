#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/hdreg.h>

#include "hd.h"
#include "hd_int.h"
#include "dac960.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Mylex RAID controller info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static void dump_dac960_data(hd_data_t *hd_data, char *fname, str_list_t *sl);

void hd_scan_dac960(hd_data_t *hd_data)
{
  hd_t *hd, *hd2;
  char *fname = NULL;
  char buf0[32], buf1[32];
  str_list_t *sl, *sl0;
  int i, j, fd;
  unsigned u0, u1, u2, secs, heads;
  unsigned long lsecs;
  unsigned pci_slot, pci_func;
  hd_res_t *res;
  struct hd_geometry geo;

  if(!hd_probe_feature(hd_data, pr_dac960)) return;

  hd_data->module = mod_dac960;

  /* some clean-up */
  remove_hd_entries(hd_data);

  for(i = 0; i < 8; i++) {
    PROGRESS(1, 1 + i, "read info");

    str_printf(&fname, 0, PROC_DAC960 "/c%d/current_status", i);
    if(!(sl0 = read_file(fname, 0, 0))) {
      str_printf(&fname, 0, PROC_DAC960 "/c%d/initial_status", i);
      sl0 = read_file(fname, 0, 0);
    }

    if(!sl0) continue;

    if(hd_data->debug) dump_dac960_data(hd_data, fname, sl0);

    for(heads = secs = pci_slot = pci_func = 0, sl = sl0; sl; sl = sl->next) {
      if(sscanf(sl->str, " PCI Bus: %u, Device: %u, Function: %u", &u0, &u1, &u2) == 3) {
        pci_slot = (u0 << 8) + u1;
        pci_func = u2;
      }
      if(sscanf(sl->str, " Stripe Size: %*uKB, Segment Size: %*uKB, BIOS Geometry: %u/%u", &u0, &u1) == 2) {
        heads = u0;
        secs = u1;
      }
      if(strstr(sl->str, " Logical Drives:")) {
        sl = sl->next;
        break;
      }
    }

    for(j = 0; sl; sl = sl->next, j++) {
      if(sscanf(sl->str, " /dev/rd/c%ud%u: %31s %31s %u blocks,", &u0, &u1, buf0, buf1, &u2) == 5) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_storage_device;
        hd->sub_class = sc_sdev_disk;
        hd->bus = bus_raid;
        hd->slot = u0;
        hd->func = u1;
        str_printf(&hd->unix_dev_name, 0, DEV_DAC960 "/c%ud%u", hd->slot, hd->func);

        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->disk_geo.type = res_disk_geo;
        res->disk_geo.logical = 1;

        lsecs = 0; geo.cylinders = 0; fd = -1;
        if((fd = open(hd->unix_dev_name, O_RDONLY)) >= 0) {
          if(!ioctl(fd, BLKGETSIZE, &lsecs)) {
            ADD2LOG("dac960 ioctl(secs) ok\n");
          }

          if(!ioctl(fd, HDIO_GETGEO, &geo)) {
            ADD2LOG("dac960 ioctl(geo) ok\n");
            res->disk_geo.cyls = geo.cylinders;
            res->disk_geo.heads = geo.heads;
            res->disk_geo.sectors = geo.sectors;
          }
        }

        if(fd >= 0) close(fd);

        if(!geo.cylinders) {
          if(secs && heads) {
            res->disk_geo.cyls = u2 / (heads * secs);
            res->disk_geo.heads = heads;
            res->disk_geo.sectors = secs;
          }
          else {
            res->disk_geo.heads = 255;
            res->disk_geo.sectors = 63;
            res->disk_geo.cyls = u2 / (63 * 255);
          }
        }

        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->size.type = res_size;
        res->size.unit = size_unit_sectors;
        res->size.val1 = lsecs ? lsecs : u2;
        res->size.val2 = 512;

        if(pci_slot || pci_func) {
          for(hd2 = hd_data->hd; hd2; hd2 = hd2->next) {
            if(
              hd2->bus == bus_pci &&
              hd2->slot == pci_slot &&
              hd2->func == pci_func
            ) {
              hd->attached_to = hd2->idx;
              break;
            }
          }
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

void dump_dac960_data(hd_data_t *hd_data, char *fname, str_list_t *sl)
{
  ADD2LOG("----- %s -----\n", fname);
  for(; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- %s end -----\n", fname);
}


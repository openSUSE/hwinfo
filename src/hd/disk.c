#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/pci.h>

#if 1	/* ###### until cciss_ioctl.h gets fixed */
#include <linux/types.h>
#include <linux/ioctl.h>
#define CCISS_IOC_MAGIC 'B'
#define CCISS_GETPCIINFO _IOR(CCISS_IOC_MAGIC, 1, cciss_pci_info_struct)
typedef struct _cciss_pci_info_struct
{
  unsigned char bus;
  unsigned char dev_fn;
  __u32 board_id;
} cciss_pci_info_struct; 
#else
#include <linux/cciss_ioctl.h>
#endif

#include "hd.h"
#include "hd_int.h"
#include "disk.h"

#ifndef IDAGETPCIINFO
#define IDAGETPCIINFO	0x32323333
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * get some generic disk info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void hd_scan_disk(hd_data_t *hd_data)
{
  hd_t *hd, *hd2;
  hd_res_t *geo, *size;
  int fd;
  char *s = NULL;
  str_list_t *sl;
  unsigned u0, u1;
  char c;
  unsigned char buf[64];
  unsigned pci_slot, pci_func, pci_info;

  if(!hd_probe_feature(hd_data, pr_disk)) return;

  hd_data->module = mod_disk;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "get info");

  for(sl = hd_data->disks; sl; sl = sl->next) {
    pci_info = pci_slot = pci_func = 0;

    /* skip md and lvm devices */
    if(
      !hd_data->flags.list_md && 
      (!strncmp(sl->str, "lvm", 3) || !strncmp(sl->str, "md", 2))
    ) continue;

    str_printf(&s, 0, "/dev/%s", sl->str);

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->base_class.id != bc_storage_device || !hd->unix_dev_name) continue;
      if(!strcmp(hd->unix_dev_name, s)) break;
    }

    if(hd) continue;

    fd = open(s, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_storage_device;
      hd->bus.id = bus_none;

      hd->sub_class.id = sc_sdev_disk;
      hd->unix_dev_name = s; s = NULL;

      str_printf(&hd->device.name, 0, "Disk");
      if(sscanf(sl->str, "ataraid/d%u", &u0) == 1) {
        hd->slot = u0;
        hd->bus.id = bus_raid;
        str_printf(&hd->device.name, 0, "IDE RAID Array %u", u0);
      }
      else if(sscanf(sl->str, "cciss/c%ud%u", &u0, &u1) == 2) {
        hd->slot = (u0 << 8) + u1;
        str_printf(&hd->device.name, 0, "CCISS disk %u/%u", u0, u1);
        if(!ioctl(fd, CCISS_GETPCIINFO, buf)) {
          pci_info = 1;
          pci_slot = PCI_SLOT(buf[1]) + (buf[0] << 8);
          pci_func = PCI_FUNC(buf[1]);
          ADD2LOG("  cciss pci ioctl: ");
          hexdump(&hd_data->log, 0, 8, buf);
          ADD2LOG("\n");
        }
      }
      else if(sscanf(sl->str, "rd/c%ud%u", &u0, &u1) == 2) {
        hd->slot = (u0 << 8) + u1;
        hd->bus.id = bus_raid;
        str_printf(&hd->device.name, 0, "DAC960 RAID Array %u/%u", u0, u1);
      }
      else if(sscanf(sl->str, "i2o/hd%c", &c) == 1) {
        u0 = c - 'a';
        hd->slot = u0;
        str_printf(&hd->device.name, 0, "I2O disk %u", u0);
      }
      else if(sscanf(sl->str, "hd%c", &c) == 1) {
        u0 = c - 'a';
        hd->slot = u0;
        str_printf(&hd->device.name, 0, "IDE Disk %u", u0);
      }
      else if(sscanf(sl->str, "sd%c", &c) == 1) {
        u0 = c - 'a';
        hd->slot = u0;
        str_printf(&hd->device.name, 0, "SCSI Disk %u", u0);
      }

      hd_getdisksize(hd_data, hd->unix_dev_name, fd, &geo, &size);

      if(geo) add_res_entry(&hd->res, geo);
      if(size) add_res_entry(&hd->res, size);

      if(pci_info) {
        ADD2LOG(
          "  pci info: bus = 0x%x, slot = 0x%x, func = 0x%x\n",
          pci_slot >> 8, pci_slot & 0xff, pci_func
        );
        for(hd2 = hd_data->hd; hd2; hd2 = hd2->next) {
          if(hd2->bus.id == bus_pci && hd2->slot == pci_slot && hd2->func == pci_func) {
            hd->attached_to = hd2->idx;
          }
        }
      }

      close(fd);
    }
  }
  s = free_mem(s);
}


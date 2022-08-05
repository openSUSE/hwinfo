#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/iso_fs.h>
#include <linux/cdrom.h>
#include <scsi/sg.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "block.h"
#include "dvd.h"

/**
 * @defgroup BLOCKint Block devices
 * @ingroup  libhdDEVint
 * @brief Block device functions
 *
 * @{
 */

static void get_block_devs(hd_data_t *hd_data);
static void add_partitions(hd_data_t *hd_data, hd_t *hd, char *path);
static void add_cdrom_info(hd_data_t *hd_data, hd_t *hd);
static void add_other_sysfs_info(hd_data_t *hd_data, hd_t *hd);
static void add_ide_sysfs_info(hd_data_t *hd_data, hd_t *hd);
static void add_scsi_sysfs_info(hd_data_t *hd_data, hd_t *hd, char *sf_dev);
static void read_partitions(hd_data_t *hd_data);
static void read_cdroms(hd_data_t *hd_data);
static cdrom_info_t *new_cdrom_entry(cdrom_info_t **ci);
static cdrom_info_t *get_cdrom_entry(cdrom_info_t *ci, int n);
static void get_scsi_tape(hd_data_t *hd_data);
static void get_generic_scsi_devs(hd_data_t *hd_data);
static void add_disk_size(hd_data_t *hd_data, hd_t *hd);


void hd_scan_sysfs_block(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_block)) return;

  hd_data->module = mod_block;

  /* some clean-up */
  remove_hd_entries(hd_data);

  hd_data->disks = free_str_list(hd_data->disks);
  hd_data->partitions = free_str_list(hd_data->partitions);
  hd_data->cdroms = free_str_list(hd_data->cdroms);

  if(hd_probe_feature(hd_data, pr_block_mods)) {
    PROGRESS(1, 0, "block modules");
    // load_module(hd_data, "ide-cd");
    load_module(hd_data, "ide-cd_mod");
    load_module(hd_data, "ide-disk");
    load_module(hd_data, "sr_mod");
    load_module(hd_data, "sd_mod");
#if !defined(__s390__) && !defined(__s390x__)
    load_module(hd_data, "st");
#endif
  }

  PROGRESS(2, 0, "sysfs drivers");

  hd_sysfs_driver_list(hd_data);

  PROGRESS(3, 0, "cdrom");

  read_cdroms(hd_data);

  PROGRESS(4, 0, "partition");

  read_partitions(hd_data);

  PROGRESS(5, 0, "get sysfs block dev data");

  get_block_devs(hd_data);

  if(hd_data->cdrom) {
    ADD2LOG("oops: cdrom list not empty\n");
  }
}


void get_block_devs(hd_data_t *hd_data)
{
  str_list_t *sl;
  char *s, *t;
  unsigned u1, u2, u3;
  uint64_t ul0;
  hd_t *hd, *hd1;
  hd_dev_num_t dev_num;
  str_list_t *sf_class, *sf_class_e;
  char *sf_cdev = NULL, *sf_dev = NULL, *sf_dev_ide;
  char *sf_drv_name, *sf_drv, *bus_id, *bus_name, *ide_bus_id;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_block_dir;

  hd_data->lsscsi = free_str_list(hd_data->lsscsi);
  hd_data->lsscsi = read_file("|/usr/bin/lsscsi -t 2>/dev/null", 0, 0);

  ADD2LOG("-----  lsscsi -----\n");
  for(sl = hd_data->lsscsi; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("-----  lsscsi end -----\n");

  sf_bus = read_dir("/sys/bus/ide/devices", 'l');

  if(sf_bus) {
    for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
      sf_dev = new_str(hd_read_sysfs_link("/sys/bus/ide/devices", sf_bus_e->str));
      ADD2LOG(
        "  ide: bus_id = %s path = %s\n",
        sf_bus_e->str,
        hd_sysfs_id(sf_dev)
      );
      free_mem(sf_dev);
    }
  }

  sf_block_dir = "/sys/subsystem/block";
  sf_class = read_dir(sf_block_dir, 'l');

  if (!sf_class) {
    sf_block_dir = "/sys/class/block";
    sf_class = read_dir(sf_block_dir, 'l');
  }

  if (!sf_class) {
    sf_block_dir = "/sys/block";
    sf_class = read_dir(sf_block_dir, 'd');
  }

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: block\n");
    return;
  }

  for(sf_class_e = sf_class; sf_class_e; sf_class_e = sf_class_e->next) {
    str_printf(&sf_cdev, 0, "%s/%s", sf_block_dir, sf_class_e->str);
    ADD2LOG(
      "  block: name = %s, path = %s\n",
      sf_class_e->str,
      hd_sysfs_id(sf_cdev)
    );

    memset(&dev_num, 0, sizeof dev_num);

    if((s = get_sysfs_attr_by_path(sf_cdev, "dev"))) {
      if(sscanf(s, "%u:%u", &u1, &u2) == 2) {
        dev_num.type = 'b';
        dev_num.major = u1;
        dev_num.minor = u2;
        dev_num.range = 1;
      }
      ADD2LOG("    dev = %u:%u\n", u1, u2);
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_cdev, "range"), &ul0, 0)) {
      dev_num.range = ul0;
      ADD2LOG("    range = %u\n", dev_num.range);
    }

    sf_dev = new_str(hd_read_sysfs_link(sf_cdev, "device"));

    bus_name = NULL;
    if(
      (s = hd_read_sysfs_link(sf_dev, "subsystem")) ||
      (s = hd_read_sysfs_link(sf_dev, "bus"))
    ) {
      bus_name = strrchr(s, '/');
      if(bus_name) bus_name++;
      bus_name = new_str(bus_name);
      /* if bus_name is nvme-subsystem reconsider sf_dev value */
      if(!strcmp(bus_name, "nvme-subsystem")) {
        for(sl = read_dir(sf_dev, 'l'); sl; sl = sl->next) {
          if(!strncmp(sl->str, "nvme", 4)) {
            sf_dev = new_str(hd_read_sysfs_link(sf_dev, sl->str));
          }
        }
      }
    }

    sf_drv_name = NULL;
    sf_drv = hd_read_sysfs_link(sf_dev, "driver");
    if(!sf_drv) {
      /* maybe older kernel */
      sf_drv = hd_read_sysfs_link(sf_cdev, "driver");
    }
    if(sf_drv) {
      sf_drv_name = strrchr(sf_drv, '/');
      if(sf_drv_name) sf_drv_name++;
    }

    sf_drv_name = new_str(sf_drv_name);

    bus_id = sf_dev ? strrchr(sf_dev, '/') : NULL;
    if(bus_id) bus_id++;


    if(sf_dev) {
      ADD2LOG(
        "    block device: bus = %s, bus_id = %s driver = %s\n      path = %s\n",
        bus_name,
        bus_id,
        sf_drv_name,
        hd_sysfs_id(sf_dev)
      );
    }

    hd = NULL;

#if defined(__s390x__) || defined(__s390__)
    /* check if disk is DASD and has already been found by s390.c */
    if(sf_drv_name && strstr(sf_drv_name,"dasd"))
    {
      char bid[9];
      hd_res_t* res;
      //fprintf(stderr,"dn %s bi %s\n",sf_drv_name,bus_id);
      for(hd=hd_data->hd;hd;hd=hd->next)
      {
	//fprintf(stderr,"bcid %d\n",hd->base_class.id);
	if(hd->base_class.id == bc_storage_device
	   && hd->detail
	   && hd->detail->ccw.type == hd_detail_ccw)
	{
	  for(res=hd->res;res;res=res->next)
	  {
	    if(res->io.type==res_io)
	    {
	      sprintf(bid,"%01x.%01x.%04x",
		      hd->detail->ccw.data->lcss >> 8,
		      hd->detail->ccw.data->lcss & 0xff,
		      (unsigned short)res->io.base);
	      //fprintf(stderr,"bid %s\n",bid);
	      if (strcmp(bid,bus_id)==0) goto out;
	    }
	  }
	}
      }
      hd=NULL;
      out:;
    }
    else
#endif

    if((sl = search_str_list(hd_data->disks, hd_sysfs_name2_dev(sf_class_e->str)))) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->sub_class.id = sc_sdev_disk;
    }
    else if((sl = search_str_list(hd_data->cdroms, hd_sysfs_name2_dev(sf_class_e->str)))) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->sub_class.id = sc_sdev_cdrom;
    }
    else if(
      bus_name &&
      (!strcmp(bus_name, "scsi") || !strcmp(bus_name, "ide"))
    ) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->sub_class.id = sc_sdev_other;
    }

    if(hd) {
      str_printf(&hd->unix_dev_name, 0, "/dev/%s", hd_sysfs_name2_dev(sf_class_e->str));

      hd->base_class.id = bc_storage_device;

      hd->sysfs_id = new_str(hd_sysfs_id(sf_cdev));

      hd->sysfs_device_link = new_str(hd_sysfs_id(sf_dev));

      hd->unix_dev_num = dev_num;

      hd->bus.id = bus_none;

      if(bus_name) {
        if(!strcmp(bus_name, "ide")) hd->bus.id = bus_ide;
        else if(!strcmp(bus_name, "scsi")) hd->bus.id = bus_scsi;
        else if(!strcmp(bus_name, "pci")) hd->bus.id = bus_pci;
        else if(!strcmp(bus_name, "nvme")) hd->bus.id = bus_nvme;
        else if(!strcmp(bus_name, "nvme-subsystem")) hd->bus.id = bus_nvme;
      }
      hd->sysfs_bus_id = new_str(bus_id);

      // add model name, if available
      if((s = get_sysfs_attr_by_path(sf_dev, "model"))) {
        char *cs = canon_str(s, strlen(s));
        ADD2LOG("    model = %s\n", cs);
        if(*cs) {
          hd->device.name = cs;
        }
        else {
          free_mem(cs);
        }
      }

      // add serial, if available
      if((s = get_sysfs_attr_by_path(sf_dev, "serial"))) {
        char *cs = canon_str(s, strlen(s));
        ADD2LOG("    serial = %s\n", cs);
        if(*cs) {
          hd->serial = cs;
        }
        else {
          free_mem(cs);
        }
      }

      if((s = hd_sysfs_id(sf_dev))) {

        /* parent has longest matching sysfs id */
        u2 = strlen(s);
        for(u3 = 0, hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
          if(hd1->sysfs_id) {
            u1 = strlen(hd1->sysfs_id);
            if(u1 > u3 && u1 <= u2 && !strncmp(s, hd1->sysfs_id, u1)) {
              u3 = u1;
              hd->attached_to = hd1->idx;
            }
          }
        }

        /* find longest matching sysfs id we have a driver for */
        s = new_str(s);
        t = strrchr(s, '/');
        if(t) *t = 0;
        t = hd_sysfs_find_driver(hd_data, s, 0);
        if(t) {
          add_str_list(&hd->drivers, t);
        }
        s = free_mem(s);

        /* look for ide-scsi handled devices */
        if(hd->bus.id == bus_scsi) {
          for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
            sf_dev_ide = new_str(hd_read_sysfs_link("/sys/bus/ide/devices", sf_bus_e->str));
            ide_bus_id = sf_dev_ide ? strrchr(sf_dev_ide, '/') : NULL;
            if(ide_bus_id) ide_bus_id++;

            if(
              strcmp(sf_dev, sf_dev_ide) &&
              !strncmp(sf_dev, sf_dev_ide, strlen(sf_dev_ide)) &&
              sscanf(ide_bus_id, "%u.%u", &u1, &u2) == 2
            ) {
              str_printf(&hd->unix_dev_name2, 0, "/dev/hd%c", 'a' + (u1 << 1) + u2);
            }

            sf_dev_ide = free_mem(sf_dev_ide);
          }
        }
      }

      /*
       * set hd->drivers before calling any of add_xxx_sysfs_info()
       */
      if(sf_drv_name) {
        add_str_list(&hd->drivers, sf_drv_name);
      }

      if(hd->bus.id == bus_ide) {
        add_ide_sysfs_info(hd_data, hd);
      }
      else if(hd->bus.id == bus_scsi || hd->bus.id == bus_pci || hd->bus.id == bus_nvme) {
        /*
         * In case there's no data in the 'device' subdir but in
         * 'device/device', try one level deeper (for some capricious
         * drivers).
         */
        if(
          !get_sysfs_attr_by_path(sf_dev, "vendor") &&
          get_sysfs_attr_by_path(sf_dev, "device/vendor")
        ) {
          char *x_dev = NULL;

          str_printf(&x_dev, 0, "%s/device", sf_dev);
          add_scsi_sysfs_info(hd_data, hd, x_dev);

          free_mem(x_dev);
        }
        else {
          add_scsi_sysfs_info(hd_data, hd, sf_dev);
        }
      }
      else {
        add_other_sysfs_info(hd_data, hd);
      }
      
      if(hd->sub_class.id == sc_sdev_cdrom) {
        add_cdrom_info(hd_data, hd);
      }

      if(
        hd->sub_class.id == sc_sdev_disk &&
        hd_probe_feature(hd_data, pr_block_part)
      ) {
        add_partitions(hd_data, hd, sf_cdev);
      }
    }

    sf_drv_name = free_mem(sf_drv_name);
    bus_name = free_mem(bus_name);
    sf_dev = free_mem(sf_dev);
  }

  sf_cdev = free_mem(sf_cdev);
  sf_class = free_str_list(sf_class);
  sf_bus = free_str_list(sf_bus);
}


/*
 * Find driver for sysfs_id.
 *
 * Return driver for id (exact = 1) or longest matching id (exact = 0).
 */
char *hd_sysfs_find_driver(hd_data_t *hd_data, char *sysfs_id, int exact)
{
  hd_sysfsdrv_t *sf;
  char *t;
  unsigned u1, u2, u3;

  if(!sysfs_id || !*sysfs_id) return NULL;

  t = NULL;

  if(exact) {
    for(sf = hd_data->sysfsdrv; sf; sf = sf->next) {
      if(sf->device && !strcmp(sysfs_id, sf->device)) {
        t = sf->driver;
        break;
      }
    }
  }
  else {
    u2 = strlen(sysfs_id);
    u3 = 0;
    for(sf = hd_data->sysfsdrv; sf; sf = sf->next) {
      if(!sf->device) continue;
      u1 = strlen(sf->device);
      if(u1 > u3 && u1 <= u2 && !strncmp(sysfs_id, sf->device, u1)) {
        u3 = u1;
        t = sf->driver;
      }
    }
  }

  return t;
}


void add_partitions(hd_data_t *hd_data, hd_t *hd, char *path)
{
  hd_t *hd1;
  str_list_t *sl;
  char *s;
  size_t len;

  s = hd->unix_dev_name + sizeof "/dev/" - 1;
  len = strlen(s);
  for(sl = hd_data->partitions; sl; sl = sl->next) {
    if(!strncmp(sl->str, s, len)) {
      hd1 = add_hd_entry(hd_data, __LINE__, 0);
      hd1->base_class.id = bc_partition;
      str_printf(&hd1->unix_dev_name, 0, "/dev/%s", sl->str);
      hd1->attached_to = hd->idx;

      str_printf(&hd1->sysfs_id, 0, "%s/%s", hd->sysfs_id, hd_sysfs_dev2_name(sl->str));
    }
  }
}


void add_cdrom_info(hd_data_t *hd_data, hd_t *hd)
{
  cdrom_info_t *ci, **prev;
  int fd, caps, caps2;

  hd->detail = free_hd_detail(hd->detail);
  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_cdrom;

  for(ci = *(prev = &hd_data->cdrom); ci; ci = *(prev = &ci->next)) {
    if(!strcmp(hd->unix_dev_name + sizeof "/dev/" - 1, ci->name)) {
      hd->detail->cdrom.data = ci;
      *prev = ci->next;
      hd->detail->cdrom.data->next = NULL;
      break;
    }
  }

  if((ci = hd->detail->cdrom.data)) {
    fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK);
    caps = caps2 = 0;
    if(fd >= 0) {
      caps = ioctl(fd, CDROM_GET_CAPABILITY, 0);
      ADD2LOG("  cdrom caps(%s): 0x%x\n", hd->unix_dev_name, caps);
      if(caps >= 0) {
        if(caps & CDC_CD_R) hd->is.cdr = 1;
        if(caps & CDC_CD_RW) hd->is.cdrw = 1;
        if(caps & CDC_DVD_R) hd->is.dvdr = 1;
        if(caps & CDC_DVD_RAM) hd->is.dvdram = 1;
        if(caps & CDC_MO_DRIVE) hd->is.mo = 1;
        if(caps & CDC_MRW) hd->is.mrw = 1;
        if(caps & CDC_MRW_W) hd->is.mrww = 1;
        if(caps & CDC_DVD) {
          hd->is.dvd = 1;
          caps2 = get_dvd_profile(fd);
          ADD2LOG("  dvd caps(%s): 0x%x\n", hd->unix_dev_name, caps2);
          if(caps2 & DRIVE_CDROM_CAPS_DVDRW) hd->is.dvdrw = 1;
          if(caps2 & DRIVE_CDROM_CAPS_DVDRDL) hd->is.dvdrdl = 1;
          if(caps2 & DRIVE_CDROM_CAPS_DVDPLUSR) hd->is.dvdpr = 1;
          if(caps2 & DRIVE_CDROM_CAPS_DVDPLUSRW) hd->is.dvdprw = 1;
          if(caps2 & DRIVE_CDROM_CAPS_DVDPLUSRWDL) hd->is.dvdprwdl = 1;
          if(caps2 & DRIVE_CDROM_CAPS_DVDPLUSRDL) hd->is.dvdprdl = 1;
          if(caps2 & DRIVE_CDROM_CAPS_BDROM) hd->is.bd = 1;
          if(caps2 & DRIVE_CDROM_CAPS_BDR) hd->is.bdr = 1;
          if(caps2 & DRIVE_CDROM_CAPS_BDRE) hd->is.bdre = 1;
          if(caps2 & DRIVE_CDROM_CAPS_HDDVDROM) hd->is.hd = 1;
          if(caps2 & DRIVE_CDROM_CAPS_HDDVDR) hd->is.hdr = 1;
          if(caps2 & DRIVE_CDROM_CAPS_HDDVDRW) hd->is.hdrw = 1;
        }
      }
      close(fd);
      if(caps <= 0) {
        if(ci->cdr) hd->is.cdr = 1;
        if(ci->cdrw) hd->is.cdrw = 1;
        if(ci->dvd) hd->is.dvd = 1;
        if(ci->dvdr) hd->is.dvdr = 1;
        if(ci->dvdram) hd->is.dvdram = 1;
      }
    }

    if(hd->is.dvd) hd->prog_if.id = pif_dvd;
    if(hd->is.cdr) hd->prog_if.id = pif_cdr;
    if(hd->is.cdrw) hd->prog_if.id = pif_cdrw;
    if(hd->is.dvdr) hd->prog_if.id = pif_dvdr;
    if(hd->is.dvdram) hd->prog_if.id = pif_dvdram;
  }

  if(
    hd_probe_feature(hd_data, pr_block_cdrom) &&
    hd_report_this(hd_data, hd)
  ) {
    hd_read_cdrom_info(hd_data, hd);
  }
}


void add_other_sysfs_info(hd_data_t *hd_data, hd_t *hd)
{
  unsigned u0, u1;
  char c;

  if(hd->sysfs_id) {
    if(
      sscanf(hd->sysfs_id, "/class/block/cciss!c%ud%u", &u0, &u1) == 2
    ) {
      hd->slot = (u0 << 8) + u1;
      str_printf(&hd->device.name, 0, "CCISS disk %u/%u", u0, u1);
    }
    else if(
      sscanf(hd->sysfs_id, "/class/block/ida!c%ud%u", &u0, &u1) == 2
    ) {
      hd->slot = (u0 << 8) + u1;
      str_printf(&hd->device.name, 0, "SMART Array %u/%u", u0, u1);
    }
    else if(
      sscanf(hd->sysfs_id, "/class/block/rd!c%ud%u", &u0, &u1) == 2
    ) {
      hd->slot = (u0 << 8) + u1;
      str_printf(&hd->device.name, 0, "DAC960 RAID Array %u/%u", u0, u1);
    }
    else if(
      sscanf(hd->sysfs_id, "/class/block/i2o!hd%c", &c) == 1 &&
      c >= 'a'
    ) {
      hd->slot = c - 'a';
      str_printf(&hd->device.name, 0, "I2O disk %u", hd->slot);
    }
    else if(
      sscanf(hd->sysfs_id, "/class/block/dasd%c", &c) == 1 &&
      c >= 'a'
    ) {
      hd->slot = c - 'a';
      hd->device.name = new_str("S390 Disk");
      hd_set_hw_class(hd, hw_redasd);
    }
  }

  add_disk_size(hd_data, hd);
}


void add_ide_sysfs_info(hd_data_t *hd_data, hd_t *hd)
{
  char *fname = NULL, buf[256], *dev_name, *s;
  unsigned u0, u1, u2, size = 0;
  str_list_t *sl, *sl0;
  hd_res_t *res;
  FILE *f;

  if(!hd_report_this(hd_data, hd)) return;

  if(hd->sysfs_bus_id && sscanf(hd->sysfs_bus_id, "%u.%u", &u0, &u1) == 2) {
    /* host.master/slave */
    hd->slot = (u0 << 1) + u1;
  }

  if(
    hd->unix_dev_name &&
    strlen(hd->unix_dev_name) > 5
  ) {
    dev_name = hd->unix_dev_name + 5;

    str_printf(&fname, 0, PROC_IDE "/%s/media", dev_name);
    if((sl = read_file(fname, 0, 1))) {

      if(strstr(sl->str, "floppy"))
        hd->sub_class.id = sc_sdev_floppy;
      else if(strstr(sl->str, "cdrom"))
        hd->sub_class.id = sc_sdev_cdrom;
      else if(strstr(sl->str, "tape"))
        hd->sub_class.id = sc_sdev_tape;

      free_str_list(sl);
    }

    str_printf(&fname, 0, PROC_IDE "/%s/model", dev_name);
    if((sl = read_file(fname, 0, 1))) {
      hd->vendor.name = canon_str(sl->str, strlen(sl->str));
      if((s = strchr(hd->vendor.name, ' '))) {
        hd->device.name = canon_str(s, strlen(s));
        if(*hd->device.name) {
          *s = 0;
        }
        else {
          hd->device.name = free_mem(hd->device.name);
        }
      }
      if(!hd->device.name) {
        hd->device.name = hd->vendor.name;
        hd->vendor.name = NULL;
      }

      free_str_list(sl);
    }

    str_printf(&fname, 0, PROC_IDE "/%s/driver", dev_name);
    if((sl = read_file(fname, 0, 1))) {
      if((s = strchr(sl->str, ' '))) *s = 0;
      s = canon_str(sl->str, strlen(sl->str));
      if(!search_str_list(hd->drivers, s)) add_str_list(&hd->drivers, s);
      s = free_mem(s);
      free_str_list(sl);
    }

    str_printf(&fname, 0, PROC_IDE "/%s/capacity", dev_name);
    if((sl = read_file(fname, 0, 1))) {
      if(sscanf(sl->str, "%u", &u0) == 1 && u0 != 0x7fffffff) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->size.type = res_size;
        res->size.unit = size_unit_sectors;
        res->size.val1 = size = u0;
        res->size.val2 = 512;		// ####### FIXME: sector size?
      }
      free_str_list(sl);
    }

    str_printf(&fname, 0, PROC_IDE "/%s/geometry", dev_name);
    if((sl0 = read_file(fname, 0, 2))) {
      for(sl = sl0; sl; sl = sl->next) {
        if(sscanf(sl->str, " physical %u / %u / %u", &u0, &u1, &u2) == 3) {
          if(u0 || u1 || u2) {
            if(size && u1 && u2) {
              u0 = size / (u1 * u2);
            }
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->disk_geo.type = res_disk_geo;
            res->disk_geo.cyls = u0;
            res->disk_geo.heads = u1;
            res->disk_geo.sectors = u2;
            res->disk_geo.geotype = geo_physical;
          }
          continue;
        }

        if(sscanf(sl->str, " logical %u / %u / %u", &u0, &u1, &u2) == 3) {
          if(size && u1 && u2) {
            u0 = size / (u1 * u2);
          }
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->disk_geo.type = res_disk_geo;
          res->disk_geo.cyls = u0;
          res->disk_geo.heads = u1;
          res->disk_geo.sectors = u2;
          res->disk_geo.geotype = geo_logical;
        }
      }
      free_str_list(sl0);
    }

    str_printf(&fname, 0, PROC_IDE "/%s/cache", dev_name);
    if((sl = read_file(fname, 0, 1))) {
      if(sscanf(sl->str, "%u", &u0) == 1 && u0) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->cache.type = res_cache;
        res->cache.size = u0;
      }
      free_str_list(sl);
    }

    str_printf(&fname, 0, PROC_IDE "/%s/identify", dev_name);
    if((f = fopen(fname, "r"))) {
      u1 = 0;
      memset(buf, 0, sizeof buf);
      while(u1 < sizeof buf - 1 && fscanf(f, "%x", &u0) == 1) {
        buf[u1++] = u0 >> 8; buf[u1++] = u0;
      }
      fclose(f);

      /* ok, we now have the ATA/ATAPI ident block */

      if(buf[0x14] || buf[0x15]) {	/* has serial id */
        hd->serial = canon_str(buf + 0x14, 20);
      }
      if(buf[0x2e] || buf[0x2f]) {	/* has revision id */
        hd->revision.name = canon_str(buf + 0x2e, 8);
      }
    }

    free_mem(fname);
  }

  if(!size) add_disk_size(hd_data, hd);
}


/*
 * assumes hd->drivers aleady includes scsi device drivers (like 'sd')
 *
 * The following code uses ioctl() calls to issue some SCSI commands
 * directly (namely the INQUIRY command).
 *
 * For reference, and to understand the layout of the ioctl() calls below,
 * google for a document named 'SCSI Primary Commands 5 (SPC-5)' (or more
 * recent versions - it doesn't matter for our purpose).
 *
 * The latest draft is usually freely available but not directly downloadable.
 */
void add_scsi_sysfs_info(hd_data_t *hd_data, hd_t *hd, char *sf_dev)
{
  hd_t *hd1;
  char *s, *t, *cs, *pr_str;
  unsigned u0, u1, u2, u3;
  int fd, k;
  unsigned char scsi_cmd_buf[0x300];
  struct sg_io_hdr hdr;
  unsigned char *uc;
  scsi_t *scsi;
  hd_res_t *geo, *size;
  uint64_t ul0;
  str_list_t *sl;
  char buf[16];
  hd_res_t *res;

  if(!hd_report_this(hd_data, hd)) return;

  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_scsi;
  hd->detail->scsi.data = scsi = new_mem(sizeof *scsi);

  if(hd->sysfs_bus_id && sscanf(hd->sysfs_bus_id, "%u:%u:%u:%u", &u0, &u1, &u2, &u3) == 4) {
    /* host:channel:id:lun */
    hd->slot = (u0 << 8) + (u1 << 4) + u2;
    hd->func = u3;
  }

  if(hd->sysfs_bus_id && sscanf(hd->sysfs_bus_id, "nvme%u", &u0) == 1) {
    hd->slot = u0;
  }

  // Looks like PCI device?
  if(get_sysfs_attr_by_path(sf_dev, "subsystem_vendor")) {
    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "vendor"), &ul0, 0)) {
      ADD2LOG("    vendor = 0x%x\n", (unsigned) ul0);
      hd->vendor.id = MAKE_ID(TAG_PCI, ul0 & 0xffff);
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "device"), &ul0, 0)) {
      ADD2LOG("    device = 0x%x\n", (unsigned) ul0);
      hd->device.id = MAKE_ID(TAG_PCI, ul0 & 0xffff);
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "subsystem_vendor"), &ul0, 0)) {
      ADD2LOG("    subvendor = 0x%x\n", (unsigned) ul0);
      hd->sub_vendor.id = MAKE_ID(TAG_PCI, ul0 & 0xffff);
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "subsystem_device"), &ul0, 0)) {
      ADD2LOG("    subdevice = 0x%x\n", (unsigned) ul0);
      hd->sub_device.id = MAKE_ID(TAG_PCI, ul0 & 0xffff);
    }
  }
  else {
    if((s = get_sysfs_attr_by_path(sf_dev, "vendor"))) {
      cs = canon_str(s, strlen(s));
      ADD2LOG("    vendor = %s\n", cs);
      if(*cs) {
        hd->vendor.name = cs;
      }
      else {
        free_mem(cs);
      }
    }
  }

  if((s = get_sysfs_attr_by_path(sf_dev, "model"))) {
    cs = canon_str(s, strlen(s));
    ADD2LOG("    model = %s\n", cs);
    if(*cs) {
      hd->device.name = cs;
    }
    else {
      free_mem(cs);
    }

    /* sata entries are somewhat strange... */
    if(
      hd->vendor.name &&
      !strcmp(hd->vendor.name, "ATA") &&
      hd->device.name
    ) {
      hd->bus.id = bus_ide;

      if((cs = strchr(hd->device.name, ' '))) {
        t = canon_str(cs, strlen(cs));
        if(*t) {
          *cs = 0;
          free_mem(hd->vendor.name);
          hd->vendor.name = hd->device.name;
          hd->device.name = t;
        }
        else {
          t = free_mem(t);
        }
      }

      if(!strcmp(hd->vendor.name, "ATA")) {
        hd->vendor.name = free_mem(hd->vendor.name);
      }
    }
  }

  if((s = get_sysfs_attr_by_path(sf_dev, "rev"))) {
    cs = canon_str(s, strlen(s));
    ADD2LOG("    rev = %s\n", cs);
    if(*cs) {
      hd->revision.name = cs;
    }
    else {
      free_mem(cs);
    }
  }

  if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "type"), &ul0, 0)) {
    ADD2LOG("    type = %u\n", (unsigned) ul0);
    if(ul0 == 6 /* scanner */) {
      hd->sub_class.id = sc_sdev_scanner;
    }
    else if(ul0 == 3 /* processor */ && hd->vendor.name) {
      if(
        !strncmp(hd->vendor.name, "HP", sizeof "HP" - 1) ||
        !strncmp(hd->vendor.name, "EPSON", sizeof "EPSON" - 1)
      ) {
        hd->sub_class.id = sc_sdev_scanner;
      }
    }

    /*
     * typically needed for usb card readers (unused slots)
     */
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_other
    ) {
      switch(ul0) {
        case 0:
          if(search_str_list(hd->drivers, "sd")) {
            hd->sub_class.id = sc_sdev_disk;
          }
          break;

        case 5:
          if(search_str_list(hd->drivers, "sr")) {
            hd->sub_class.id = sc_sdev_cdrom;
          }
          break;
      }
    }
  }

  res = new_mem(sizeof *res);
  res->any.type = res_fc;

  if(hd->sysfs_bus_id) {
    for(sl = hd_data->lsscsi; sl; sl = sl->next) {
      if(
        sscanf(sl->str, "[%15[^]]] %*s fc:%"SCNx64" , %x ", buf, &ul0, &u0) == 3 &&
        !strcmp(hd->sysfs_bus_id, buf)
      ) {
        ADD2LOG("    lsscsi: wwpn = 0x%"PRIx64", port_id = 0x%x\n", ul0, u0);
        res->fc.wwpn = ul0;
        res->fc.wwpn_ok = 1;
        res->fc.port_id = u0;
        res->fc.port_id_ok = 1;
        if(hd->sysfs_device_link && strstr(hd->sysfs_device_link, "/net/")) hd->is.fcoe = 1;
      }
    }
  }

  /* s390: wwpn & fcp lun */
  if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "wwpn"), &ul0, 0)) {
    ADD2LOG("    wwpn = 0x%016"PRIx64"\n", ul0);
    res->fc.wwpn = ul0;
    res->fc.wwpn_ok = 1;
    scsi->wwpn = ul0;
    scsi->wwpn_ok = 1;

    /* it's a bit of a hack, actually */
    t = new_str(hd_sysfs_id(sf_dev));
    if(t) {
      if((s = strrchr(t, '/'))) *s = 0;
      if((s = strrchr(t, '/'))) *s = 0;
      if((s = strrchr(t, '/'))) *s = 0;
      if((s = strrchr(t, '/'))) *s = 0;
      if((s = strrchr(t, '/'))) {
        scsi->controller_id = new_str(s + 1);
        res->fc.controller_id = new_str(s + 1);
      }
    }
    t = free_mem(t);
  }

  if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "fcp_lun"), &ul0, 0)) {
    ADD2LOG("    fcp_lun = 0x%016"PRIx64"\n", ul0);
    res->fc.fcp_lun = ul0;
    res->fc.fcp_lun_ok = 1;
    scsi->fcp_lun = ul0;
    scsi->fcp_lun_ok = 1;
  }

  if(res->fc.wwpn_ok || res->fc.port_id_ok || res->fc.fcp_lun_ok || res->fc.controller_id) {
    add_res_entry(&hd->res, res);
  }
  else {
    free_res_list(res);
  }

  /* ppc: get rom id */
  if((hd1 = hd_get_device_by_idx(hd_data, hd->attached_to)) && hd1->rom_id) {
    str_printf(&hd->rom_id, 0, "%s/@%u", hd1->rom_id, (hd->slot & 0xf));
  }

  pr_str = NULL;

  if(
    hd_report_this(hd_data, hd) &&
    hd->unix_dev_name &&
    hd->sub_class.id == sc_sdev_cdrom &&
    !hd_data->flags.vmware		/* VMware doesn't like it */
  ) {
    PROGRESS(5, 0, hd->unix_dev_name);
    fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {

      str_printf(&pr_str, 0, "%s cache", hd->unix_dev_name);
      PROGRESS(5, 1, pr_str);

      memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);
      memset(&hdr, 0, sizeof(hdr));

      hdr.interface_id = 'S';
      hdr.cmd_len = 6;
      hdr.dxfer_direction = SG_DXFER_FROM_DEV;
      hdr.dxferp = scsi_cmd_buf + 8 + 6;
      hdr.dxfer_len = 0xff;
      hdr.cmdp = scsi_cmd_buf + 8;
      hdr.cmdp[0] = 0x1a;
      hdr.cmdp[2] = 0x08;
      hdr.cmdp[4] = 0xff;

      k = ioctl(fd, SG_IO, &hdr);

      if(k) {
        ADD2LOG("%s status(0x1a:8) 0x%x\n", hd->unix_dev_name, k);
      }
      else {
        unsigned char *ptr = hdr.dxferp;

        uc = ptr + 4 + ptr[3] + 2;
        scsi->cache = uc[0];
        ADD2LOG("  scsi cache: 0x%02x\n", uc[0]);
    
        if((scsi->cache & 4)) {
          hd->prog_if.id = pif_cdr;
        }
      }

      close(fd);
    }
  }


  if(
    hd_report_this(hd_data, hd) &&
    hd->unix_dev_name &&
    hd->sub_class.id == sc_sdev_disk &&
    !hd_probe_feature(hd_data, pr_scsi_noserial)
  ) {
    PROGRESS(5, 0, hd->unix_dev_name);
    fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {

      str_printf(&pr_str, 0, "%s geo", hd->unix_dev_name);
      PROGRESS(5, 1, pr_str);

      if(hd_getdisksize(hd_data, hd->unix_dev_name, fd, &geo, &size) == 1) {
        /* (low-level) unformatted disk */
        hd->is.notready = 1;
      }

      if(geo) add_res_entry(&hd->res, geo);
      if(size) add_res_entry(&hd->res, size);

      str_printf(&pr_str, 0, "%s serial", hd->unix_dev_name);
      PROGRESS(5, 2, pr_str);

      unsigned char *serial_buf = NULL;
      unsigned serial_buf_len = 0;
      memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);

      // get the page from sysfs, if it's there already
      if(hd->sysfs_device_link) {
        char *path = NULL;

        str_printf(&path, 0, "/sys/%s/vpd_pg80", hd->sysfs_device_link);

        int fd = open(path, O_RDONLY);
        if(fd >= 0) {
          serial_buf = scsi_cmd_buf;
          int i = read(fd, scsi_cmd_buf, sizeof scsi_cmd_buf - 1);
          close(fd);
          if(i > 0) serial_buf_len = i;
        }

        path = free_mem(path);
      }

      // ... else go and fetch it
      if(!serial_buf) {
        memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);
        memset(&hdr, 0, sizeof(hdr));

        hdr.interface_id = 'S';
        hdr.cmd_len = 6;
        hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        hdr.dxferp = scsi_cmd_buf + 8 + 6;
        hdr.dxfer_len = 0x24;
        hdr.cmdp = scsi_cmd_buf + 8;
        hdr.cmdp[0] = 0x12;
        hdr.cmdp[1] = 0x01;
        hdr.cmdp[2] = 0x80;
        hdr.cmdp[4] = 0x24;

        k = ioctl(fd, SG_IO, &hdr);

        if(k) {
          ADD2LOG("%s status(0x12) 0x%x\n", scsi->dev_name, k);
        }
        else {
          serial_buf = hdr.dxferp;
          serial_buf_len = serial_buf[3] + 4;
        }
      }
      else {
        ADD2LOG("  got it from vpd_pg80\n");
      }

      // sanity check: serial_buf[3] holds the lenght of user data starting at offset 4
      if(serial_buf_len < 4 || serial_buf_len < serial_buf[3] + 4) serial_buf = NULL;

      if(serial_buf) {
        unsigned u;

        ADD2LOG("  serial id len: %u\n", serial_buf[3]);

        for(u = 0; u < serial_buf_len; u += 0x10) {
          ADD2LOG("    ");
          hd_log_hex(hd_data, 1, serial_buf_len - u >= 0x10 ? 0x10 : serial_buf_len - u, serial_buf + u);
          ADD2LOG("\n");
        }

        // bsc#1078511: additional consistency check:
        // vpd page number should be returned at offset 1
        if(serial_buf[1] == 0x80 && (hd->serial = canon_str(serial_buf + 4, serial_buf[3]))) {
          if(!*hd->serial) {
            hd->serial = free_mem(hd->serial);
          }
          else {
            ADD2LOG("  serial id: \"%s\"\n", hd->serial);
          }
        }
        else {
          ADD2LOG("  invalid response\n");
        }
      }
      else {
        ADD2LOG("  no serial id\n");
      }

      str_printf(&pr_str, 0, "%s model", hd->unix_dev_name);
      PROGRESS(5, 3, pr_str);

      memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);
      memset(&hdr, 0, sizeof(hdr));

      hdr.interface_id = 'S';
      hdr.cmd_len = 6;
      hdr.dxfer_direction = SG_DXFER_FROM_DEV;
      hdr.dxferp = scsi_cmd_buf + 8 + 6;
      hdr.dxfer_len = 0x60;
      hdr.cmdp = scsi_cmd_buf + 8;
      hdr.cmdp[0] = 0x12;	// inquiry cmd
      hdr.cmdp[4] = 0x60;	// max transfer len

      k = ioctl(fd, SG_IO, &hdr);

      if(k) {
        ADD2LOG("%s status(0x12) 0x%x\n", scsi->dev_name, k);
      }
      else {
        unsigned u, len;
        unsigned char *ptr = hdr.dxferp;

        len = ptr[4] + 5;
        ADD2LOG("  inq resp len: %u\n", len);

        for(u = 0; u < len; u += 0x10) {
          ADD2LOG("    ");
          hd_log_hex(hd_data, 1, len - u >= 0x10 ? 0x10 : len - u, ptr + u);
          ADD2LOG("\n");
        }

        if(len >= 36) {
          // extract vendor, device, revision
          char *v = canon_str(ptr + 8, 8);
          char *d = canon_str(ptr + 16, 16);
          char *r = canon_str(ptr + 32, 4);

          ADD2LOG("  vendor = \"%s\", device = \"%s\", rev = \"%s\"\n", v, d, r);

          // set values unless we already have them
          if(!hd->vendor.name && *v && strcmp(v, "ATA")) {
            hd->vendor.name = v;
          }
          else {
            free_mem(v);
          }

          if(!hd->device.name && *d) {
            hd->device.name = d;
          }
          else {
            free_mem(d);
          }

          if(!hd->revision.name && *r) {
            hd->revision.name = r;
          }
          else {
            free_mem(r);
          }
        }
      }

      close(fd);
    }
  }

  pr_str = free_mem(pr_str);


  if(
    hd->base_class.id == bc_storage_device &&
    hd->sub_class.id == sc_sdev_scanner
  ) {
    hd->base_class.id = bc_scanner;
  }

  // ###### FIXME: usb-storage: disk vs. floppy?
 
}


void read_partitions(hd_data_t *hd_data)
{
  str_list_t *sl, *sl0, *pl0 = NULL;
  char buf[256], *s1, *name, *base;
  char *last_base = new_str(" ");
  char *last_name = new_str(" ");
  int l, is_disk;

  if(!(sl0 = read_file(PROC_PARTITIONS, 2, 0))) return;

  if(hd_data->debug) {
    ADD2LOG("----- "PROC_PARTITIONS" -----\n");
    for(sl = sl0; sl; sl = sl->next) {
      ADD2LOG("  %s", sl->str);
    }
    ADD2LOG("----- "PROC_PARTITIONS" end -----\n");
  }

  for(sl = sl0; sl; sl = sl->next) {
    *buf = 0;
    if(sscanf(sl->str, "%*s %*s %*s %255s", buf) > 0) {
      if(*buf) add_str_list(&pl0, buf);
    }
  }

  free_str_list(sl0);

  for(is_disk = 1, sl = pl0; sl; sl = sl->next) {
    base = sl->str;
    l = strlen(base);
    if(!l) continue;

    s1 = base + l - 1;
    while(isdigit(*s1) && s1 > base) s1--;
    if(s1 == base) continue;

    name = new_str(base);
    s1[1] = 0;

    if(!strcmp(last_base, base)) {
      if(!strcmp(last_name, base)) is_disk = 0;
    }
    else {
      is_disk = strncmp(last_name, base, strlen(last_name)) ? 1 : 0;
    }

    if(!search_str_list(hd_data->cdroms, name)) {
      if(
        strncmp(name, "loop", sizeof "loop" - 1) &&
        (
          hd_data->flags.list_md ||
          (
            strncmp(name, "md", sizeof "md" - 1) &&
            strncmp(name, "dm-", sizeof "dm-" - 1) &&
            strncmp(name, "bcache", sizeof "bcache" - 1)
          )
        )
      ) {
        add_str_list(is_disk ? &hd_data->disks : &hd_data->partitions, name);
      }
    }
    free_mem(last_base);
    free_mem(last_name);

    last_base = new_str(base);
    last_name = name; name = NULL;
  }

  free_mem(last_base);
  free_mem(last_name);

  free_str_list(pl0);

  if(hd_data->debug) {
    ADD2LOG("disks:\n");
    for(sl = hd_data->disks; sl; sl = sl->next) ADD2LOG("  %s\n", sl->str);
    ADD2LOG("partitions:\n");
    for(sl = hd_data->partitions; sl; sl = sl->next) ADD2LOG("  %s\n", sl->str);
  }
}


/*
 * Read iso9660/el torito info, if there is a CD inserted.
 * Returns NULL if nothing was found
 */
cdrom_info_t *hd_read_cdrom_info(hd_data_t *hd_data, hd_t *hd)
{
  int fd;
  char *s;
  cdrom_info_t *ci;
  struct iso_primary_descriptor iso_desc;
  unsigned char sector[0x800];
  unsigned et;
  unsigned u0, u1, u2;

  /* free existing entry */
  if(hd->detail && hd->detail->type != hd_detail_cdrom) {
    hd->detail = free_hd_detail(hd->detail);
  }

  if(!hd->detail) {
    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_cdrom;
    hd->detail->cdrom.data = new_mem(sizeof *hd->detail->cdrom.data);
  }

  ci = hd->detail->cdrom.data;

  fd = open(hd->unix_dev_name, O_RDONLY|O_NONBLOCK);

  /* we get CDS_DISK_OK if there is a CD in the drive */
  hd->is.notready = fd != -1 && ioctl(fd, CDROM_DRIVE_STATUS, 0) == CDS_DISC_OK ? 0 : 1;

  if(hd->is.notready) {
    if(fd != -1) close(fd);
    return NULL;
  }

  ci->iso9660.ok = 0;
  if(
    lseek(fd, 0x8000, SEEK_SET) >= 0 &&
    read(fd, &iso_desc, sizeof iso_desc) == sizeof iso_desc
  ) {
    ci->cdrom = 1;
    if(!memcmp(iso_desc.id, "CD001", 5)) {
      ci->iso9660.ok = 1;
      /* now, fill in the fields */
      s = canon_str(iso_desc.volume_id, sizeof iso_desc.volume_id);
      if(!*s) s = free_mem(s);
      ci->iso9660.volume = s;

      s = canon_str(iso_desc.publisher_id, sizeof iso_desc.publisher_id);
      if(!*s) s = free_mem(s);
      ci->iso9660.publisher = s;

      s = canon_str(iso_desc.preparer_id, sizeof iso_desc.preparer_id);
      if(!*s) s = free_mem(s);
      ci->iso9660.preparer = s;

      s = canon_str(iso_desc.application_id, sizeof iso_desc.application_id);
      if(!*s) s = free_mem(s);
      ci->iso9660.application = s;

      s = canon_str(iso_desc.creation_date, sizeof iso_desc.creation_date);
      if(!*s) s = free_mem(s);
      ci->iso9660.creation_date = s;
    }
  }

  if(
    ci->iso9660.ok &&
    lseek(fd, 0x8800, SEEK_SET) >= 0 &&
    read(fd, &sector, sizeof sector) == sizeof sector
  ) {
    if(
      sector[0] == 0 && sector[6] == 1 &&
      !memcmp(sector + 1, "CD001", 5) &&
      !memcmp(sector + 7, "EL TORITO SPECIFICATION", 23)
    ) {
      et = sector[0x47] + (sector[0x48] << 8) + (sector[0x49] << 16) + (sector[0x4a] << 24);
      ADD2LOG("  %s: el torito boot catalog at 0x%04x\n", ci->name, et);
      if(
        lseek(fd, et * 0x800, SEEK_SET) >= 0 &&
        read(fd, &sector, sizeof sector) == sizeof sector &&
        sector[0] == 1
      ) {
        ci->el_torito.ok = 1;
        ci->el_torito.catalog = et;
        ci->el_torito.platform = sector[1];
        s = canon_str(sector + 4, 24);
        if(!*s) s = free_mem(s);
        ci->el_torito.id_string = s;
        ci->el_torito.bootable = sector[0x20] == 0x88 ? 1 : 0;
        ci->el_torito.media_type = sector[0x21];
        ADD2LOG("    media type: %u\n",  ci->el_torito.media_type);
        ci->el_torito.load_address = (sector[0x22] + (sector[0x23] << 8)) << 4;
        ADD2LOG("    load address: 0x%04x\n",  ci->el_torito.load_address);
#if 0
        if(ci->el_torito.platform == 0 && ci->el_torito.load_address == 0)
          ci->el_torito.load_address = 0x7c00;
#endif
        ci->el_torito.load_count = sector[0x26] + (sector[0x27] << 8);
        ci->el_torito.start = sector[0x28] + (sector[0x29] << 8) + (sector[0x2a] << 16) + (sector[0x2b] << 24);
        if(ci->el_torito.media_type >= 1 && ci->el_torito.media_type <= 3) {
          ci->el_torito.geo.c = 80;
          ci->el_torito.geo.h = 2;
        }
        switch(ci->el_torito.media_type) {
          case 1:
            ci->el_torito.geo.s = 15;
            break;
          case 2:
            ci->el_torito.geo.s = 18;
            break;
          case 3:
            ci->el_torito.geo.s = 36;
            break;
        }
        if(
          lseek(fd, ci->el_torito.start * 0x800, SEEK_SET) >= 0 &&
          read(fd, &sector, sizeof sector) == sizeof sector
        ) {
          if(ci->el_torito.media_type == 4) {
            /* ##### we should go on and read the 1st partition sector in this case... */
            ci->el_torito.geo.h = (unsigned) sector[0x1be + 5] + 1;
            ci->el_torito.geo.s = sector[0x1be + 6] & 0x3f;
            ci->el_torito.geo.c = sector[0x1be + 7] + (((unsigned) sector[0x1be + 6] >> 6) << 8);
          }
          if(
            sector[0x1fe] == 0x55 && sector[0x1ff] == 0xaa &&
            sector[0x0b] == 0 && sector[0x0c] == 2 &&
            sector[0x0e] == 1 && sector[0x0f] == 0
          ) {
            u0 = sector[0x13] + (sector[0x14] << 8);	/* partition size */
            u1 = sector[0x18] + (sector[0x19] << 8);	/* sectors per track */
            u2 = sector[0x1a] + (sector[0x1b] << 8);	/* heads */
            u0 = u0 ? u0 : sector[0x20] + (sector[0x21] << 8) + (sector[0x22] << 16) + ((unsigned) sector[0x23] << 24);
            if(sector[0x26] == 0x29) {
              s = canon_str(sector + 0x2b, 11);
              if(!*s) s = free_mem(s);
              ci->el_torito.label = s;
            }
            if(!ci->el_torito.label) {
              s = canon_str(sector + 3, 8);
              if(!*s) s = free_mem(s);
              ci->el_torito.label = s;
            }
            if(
              (ci->el_torito.media_type == 0 || ci->el_torito.media_type > 3) &&
              u0 && u1 && u2
            ) {
              ci->el_torito.geo.h = u2;
              ci->el_torito.geo.s = u1;
              ci->el_torito.geo.size = u0;
              ci->el_torito.geo.c = ci->el_torito.geo.size / (u1 * u2);
            }
          }
        }

        ci->el_torito.geo.size = ci->el_torito.geo.s * ci->el_torito.geo.c * ci->el_torito.geo.h;
      }
    }
  }

  close(fd);

  return ci;
}


/*
 * Read the list of CDROM devices known to the kernel. The info is taken
 * from /proc/sys/dev/cdrom/info.
 */
void read_cdroms(hd_data_t *hd_data)
{
  char *s, *t, *v;
  str_list_t *sl, *sl0;
  cdrom_info_t *ci;
  int i, line, entries = 0;
  unsigned val;

  if(!(sl0 = read_file(PROC_CDROM_INFO, 2, 0))) return;

  if((hd_data->debug & HD_DEB_CDROM)) {
    ADD2LOG("----- "PROC_CDROM_INFO" -----\n");
    for(sl = sl0; sl; sl = sl->next) {
      if(*sl->str != '\n') ADD2LOG("%s", sl->str);
    }
    ADD2LOG("----- "PROC_CDROM_INFO" end -----\n");
  }

  for(sl = sl0; sl; sl = sl->next) {
    if(
      (line = 0, strstr(sl->str, "drive name:") == sl->str) ||
      (line++, strstr(sl->str, "drive speed:") == sl->str) ||
      (line++, strstr(sl->str, "Can write CD-R:") == sl->str) ||
      (line++, strstr(sl->str, "Can write CD-RW:") == sl->str) ||
      (line++, strstr(sl->str, "Can read DVD:") == sl->str) ||
      (line++, strstr(sl->str, "Can write DVD-R:") == sl->str) ||
      (line++, strstr(sl->str, "Can write DVD-RAM:") == sl->str)
    ) {
      s = strchr(sl->str, ':') + 1;
      i = 0;
      while((t = strsep(&s, " \t\n"))) {
        if(!*t) continue;
        i++;
        switch(line) {
          case 0:	/* drive name */
            ci = new_cdrom_entry(&hd_data->cdrom);
            entries++;
            add_str_list(&hd_data->cdroms, t);
            ci->name = new_str(t);
            break;

          case 1:	/* drive speed */
          case 2:	/* Can write CD-R */
          case 3:	/* Can write CD-RW */
          case 4:	/* Can read DVD */
          case 5:	/* Can write DVD-R */
          case 6:	/* Can write DVD-RAM */
            ci = get_cdrom_entry(hd_data->cdrom, entries - i);
            if(ci) {
              val = strtoul(t, &v, 10);
              if(!*v) {
                switch(line) {
                  case 1:
                    ci->speed = val;
                    break;
                  case 2:
                    ci->cdr = val;
                    break;
                  case 3:
                    ci->cdrw = val;
                    break;
                  case 4:
                    ci->dvd = val;
                    break;
                  case 5:
                    ci->dvdr = val;
                    break;
                  case 6:
                    ci->dvdram = val;
                    break;
                }
              }
            }
            break;
        }
      }
    }  
  }

  free_str_list(sl0);
}


/* add new entries at the _start_ of the list */
cdrom_info_t *new_cdrom_entry(cdrom_info_t **ci)
{
  cdrom_info_t *new_ci = new_mem(sizeof *new_ci);

  new_ci->next = *ci;
  return *ci = new_ci;
}


/* return nth entry */
cdrom_info_t *get_cdrom_entry(cdrom_info_t *ci, int n)
{
  if(n < 0) return NULL;

  while(n--) {
    if(!ci) return NULL;
    ci = ci->next;
  }

  return ci;
}


/*
 * Add generic scsi devs.
 */
void hd_scan_sysfs_scsi(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_scsi)) return;

  hd_data->module = mod_scsi;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "scsi modules");

  load_module(hd_data, "sg");

  PROGRESS(2, 0, "scsi tape");

  get_scsi_tape(hd_data);

  PROGRESS(3, 0, "scsi generic");

  get_generic_scsi_devs(hd_data);
}


void get_scsi_tape(hd_data_t *hd_data)
{
  char *s, *t;
  unsigned u1, u2, u3;
  uint64_t ul0;
  hd_t *hd, *hd1;
  hd_dev_num_t dev_num;
  str_list_t *sf_class, *sf_class_e;
  char *sf_cdev = NULL, *sf_dev = NULL;
  char *sf_drv_name, *sf_drv, *bus_id;

  sf_class = read_dir("/sys/class/scsi_tape", 'D');

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: scsi_tape\n");
    return;
  }

  for(sf_class_e = sf_class; sf_class_e; sf_class_e = sf_class_e->next) {
    str_printf(&sf_cdev, 0, "/sys/class/scsi_tape/%s", sf_class_e->str);

    ADD2LOG(
      "  scsi tape: name = %s, path = %s\n",
      sf_class_e->str,
      hd_sysfs_id(sf_cdev)
    );

    memset(&dev_num, 0, sizeof dev_num);

    if((s = get_sysfs_attr_by_path(sf_cdev, "dev"))) {
      if(sscanf(s, "%u:%u", &u1, &u2) == 2) {
        dev_num.type = 'c';
        dev_num.major = u1;
        dev_num.minor = u2;
        dev_num.range = 1;
      }
      ADD2LOG("    dev = %u:%u\n", u1, u2);
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_cdev, "range"), &ul0, 0)) {
      dev_num.range = ul0;
      ADD2LOG("    range = %u\n", dev_num.range);
    }

    sf_dev = new_str(hd_read_sysfs_link(sf_cdev, "device"));
    sf_drv_name = NULL;
    sf_drv = hd_read_sysfs_link(sf_dev, "driver");
    if(!sf_drv) {
      /* maybe older kernel */
      sf_drv = hd_read_sysfs_link(sf_cdev, "driver");
    }
    if(sf_drv) {
      sf_drv_name = strrchr(sf_drv, '/');
      if(sf_drv_name) sf_drv_name++;
    }

    bus_id = sf_dev ? strrchr(sf_dev, '/') : NULL;
    if(bus_id) bus_id++;

    if(sf_dev) {
      s = hd_sysfs_id(sf_dev);

      ADD2LOG(
        "    scsi device: bus_id = %s driver = %s\n      path = %s\n",
        bus_id,
        sf_drv_name,
        s
      );

      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(
          hd->module == hd_data->module &&
          hd->sysfs_device_link &&
          hd->base_class.id == bc_storage_device &&
          hd->sub_class.id == sc_sdev_tape &&
          s &&
          !strcmp(hd->sysfs_device_link, s)
        ) break;
      }

      if(!hd) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_storage_device;
        hd->sub_class.id = sc_sdev_tape;

        hd->bus.id = bus_scsi;

        hd->sysfs_device_link = new_str(s);

        hd->sysfs_bus_id = new_str(bus_id);

        /* parent has longest matching sysfs id */
        u2 = strlen(s);
        for(u3 = 0, hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
          if(hd1->sysfs_id) {
            u1 = strlen(hd1->sysfs_id);
            if(u1 > u3 && u1 <= u2 && !strncmp(s, hd1->sysfs_id, u1)) {
              u3 = u1;
              hd->attached_to = hd1->idx;
            }
          }
        }

        /* find longest matching sysfs id we have a driver for */
        s = new_str(s);
        t = strrchr(s, '/');
        if(t) *t = 0;
        t = hd_sysfs_find_driver(hd_data, s, 0);
        if(t) {
          add_str_list(&hd->drivers, t);
        }
        s = free_mem(s);

        if(sf_drv_name) {
          add_str_list(&hd->drivers, sf_drv_name);
        }

        add_scsi_sysfs_info(hd_data, hd, sf_dev);
      }

      s = hd_sysfs_name2_dev(sf_class_e->str);

      if(!hd->unix_dev_name || strlen(s) + sizeof "/dev/" - 1 < strlen(hd->unix_dev_name)) {
        str_printf(&hd->unix_dev_name, 0, "/dev/%s", s);
        hd->unix_dev_num = dev_num;
        free_mem(hd->sysfs_id);
        hd->sysfs_id = new_str(hd_sysfs_id(sf_cdev));
      }
    }
  }

  sf_cdev = free_mem(sf_cdev);
  sf_class = free_str_list(sf_class);
}


void get_generic_scsi_devs(hd_data_t *hd_data)
{
  char *s, *t;
  unsigned u1, u2, u3;
  uint64_t ul0;
  hd_t *hd, *hd1;
  hd_dev_num_t dev_num;
  str_list_t *sf_class, *sf_class_e;
  char *sf_cdev = NULL, *sf_dev = NULL;
  char *sf_drv_name, *sf_drv, *bus_id;

  sf_class = read_dir("/sys/class/scsi_generic", 'D');

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: scsi_generic\n");
    return;
  }

  for(sf_class_e = sf_class; sf_class_e; sf_class_e = sf_class_e->next) {
    str_printf(&sf_cdev, 0, "/sys/class/scsi_generic/%s", sf_class_e->str);

    ADD2LOG(
      "  scsi: name = %s, path = %s\n",
      sf_class_e->str,
      hd_sysfs_id(sf_cdev)
    );

    memset(&dev_num, 0, sizeof dev_num);

    if((s = get_sysfs_attr_by_path(sf_cdev, "dev"))) {
      if(sscanf(s, "%u:%u", &u1, &u2) == 2) {
        dev_num.type = 'c';
        dev_num.major = u1;
        dev_num.minor = u2;
        dev_num.range = 1;
      }
      ADD2LOG("    dev = %u:%u\n", u1, u2);
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_cdev, "range"), &ul0, 0)) {
      dev_num.range = ul0;
      ADD2LOG("    range = %u\n", dev_num.range);
    }

    sf_dev = new_str(hd_read_sysfs_link(sf_cdev, "device"));
    sf_drv_name = NULL;
    sf_drv = hd_read_sysfs_link(sf_dev, "driver");
    if(!sf_drv) {
      /* maybe older kernel */
      sf_drv = hd_read_sysfs_link(sf_cdev, "driver");
    }
    if(sf_drv) {
      sf_drv_name = strrchr(sf_drv, '/');
      if(sf_drv_name) sf_drv_name++;
    }

    bus_id = sf_dev ? strrchr(sf_dev, '/') : NULL;
    if(bus_id) bus_id++;

    s = NULL;

    if(sf_dev) {
      s = hd_sysfs_id(sf_dev);

      ADD2LOG(
        "    scsi device: bus_id = %s driver = %s\n      path = %s\n",
        bus_id,
        sf_drv_name,
        s
      );
    }

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->sysfs_device_link &&
        hd->bus.id == bus_scsi &&
        s &&
        !strcmp(hd->sysfs_device_link, s)
      ) break;
    }

    if(hd) {
      if(!hd->unix_dev_name2) {
        str_printf(&hd->unix_dev_name2, 0, "/dev/%s", hd_sysfs_name2_dev(sf_class_e->str));
        hd->unix_dev_num2 = dev_num;
      }
    }

    hd = NULL;

    if(sf_dev && !sf_drv_name) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_storage_device;
      hd->sub_class.id = sc_sdev_other;

      str_printf(&hd->unix_dev_name, 0, "/dev/%s", hd_sysfs_name2_dev(sf_class_e->str));

      hd->bus.id = bus_scsi;

      hd->sysfs_id = new_str(hd_sysfs_id(sf_cdev));

      hd->unix_dev_num = dev_num;

      hd->sysfs_bus_id = new_str(bus_id);

      if((s = hd_sysfs_id(sf_dev))) {

        /* parent has longest matching sysfs id */
        u2 = strlen(s);
        for(u3 = 0, hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
          if(hd1->sysfs_id) {
            u1 = strlen(hd1->sysfs_id);
            if(u1 > u3 && u1 <= u2 && !strncmp(s, hd1->sysfs_id, u1)) {
              u3 = u1;
              hd->attached_to = hd1->idx;
            }
          }
        }

        /* find longest matching sysfs id we have a driver for */
        s = new_str(s);
        t = strrchr(s, '/');
        if(t) *t = 0;
        t = hd_sysfs_find_driver(hd_data, s, 0);
        if(t) {
          add_str_list(&hd->drivers, t);
        }
        s = free_mem(s);

      }

      add_scsi_sysfs_info(hd_data, hd, sf_dev);
    }

    sf_dev = free_mem(sf_dev);
  }

  sf_cdev = free_mem(sf_cdev);
  sf_class = free_str_list(sf_class);
}


void add_disk_size(hd_data_t *hd_data, hd_t *hd)
{
  hd_res_t *geo, *size;
  int fd;
  char *pr_str;

  pr_str = NULL;

  if(
    hd->unix_dev_name &&
    hd->sub_class.id == sc_sdev_disk
  ) {
    PROGRESS(5, 0, hd->unix_dev_name);
    fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {

      str_printf(&pr_str, 0, "%s geo", hd->unix_dev_name);
      PROGRESS(5, 1, pr_str);

      if(hd_getdisksize(hd_data, hd->unix_dev_name, fd, &geo, &size) == 1) {
        /* (low-level) unformatted disk */
        hd->is.notready = 1;
      }

      if(geo) add_res_entry(&hd->res, geo);
      if(size) add_res_entry(&hd->res, size);

      close(fd);
    }
  }

  pr_str = free_mem(pr_str);
}

/** @} */


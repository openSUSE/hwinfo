#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <linux/hdreg.h>
#include <sysfs/libsysfs.h>

#include "hd.h"
#include "hd_int.h"
#include "scsi.h"

#if !defined(UCLIBC)
#include <scsi/scsi.h>
#else
#define SCSI_IOCTL_GET_BUS_NUMBER	0x5386
#define SCSI_IOCTL_GET_IDLUN		0x5382
#define SCSI_IOCTL_PROBE_HOST		0x5385
#endif

#ifndef SCSI_IOCTL_SEND_COMMAND
#define SCSI_IOCTL_SEND_COMMAND		1
#endif

#ifndef SCSI_IOCTL_GET_PCI
#define SCSI_IOCTL_GET_PCI		0x5387
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * scsi info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static scsi_t *do_basic_ioctl(hd_data_t *hdata, scsi_t **ioctl_scsi, int fd);
static scsi_t *get_ioctl_scsi(hd_data_t *hd_data);
static void get_proc_scsi(hd_data_t *hd_data);
static void read_more_proc_info(hd_data_t *hd_data, scsi_t *scsi);
static scsi_t *add_scsi_entry(scsi_t **scsi, scsi_t *new);
static void dump_proc_scsi_data(hd_data_t *hd_data, str_list_t *proc_scsi);
static void dump_scsi_data(hd_data_t *hd_data, scsi_t *scsi, char *text);

/* modules, where /proc/scsi/xyz is not the module name */
static char *exlist[][2] = {
  { "esp",	"NCR53C9x" },
  { "A2091",	"a2091" },
  { "A3000",	"a3000" },
  { "Amiga7xx",	"amiga7xx" },
  { "Atari",	"atari_scsi" },
  { "BVME6000",	"bvme6000" },
  { "dpt-I2O",	"dpt_i2o" },
  { "dtc3x80",	"dtc" },
  { "eata2x",	"eata" },
  { "GVP11",	"gvp11" },
  { "INI9100U",	"ini9100u" },
  { "INIA100",	"inia100" },
  { "53c94",	"mac53c94" },
  { "mac_5380",	"mac_scsi" },
  { "MVME16x",	"mvme16x" },
  { "isp2x00",	"qlogicfc" },
  { "isp1020",	"qlogicisp" },
  { "SGIWD93",	"sgiwd93" },
  { "u14_34f",	"u14-34f" }
};

void hd_scan_scsi(hd_data_t *hd_data)
{
  hd_t *hd, *hd2;
  hd_res_t *res;
  scsi_t *ioctl_scsi, *scsi, *scsi2, *scsi3, *next;
  str_list_t *sl;
  int i, j;
  unsigned found, found_a;
  driver_info_t *di;
  struct {
    char *driver;
    unsigned hd_idx;
  } scsi_ctrl[16];		/* max. 16 scsi host adpaters */

  if(!hd_probe_feature(hd_data, pr_scsi)) return;

  hd_data->module = mod_scsi;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->scsi = NULL;

  PROGRESS(1, 0, "read info");

  get_proc_scsi(hd_data);

  // only if get_proc_scsi() found at least 1 device
  ioctl_scsi = hd_data->scsi ? get_ioctl_scsi(hd_data) : NULL;

  for(scsi = hd_data->scsi; scsi; scsi = scsi->next) {
    for(scsi2 = ioctl_scsi; scsi2; scsi2 = scsi2->next) {
      if(
        !scsi2->deleted &&
        !scsi2->fake &&
        scsi2->host == scsi->host &&
        scsi2->channel == scsi->channel &&
        scsi2->id == scsi->id &&
        scsi2->lun == scsi->lun
      ) {
        free_mem(scsi2->vendor);
        free_mem(scsi2->model);
        free_mem(scsi2->rev);
        free_mem(scsi2->type_str);
        free_mem(scsi2->guessed_dev_name);
        scsi2->vendor = new_str(scsi->vendor);
        scsi2->model = new_str(scsi->model);
        scsi2->rev = new_str(scsi->rev);
        scsi2->type_str = new_str(scsi->type_str);
        scsi2->guessed_dev_name = new_str(scsi->guessed_dev_name);
        if(scsi2->type == sc_sdev_other) scsi2->type = scsi->type;

        scsi3 = scsi->next;
        free_scsi(scsi, 0);
        *scsi = *scsi2;
        scsi->next = scsi3;

        scsi3 = scsi2->next;
        memset(scsi2, 0, sizeof *scsi2);
        scsi2->next = scsi3;
        scsi2->deleted = 1;
      }
    }
  }

  for(scsi = ioctl_scsi; scsi; scsi = scsi->next) {
    if(!scsi->deleted) {
      scsi2 = add_scsi_entry(&hd_data->scsi, new_mem(sizeof *scsi));
      *scsi2 = *scsi;
      scsi2->next = NULL;
      scsi3 = scsi->next;
      memset(scsi, 0, sizeof *scsi);
      scsi->next = scsi3;
      scsi->deleted = 1;
    }
  }

  /*
   * Fix device names: for removable media, a device open may fail (Zip
   * drives in particular...) and so we don't have a proper device name
   * assigned yet (apart from the generic device names). We'll try and do
   * some guessing here.
   */
  for(scsi = hd_data->scsi; scsi; scsi = scsi->next) {
    if(scsi->fake) continue;
    if(
      scsi->type != sc_sdev_other &&
      scsi->guessed_dev_name &&
      (
        !scsi->dev_name ||
        strstr(scsi->dev_name, "/sg")
      )
    ) {
      free_mem(scsi->dev_name);
      scsi->dev_name = new_str(scsi->guessed_dev_name);
    }
  }

  if((hd_data->debug & HD_DEB_SCSI)) dump_scsi_data(hd_data, hd_data->scsi, "final scsi");

  /*
   * find out which scsi host adapter number belongs to which scsi controller
   */
  memset(scsi_ctrl, 0, sizeof scsi_ctrl);
  for(scsi = hd_data->scsi; scsi; scsi = scsi->next) {
    if(
      scsi->host < sizeof scsi_ctrl / sizeof *scsi_ctrl &&
      !scsi_ctrl[scsi->host].driver
    ) {
      scsi_ctrl[scsi->host].driver = scsi->driver;
    }
  }

  for(i = 0; (unsigned) i < sizeof scsi_ctrl / sizeof *scsi_ctrl; i++) {
    if(!scsi_ctrl[i].driver) continue;

    /* look for a matching scsi card */
    found = found_a = 0;
    for(hd = hd_data->hd; hd; hd = hd->next) {
      /* skip cards we already dealt with */
      for(j = 0; (unsigned) j < sizeof scsi_ctrl / sizeof *scsi_ctrl; j++) {
        if(hd->idx == scsi_ctrl[j].hd_idx) break;
      }
      if((unsigned) j < sizeof scsi_ctrl / sizeof *scsi_ctrl) continue;

      if(!hd->driver_info) hddb_add_info(hd_data, hd);
      for(di = hd->driver_info; di; di = di->next) {
        if(
          di->any.type == di_module &&
          di->module.names
        ) {
          for(sl = di->module.names; sl; sl = sl->next) {
            if(!strcmp(sl->str, scsi_ctrl[i].driver)) {
              if(!found) found = hd->idx;
              if(di->module.active && !found_a) found_a = hd->idx;
              break;
            }
          }
        }
      }

      if(found_a) break;
    }
    scsi_ctrl[i].hd_idx = found_a ? found_a : found;
  }

  if((hd_data->debug & HD_DEB_SCSI)) {
    for(i = 0; (unsigned) i < sizeof scsi_ctrl / sizeof *scsi_ctrl; i++) {
      if(scsi_ctrl[i].driver)
        ADD2LOG("  %2d: %10s %2u\n", i, scsi_ctrl[i].driver, scsi_ctrl[i].hd_idx);
    }
  }

  /*
   * finally, create the hardware entries
   */
  for(scsi = hd_data->scsi; scsi; scsi = next) {
    next = scsi->next;

    if(!scsi->fake) read_more_proc_info(hd_data, scsi);

    if(scsi->fake) {
      scsi->next = NULL;
      free_scsi(scsi, 1);
      continue;
    }

    hd = add_hd_entry(hd_data, __LINE__, 0);
    if(scsi->type != sc_sdev_scanner) {
      hd->base_class.id = bc_storage_device;
      hd->sub_class.id = scsi->type;
    }
    else {
      hd->base_class.id = bc_scanner;
    }
    hd->bus.id = bus_scsi;
    hd->slot = (scsi->host << 8) + (scsi->channel << 4) + (scsi->id);
    hd->func = scsi->lun;
    hd->unix_dev_name = new_str(scsi->dev_name);
    hd->vendor.name = new_str(scsi->vendor);
    hd->device.name = new_str(scsi->model);
    hd->revision.name = new_str(scsi->rev);
    hd->serial = new_str(scsi->serial);
    hd->usb_guid = new_str(scsi->usb_guid);
    if(scsi->host < sizeof scsi_ctrl / sizeof *scsi_ctrl) {
      hd->attached_to = scsi_ctrl[scsi->host].hd_idx;
      add_str_list(&hd->drivers, scsi_ctrl[scsi->host].driver);
    }
    if(scsi->pci_info) {
      for(hd2 = hd_data->hd; hd2; hd2 = hd2->next) {
        if(
          hd2->bus.id == bus_pci &&
          hd2->slot == (scsi->pci_bus << 8) + scsi->pci_slot &&
          hd2->func == scsi->pci_func
        ) {
          hd->attached_to = hd2->idx;
          // ###### add driver
        }
      }
    }

#ifdef __PPC__
    /* ###### move this to int.c ? */
    if(
      (hd2 = hd_get_device_by_idx(hd_data, hd->attached_to)) &&
      hd2->rom_id
    ) {
      str_printf(&hd->rom_id, 0, "%s/@%u", hd2->rom_id, scsi->id);
    }
#endif

#if defined(__s390__) || defined(__s390x__)
    /* find out WWPN and FCP LUN */
    scsi->wwpn=scsi->fcp_lun=(uint64_t)-1;
    if(hd->unix_dev_name)
    {
      char sysfs_path[SYSFS_PATH_MAX];
      struct sysfs_attribute* attr;
      
      sprintf(sysfs_path,"/sys/block%s/device/wwpn",hd->unix_dev_name+4);
      attr=sysfs_open_attribute(sysfs_path);
      if(!sysfs_read_attribute(attr))
	scsi->wwpn=strtoull(attr->value,0,16);
      else
	ADD2LOG("failed to read sysfs attribute %s\n",sysfs_path);
      
      sprintf(sysfs_path,"/sys/block%s/device/fcp_lun",hd->unix_dev_name+4);
      attr=sysfs_open_attribute(sysfs_path);
      if(!sysfs_read_attribute(attr))
	scsi->fcp_lun=strtoull(attr->value,0,16);
      else
	ADD2LOG("failed to read sysfs attribute %s\n",sysfs_path);

      sprintf(sysfs_path,"/sys/block%s/device",hd->unix_dev_name+4);
      if(readlink(sysfs_path,sysfs_path,SYSFS_PATH_MAX)<0)
	ADD2LOG("failed to determine controller (unable to follow device link)");
      else
      {
	rindex(sysfs_path,'/')[0]=0;
	rindex(sysfs_path,'/')[0]=0;
	scsi->controller_id=new_str(rindex(sysfs_path,'/')+1);
      }
    }
#endif
    
    if(scsi->size) {
      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->size.type = res_size;
      res->size.unit = size_unit_sectors;
      res->size.val1 = scsi->size;
      res->size.val2 = 512;
    }

    if(scsi->lgeo_c) {
      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->disk_geo.type = res_disk_geo;
      res->disk_geo.cyls = scsi->lgeo_c;
      res->disk_geo.heads = scsi->lgeo_h;
      res->disk_geo.sectors = scsi->lgeo_s;
      res->disk_geo.logical = 1;
    }

    if(scsi->pgeo_c) {
      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->disk_geo.type = res_disk_geo;
      res->disk_geo.cyls = scsi->pgeo_c;
      res->disk_geo.heads = scsi->pgeo_h;
      res->disk_geo.sectors = scsi->pgeo_s;
      res->disk_geo.logical = 0;
    }

    if((scsi->cache & 4) && scsi->type == sc_sdev_cdrom) {
      hd->prog_if.id = pif_cdr;
    }

    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_scsi;
    hd->detail->scsi.data = scsi;

    scsi->next = NULL;
  }
  hd_data->scsi = NULL;

  ioctl_scsi = free_scsi(ioctl_scsi, 1);

}

scsi_t *do_basic_ioctl(hd_data_t *hd_data, scsi_t **ioctl_scsi, int fd)
{
  scsi_t *scsi = NULL;
  unsigned char buf[0x400];
  unsigned scsi_host;
  unsigned u, u1, u2;

  memset(buf, 0, sizeof buf);
  if(!ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, buf)) {
    scsi_host = *(unsigned *) buf;
    memset(buf, 0, sizeof buf);
    if(!ioctl(fd, SCSI_IOCTL_GET_IDLUN, buf)) {
#if 0
      ADD2LOG("  ioctl: ");
      hexdump(&hd_data->log, 0, 16, buf);
      ADD2LOG("\n");
#endif
      scsi = add_scsi_entry(ioctl_scsi, new_mem(sizeof *scsi));
      scsi->type = sc_sdev_other;
      scsi->generic_dev = -1;
      scsi->host = scsi_host;
      u = *(unsigned *) buf;
      scsi->id = u & 0xff;
      scsi->lun = (u >> 8) & 0xff;
      scsi->channel = (u >> 16) & 0xff;
      scsi->inode_low = (u >> 24) & 0xff;
      scsi->unique = *(unsigned *) (buf + 4);

      memset(buf, 0, sizeof buf);
      *(unsigned *) buf = sizeof buf - 0x10;

      if(ioctl(fd, SCSI_IOCTL_PROBE_HOST, buf) == 1) {
        scsi->info = new_str(buf);
      }

      memset(buf, 0, sizeof buf);
      if(!ioctl(fd, SCSI_IOCTL_GET_PCI, buf)) {
        ADD2LOG("  scsi pci ioctl: ");
        hexdump(&hd_data->log, 1, 9, buf);
        ADD2LOG("\n");
        buf[9] = 0;
        if(sscanf(buf, "%x:%x.%x", &u, &u1, &u2) == 3) {
          scsi->pci_info = 1;
          scsi->pci_bus = u;
          scsi->pci_slot = u1;
          scsi->pci_func = u2;
          ADD2LOG(
            "  pci info: bus = 0x%x, slot = 0x%x, func = 0x%x\n",
            scsi->pci_bus, scsi->pci_slot, scsi->pci_func
          );
        }
      }
    }
  }

  return scsi;
}

scsi_t *get_ioctl_scsi(hd_data_t *hd_data)
{
  unsigned char scsi_cmd_buf[0x400];
  unsigned inode_low;
  int i, j, k, fd;
  char *dev_name = NULL;
  scsi_t *ioctl_scsi = NULL, *scsi, *gen;
  char *sdevs[] = { "g", "r", "t" };	/* generic first, no *disk* devices */
  unsigned char *uc;
  char s[3], *t, *t2, *t3;
  DIR *dir, *dir2;
  struct dirent *de;
  struct stat sbuf;
  hd_res_t *geo, *size;
  str_list_t *proc_dirs, *sl;

  /* TODO?: sr.c:get_capabilities */

  PROGRESS(2, 0, "ioctl");  

  for(i = 0; (unsigned) i < sizeof sdevs / sizeof *sdevs; i++) {
    for(j = 0; j < 256; j++) {
      str_printf(&dev_name, 0, "/dev/s%s%d", sdevs[i], j);
      fd = open(dev_name, O_RDONLY | O_NONBLOCK);
      if(fd >= 0) {
        PROGRESS(2, i * 100 + j, "ioctl");
        scsi = do_basic_ioctl(hd_data, &ioctl_scsi, fd);
        if(scsi) {
          scsi->dev_name = dev_name;
          dev_name = NULL;
          if(!i) {
            scsi->generic_dev = j;
            scsi->generic = 1;
          }
          switch(i) {
            case 1: scsi->type = sc_sdev_cdrom; break;
            case 2: scsi->type = sc_sdev_tape; break;
          }

          if(scsi->type == sc_sdev_cdrom) {
            if(
              hd_probe_feature(hd_data, pr_scsi_cache) &&
              hd_data->in_vmware != 1		/* VMWare doesn't like this */
            ) {
              PROGRESS(2, i * 100 + j, "cache");

              memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);
              *((unsigned *) (scsi_cmd_buf + 4)) = sizeof scsi_cmd_buf - 0x100;
              scsi_cmd_buf[8 + 0] = 0x1a;
              scsi_cmd_buf[8 + 2] = 0x08;
              scsi_cmd_buf[8 + 4] = 0xff;

              k = ioctl(fd, SCSI_IOCTL_SEND_COMMAND, scsi_cmd_buf);

              if(k) {
                ADD2LOG("%s status(0x1a:8) 0x%x\n", scsi->dev_name, k);
              }
              else {
                uc = scsi_cmd_buf + 8 + 4 + scsi_cmd_buf[8 + 3] + 2;
                scsi->cache = uc[0];
                ADD2LOG("  scsi cache: 0x%02x\n", uc[0]);
              }
            }
          }

        }

        close(fd);
      }
      else {
        if(errno == ENOENT) break;	/* stop on non-existent devices */
      }
    }
  }

  for(i = 0; i < 128; i++) {
    if(i < 26) {
      s[0] = i + 'a';
      s[1] = 0;
    }
    else {
      s[0] = i / 26 + 'a' - 1;
      s[1] = i % 26 + 'a';
      s[2] = 0;
    }
    if(hd_data->disks) {
      str_printf(&dev_name, 0, "sd%s", s);
      if(!search_str_list(hd_data->disks, dev_name)) continue;
    }
    str_printf(&dev_name, 0, "/dev/sd%s", s);
    fd = open(dev_name, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {
      PROGRESS(2, 300 + i, "ioctl");
      scsi = do_basic_ioctl(hd_data, &ioctl_scsi, fd);
      if(scsi) {
        scsi->dev_name = dev_name;
        dev_name = NULL;
        scsi->type = sc_sdev_disk;

        hd_getdisksize(hd_data, scsi->dev_name, fd, &geo, &size);

        if(geo) {
          scsi->lgeo_c = geo->disk_geo.cyls;
          scsi->lgeo_h = geo->disk_geo.heads;
          scsi->lgeo_s = geo->disk_geo.sectors;
        }

        if(size) {
          scsi->size = size->size.val1;
        }

        free_res_list(geo);
        free_res_list(size);

        if(hd_data->flags.fast) {
          close(fd);
          continue;
        }

        PROGRESS(2, 300 + i, "scsi cmd");

        memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);
        *((unsigned *) (scsi_cmd_buf + 4)) = sizeof scsi_cmd_buf - 0x100;
        scsi_cmd_buf[8 + 0] = 0x12;
        scsi_cmd_buf[8 + 1] = 0x01;
        scsi_cmd_buf[8 + 2] = 0x80;
        scsi_cmd_buf[8 + 4] = 0xff;

        j = ioctl(fd, SCSI_IOCTL_SEND_COMMAND, scsi_cmd_buf);

        if(j) {
          ADD2LOG("%s status(0x12) 0x%x\n", scsi->dev_name, j);
        }
        else {
          if((scsi->serial = canon_str(scsi_cmd_buf + 8 + 4, scsi_cmd_buf[8 + 3]))) {
            if(!*scsi->serial) scsi->serial = free_mem(scsi->serial);
          }
        }

        if(hd_probe_feature(hd_data, pr_scsi_geo) && scsi->size) {
          memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);
          *((unsigned *) (scsi_cmd_buf + 4)) = sizeof scsi_cmd_buf - 0x100;
          scsi_cmd_buf[8 + 0] = 0x1a;
          scsi_cmd_buf[8 + 2] = 0x04;
          scsi_cmd_buf[8 + 4] = 0xff;

          j = ioctl(fd, SCSI_IOCTL_SEND_COMMAND, scsi_cmd_buf);

          if(j) {
            ADD2LOG("%s status(0x1a:4) 0x%x\n", scsi->dev_name, j);
          }
          else {
            uc = scsi_cmd_buf + 8 + 4 + scsi_cmd_buf[8 + 3] + 2;
            scsi->pgeo_c = (uc[0] << 16) + (uc[1] << 8) + uc[2];
            scsi->pgeo_h = uc[3];
            if(scsi->pgeo_c && scsi->pgeo_h) {
              scsi->pgeo_s = scsi->size / (scsi->pgeo_c * scsi->pgeo_h);
            }
          }
        }
      }

      close(fd);
    }
    else {
      if(errno == ENOENT) break;	/* stop on non-existent devices */
    }
  }

  dev_name = free_mem(dev_name);

  for(scsi = ioctl_scsi; scsi; scsi = scsi->next) {
    if(scsi->deleted) continue;
    if(!scsi->generic) {
      for(gen = ioctl_scsi; gen; gen = gen->next) {
        if(
          gen->generic &&
          gen->host == scsi->host &&
          gen->channel == scsi->channel &&
          gen->id == scsi->id &&
          gen->lun == scsi->lun
        ) {
          if(scsi->generic_dev == -1) scsi->generic_dev = gen->generic_dev;
          gen->deleted = 1;
        }
      }
    }
  }

  proc_dirs = read_dir(PROC_SCSI, 'd');

  /* evil hack for usb-storage devices */
  for(scsi = ioctl_scsi; scsi; scsi = scsi->next) {
    if(scsi->proc_dir || !scsi->info) continue;

    if(!strcmp(scsi->info, "SCSI emulation for USB Mass Storage devices")) {
      for(sl = proc_dirs; sl; sl = sl->next) {
        str_printf(&scsi->proc_dir, 0, PROC_SCSI "/%s/%d", sl->str, scsi->host);
        scsi->host_info = read_file(scsi->proc_dir, 0, 0);
        scsi->proc_dir = free_mem(scsi->proc_dir);
        if(scsi->host_info) {
          str_printf(&scsi->proc_dir, 0, "%s/%d", sl->str, scsi->host);
          break;
        }
      }
    }
  }

  proc_dirs = free_str_list(proc_dirs);

  /* get name of /proc/scsi entry */
  if((dir = opendir(PROC_SCSI))) {
    while((de = readdir(dir))) {
      if(
        !strcmp(de->d_name, ".") ||
        !strcmp(de->d_name, "..")
      ) continue;
      t = NULL;
      str_printf(&t, 0, PROC_SCSI "/%s", de->d_name);
      if(!stat(t, &sbuf)) {
        inode_low = sbuf.st_ino & 0xff;
        i = 0;
        for(scsi = ioctl_scsi; scsi; scsi = scsi->next) {
          if(scsi->inode_low == inode_low && !scsi->proc_dir) {
            scsi->proc_dir = new_str(de->d_name);
            i = 1;
          }
        }
        if(!i) {
          /* hostadapter with no devices attached */
          t2 = new_str(de->d_name);
          if((dir2 = opendir(t))) {
            while((de = readdir(dir2))) {
              j = strtoul(de->d_name, &t3, 10);
              if(t3 != de->d_name && !*t3) {
                /*
                 * create a fake entry that will not show up in the device
                 * list but keeps track of the scsi host number <-> driver name
                 * assignment
                 */
                scsi = add_scsi_entry(&ioctl_scsi, new_mem(sizeof *scsi));
                scsi->fake = 1;
                scsi->type = sc_sdev_other;
                scsi->generic_dev = -1;
                scsi->host = j;
                scsi->inode_low = inode_low;
                scsi->proc_dir = new_str(t2);
              }
            }
            closedir(dir2);
          }
          t2 = free_mem(t2);
        }
      }
      t = free_mem(t);
    }
    closedir(dir);
  }

#if 0
  kernel 2.2.16
  name		module / *.c	inode number
  --------------------------------------------
  esp		NCR53C9x	PROC_SCSI_ESP	// conflicts with esp.c !!!
  A2091		a2091		PROC_SCSI_A2091
  A3000		a3000		PROC_SCSI_A3000
  Amiga7xx	amiga7xx	PROC_SCSI_AMIGA7XX
  Atari		atari_scsi	PROC_SCSI_ATARI
  BVME6000	bvme6000	PROC_SCSI_BVME6000
  dpt-I2O	dpt_i2o		PROC_SCSI_DPT_I2O
  dtc3x80	dtc		PROC_SCSI_T128
  eata2x	eata		PROC_SCSI_EATA2X
  GVP11		gvp11		PROC_SCSI_GVP11
  INI9100U	ini9100u	PROC_SCSI_INI9100U
  INIA100	inia100		PROC_SCSI_INIA100
  53c94		mac53c94	PROC_SCSI_53C94
  mac_5380	mac_scsi	PROC_SCSI_MAC
  MVME16x	mvme16x		PROC_SCSI_MVME16x
  isp2x00	qlogicfc	PROC_SCSI_QLOGICFC
  isp1020	qlogicisp	PROC_SCSI_QLOGICISP
  SGIWD93	sgiwd93		PROC_SCSI_SGIWD93
  u14_34f	u14-34f		PROC_SCSI_U14_34F
#endif

  /* map /proc/scsi/xyz --> module name */
  for(scsi = ioctl_scsi; scsi; scsi = scsi->next) {
    if(scsi->proc_dir) {
      for(i = 0; (unsigned) i < sizeof exlist / sizeof *exlist; i++) {
        if(!strcmp(scsi->proc_dir, exlist[i][0])) {
          scsi->driver = new_str(exlist[i][1]);
          break;
        }
      }
      if(!scsi->driver) {
        /* usb-storage is somewhat special... */
        if(strstr(scsi->proc_dir, "usb-storage") == scsi->proc_dir) {
          scsi->driver = new_str("usb-storage");
        }
      }
      if(!scsi->driver) scsi->driver = new_str(scsi->proc_dir);
    }
  }

  if((hd_data->debug & HD_DEB_SCSI)) dump_scsi_data(hd_data, ioctl_scsi, "ioctl scsi");

  return ioctl_scsi;
}

void get_proc_scsi(hd_data_t *hd_data)
{
  unsigned u0, u1, u2, u3;
  char s[3], *s0, *s1, *s2;
  str_list_t *proc_scsi;
  scsi_t *scsi = NULL;
  str_list_t *sl;
  char scsi_type[32];
  unsigned sd_cnt = 0, sr_cnt = 0, st_cnt = 0;

  PROGRESS(1, 1, "proc");

  if(!(proc_scsi = read_file(PROC_SCSI_SCSI, 1, 0))) return;
  if((hd_data->debug & HD_DEB_SCSI)) dump_proc_scsi_data(hd_data, proc_scsi);

  PROGRESS(1, 2, "build list");  

  for(sl = proc_scsi; sl; sl = sl->next) {
    if(sscanf(sl->str, "Host: scsi%u Channel: %u Id: %u Lun: %u", &u0, &u1, &u2, &u3) == 4) {
      scsi = add_scsi_entry(&hd_data->scsi, new_mem(sizeof *scsi));
      scsi->host = u0;
      scsi->channel = u1;
      scsi->id = u2;
      scsi->lun = u3;
      scsi->generic_dev = -1;
      continue;
    }

    if(scsi && sscanf(sl->str, " Type: %29[^\n]", scsi_type) == 1) {
      /*
       * cf.
       *   drivers/scsi/scsi.c:scsi_device_types
       *   include/scsi/scsi.h:TYPE_...
       *   drivers/scsi/sd.c:sd_attach()
       *   drivers/scsi/sr.c:sr_attach()
       */

      if(strstr(scsi_type, "Direct-Access") == scsi_type) {
        scsi->type = sc_sdev_disk;
      }
      else if(strstr(scsi_type, "Optical Device") == scsi_type) {
        scsi->type = sc_sdev_disk;
      }
      else if(strstr(scsi_type, "CD-ROM") == scsi_type) {
        scsi->type = sc_sdev_cdrom;
      }
      else if(strstr(scsi_type, "WORM") == scsi_type) {
        scsi->type = sc_sdev_cdrom;
      }
      else if(strstr(scsi_type, "Sequential-Access") == scsi_type) {
        scsi->type = sc_sdev_tape;
      }
      else if(strstr(scsi_type, "Scanner") == scsi_type) {
        scsi->type = sc_sdev_scanner;
      }
      else if(
        strstr(scsi_type, "Processor") == scsi_type &&
        (
          strstr(scsi->vendor, "HP") == scsi->vendor ||
          /* seems Epson learned from HP... */
          strstr(scsi->vendor, "EPSON") == scsi->vendor
        )
      ) {
        scsi->type = sc_sdev_scanner;
      }
      else {
        scsi->type = sc_sdev_other;
      }

      switch(scsi->type) {
        case sc_sdev_disk:
          if(sd_cnt < 26) {
            s[0] = sd_cnt + 'a';
            s[1] = 0;
          }
          else {
            s[0] = sd_cnt / 26 + 'a' - 1;
            s[1] = sd_cnt % 26 + 'a';
            s[2] = 0;
          }
          str_printf(&scsi->guessed_dev_name, 0, "/dev/sd%s", s);
          sd_cnt++;
          break;

        case sc_sdev_cdrom:
          str_printf(&scsi->guessed_dev_name, 0, "/dev/sr%u", sr_cnt++);
          break;

        case sc_sdev_tape:
          str_printf(&scsi->guessed_dev_name, 0, "/dev/st%u", st_cnt++);
          break;
      }

      scsi->type_str = canon_str(scsi_type, strlen(scsi_type));
      scsi = NULL;
      continue;
    }

    if(
      scsi &&
      (s0 = strstr(sl->str, "Vendor:")) &&
      (s1 = strstr(sl->str, "Model:")) &&
      (s2 = strstr(sl->str, "Rev:"))
    ) {
      *s1 = *s2 = 0;
      s0 += sizeof "Vendor:" - 1;
      s1 += sizeof "Model:" - 1;
      s2 += sizeof "Rev:" - 1;
      scsi->vendor = canon_str(s0, strlen(s0));
      scsi->model = canon_str(s1, strlen(s1));
      scsi->rev = canon_str(s2, strlen(s2));
      continue;
    }
  }

  free_str_list(proc_scsi);

  if((hd_data->debug & HD_DEB_SCSI)) dump_scsi_data(hd_data, hd_data->scsi, "proc scsi");
}


void read_more_proc_info(hd_data_t *hd_data, scsi_t *scsi)
{
  str_list_t *sl;
  char buf[64];

  if(!scsi->proc_dir) return;

  if(scsi->host_info) {

    /* handle usb floppies */
    if(scsi->driver && !strcmp(scsi->driver, "usb-storage")) {
      for(sl = scsi->host_info; sl; sl = sl->next) {
        if(
          (scsi->type == sc_sdev_other || scsi->type == sc_sdev_disk) &&
          strstr(sl->str, "Protocol:") &&
          strstr(sl->str, "UFI")
        ) {
          scsi->type = sc_sdev_floppy;
        }
        if(strstr(sl->str, "Attached:") && strstr(sl->str, "No")) {
          scsi->fake = 1;
        }
        if(sscanf(sl->str, " GUID: %63s", buf) == 1) {
          scsi->usb_guid = new_str(buf);
        }
      }
    }

    if((hd_data->debug & HD_DEB_SCSI)) {
      ADD2LOG("----- %s -----\n", scsi->proc_dir);
      for(sl = scsi->host_info; sl; sl = sl->next) {
        ADD2LOG("  %s", sl->str);
      }
      ADD2LOG("----- %s end -----\n", scsi->proc_dir);
    }
  }
}


/*
 * Store a raw SCSI entry; just for convenience.
 */
scsi_t *add_scsi_entry(scsi_t **scsi, scsi_t *new)
{
  while(*scsi) scsi = &(*scsi)->next;
  return *scsi = new;
}


/*
 * add proc/scsi/scsi to global log
 */
void dump_proc_scsi_data(hd_data_t *hd_data, str_list_t *proc_scsi)
{
  ADD2LOG("----- /proc/scsi/scsi -----\n");
  for(; proc_scsi; proc_scsi = proc_scsi->next) {
    ADD2LOG("  %s", proc_scsi->str);
  }
  ADD2LOG("----- /proc/scsi/scsi end -----\n");
}

/*
 * Add the scsi data to global log.
 */
void dump_scsi_data(hd_data_t *hd_data, scsi_t *scsi, char *text)
{
  int i;

  if(!scsi) return;

  ADD2LOG("----- %s data -----\n", text);
  for(; scsi; scsi = scsi->next) {
    if(scsi->deleted) continue;
    ADD2LOG(
      "  host %u, channel %u, id %u, lun %u, type %d, cache 0x%02x%s\n",
      scsi->host, scsi->channel, scsi->id, scsi->lun, scsi->type, scsi->cache,
      scsi->fake ? " [*]" : ""
    );
    if(scsi->dev_name || scsi->guessed_dev_name || scsi->generic_dev != -1) {
      ADD2LOG(
        "    %s [%s] (sg%d)\n",
        scsi->dev_name ? scsi->dev_name : "<no device name>",
        scsi->guessed_dev_name ? scsi->guessed_dev_name : "?",
        scsi->generic_dev
      );
    }
    i = 0;
    if(scsi->lgeo_s) {
      ADD2LOG("    chs(log) %u/%u/%u", scsi->lgeo_c, scsi->lgeo_h, scsi->lgeo_s);
      i++;
    }
    if(scsi->pgeo_s) {
      ADD2LOG("%schs(phys) %u/%u/%u", i ? ", " : "    ", scsi->pgeo_c, scsi->pgeo_h, scsi->pgeo_s);
      i++;
    }
    if(scsi->size) {
      ADD2LOG("%s%"PRIu64" sectors", i ? ", " : "    ", scsi->size);
      i++;
    }
    if(i) ADD2LOG("\n");
    if(scsi->vendor || scsi->model || scsi->rev || scsi->type_str) {
      ADD2LOG(
        "    vendor \"%s\", model \"%s\", rev \"%s\", type \"%s\"\n",
        scsi->vendor, scsi->model, scsi->rev, scsi->type_str
      );
    }
    if(scsi->serial) {
      ADD2LOG("    serial \"%s\"\n", scsi->serial);
    }
    if(scsi->inode_low || scsi->proc_dir || scsi->driver) {
      ADD2LOG(
        "    inode 0x%02x, proc_dir \"%s\", driver \"%s\"\n",
        scsi->inode_low,
        scsi->proc_dir ? scsi->proc_dir : "<no proc dir>",
        scsi->driver ? scsi->driver : "<no driver>"
      );
    }
    if(scsi->unique) {
      ADD2LOG("    unique 0x%x\n", scsi->unique);
    }
    if(scsi->info) {
      ADD2LOG("    info \"%s\"\n", scsi->info);
    }
    if(scsi->next) ADD2LOG("\n");
  }
  ADD2LOG("----- %s data end -----\n", text);
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <linux/hdreg.h>
#define _LINUX_STRING_H_ /* HACK */
#include <linux/fs.h>

#include "hd.h"
#include "hd_int.h"
#include "scsi.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * scsi info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void dump_scsi_data(hd_data_t *hd_data);

void hd_scan_scsi(hd_data_t *hd_data)
{

  hd_t *hd;
  unsigned char scsi_cmd_buf[0x400];
  unsigned u0, u1, u2;
  unsigned long secs;
  int i, fd, host_line, prog_cnt = 0;
  char scsi_vend[20], scsi_model[40], scsi_rev[20], scsi_type[30];
  char *s0, *s1, *s2;
  unsigned sd_cnt = 0, sr_cnt = 0, st_cnt = 0, sg_cnt = 0;
  char sd_dev[] = "/dev/sd";
  char sr_dev[] = "/dev/sr";		// or scd?
  char st_dev[] = "/dev/st";
  char sg_dev[] = "/dev/sg";
  struct hd_geometry geo;
  str_list_t *sl;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_scsi)) return;

  hd_data->module = mod_scsi;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->scsi = NULL;

  PROGRESS(1, 0, "read info");

  if(!(hd_data->scsi = read_file(PROC_SCSI_SCSI, 1, 0))) return;
  if((hd_data->debug & HD_DEB_SCSI)) dump_scsi_data(hd_data);

  PROGRESS(2, 0, "build list");  

  host_line = 0;
  for(sl = hd_data->scsi; sl; sl = sl->next) {
    if(sscanf(sl->str, "Host: %*s Channel: %u Id: %u Lun: %u", &u0, &u1, &u2) == 3) {
      host_line = 1;
      *scsi_vend = *scsi_model = *scsi_rev = *scsi_type = 0;
      continue;
    }

    if(sscanf(sl->str, " Type: %29s", scsi_type) == 1) {
      if(host_line) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_storage_device;
        hd->bus = bus_scsi;
        hd->slot = (u0 << 8) + (u1 & 0xff);
        hd->func = u2;

        /*
         * cf.
         *   drivers/scsi/scsi.c:scsi_device_types
         *   include/scsi/scsi.h:TYPE_...
         *   drivers/scsi/sd.c:sd_attach()
         *   drivers/scsi/sr.c:sr_attach()
         */

        // fix: ##### sdaa, sdab, etc. for >26
        // TODO: sr.c:get_capabilities
        // ##### strstr() or strcmp()???

        if(!strcmp(scsi_type, "Direct-Access")) {
          hd->sub_class = sc_sdev_disk;
          str_printf(&hd->unix_dev_name, 0, "%s%c", sd_dev, sd_cnt + 'a');
          sd_cnt++;
        }
        else if(strstr(sl->str, "Optical Device")) {
          hd->sub_class = sc_sdev_disk;
          str_printf(&hd->unix_dev_name, 0, "%s%c", sd_dev, sd_cnt + 'a');
          sd_cnt++;
        }
        else if(strstr(sl->str, "CD-ROM")) {
          hd->sub_class = sc_sdev_cdrom;
          str_printf(&hd->unix_dev_name, 0, "%s%u", sr_dev, sr_cnt);
          sr_cnt++;
        }
        else if(strstr(sl->str, "WORM")) {
          hd->sub_class = sc_sdev_cdrom;
          str_printf(&hd->unix_dev_name, 0, "%s%u", sr_dev, sr_cnt);
          sr_cnt++;
        }
        else if(strstr(sl->str, "Sequential-Access")) {	// ##### is that so???
          hd->sub_class = sc_sdev_tape;
          str_printf(&hd->unix_dev_name, 0, "%s%u", st_dev, st_cnt);
          st_cnt++;
        }
        else {
          hd->sub_class = sc_sdev_other;
          str_printf(&hd->unix_dev_name, 0, "%s%u", sg_dev, sg_cnt);
        }

        sg_cnt++;

        if(*scsi_vend) hd->vend_name = canon_str(scsi_vend, strlen(scsi_vend));
        if(*scsi_model) hd->dev_name = canon_str(scsi_model, strlen(scsi_model));
        if(*scsi_rev) hd->rev_name = canon_str(scsi_rev, strlen(scsi_rev));

        PROGRESS(3, ++prog_cnt, "get geo");  

        fd = -1;
        secs = 0;
        if(
          hd->sub_class == sc_sdev_disk &&
          hd->unix_dev_name &&
          (fd = open(hd->unix_dev_name, O_RDONLY)) >= 0
        ) {
          u0 = 0;

          if(!ioctl(fd, BLKGETSIZE, &secs)) {
            ADD2LOG("scsi ioctl(secs) ok\n");
          }

          if(!ioctl(fd, HDIO_GETGEO, &geo)) {
            u0 = 1;
            ADD2LOG("scsi ioctl(geo) ok\n");
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->disk_geo.type = res_disk_geo;
            res->disk_geo.cyls = geo.cylinders;
            res->disk_geo.heads = geo.heads;
            res->disk_geo.sectors = geo.sectors;
            res->disk_geo.logical = 1;
          }

          if(secs || u0) {
            if(!secs) secs = geo.cylinders * geo.heads * geo.sectors;
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->size.type = res_size;
            res->size.unit = size_unit_sectors;
            res->size.val1 = secs;
            res->size.val2 = 512;
          }
        }

        if(fd >= 0) close(fd);

        fd = -1;
        if(
          hd->sub_class == sc_sdev_disk &&
          hd->unix_dev_name &&
          (fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK)) >= 0
        ) {

          PROGRESS(3, prog_cnt, "scsi cmd");  

          memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);
          *((unsigned *) (scsi_cmd_buf + 4)) = sizeof scsi_cmd_buf - 0x100;
          scsi_cmd_buf[8 + 0] = 0x12;
          scsi_cmd_buf[8 + 1] = 0x01;
          scsi_cmd_buf[8 + 2] = 0x80;
          scsi_cmd_buf[8 + 4] = 0xff;

          i = ioctl(fd, 1 /* SCSI_IOCTL_SEND_COMMAND */, scsi_cmd_buf);

          if(i) {
            ADD2LOG("%s: scsi status(0x12) %d\n", hd->unix_dev_name, i);
          }
          else {
            hd->serial = canon_str(scsi_cmd_buf + 8 + 4, scsi_cmd_buf[8 + 3]);
          }


          if(hd_probe_feature(hd_data, pr_scsi_geo) && secs) {
            memset(scsi_cmd_buf, 0, sizeof scsi_cmd_buf);
            *((unsigned *) (scsi_cmd_buf + 4)) = sizeof scsi_cmd_buf - 0x100;
            scsi_cmd_buf[8 + 0] = 0x1a;
            scsi_cmd_buf[8 + 2] = 0x04;
            scsi_cmd_buf[8 + 4] = 0xff;

            i = ioctl(fd, 1 /* SCSI_IOCTL_SEND_COMMAND */, scsi_cmd_buf);

            if(i) {
              ADD2LOG("%s: scsi status(0x1a) %d\n", hd->unix_dev_name, i);
            }
            else {
              unsigned char *uc;
              unsigned cyls, heads;
              
              uc = scsi_cmd_buf + 8 + 4 + scsi_cmd_buf[8 + 3] + 2;
              cyls = (uc[0] << 16) + (uc[1] << 8) + uc[2];
              heads = uc[3];
              if(cyls && heads) {
                res = add_res_entry(&hd->res, new_mem(sizeof *res));
                res->disk_geo.type = res_disk_geo;
                res->disk_geo.cyls = cyls;
                res->disk_geo.heads = heads;
                res->disk_geo.sectors = secs / (cyls * heads);
                res->disk_geo.logical = 0;
              }
            }
          }

        }

        if(fd >= 0) close(fd);

        host_line = 0;
      }
    }

    if(
      (s0 = strstr(sl->str, "Vendor:")) &&
      (s1 = strstr(sl->str, "Model:")) &&
      (s2 = strstr(sl->str, "Rev:"))
    ) {
      *s1 = *s2 = 0;
      s0 += sizeof "Vendor:" - 1;
      s1 += sizeof "Model:" - 1;
      s2 += sizeof "Rev:" - 1;
      strncpy(scsi_vend, s0, sizeof scsi_vend - 1);
      strncpy(scsi_model, s1, sizeof scsi_model - 1);
      strncpy(scsi_rev, s2, sizeof scsi_rev - 1);
      scsi_vend[sizeof scsi_vend - 1] =
      scsi_model[sizeof scsi_model - 1] =
      scsi_rev[sizeof scsi_rev - 1] = 0;
    }
  }
}

/*
 * Add some scsi data to the global log.
 */
void dump_scsi_data(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("----- /proc/scsi/scsi -----\n");
  for(sl = hd_data->scsi; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/scsi/scsi end -----\n");
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/pci.h>

#include "hd.h"
#include "hd_int.h"
#include "ide.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * ide info
 *
 *
 * HDIO_GET_IDENTITY?
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__PPC__)
static void scan_ide2(hd_data_t *hd_data);
#endif

void hd_scan_ide(hd_data_t *hd_data)
{

  hd_t *hd;
  char *fname = NULL, buf[256], *s, *t;
  FILE *f;
  unsigned u0, u1, u2, u3;
  int i, j, ide_ifs = 0, found = 0;
  str_list_t *sl, *sl0, *sl_hd;
  hd_res_t *res;
  unsigned vend, dev, slot, func;
  unsigned parent;
  unsigned *if_table = NULL;
  char hd_buf[] = "hda";
  
  if(!hd_probe_feature(hd_data, pr_ide)) return;

  hd_data->module = mod_ide;

  /* some clean-up */
  remove_hd_entries(hd_data);

  sl0 = read_dir(PROC_IDE, 'd');

  for(sl = sl0; sl; sl = sl->next) {
    if(sscanf(sl->str, "ide%u", &u0) == 1) {
      if(u0 >= ide_ifs) ide_ifs = u0 + 1;
    }
  }

  free_str_list(sl0);

  if(ide_ifs) if_table = new_mem(ide_ifs * sizeof *if_table);

  s = t = NULL;
  for(i = 0; i < ide_ifs; i++) {
    str_printf(&s, 0, PROC_IDE "/ide%u/config", i);
    str_printf(&t, 0, "ide%u", i);
    sl0 = read_file(s, 0, 1);
    if(sl0 && sl0->str) {
      if(sscanf(sl0->str, "pci bus %x device %x vid %x did %x", &u0, &u1, &u2, &u3) == 4) {
        slot = PCI_SLOT(u1 & 0xff) + (u0 << 8);
        func = PCI_FUNC(u1 & 0xff);
        vend = MAKE_ID(TAG_PCI, u2);
        dev = MAKE_ID(TAG_PCI, u3);
        for(hd = hd_data->hd; hd; hd = hd->next) {
          if(hd->slot == slot && hd->func == func && hd->vend == vend && hd->dev == dev) {
            if_table[i] = hd->idx;
            if(!search_str_list(hd->extra_info, t)) {
              add_str_list(&hd->extra_info, t);
            }
          }
        }
      }
    }
    free_str_list(sl0);
  }

  free_mem(s);
  free_mem(t);

  sl0 = read_dir(PROC_IDE, 'l');

  for(sl_hd = NULL, sl = sl0; sl; sl = sl->next) {
    if(strstr(sl->str, "hd") == sl->str) {
      add_str_list(&sl_hd, sl->str);
    }
  }

  free_str_list(sl0);

  // for(sl = sl_hd; sl; sl = sl->next) ADD2LOG("hd: %s\n", sl->str);

  /* go through hda...hdp */
  for(i = 0; i < 16; i++) {

    hd_buf[2] = i + 'a';
    if(!search_str_list(sl_hd, hd_buf)) continue;

    PROGRESS(1, 1 + i, "read info");

    parent = 0;
    str_printf(&fname, 0, PROC_IDE "/hd%c", i + 'a');
    s = hd_read_symlink(fname);
    if(s && (t = strchr(s, '/'))) {
      *t = 0;
      if(sscanf(s, "ide%u", &u0) == 1 && u0 < ide_ifs && if_table[u0]) {
        parent = if_table[u0];
      }
    }

    str_printf(&fname, 0, PROC_IDE "/hd%c/media", i + 'a');
    if((sl = read_file(fname, 0, 1))) {
      /* ok, assume the ide drive exists */

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_storage_device;
      hd->bus = bus_ide;
      hd->slot = i;
      hd->attached_to = parent;
      found++;

      str_printf(&hd->unix_dev_name, 0, "/dev/hd%c", i + 'a');

      u0 = sc_sdev_disk;
      if(strstr(sl->str, "floppy"))
        u0 = sc_sdev_floppy;
      else if(strstr(sl->str, "cdrom"))
        u0 = sc_sdev_cdrom;
      else if(strstr(sl->str, "tape"))
        u0 = sc_sdev_tape;
      hd->sub_class = u0;

      free_str_list(sl);

      str_printf(&fname, 0, PROC_IDE "/hd%c/model", i + 'a');
      if((sl = read_file(fname, 0, 1))) {
        hd->dev_name = canon_str(sl->str, strlen(sl->str));
        free_str_list(sl);
      }

      str_printf(&fname, 0, PROC_IDE "/hd%c/driver", i + 'a');
      if((sl = read_file(fname, 0, 1))) {
        if((s = strchr(sl->str, ' '))) *s = 0;
        hd->driver = canon_str(sl->str, strlen(sl->str));
        free_str_list(sl);
      }

      str_printf(&fname, 0, PROC_IDE "/hd%c/geometry", i + 'a');
      if((sl0 = read_file(fname, 0, 2))) {
        for(sl = sl0; sl; sl = sl->next) {
          if(sscanf(sl->str, " physical %u / %u / %u", &u0, &u1, &u2) == 3) {
            if(u0 || u1 || u2) {
              res = add_res_entry(&hd->res, new_mem(sizeof *res));
              res->disk_geo.type = res_disk_geo;
              res->disk_geo.cyls = u0;
              res->disk_geo.heads = u1;
              res->disk_geo.sectors = u2;
            }
            continue;
          }

          if(sscanf(sl->str, " logical %u / %u / %u", &u0, &u1, &u2) == 3) {
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->disk_geo.type = res_disk_geo;
            res->disk_geo.cyls = u0;
            res->disk_geo.heads = u1;
            res->disk_geo.sectors = u2;
            res->disk_geo.logical = 1;
          }
        }
        free_str_list(sl0);
      }

      str_printf(&fname, 0, PROC_IDE "/hd%c/capacity", i + 'a');
      if((sl = read_file(fname, 0, 1))) {
        if(sscanf(sl->str, "%u", &u0) == 1 && u0 != 0x7fffffff) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->size.type = res_size;
          res->size.unit = size_unit_sectors;
          res->size.val1 = u0;
          res->size.val2 = 512;			// ####### sector size!!!
        }
        free_str_list(sl);
      }

      str_printf(&fname, 0, PROC_IDE "/hd%c/cache", i + 'a');
      if((sl = read_file(fname, 0, 1))) {
        if(sscanf(sl->str, "%u", &u0) == 1 && u0) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->cache.type = res_cache;
          res->cache.size = u0;
        }
        free_str_list(sl);
      }

      str_printf(&fname, 0, PROC_IDE "/hd%c/identify", i + 'a');
      if((f = fopen(fname, "r"))) {
        j = 0;
        memset(buf, sizeof buf, 0);
        while(j < sizeof buf - 1 && fscanf(f, "%x", &u0) == 1) {
          buf[j++] = u0 >> 8; buf[j++] = u0;
        }
        fclose(f);

        /* ok, we now have the ATA/ATAPI ident block */

        if(buf[0x14] || buf[0x15]) {	/* has serial id */
          hd->serial = canon_str(buf + 0x14, 20);
        }
        if(buf[0x2e] || buf[0x2f]) {	/* has revision id */
          hd->rev_name = canon_str(buf + 0x2e, 8);
        }
      }
    }
  }

  free_mem(if_table);
  free_str_list(sl_hd);

#if defined(__PPC__)
  if(!found) scan_ide2(hd_data);
#endif

  free_mem(fname);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * DASD disks that appear as ide drives
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__PPC__)

void scan_ide2(hd_data_t *hd_data)
{
  hd_t *hd;
  char *s = NULL;
  str_list_t *sl, *sl0;
  hd_res_t *geo, *size;
  int i, fd;
  int max_disks = 0;

  PROGRESS(2, 0, "dasd info");

  sl0 = read_file(PROC_ISERIES "/viodasd", 0, 0);

  for(sl = sl0; sl; sl = sl->next) {
    if(!strncmp(sl->str, "DISK", sizeof "DISK" - 1)) max_disks++;
  }

  if(max_disks) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class = bc_storage;
    hd->sub_class = sc_sto_other;
    hd->vend_name = new_str("IBM");
    hd->dev_name = new_str("VIO DASD");
  }

  for(i = 0; i < max_disks; i++) {
    str_printf(&s, 0, "/dev/hd%c", i + 'a');
    fd = open(s, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_storage_device;
      hd->sub_class = sc_sdev_disk;
      hd->bus = bus_none;
      hd->slot = i;

      hd->unix_dev_name = new_str(s);
      str_printf(&hd->dev_name, 0, "iSeries DASD #%u", i);

      hd_getdisksize(hd_data, hd->unix_dev_name, fd, &geo, &size);
              
      if(geo) add_res_entry(&hd->res, geo);
      if(size) add_res_entry(&hd->res, size);

      close(fd);
    }
  }

  free_mem(s);
  free_str_list(sl0);
}

#endif	/* defined(__PPC__) */



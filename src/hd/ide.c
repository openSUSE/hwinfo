#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


void hd_scan_ide(hd_data_t *hd_data)
{

  hd_t *hd;
  char *fname = NULL, buf[256];
  FILE *f;
  unsigned u0, u1, u2;
  int i, j;
  str_list_t *sl, *sl0;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_ide)) return;

  hd_data->module = mod_ide;

  /* some clean-up */
  remove_hd_entries(hd_data);

  /* got through hda...hdp */
  for(i = 0; i < 16; i++) {
    PROGRESS(1, 1 + i, "read info");

    str_printf(&fname, 0, PROC_IDE "/hd%c/media", i + 'a');
    if((sl = read_file(fname, 0, 1))) {
      /* ok, assume the ide drive exists */

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_storage_device;
      hd->bus = bus_ide;
      hd->slot = i;

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

      str_printf(&fname, 0, PROC_IDE "/hd%c/geometry", i + 'a');
      if((sl0 = read_file(fname, 0, 2))) {
        for(sl = sl0; sl; sl = sl->next) {
          if(sscanf(sl->str, " physical %u / %u / %u", &u0, &u1, &u2) == 3) {
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->disk_geo.type = res_disk_geo;
            res->disk_geo.cyls = u0;
            res->disk_geo.heads = u1;
            res->disk_geo.sectors = u2;
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
}


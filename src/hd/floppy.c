#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "floppy.h"

static void dump_floppy_data(hd_data_t *hd_data);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * floppy info
 *
 * It *requires* nvram support!
 *
 * This should currently be called *before* scan_misc() so we can try to get
 * the floppy controller resources in scan_misc() by actually accessing the
 * floppy drive. (Otherwise there would be a rather longish timeout.)
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


void hd_scan_floppy(hd_data_t *hd_data)
{
  hd_t *hd;
  char b0[10], b1[10], c;
  unsigned u;
  int fd, i, floppy_ctrls = 0, floppy_ctrl_idx = 0;
  str_list_t *sl;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_floppy)) return;

  hd_data->module = mod_floppy;

   /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->floppy = free_str_list(hd_data->floppy);

  PROGRESS(1, 0, "get nvram");

  /*
   * Look for existing floppy controller entries (typically there will be
   * *none*).
   */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_storage && hd->sub_class == sc_sto_floppy) {
      floppy_ctrls++;
      floppy_ctrl_idx = hd->idx;
    }
  }

  /*
   * Is enough to load the nvram module.
   *
   * Note: although you must be root to access /dev/nvram, every
   * user can read /proc/nvram.
   */
  fd = open(DEV_NVRAM, O_RDONLY | O_NONBLOCK);
  if(fd >= 0) close(fd);

  if(
    !(hd_data->floppy = read_file(PROC_NVRAM_24, 0, 0)) &&
    !(hd_data->floppy = read_file(PROC_NVRAM_22, 0, 0))
  ) return;

  if((hd_data->debug & HD_DEB_FLOPPY)) dump_floppy_data(hd_data);

  PROGRESS(2, 0, "create entry");

  for(sl = hd_data->floppy; sl; sl = sl->next) {
    if(sscanf(sl->str, " Floppy %u type : %8[0-9.]'' %8[0-9.]%c", &u, b0, b1, &c) == 4) {
      if(!floppy_ctrls) {
        /* create one, if missing (there's no floppy without a controller...) */
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_storage;
        hd->sub_class = sc_sto_floppy;
        floppy_ctrl_idx = hd->idx;
        floppy_ctrls++;
      }

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_storage_device;
      hd->sub_class = sc_sdev_floppy;
      hd->bus = bus_floppy;
      hd->slot = u;
      str_printf(&hd->unix_dev_name, 0, "/dev/fd%u", u);

      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->size.type = res_size;
      res->size.val1 = str2float(b0, 2);
      res->size.unit = size_unit_cinch;

      /* 'k' or 'M' */
      i = c == 'M' ? str2float(b1, 3) : str2float(b1, 0);

      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->size.type = res_size;
      res->size.val1 = i << 1;
      res->size.val2 = 0x200;
      res->size.unit = size_unit_sectors;

      /* the only choice... */
      if(floppy_ctrls == 1) hd->attached_to = floppy_ctrl_idx;
    }
  }
}


/*
 * Add floppy data to the global log.
 */
void dump_floppy_data(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("----- /proc/nvram -----\n");
  for(sl = hd_data->floppy; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/nvram end -----\n");
}

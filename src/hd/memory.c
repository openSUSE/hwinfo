#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "memory.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * memory stuff
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */
void hd_scan_memory(hd_data_t *hd_data)
{
  hd_t *hd;
  uint64 u;
  size_t ps = getpagesize();
  struct stat sb;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_memory)) return;

  hd_data->module = mod_memory;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "main memory size");

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class = bc_internal;
  hd->sub_class = sc_int_main_mem;

  if(!stat(PROC_KCORE, &sb)) {
    u = sb.st_size;
    if(u > ps) u -= ps;

    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->mem.type = res_mem;
    res->mem.range = u;
    res->mem.access = acc_rw;
    res->mem.enabled = 1;

    /* we'll assume no mem modules with less than 256k */
    u += 1 << 17;
    u &= -1 << 18;

    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->phys_mem.type = res_phys_mem;
    res->phys_mem.range = u;
  }
}


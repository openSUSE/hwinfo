#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hd.h"
#include "hd_int.h"
#include "prom.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * prom info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__PPC__)

void hd_scan_prom(hd_data_t *hd_data)
{
  hd_t *hd;
  unsigned char buf[16];
  FILE *f;
  prom_info_t *pt;

  if(!hd_probe_feature(hd_data, pr_prom)) return;

  hd_data->module = mod_prom;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "color");

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class = bc_internal;
  hd->sub_class = sc_int_prom;
  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_prom;
  hd->detail->prom.data = pt = new_mem(sizeof *pt);

  if((f = fopen(PROC_PROM "/color-code", "r"))) {
    if(fread(buf, 1, 2, f) == 2) {
      pt->has_color = 1;
      pt->color = buf[1];
      hd_data->color_code = pt->color | 0x10000;
      ADD2LOG("color-code: 0x%04x\n", (buf[0] << 8) + buf[1]);
    }

    fclose(f);
  }


}

#endif /* defined(__PPC__) */


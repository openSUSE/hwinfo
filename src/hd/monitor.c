#include <stdio.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "monitor.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * monitor info
 *
 * Read the info out of the 'SuSE=' entry in /proc/cmdline. It contains
 * (among others) info from the EDID record got by our syslinux extension.
 *
 * We will try to look up our monitor id in the id file to get additional
 * info.
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void add_monitor_res(hd_t *hd, unsigned x, unsigned y, unsigned hz);


/*
 * The actual probe function.
 */
void hd_scan_monitor(hd_data_t *hd_data)
{
  hd_t *hd;
  int i, j, k;
  char *s, *s0, *se, m[8];
  unsigned u;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_monitor)) return;

  hd_data->module = mod_monitor;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "cmdline");

  if(!(s = s0 = get_cmd_param(hd_data, 0))) return;

  se = s + strlen(s);

  if(se - s < 7 + 2 * 4) {
    free_mem(s0);
    return;
  }

  /* Ok, we've got it. Now we split the fields. */

  memcpy(m, s, 7); m[7] = 0; s += 7;

  hd = add_hd_entry(hd_data, __LINE__, 0);

  hd->base_class = bc_monitor;
  hd->vend = name2eisa_id(m);
  if(sscanf(m + 3, "%x", &u) == 1) hd->dev = MAKE_ID(TAG_EISA, u);
  if((u = device_class(hd_data, hd->vend, hd->dev))) {
    if((u >> 8) == bc_monitor) hd->sub_class = u & 0xff;
  }

  i = hex(s, 2); j = hex(s + 2, 2); s += 4;
  if(i > 0 && j > 0) {
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->size.type = res_size;
    res->size.unit = size_unit_cm;
    res->size.val1 = i;		/* width */
    res->size.val2 = j;		/* height */
  }

  i = hex(s, 2); s+= 2;
  if(i & (1 << 0)) add_monitor_res(hd, 720, 400, 70);
  if(i & (1 << 1)) add_monitor_res(hd, 720, 400, 88);
  if(i & (1 << 2)) add_monitor_res(hd, 640, 480, 60);
  if(i & (1 << 3)) add_monitor_res(hd, 640, 480, 67);
  if(i & (1 << 4)) add_monitor_res(hd, 640, 480, 72);
  if(i & (1 << 5)) add_monitor_res(hd, 640, 480, 75);
  if(i & (1 << 6)) add_monitor_res(hd, 800, 600, 56);
  if(i & (1 << 7)) add_monitor_res(hd, 800, 600, 60);

  i = hex(s, 2); s+= 2;
  if(i & (1 << 0)) add_monitor_res(hd,  800,  600, 72);
  if(i & (1 << 1)) add_monitor_res(hd,  800,  600, 75);
  if(i & (1 << 2)) add_monitor_res(hd,  832,  624, 75);

/*
  this would be an interlaced timing; dropping it
  if(i & (1 << 3)) add_monitor_res(hd, 1024,  768, 87);
*/

  if(i & (1 << 4)) add_monitor_res(hd, 1024,  768, 60);
  if(i & (1 << 5)) add_monitor_res(hd, 1024,  768, 70);
  if(i & (1 << 6)) add_monitor_res(hd, 1024,  768, 75);
  if(i & (1 << 7)) add_monitor_res(hd, 1280, 1024, 75);

  if(((se - s) & 1) || se - s > 8 * 4) {
//    h->error = 1;
//    h->err_text1 = "strange timing data";
    free_mem(s0);
    return;
  }

  while(s < se) {
    i = (hex(s, 2) + 31) * 8; j = hex(s + 2, 2); s += 4;
    k = 0;
    switch((j >> 6) & 3) {
      case 1: k = (i * 3) / 4; break;
      case 2: k = (i * 4) / 5; break;
      case 3: k = (i * 9) / 16; break;
    }
    if(k) add_monitor_res(hd, i, k, (j & 0x3f) + 60);
  }

  free_mem(s0);
}

void add_monitor_res(hd_t *hd, unsigned width, unsigned height, unsigned vfreq)
{
  hd_res_t *res;

  res = add_res_entry(&hd->res, new_mem(sizeof *res));
  res->monitor.type = res_monitor;
  res->monitor.width = width;
  res->monitor.height = height;
  res->monitor.vfreq = vfreq;
}

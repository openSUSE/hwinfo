#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "hd.h"
#include "hd_int.h"
#include "adb.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * adb info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifdef __PPC__

void hd_scan_adb(hd_data_t *hd_data)
{
  int i;
  unsigned u, adr = 0;
  hd_t *hd;
  str_list_t *sl;

  if(!hd_probe_feature(hd_data, pr_adb)) return;

  hd_data->module = mod_adb;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "get info");

  for(sl = hd_data->klog; sl; sl = sl->next) {
    if(sscanf(sl->str, "<4>ADB mouse at %u, %*[a-z ] %i", &u, &i) == 2 && u < 32) {
      /* u: max 15 actually, but who cares... */
      if(!(adr & (1 << u))) {
        adr |= 1 << u;
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_mouse;
        hd->sub_class.id = sc_mou_bus;
        hd->bus.id = bus_adb;
        hd->slot = u;
//        hd->func = i;
        hd->unix_dev_name = new_str(DEV_MICE);

        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0100);
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0300 + i);
      }
    }

    if(sscanf(sl->str, "<4>ADB keyboard at %u, %*[a-z ] %i", &u, &i) == 2 && u < 32) {
      /* u: max 15 actually, but who cares... */
      if(!(adr & (1 << u))) {
        adr |= 1 << u;
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_keyboard;
        hd->sub_class.id = 0;
        hd->bus.id = bus_adb;
        hd->slot = u;
//        hd->func = i;
//        hd->unix_dev_name = new_str(DEV_ADBMOUSE);

        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0100);
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0200+i);
      }
    }
  }
}

#endif	/* __PPC__ */

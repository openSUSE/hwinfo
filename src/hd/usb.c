#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "usb.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * usb info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void dump_usb_data(hd_data_t *hd_data);

void hd_scan_usb(hd_data_t *hd_data)
{
  hd_t *hd;
  str_list_t *sl;
  char rev[32], drv[80], *s;
  int vend, dev;
  int kcnt = 0, mcnt = 0;
  char *kdev = "hidbp-kbd-", *mdev = "hidbp-mse-";

  if(!hd_probe_feature(hd_data, pr_usb)) return;

  hd_data->module = mod_usb;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->usb = NULL;

  PROGRESS(1, 0, "read info");

  if(!(hd_data->usb = read_file(PROC_USB_DEVICES, 0, 0))) return;
  if((hd_data->debug & HD_DEB_USB)) dump_usb_data(hd_data);

  PROGRESS(2, 0, "build list");  

  vend = dev = 0;
  *rev = *drv = 0;
  /*
   * We just take the 'Driver=' entry to determine the device class;
   * alternatively, we could use  the "Cls, Sub, Prot" entries.
   */
  for(sl = hd_data->usb; sl || *drv; sl = sl->next) {
    if(!sl || *sl->str == 'T') {	/* !sl -> just past last line ('EOF') */
      if(*drv) {
        if(!strcmp(drv, "keyboard")) {
          hd = add_hd_entry(hd_data, __LINE__, 0);
          hd->base_class = bc_keyboard;
          hd->bus = bus_usb;
          if(*rev) {
            hd->vend = MAKE_ID(TAG_USB, vend);
            hd->dev = MAKE_ID(TAG_USB, dev);
            hd->rev_name = new_str(rev);
          }
          str_printf(&hd->unix_dev_name, 0, "/dev/%s%d", kdev, kcnt++);
        }
        else if(!strcmp(drv, "mouse")) {
          hd = add_hd_entry(hd_data, __LINE__, 0);
          hd->base_class = bc_mouse;
          hd->sub_class = sc_mou_usb;
          hd->bus = bus_usb;
          if(*rev) {
            hd->vend = MAKE_ID(TAG_USB, vend);
            hd->dev = MAKE_ID(TAG_USB, dev);
            hd->rev_name = new_str(rev);
          }
          str_printf(&hd->unix_dev_name, 0, "/dev/%s%d", mdev, mcnt++);
        }
        
      }
      vend = dev = 0;
      *rev = *drv = 0;

      if(!sl) break;	/* last line */

      if((s = strstr(sl->str, "Driver="))) {
        if(sscanf(s + sizeof "Driver=" - 1, "%79[^\n]", drv) != 1) *drv = 0;
      }
    }

    if(*sl->str == 'P') {	/* sl != NULL now */
      if(sscanf(sl->str + 3, " Vendor=%x ProdID=%x Rev= %31s", &vend, &dev, rev) != 3) {
        vend = dev = 0;
        *rev = 0;
      }
    }
  }

}

/*
 * Add some scsi data to the global log.
 */
void dump_usb_data(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("----- /proc/bus/usb/devices -----\n");
  for(sl = hd_data->usb; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/bus/usb/devices end -----\n");
}

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
#include "kbd.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * kbd detection
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__i386__) || defined(__PPC__) || defined(__alpha__)

void hd_scan_kbd(hd_data_t *hd_data)
{
  int i, j, k;
  unsigned keyb_idx, u, kid;
  char *s;
  hd_t *hd;
  str_list_t *sl;

  if(!hd_probe_feature(hd_data, pr_kbd)) return;

  hd_data->module = mod_kbd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "get info");

  k = 0; keyb_idx = 0;
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_input &&
      hd->sub_class == sc_inp_keyb
    ) {
      if(!k) keyb_idx = hd->idx;
      k++;
    }
  }

  i = j = 0;
  for(sl = hd_data->klog; sl; sl = sl->next) {
    if(strstr(sl->str, "keyboard: Too many NACKs")) i++;
    if(strstr(sl->str, "Keyboard timeout")) i++;
    if(strstr(sl->str, "Keyboard timed out")) i++;
    if(strstr(sl->str, "Detected PS/2 Mouse Port")) j = 1;
  }

#ifdef __PPC__
  if(!j) return;
#endif

  /* more than 2 timeouts -> assume no keyboard */
  kid = 0;
  if(i < 2) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class = bc_keyboard;
    if(j) {
      hd->bus = bus_ps2;
      kid = 1;
    }
    if(k == 1) hd->attached_to = keyb_idx;
    hd->vend = MAKE_ID(TAG_SPECIAL, 0x0201);
    if((s = get_cmd_param(3))) {
      if(*s && sscanf(s, "%x", &u) == 1) kid = u;
      free_mem(s);
    }
    hd->dev = MAKE_ID(TAG_SPECIAL, kid);
  }
}

#endif	/* __i386__ || __PPC__ || __alpha__ */

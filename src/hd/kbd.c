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

#ifdef __sparc__
#include <linux/serial.h>
#include <asm/kbio.h>
#include <asm/openpromio.h>
#endif

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
    hd->sub_class = sc_keyboard_kbd;
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


#if defined(__sparc__)

void hd_scan_kbd(hd_data_t *hd_data)
{
  int fd, kid, kid2, klay, ser_cons;
  unsigned u;
  struct serial_struct ser_info;
  unsigned char buf[OPROMMAXPARAM];
  struct openpromio *opio = (struct openpromio *) buf;
  hd_t *hd;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_kbd)) return;

  hd_data->module = mod_kbd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "sun kbd");

  if((fd = open(DEV_KBD, O_RDWR | O_NONBLOCK | O_NOCTTY)) >= 0) {
    if(ioctl(fd, KIOCTYPE, &kid)) kid = -1;
    if(ioctl(fd, KIOCLAYOUT, &klay)) klay = -1;
    close(fd);

    if(kid != -1) {
      ADD2LOG("sun keyboard: type %d, layout %d\n", kid, klay);

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_keyboard;
      hd->sub_class = sc_keyboard_kbd;
      hd->bus = bus_serial;
      if(kid == 4 && klay >= 0) hd->prog_if = klay;

      hd->vend = MAKE_ID(TAG_SPECIAL, 0x0202);
      kid2 = kid;
      if(kid == 4 && klay > 0x20) kid2 = 5;
      hd->dev = MAKE_ID(TAG_SPECIAL, kid2);
      if(kid2 == 5) {
        if(klay == 0x22 || klay == 0x51) {
          hd->sub_vend = MAKE_ID(TAG_SPECIAL, 0x0202);
          hd->sub_dev = MAKE_ID(TAG_SPECIAL, 0x0001);
        }
        else if(!(
          klay == 0x21 || (klay >= 0x2f && klay <= 0x31) ||
          klay == 0x50 || (klay >= 0x5e && klay <= 0x60)
        )) {
          hd->sub_vend = MAKE_ID(TAG_SPECIAL, 0x0202);
          hd->sub_dev = MAKE_ID(TAG_SPECIAL, 0x0002);
        }
      }
    }
  }


  if((fd = open(DEV_CONSOLE, O_RDWR | O_NONBLOCK | O_NOCTTY)) >= 0) {
    if(ioctl(fd, TIOCGSERIAL, &ser_info)) {
      ser_cons = -1;
    }
    else {
      ser_cons = ser_info.line;
      ADD2LOG("serial console at line %d\n", ser_cons);
    }
    close(fd);

    if(ser_cons >= 0 && (fd = open(DEV_OPENPROM, O_RDWR | O_NONBLOCK)) >= 0) {
      sprintf(opio->oprom_array, "tty%c-mode", (ser_cons & 1) + 'a');
      opio->oprom_size = sizeof buf - 0x100;
      if(!ioctl(fd, OPROMGETOPT, opio)) {
        if(opio->oprom_size < 0x100) {
          opio->oprom_array[opio->oprom_size] = 0;
          ADD2LOG(
            "prom(tty%c-mode) = \"%s\" (%d bytes)\n",
            (ser_cons & 1) + 'a', opio->oprom_array,
            opio->oprom_size
          );
          hd = add_hd_entry(hd_data, __LINE__, 0);
          hd->base_class = bc_keyboard;
          hd->sub_class = sc_keyboard_console;
          hd->bus = bus_serial;
          hd->vend = MAKE_ID(TAG_SPECIAL, 0x0203);
          hd->dev = MAKE_ID(TAG_SPECIAL, 0x0000);
          if(sscanf(opio->oprom_array, "%u,", &u) == 1) {
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->baud.type = res_baud;
            res->baud.speed = u;
          }
        }
      }
      close(fd);
    }

  }

}

#endif	/* __sparc__ */


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

#ifdef __PPC__
#include <linux/serial.h>
#endif

#ifdef __sparc__

struct serial_struct {
  int     type;
  int     line;
  unsigned long   port;
  int     irq;
  int     flags;
  int     xmit_fifo_size;
  int     custom_divisor;
  int     baud_base;
  unsigned short  close_delay;
  char    io_type;
  char    reserved_char[1];
  int     hub6;
  unsigned short  closing_wait; /* time to wait before closing */
  unsigned short  closing_wait2; /* no longer used... */
  unsigned char   *iomem_base;
  unsigned short  iomem_reg_shift;
  int     reserved[2];
};

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

#if defined(__i386__) || defined(__PPC__) || defined(__alpha__) || defined(__ia64__)

void hd_scan_kbd(hd_data_t *hd_data)
{
  int i, j, k;
  unsigned keyb_idx, u, kid;
  char *s;
  hd_t *hd;
#ifdef __PPC__
  hd_t *hd1;
  hd_res_t *res;
  int fd;
#endif
  str_list_t *sl;
#ifdef __PPC__
  struct serial_struct ser_info;
#endif

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
    if(strstr(sl->str, "keyboard: Timeout - AT keyboard not present")) i++;
    if(strstr(sl->str, "Keyboard timeout")) i++;
    if(strstr(sl->str, "Keyboard timed out")) i++;
    if(strstr(sl->str, "Detected PS/2 Mouse Port")) j = 1;
  }

#ifdef __PPC__
  PROGRESS(2, 0, "serial console");

  if((fd = open(DEV_CONSOLE, O_RDWR | O_NONBLOCK | O_NOCTTY)) >= 0) {
    if(!ioctl(fd, TIOCGSERIAL, &ser_info)) {
      ADD2LOG("serial console at line %d\n", ser_info.line);

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_keyboard;
      hd->sub_class = sc_keyboard_console;
      hd->bus = bus_serial;
      hd->dev_name = new_str("serial console");
      str_printf(&hd->unix_dev_name, 0, "/dev/ttyS%d", ser_info.line);
      u = 9600;
      for(hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
        if(
          hd1->base_class == bc_comm &&
          hd1->sub_class == sc_com_ser &&
          hd1->unix_dev_name &&
          !strcmp(hd1->unix_dev_name, hd->unix_dev_name)
        ) {
          hd->attached_to = hd1->idx;
          for(res = hd1->res; res; res = res->next) {
            if(res->any.type == res_baud) {
              u = res->baud.speed;
              break;
            }
          }
          break;
        }
      }

      /* get baud settings from /proc/cmdline */
      s = get_cmdline(hd_data, "console");
      if(s) {
        unsigned u0, u1;
        if(sscanf(s, "ttyS%u,%u", &u0, &u1) == 2) {
          if(ser_info.line == u0 && u1) u = u1;
        }
      }
      free_mem(s);

      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->baud.type = res_baud;
      res->baud.speed = u;
    }
    close(fd);
  }

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
    if((s = get_cmd_param(hd_data, 3))) {
      if(*s && sscanf(s, "%x", &u) == 1) kid = u;
      free_mem(s);
    }
    hd->dev = MAKE_ID(TAG_SPECIAL, kid);
  }
}

#endif	/* __i386__ || __PPC__ || __alpha__ || __ia64__ */


#if defined(__sparc__)

void hd_scan_kbd(hd_data_t *hd_data)
{
  int fd, kid, kid2, klay, ser_cons, i;
  unsigned u, u1, u2;
  char c1, c2;
  struct serial_struct ser_info;
  unsigned char buf[OPROMMAXPARAM];
  struct openpromio *opio = (struct openpromio *) buf;
  hd_t *hd;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_kbd)) return;

  hd_data->module = mod_kbd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "sun serial console");

  if((fd = open(DEV_CONSOLE, O_RDWR | O_NONBLOCK | O_NOCTTY)) >= 0)
    {
      if(ioctl(fd, TIOCGSERIAL, &ser_info))
	{
	  ser_cons = -1;
	}
      else
	{
	  ser_cons = ser_info.line;
	  ADD2LOG("serial console at line %d\n", ser_cons);
	}
      close(fd);

      if(ser_cons >= 0 && (fd = open(DEV_OPENPROM, O_RDWR | O_NONBLOCK)) >= 0)
	{
	  sprintf(opio->oprom_array, "tty%c-mode", (ser_cons & 1) + 'a');
	  opio->oprom_size = sizeof buf - 0x100;
	  if(!ioctl(fd, OPROMGETOPT, opio))
	    {
	      if(opio->oprom_size < 0x100)
		{
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
		  str_printf(&hd->unix_dev_name, 0, "/dev/ttyS%d", ser_cons);
		  if((i = sscanf(opio->oprom_array, "%u,%u,%c,%u,%c",
				 &u, &u1, &c1, &u2, &c2)) >= 1)
		    {
		      res = add_res_entry(&hd->res, new_mem(sizeof *res));
		      res->baud.type = res_baud;
		      res->baud.speed = u;
		      if(i >= 2) res->baud.bits = u1;
		      if(i >= 3) res->baud.parity = c1;
		      if(i >= 4) res->baud.stopbits = u2;
		      if(i >= 5) res->baud.handshake = c2;
		    }
		}
	    }
	  close(fd);
	  /* We have a serial console, so don't test for keyboard. Else
	     we will always find a PS/2 keyboard */
	  return;
	}
    }

  PROGRESS(1, 0, "sun kbd");

  if((fd = open(DEV_KBD, O_RDWR | O_NONBLOCK | O_NOCTTY)) >= 0)
    {
      if(ioctl(fd, KIOCTYPE, &kid)) kid = -1;
      if(ioctl(fd, KIOCLAYOUT, &klay)) klay = -1;
      close(fd);

      if(kid != -1)
	{
	  ADD2LOG("sun keyboard: type %d, layout %d\n", kid, klay);

	  hd = add_hd_entry(hd_data, __LINE__, 0);
	  hd->base_class = bc_keyboard;
	  hd->sub_class = sc_keyboard_kbd;
	  hd->bus = bus_serial;
	  if(kid == 4 && klay >= 0)
	    hd->prog_if = klay;

	  hd->vend = MAKE_ID(TAG_SPECIAL, 0x0202);
	  kid2 = kid;
	  if(kid == 4 && klay > 0x20)
	    kid2 = 5;
	  hd->dev = MAKE_ID(TAG_SPECIAL, kid2);
	  if(kid2 == 5) {
	    if(klay == 0x22 || klay == 0x51)
	      {
		hd->sub_vend = MAKE_ID(TAG_SPECIAL, 0x0202);
		hd->sub_dev = MAKE_ID(TAG_SPECIAL, 0x0001);
	      }
	    else if(!(
		      klay == 0x21 || (klay >= 0x2f && klay <= 0x31) ||
		      klay == 0x50 || (klay >= 0x5e && klay <= 0x60)
		      ))
	      {
		hd->sub_vend = MAKE_ID(TAG_SPECIAL, 0x0202);
		hd->sub_dev = MAKE_ID(TAG_SPECIAL, 0x0002);
	      }
	  }
	}
    }
  else
    {
      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(hd->base_class == bc_keyboard) break;
      }
      if(!hd) {
        /* We must have a PS/2 Keyboard */
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_keyboard;
        hd->sub_class = sc_keyboard_kbd;
        hd->bus = bus_ps2;
        hd->vend = MAKE_ID(TAG_SPECIAL, 0x0201);
        hd->dev = MAKE_ID(TAG_SPECIAL, 1);
      }
    }
}

#endif	/* __sparc__ */


#if defined(__s390__) || defined(__s390x__)
void hd_scan_kbd(hd_data_t *hd_data)
{

}
#endif


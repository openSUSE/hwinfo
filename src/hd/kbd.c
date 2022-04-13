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
#include <linux/serial.h>

#ifndef TIOCGDEV
#define TIOCGDEV      _IOR('T', 0x32, unsigned int)
#endif

/**
 * @defgroup KDBint Keyboard devices
 * @ingroup libhdDEVint
 * @brief Keyboard device functions
 *
 * @{
 */

#ifdef __sparc__

#ifdef DIET
typedef unsigned int u_int;
#endif

#ifndef KIOCTYPE
/* Return keyboard type */
#	define KIOCTYPE    _IOR('k', 9, int)
#endif
#ifndef KIOCLAYOUT
/* Return Keyboard layout */
#	define KIOCLAYOUT  _IOR('k', 20, int)
#endif

#include <asm/openpromio.h>
#endif

#include "hd.h"
#include "hd_int.h"
#include "kbd.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Look for keyboards not covered by kernel input device driver, mainly
 * some sort of serial consoles.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifdef __sparc__
static void add_sun_console(hd_data_t *hd_data);
#else
static void add_serial_console(hd_data_t *hd_data);
#endif


void hd_scan_kbd(hd_data_t *hd_data)
{
  hd_t *hd;

  if(!hd_probe_feature(hd_data, pr_kbd)) return;

  hd_data->module = mod_kbd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(2, 0, "uml");

  if(hd_is_uml(hd_data)) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_keyboard;
    hd->sub_class.id = sc_keyboard_kbd;
    hd->bus.id = bus_none;
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0201);
    hd->device.id = MAKE_ID(TAG_SPECIAL, 2);
  }

  PROGRESS(3, 0, "serial console");

#ifdef __sparc__
  add_sun_console(hd_data);
#else
  add_serial_console(hd_data);
#endif
}


#ifndef __sparc__

void add_serial_console(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res = NULL;
  int fd, i;
  str_list_t *cmd, *cmd0, *sl;
  unsigned u, u1;
  struct serial_struct ser_info;
  unsigned tty_major = 0, tty_minor = 0;
  char c, *dev = NULL, *s;

  /* first, try console= option */
  cmd = cmd0 = get_cmdline(hd_data, "console");

  /* use last console entry */
  if(cmd) while(cmd->next) cmd = cmd->next;

  if(
    cmd &&
    (
      /* everything != "ttyN" */
      strncmp(cmd->str, "tty", 3) ||
      !(cmd->str[3] == 0 || (cmd->str[3] >= '0' && cmd->str[3] <= '9'))
    )
  ) {
    sl = hd_split(',', cmd->str);
    s = sl->str;
    if(!strncmp(s, "/dev/", sizeof "/dev/" - 1)) s += sizeof "/dev/" - 1;
    dev = new_str(s);
    if(sl->next && (i = sscanf(sl->next->str, "%u%c%u", &u, &c, &u1)) >= 1) {
      res = add_res_entry(&res, new_mem(sizeof *res));
      res->baud.type = res_baud;
      res->baud.speed = u;
      if(i >= 2) res->baud.parity = c;
      if(i >= 3) res->baud.bits = u1;
    }
    free_str_list(sl);
  }

  if(!dev && (fd = open(DEV_CONSOLE, O_RDWR | O_NONBLOCK | O_NOCTTY)) >= 0) {
    if(ioctl(fd, TIOCGDEV, &u) != -1) {
      tty_major = (u >> 8) & 0xfff;
      tty_minor = (u & 0xff) | ((u >> 12) & 0xfff00);
      // get char device name from major:minor numbers
      char *dev_link = NULL, *dev_name = NULL;
      str_printf(&dev_link, 0, "/dev/char/%u:%u", tty_major, tty_minor);
      dev_name = realpath(dev_link, NULL);
      if(
        dev_name &&
        strcmp(dev_name, dev_link) &&
        !strncmp(dev_name, "/dev/", sizeof "/dev/" - 1)
      ) {
        dev = new_str(dev_name + sizeof "/dev/" - 1);
      }
      ADD2LOG(DEV_CONSOLE ": major %u, minor %u, name %s\n", tty_major, tty_minor, dev);
      free_mem(dev_link);
      free_mem(dev_name);
    }

    if (dev)
	    ;
#ifdef __powerpc__
    else if(tty_major == 229 /* iseries hvc */) {
      if (tty_minor >= 128) {
        str_printf(&dev, 0, "hvsi%u", tty_minor-128);
      } else {
        str_printf(&dev, 0, "hvc%u", tty_minor);
      }
    } else if (tty_major == 204 /* SERIAL_PSC_MAJOR */ && tty_minor == 148 /* SERIAL_PSC_MINOR */) {
        str_printf(&dev, 0, "ttyPSC0"); /* EFIKA5K2 */
    }
#endif /* __powerpc__ */
    else if(!ioctl(fd, TIOCGSERIAL, &ser_info)) {
      ADD2LOG("serial console at line %d\n", ser_info.line);
      str_printf(&dev, 0, "ttyS%d", ser_info.line);
    }
    close(fd);
  }

  if(dev) {

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_keyboard;
    hd->sub_class.id = sc_keyboard_console;
    hd->bus.id = bus_serial;
    hd->device.name = new_str("serial console");

    if(*dev) str_printf(&hd->unix_dev_name, 0, "/dev/%s", dev);

    hd->res = res;

    free_mem(dev);
  }

  free_str_list(cmd0);
}


#else	/* defined(__sparc__) */


void add_sun_console(hd_data_t *hd_data)
{
  int fd, kid, kid2, klay, ser_cons, i;
  unsigned u, u1, u2;
  char c1, c2;
  struct serial_struct ser_info;
  unsigned char buf[OPROMMAXPARAM];
  struct openpromio *opio = (struct openpromio *) buf;
  hd_t *hd;
  hd_res_t *res;

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
		  hd->base_class.id = bc_keyboard;
		  hd->sub_class.id = sc_keyboard_console;
		  hd->bus.id = bus_serial;
		  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0203);
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0000);
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
	  hd->base_class.id = bc_keyboard;
	  hd->sub_class.id = sc_keyboard_kbd;
	  hd->bus.id = bus_serial;
	  if(kid == 4 && klay >= 0)
	    hd->prog_if.id = klay;

	  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0202);
	  kid2 = kid;
	  if(kid == 4 && klay > 0x20)
	    kid2 = 5;
	  hd->device.id = MAKE_ID(TAG_SPECIAL, kid2);
	  if(kid2 == 5) {
	    if(klay == 0x22 || klay == 0x51)
	      {
		hd->sub_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0202);
		hd->sub_device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
	      }
	    else if(!(
		      klay == 0x21 || (klay >= 0x2f && klay <= 0x31) ||
		      klay == 0x50 || (klay >= 0x5e && klay <= 0x60)
		      ))
	      {
		hd->sub_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0202);
		hd->sub_device.id = MAKE_ID(TAG_SPECIAL, 0x0002);
	      }
	  }
	}
    }
  else
    {
      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(hd->base_class.id == bc_keyboard) break;
      }
      if(!hd) {
        /* We must have a PS/2 Keyboard */
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_keyboard;
        hd->sub_class.id = sc_keyboard_kbd;
        hd->bus.id = bus_ps2;
        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0201);
        hd->device.id = MAKE_ID(TAG_SPECIAL, 1);
      }
    }
}

#endif	/* __sparc__ */

/** @} */


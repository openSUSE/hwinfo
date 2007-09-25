#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "hd.h"
#include "hd_int.h"
#include "braille.h"

/**
 * @defgroup BRAILLEint Braille devices
 * @ingroup  libhdDEVint
 * @brief Braille displays functions
 *
 * @{
 */

#if !defined(LIBHD_TINY) && !defined(__sparc__)

static unsigned do_alva(hd_data_t *hd_data, char *dev_name, int cnt);
static unsigned do_fhp(hd_data_t *hd_data, char *dev_name, unsigned baud, int cnt);
static unsigned do_ht(hd_data_t *hd_data, char *dev_name, int cnt);
static unsigned do_baum(hd_data_t *hd_data, char *dev_name, int cnt);

void hd_scan_braille(hd_data_t *hd_data)
{
  hd_t *hd, *hd_tmp;
  int cnt = 0;
  unsigned *dev, *vend;

  if(!hd_probe_feature(hd_data, pr_braille)) return;

  hd_data->module = mod_braille;

  /* some clean-up */
  remove_hd_entries(hd_data);

  dev = hd_shm_add(hd_data, NULL, sizeof *dev);
  vend = hd_shm_add(hd_data, NULL, sizeof *vend);

  if(!dev || !vend) return;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_comm &&
      hd->sub_class.id == sc_com_ser &&
      hd->unix_dev_name &&
      !hd->tag.ser_skip &&
      !has_something_attached(hd_data, hd)
    ) {
      cnt++;
      *dev = *vend = 0;

      hd_fork(hd_data, 10, 10);

      if(hd_data->flags.forked) {

        if(hd_probe_feature(hd_data, pr_braille_alva)) {
          PROGRESS(1, cnt, "alva");
          *vend = MAKE_ID(TAG_SPECIAL, 0x5001);
          *dev = do_alva(hd_data, hd->unix_dev_name, cnt);
        }

        if(!*dev && hd_probe_feature(hd_data, pr_braille_fhp)) {
          PROGRESS(1, cnt, "fhp_old");
          *vend = MAKE_ID(TAG_SPECIAL, 0x5002);
          *dev = do_fhp(hd_data, hd->unix_dev_name, B19200, cnt);
          if(!*dev) {
            PROGRESS(1, cnt, "fhp_el");
            *dev = do_fhp(hd_data, hd->unix_dev_name, B38400, cnt);
          }
        }

        if(!*dev && hd_probe_feature(hd_data, pr_braille_ht)) {
          PROGRESS(1, cnt, "ht");
          *vend = MAKE_ID(TAG_SPECIAL, 0x5003);
          *dev = do_ht(hd_data, hd->unix_dev_name, cnt);
        }

        if(!*dev && hd_probe_feature(hd_data, pr_braille_baum)) {
          PROGRESS(1, cnt, "baum");
          *vend = MAKE_ID(TAG_SPECIAL, 0x5004);
          *dev = do_baum(hd_data, hd->unix_dev_name, cnt);
        }

      }

      hd_fork_done(hd_data);

      if(*dev && *vend) {
        hd_tmp = add_hd_entry(hd_data, __LINE__, 0);
        hd_tmp->base_class.id = bc_braille;
        hd_tmp->bus.id = bus_serial;
        hd_tmp->unix_dev_name = new_str(hd->unix_dev_name);
        hd_tmp->attached_to = hd->idx;
        hd_tmp->vendor.id = *vend;
        hd_tmp->device.id = *dev;
      }
    }
  }

  hd_shm_clean(hd_data);
}


/*
 * autodetect for Alva Braille-displays
 * Author: marco Skambraks <marco@suse.de>
 * Suse GmbH Nuernberg
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
*/

/* Communication codes */
#define BRL_ID	"\033ID="


#define WAIT_DTR	700000
#define WAIT_FLUSH	200

unsigned do_alva(hd_data_t *hd_data, char *dev_name, int cnt)
{
  int fd, i, timeout = 100;
  struct termios oldtio, newtio;		/* old & new terminal settings */
  int model = -1;
  unsigned char buffer[sizeof BRL_ID];
  unsigned dev = 0;

  PROGRESS(2, cnt, "alva open");

  /* Open the Braille display device for random access */
  fd = open(dev_name, O_RDWR | O_NOCTTY);
  if(fd < 0) return 0;

  tcgetattr(fd, &oldtio);	/* save current settings */

  /* Set flow control and 8n1, enable reading */
  memset(&newtio, 0, sizeof newtio);
  newtio.c_cflag = CRTSCTS | CS8 | CLOCAL | CREAD;
  /* Ignore bytes with parity errors and make terminal raw and dumb */
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;		/* raw output */
  newtio.c_lflag = 0;		/* don't echo or generate signals */
  newtio.c_cc[VMIN] = 0;	/* set nonblocking read */
  newtio.c_cc[VTIME] = 0;

  PROGRESS(3, cnt, "alva init ok");

  PROGRESS(4, cnt, "alva read data");

  /* autodetecting ABT model */
  /* to force DTR off */
  cfsetispeed(&newtio, B0);
  cfsetospeed(&newtio, B0);
  tcsetattr(fd, TCSANOW, &newtio);	/* activate new settings */
  usleep(WAIT_DTR);

  tcflush(fd, TCIOFLUSH);		/* clean line */
  usleep(WAIT_FLUSH);

  /* DTR back on */
  cfsetispeed(&newtio, B9600);
  cfsetospeed(&newtio, B9600);
  tcsetattr(fd, TCSANOW, &newtio);	/* activate new settings */
  usleep(WAIT_DTR);			/* give time to send ID string */

  if((i = read(fd, buffer, sizeof buffer)) == sizeof buffer) {
    if(!strncmp(buffer, BRL_ID, sizeof BRL_ID - 1)) {
      /* Find out which model we are connected to... */
      switch(model = buffer[sizeof buffer - 1])
      {
        case    1:
        case    2:
        case    3:
        case    4:
        case 0x0b:
        case 0x0d:
        case 0x0e:
         dev = MAKE_ID(TAG_SPECIAL, model);
         break;
      }
    }
  }
  ADD2LOG("alva.%d@%s[%d]: ", timeout, dev_name, i);
  if(i > 0) hexdump(&hd_data->log, 1, i, buffer);
  ADD2LOG("\n");

  PROGRESS(5, cnt, "alva read done");

  /* reset serial lines */
  tcflush(fd, TCIOFLUSH);
  tcsetattr(fd, TCSAFLUSH, &oldtio);
  close(fd);

  return dev;
}


/*
 * autodetect for Papenmeier Braille-displays
 * Author: marco Skambraks <marco@suse.de>
 * Suse GmbH Nuernberg
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
 */

unsigned do_fhp(hd_data_t *hd_data, char *dev_name, unsigned baud, int cnt)
{
  int fd, i;
  char crash[] = { 2, 'S', 0, 0, 0, 0 };
  unsigned char buf[10];
  struct termios oldtio, newtio;	/* old & new terminal settings */
  unsigned dev;

  PROGRESS(2, cnt, "fhp open");

  /* Now open the Braille display device for random access */
  fd = open(dev_name, O_RDWR | O_NOCTTY);
  if(fd < 0) return 0;

  tcgetattr(fd, &oldtio);	/* save current settings */

  /* Set bps, flow control and 8n1, enable reading */
  memset(&newtio, 0, sizeof newtio);
  newtio.c_cflag = baud | CS8 | CLOCAL | CREAD;

  /* Ignore bytes with parity errors and make terminal raw and dumb */
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;				/* raw output */
  newtio.c_lflag = 0;				/* don't echo or generate signals */
  newtio.c_cc[VMIN] = 0;			/* set nonblocking read */
  newtio.c_cc[VTIME] = 0;
  tcflush(fd, TCIFLUSH);			/* clean line */
  tcsetattr(fd, TCSANOW, &newtio);		/* activate new settings */

  PROGRESS(3, cnt, "fhp init ok");

  crash[2] = 0x200 >> 8;
  crash[3] = 0x200 & 0xff;
  crash[5] = (7+10) & 0xff;

  write(fd, crash, sizeof crash);
  write(fd, "1111111111",10);
  write(fd, "\03", 1);

  crash[2] = 0x0 >> 8;
  crash[3] = 0x0 & 0xff;
  crash[5] = 5 & 0xff;

  write(fd, crash, sizeof crash);
  write(fd, "1111111111", 10);
  write(fd, "\03", 1);

  usleep(500000);		/* 100000 should be enough */

  PROGRESS(4, cnt, "fhp write ok");

  i = read(fd, &buf, 10);

  PROGRESS(5, cnt, "fhp read done");

  ADD2LOG("fhp@%s[%d]: ", dev_name, i);
  if(i > 0) hexdump(&hd_data->log, 1, i, buf);
  ADD2LOG("\n");

  dev = 0;
  if(i == 10 && buf[0] == 0x02 && buf[1] == 0x49) {
    switch(buf[2]) {
      case  1:
      case  2:
      case  3:
      case 64:
      case 65:
      case 66:
      case 67:
      case 68:
        dev = buf[2];
        dev = MAKE_ID(TAG_SPECIAL, dev);
        break;
    }
  }
  if(!dev) ADD2LOG("no fhp display: 0x%02x\n", i >= 2 ? buf[2] : 0);

  /* reset serial lines */
  tcflush(fd, TCIOFLUSH);
  tcsetattr(fd, TCSAFLUSH, &oldtio);
  close(fd);

  return dev;
}


/*
 * autodetect for Handy Tech  Braille-displays
 * Author: marco Skambraks <marco@suse.de>
 * Suse GmbH Nuernberg
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
*/

unsigned do_ht(hd_data_t *hd_data, char *dev_name, int cnt)
{
  int fd, i;
  unsigned char code = 0xff, buf[2] = { 0, 0 };
  struct termios oldtio, newtio;
  unsigned dev = 0;

  PROGRESS(2, cnt, "ht open");

  fd = open(dev_name, O_RDWR | O_NOCTTY);
  if(fd < 0) return 0;

  tcgetattr(fd, &oldtio);

  newtio = oldtio;
  newtio.c_cflag = CLOCAL | PARODD | PARENB | CREAD | CS8;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;
  newtio.c_lflag = 0;
  newtio.c_cc[VMIN] = 0;
  newtio.c_cc[VTIME] = 0;

  i = 0;
  /*
   * Force down DTR, flush any pending data and then the port to what we
   * want it to be
   */
  if(
    !(
      cfsetispeed(&newtio, B0) ||
      cfsetospeed(&newtio, B0) ||
      tcsetattr(fd, TCSANOW, &newtio) ||
      tcflush(fd, TCIOFLUSH) ||
      cfsetispeed(&newtio, B19200) ||
      cfsetospeed(&newtio, B19200) ||
      tcsetattr(fd, TCSANOW, &newtio)
    )
  ) {
    /* Pause to let them take effect */
    usleep(12000);

    PROGRESS(3, cnt, "ht init ok");

    write(fd, &code, 1);	/* reset brl */
    usleep(12000);		/* wait for reset */

    PROGRESS(4, cnt, "ht write ok");

    read(fd, buf, 1);
    i = 1;

    PROGRESS(5, cnt, "ht read done");

    if(buf[0] == 0xfe) {	/* resetok now read id */
      usleep(12000);
      read(fd, buf + 1, 1);
      i = 2;

      PROGRESS(6, cnt, "ht read done");

      switch(buf[1]) {
  	case 0x05:
  	case 0x09:
  	case 0x44:
  	case 0x74:
  	case 0x80:
  	case 0x84:
  	case 0x88:
  	case 0x89:
          dev = buf[1];
          dev = MAKE_ID(TAG_SPECIAL, dev);
          break;
      }
    }
  }

  ADD2LOG("ht@%s[%d]: ", dev_name, i);
  if(i > 0) hexdump(&hd_data->log, 1, i, buf);
  ADD2LOG("\n");

  if(!dev) ADD2LOG("no ht display: 0x%02x\n", buf[1]);

  /* reset serial lines */
  tcflush(fd, TCIOFLUSH);
  tcsetattr(fd, TCSAFLUSH, &oldtio);
  close(fd);

  return dev;
}


/*
 * autodetect for Baum Braille-displays
 * Author: marco Skambraks <marco@suse.de>
 * Suse GmbH Nuernberg
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
*/

#define BAUDRATE	B19200		/* But both run at 19k2 */
#define MAXREAD		18

unsigned do_baum(hd_data_t *hd_data, char *dev_name, int cnt)
{
  static char device_id[] = { 0x1b, 0x84 };
  int fd;
  struct termios oldtio, curtio;
  unsigned char buf[MAXREAD + 1];
  int i;

  PROGRESS(2, cnt, "baum open");

  fd = open(dev_name, O_RDWR | O_NOCTTY);
  if(fd < 0) return 0;

  tcgetattr(fd, &curtio);

  oldtio = curtio;
  cfmakeraw(&curtio);

  /* no SIGTTOU to backgrounded processes */
  curtio.c_lflag &= ~TOSTOP;
  curtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  /* no input parity check, no XON/XOFF */
  curtio.c_iflag &= ~(INPCK | ~IXOFF);

  curtio.c_cc[VTIME] = 1;	/* 0.1s timeout between chars on input */
  curtio.c_cc[VMIN] = 0;	/* no minimum input */

  tcsetattr(fd, TCSAFLUSH, &curtio);

  /* write ID-request */
  write(fd, device_id, sizeof device_id);

  /* wait for response */
  usleep(100000);

  PROGRESS(3, cnt, "baum write ok");

  i = read(fd, buf, sizeof buf - 1);
  buf[sizeof buf - 1] = 0;

  PROGRESS(4, cnt, "baum read done");

  ADD2LOG("baum@%s[%d]: ", dev_name, i);
  if(i > 0) hexdump(&hd_data->log, 1, i, buf);
  ADD2LOG("\n");

  /* reset serial lines */
  tcflush(fd, TCIOFLUSH);
  tcsetattr(fd, TCSAFLUSH, &oldtio);
  close(fd);

  if(!strcmp(buf + 2, "Baum Vario40")) return 1;
  if(!strcmp(buf + 2, "Baum Vario80")) return 2;

  return 0;
}


#endif	/* !defined(LIBHD_TINY) && !defined(__sparc__) */

/** @} */


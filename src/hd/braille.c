#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "hd.h"
#include "hd_int.h"
#include "braille.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * braille displays
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static unsigned do_alva(hd_data_t *hd_data, char *dev_name, int cnt);
static unsigned do_fhp(hd_data_t *hd_data, char *dev_name, int cnt);
static unsigned do_ht(hd_data_t *hd_data, char *dev_name, int cnt);

void hd_scan_braille(hd_data_t *hd_data)
{
  hd_t *hd, *hd_tmp;
  int cnt = 0;
  unsigned dev, vend;

  if(!hd_probe_feature(hd_data, pr_braille)) return;

  hd_data->module = mod_braille;

  /* some clean-up */
  remove_hd_entries(hd_data);

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_comm &&
      hd->sub_class == sc_com_ser &&
      hd->unix_dev_name &&
      !has_something_attached(hd_data, hd)
    ) {
      cnt++;
      dev = vend = 0;

      if(hd_probe_feature(hd_data, pr_braille_alva)) {
        PROGRESS(1, cnt, "alva");
        vend = MAKE_ID(TAG_SPECIAL, 0x5001);
        dev = do_alva(hd_data, hd->unix_dev_name, cnt);
      }

      if(!dev && hd_probe_feature(hd_data, pr_braille_fhp)) {
        PROGRESS(1, cnt, "fhp");
        vend = MAKE_ID(TAG_SPECIAL, 0x5002);
        dev = do_fhp(hd_data, hd->unix_dev_name, cnt);
      }

      if(!dev && hd_probe_feature(hd_data, pr_braille_ht)) {
        PROGRESS(1, cnt, "ht");
        vend = MAKE_ID(TAG_SPECIAL, 0x5003);
        dev = do_ht(hd_data, hd->unix_dev_name, cnt);
      }

      if(dev) {
        hd_tmp = add_hd_entry(hd_data, __LINE__, 0);
        hd_tmp->base_class = bc_braille;
        hd_tmp->bus = bus_serial;
        hd_tmp->unix_dev_name = new_str(hd->unix_dev_name);
        hd_tmp->attached_to = hd->idx;
        hd_tmp->vend = vend;
        hd_tmp->dev = dev;
      }
    }
  }
}


unsigned do_alva(hd_data_t *hd_data, char *dev_name, int cnt)
{
  return 0;
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

unsigned do_fhp(hd_data_t *hd_data, char *dev_name, int cnt)
{
  int fd, i;
  char crash[] = { 2, 'S', 0, 0, 0, 0 };
  unsigned char buf[10];
  struct termios oldtio;	/* old terminal settings */
  struct termios newtio;	/* new terminal settings */
  unsigned dev;

  PROGRESS(2, cnt, "fhp open");

  /* Now open the Braille display device for random access */
  fd = open(dev_name, O_RDWR | O_NOCTTY);
  if(fd < 0) return 0;

  tcgetattr(fd, &oldtio);	/* save current settings */

  /* Set bps, flow control and 8n1, enable reading */
  newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;

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

  sleep(1);

  PROGRESS(4, cnt, "fhp write ok");

  i = read(fd, &buf, 10);

  PROGRESS(5, cnt, "fhp read done");

  ADD2LOG("fhp@%s[%d]: ", dev_name, i);
  if(i > 0) hexdump(&hd_data->log, 1, i, buf);
  ADD2LOG("\n");

  dev = 0;
  if(i >= 2) {
    switch(buf[2]) {
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
  if(!dev) ADD2LOG("no fhp display: 0x%02x\n", buf[2]);

    /* reset serial lines */
  tcflush(fd, TCIOFLUSH);
  tcsetattr(fd, TCSAFLUSH, &oldtio);
  close(fd);

  return dev;
}


unsigned do_ht(hd_data_t *hd_data, char *dev_name, int cnt)
{
  return 0;
}


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
#include "hddb.h"
#include "modem.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * modem info
 *
 *
 * Note: what about modem speed?
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static void get_serial_modem(hd_data_t* hd_data);
static void write_modem(hd_data_t *hd_data, char *msg);
static void read_modem(hd_data_t *hd_data, fd_set *pset, int fd_max);
static ser_modem_t *add_ser_modem_entry(ser_modem_t **sm, ser_modem_t *new_sm);
static int init_modem(ser_modem_t *mi);
static int is_pnpinfo(ser_modem_t *mi, int ofs);
static unsigned chk4id(ser_modem_t *mi);
static void dump_ser_modem_data(hd_data_t *hd_data);

void hd_scan_modem(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_modem)) return;

  hd_data->module = mod_modem;

  /* some clean-up */
  remove_hd_entries(hd_data);
//  hd_data->ser_mouse = NULL;

  PROGRESS(1, 0, "serial");

  get_serial_modem(hd_data);
  if((hd_data->debug & HD_DEB_MODEM)) dump_ser_modem_data(hd_data);
}

void get_serial_modem(hd_data_t *hd_data)
{
  hd_t *hd;
  int fd, fd_max = 0;
  unsigned modem_info;
  fd_set set;
  char buf[4];
  ser_modem_t *sm;

  FD_ZERO(&set);

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_comm && hd->sub_class == sc_com_ser && hd->unix_dev_name) {
      if((fd = open(hd->unix_dev_name, O_RDWR)) >= 0) {
        sm = add_ser_modem_entry(&hd_data->ser_modem, new_mem(sizeof *sm));
        sm->dev_name = new_str(hd->unix_dev_name);
        sm->fd = fd;
        sm->hd_idx = hd->idx;
        if(fd > fd_max) fd_max = fd;
        FD_SET(fd, &set);
        init_modem(sm);
        sm->is_modem = 1;	/* remove non-modems later */
      }
    }
  }

  if(!hd_data->ser_modem) return;

  usleep(300000);		/* PnP protocol; 200ms seems to be too fast  */
  
  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    modem_info = TIOCM_DTR | TIOCM_RTS;
    ioctl(sm->fd, TIOCMBIS, &modem_info);
  }

  write_modem(hd_data, "AT\r");
  read_modem(hd_data, &set, fd_max);

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(!strstr(sm->buf, "OK")) {
      sm->is_modem = 0;
      FD_CLR(sm->fd, &set);
    }
    sm->buf_len = 0;		/* clear buffer */
  }

  write_modem(hd_data, "ATI9\r");
  read_modem(hd_data, &set, fd_max);

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    chk4id(sm);

    /* reset serial lines */
    tcsetattr(sm->fd, TCSAFLUSH, &sm->tio);
    close(sm->fd);

    if(sm->is_modem) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_modem;
      hd->bus = bus_serial;
      hd->unix_dev_name = new_str(sm->dev_name);
      hd->attached_to = sm->hd_idx;
      if(*sm->pnp_id) {
        strncpy(buf, sm->pnp_id, 3);
        buf[3] = 0;
        hd->vend = name2eisa_id(buf);
        hd->dev = MAKE_ID(TAG_EISA, strtol(sm->pnp_id + 3, NULL, 16));
      }
      hd->serial = new_str(sm->serial);
      if(sm->user_name)
        add_device_name(hd_data, hd->vend, hd->dev, sm->user_name);
    }

  }
}


void write_modem(hd_data_t *hd_data, char *msg)
{
  ser_modem_t *sm;
  int i, len = strlen(msg);

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->is_modem) {
      i = write(sm->fd, msg, len);
      if(i != len) {
        ADD2LOG("%s write oops: %d/%d (\"%s\")\n", sm->dev_name, i, len, msg);
      }
    }
  }
}

void read_modem(hd_data_t *hd_data, fd_set *pset, int fd_max)
{
  int i, sel;
  fd_set set, set0;
  struct timeval to;
  ser_modem_t *sm;

  to.tv_sec = 0; to.tv_usec = 300000;

  set0 = set = *pset;
  for(;;) {
    to.tv_sec = 0; to.tv_usec = 300000;
    set = set0;
    if((sel = select(fd_max + 1, &set, NULL, NULL, &to)) > 0) {
//      fprintf(stderr, "sel: %d\n", sel);
      for(sm = hd_data->ser_modem; sm; sm = sm->next) {
        if(FD_ISSET(sm->fd, &set)) {
          if((i = read(sm->fd, sm->buf + sm->buf_len, sizeof sm->buf - sm->buf_len)) > 0)
            sm->buf_len += i;
//          fprintf(stderr, "%s: got %d\n", sm->dev_name, i);
          if(i <= 0) FD_CLR(sm->fd, &set0);
        }
      }
    }
    else {
      break;
    }
  }

  /* make the strings \000 terminated */
  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->buf_len == sizeof sm->buf) sm->buf_len--;
    sm->buf[sm->buf_len] = 0;
  }
}

int init_modem(ser_modem_t *sm)
{
  struct termios tio;

  if(tcgetattr(sm->fd, &tio)) return errno;
  
  sm->tio = tio;

  tio.c_iflag = IGNBRK | IGNPAR;
  tio.c_oflag = 0;
  tio.c_lflag = 0;
  tio.c_line = 0;
  tio.c_cc[VTIME] = 0;
  tio.c_cc[VMIN] = 1;

  tio.c_cflag = CREAD | CLOCAL | HUPCL | B1200 | CS8;

  if(tcsetattr(sm->fd, TCSAFLUSH, &tio)) return errno;

  return 0;
}


/*
 * Check for a PnP info field starting at ofs;
 * returns either the length of the field or 0 if none was found.
 *
 * the minfo_t struct is updated with the PnP data
 */
int is_pnpinfo(ser_modem_t *mi, int ofs)
{
  int i, j, k, l;
  unsigned char c, *s = mi->buf + ofs;
  int len = mi->buf_len - ofs;
  unsigned serial, class_name, dev_id, user_name;

  if(len <= 0) return 0;

  switch(*s) {
    case 0x08:
      mi->bits = 6; break;
    case 0x28:
      mi->bits = 7; break;
    default:
      return 0;
  }

  if(len < 11) return 0;

  /* six bit values */
  if((s[1] & ~0x3f) || (s[2] & ~0x3f)) return 0;
  mi->pnp_rev = (s[1] << 6) + s[2];

  /* the eisa id */
  for(i = 0; i < 7; i++) {
    mi->pnp_id[i] = s[i + 3];
    if(mi->bits == 6) mi->pnp_id[i] += 0x20;
  }
  mi->pnp_id[7] = 0;

  /* now check the id */
  for(i = 0; i < 3; i++) {
    if(
      (mi->pnp_id[i] < 'A' || mi->pnp_id[i] > 'Z') &&
      mi->pnp_id[i] != '_'
    ) return 0;
  }

  for(i = 3; i < 7; i++) {
    if(
      (mi->pnp_id[i] < '0' || mi->pnp_id[i] > '9') &&
      (mi->pnp_id[i] < 'A' || mi->pnp_id[i] > 'F')
    ) return 0;
  }

  if((mi->bits == 6 && s[10] == 0x09) || (mi->bits == 7 && s[10] == 0x29)) {
    return 11;
  }

  if((mi->bits != 6 || s[10] != 0x3c) && (mi->bits != 7 || s[10] != 0x5c)) {
    return 0;
  }

  /* parse extended info */
  serial = class_name = dev_id = user_name = 0;
  for(j = 0, i = 10; i < len; i++) {
    if((mi->bits == 6 && s[i] == 0x09) || (mi->bits == 7 && s[i] == 0x29)) {
      
      if(serial) for(k = serial; k < len; k++) {
        c = s[k];
        if(mi->bits == 6) c += 0x20;
        if(c == '\\') break;
        str_printf(&mi->serial, -1, "%c", c);
      }

      if(class_name) for(k = class_name; k < len; k++) {
        c = s[k];
        if(mi->bits == 6) c += 0x20;
        if(c == '\\') break;
        str_printf(&mi->class_name, -1, "%c", c);
      }

      if(dev_id) for(k = dev_id; k < len; k++) {
        c = s[k];
        if(mi->bits == 6) c += 0x20;
        if(c == '\\') break;
        str_printf(&mi->dev_id, -1, "%c", c);
      }

      if(user_name) {
        for(k = user_name; k < len; k++) {
          c = s[k];
          if(mi->bits == 6) c += 0x20;
          if(c == '\\' || c == ')') break;
          str_printf(&mi->user_name, -1, "%c", c);
        }
        if(mi->user_name && (l = strlen(mi->user_name)) >= 2) {
          /* skip 2 char checksum */
          mi->user_name[l - 2] = 0;
        }
      }

      return i + 1;
    }

    if(((mi->bits == 6 && s[i] == 0x3c) || (mi->bits == 7 && s[i] == 0x5c)) && i < len - 1) {
      switch(j) {
        case 0:
          serial = i + 1; j++; break;
        case 1:
          class_name = i + 1; j++; break;
        case 2:
          dev_id = i + 1; j++; break;
        case 3:
          user_name = i + 1; j++; break;
        default:
          fprintf(stderr, "PnP-ID oops\n");
      }
    }
  }

  /* no end token... */

  return 0;
}


unsigned chk4id(ser_modem_t *mi)
{
  int i;

  if(!mi->buf_len) return 0;

  for(i = 0; i < mi->buf_len; i++) {
    if((mi->pnp = is_pnpinfo(mi, i))) break;
  }
  if(i == mi->buf_len) return 0;

  mi->garbage = i;

  return 1;
}

ser_modem_t *add_ser_modem_entry(ser_modem_t **sm, ser_modem_t *new_sm)
{
  while(*sm) sm = &(*sm)->next;
  return *sm = new_sm;
}

void dump_ser_modem_data(hd_data_t *hd_data)
{
  int j;
  ser_modem_t *sm;

  if(!(sm = hd_data->ser_modem)) return;

  ADD2LOG("----- serial modems -----\n");

  for(; sm; sm = sm->next) {
    ADD2LOG("%s\n", sm->dev_name);
    if(sm->serial) ADD2LOG("serial: \"%s\"\n", sm->serial);
    if(sm->class_name) ADD2LOG("class_name: \"%s\"\n", sm->class_name);
    if(sm->dev_id) ADD2LOG("dev_id: \"%s\"\n", sm->dev_id);
    if(sm->user_name) ADD2LOG("user_name: \"%s\"\n", sm->user_name);

    if(sm->garbage) {
      ADD2LOG("  pre_garbage[%u]: ", sm->garbage);
      hexdump(&hd_data->log, 1, sm->garbage, sm->buf);
      ADD2LOG("\n");  
    }

    if(sm->pnp) {
      ADD2LOG("  pnp[%u]: ", sm->pnp);
      hexdump(&hd_data->log, 1, sm->pnp, sm->buf + sm->garbage);
      ADD2LOG("\n");
    }

    if((j = sm->buf_len - (sm->garbage + sm->pnp))) {
      ADD2LOG("  post_garbage[%u]: ", j);
      hexdump(&hd_data->log, 1, j, sm->buf + sm->garbage + sm->pnp);
      ADD2LOG("\n");
    }

    if(sm->is_modem)
      ADD2LOG("  is modem\n");
    else
      ADD2LOG("  not a modem\n");

    if(sm->pnp) {
      ADD2LOG("  bits: %u\n", sm->bits);
      ADD2LOG("  PnP Rev: %u.%02u\n", sm->pnp_rev / 100, sm->pnp_rev % 100);
      ADD2LOG("  PnP ID: \"%s\"\n", sm->pnp_id);
    }

    if(sm->next) ADD2LOG("\n");
  }

  ADD2LOG("----- serial modems end -----\n");
}


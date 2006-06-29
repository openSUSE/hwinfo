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
#include "mouse.h"

/**
 * @defgroup MOUSEdev Mouse devices
 * @ingroup libhdDEVint
 * @brief Mouse detection
 *
 * @todo reset serial lines to old values (cf. modem.c)
 * @{
 */

#ifndef LIBHD_TINY

#if 0
static unsigned read_data(hd_data_t *hd_data, int fd, unsigned char *buf, unsigned buf_size);
static void get_ps2_mouse(hd_data_t *hd_data);
static void test_ps2_open(void *arg);
#endif

static void get_serial_mouse(hd_data_t* hd_data);
static void add_serial_mouse(hd_data_t* hd_data);
static int _setspeed(int fd, int old, int new, int needtowrite, unsigned short flags);
static void setspeed(int fd, int new, int needtowrite, unsigned short flags);
static unsigned chk4id(ser_device_t *mi);
static ser_device_t *add_ser_mouse_entry(ser_device_t **sm, ser_device_t *new_sm);
static void dump_ser_mouse_data(hd_data_t *hd_data);
#if 0
static void get_sunmouse(hd_data_t *hd_data);
#endif

void hd_scan_mouse(hd_data_t *hd_data)
{
  ser_device_t *sm, *sm_next;

  if(!hd_probe_feature(hd_data, pr_mouse)) return;

  hd_data->module = mod_mouse;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->ser_mouse = NULL;

#if 0
  PROGRESS(1, 0, "ps/2");

  get_ps2_mouse(hd_data);
#endif

  PROGRESS(2, 0, "serial");

  hd_fork(hd_data, 20, 20);

  if(hd_data->flags.forked) {
    get_serial_mouse(hd_data);
    hd_move_to_shm(hd_data);
    if((hd_data->debug & HD_DEB_MOUSE)) dump_ser_mouse_data(hd_data);
  }
  else {
    /* take data from shm */
    hd_data->ser_mouse = ((hd_data_t *) (hd_data->shm.data))->ser_mouse;
    if((hd_data->debug & HD_DEB_MOUSE)) dump_ser_mouse_data(hd_data);
  }

  hd_fork_done(hd_data);

  add_serial_mouse(hd_data);

  hd_shm_clean(hd_data);

  for(sm = hd_data->ser_mouse; sm; sm = sm_next) {
    sm_next = sm->next;

    free_mem(sm->dev_name);
    free_mem(sm);
  }
  hd_data->ser_mouse = NULL;

#if 0
  PROGRESS(3, 0, "sunmouse");

  get_sunmouse(hd_data);
#endif
}


#if 0
unsigned read_data(hd_data_t *hd_data, int fd, unsigned char *buf, unsigned buf_size)
{
  int k, len = 0;
  unsigned char *bp;

  while(
    (unsigned) len < buf_size &&
    (k = read(fd, buf + len, buf_size - len)) >= 0
  ) len += k;

  bp = buf;
  if(len && (*bp == 0xfe || *bp == 0xfa)) { bp++; len--; }

  for(k = 0; k < len; k++) buf[k] = bp[k];

  if((hd_data->debug & HD_DEB_MOUSE)) {
    ADD2LOG("ps/2[%d]: ", len);
    hexdump(&hd_data->log, 1, len, buf);
    ADD2LOG("\n");
  }

  return len;
}


/*
 * How it works:
 *
 * 1. There must exist a PS/2 controller entry (-> there is a PS/2 port).
 * 2. If there are PS/2 mouse irq events, assume a PS/2 mouse is attached.
 * 3. Otherwise:
 *      - open /dev/psaux
 *      - write the "get mouse info" command (0xe9)
 *      - read back the response, which should be either 0xfe "resend data"
 *        or, e.g. (0xfa) 0x20 0x02 0x3c (0xfa = "ACK" (should be swallowed
 *        by the psaux driver, but isn't), the rest are settings)
 *      - ignore the first byte if it is 0xfa or 0xfe
 *      - if there are at least 2 bytes left, assume a mouse is attached.
 *
 * Note1: we could use the command 0xfe "get mouse ID" instead. But that turned
 *        out to be less reliable, as this command returns only one byte, which
 *        is even 0.
 * Note2: step 2 is mainly relevant if the mouse is already in use. In that
 *        case we would have problems reading back the respose of our command.
 *        (Typically the mouse driver will get it (and choke on it).)
 */

static void get_ps2_mouse(hd_data_t *hd_data)
{
  hd_t *hd, *hd1;
  hd_res_t *res;
  int fd;
  fd_set set;
  struct timeval tv;
  unsigned char cmd_mouse_info = 0xe9;	/* read mouse info (3 bytes) */
  unsigned char cmd_mouse_id = 0xf2;	/* read mouse id (1 byte) */
  unsigned char buf[100];
  unsigned mouse_id = -1;
  static unsigned char intelli_init[] = { 0xf3, 200, 0xf3, 100, 0xf3, 80 };
  int buf_len = 0;
#ifdef __PPC__
  int always_ps2_mouse = 0;
#endif

  for(hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
    /* look for a PS/2 controller entry... */
    if(hd1->base_class.id == bc_ps2) {
      /* ...and see if there were irq events... */
      for(res = hd1->res; res; res = res->next) {
        if(res->irq.type == res_irq && res->irq.triggered) break;
      }

#ifdef __PPC__
      /*
       * On PReP & CHRP, assume a PS/2 mouse to be attached.
       * There seems to be no way to actually *detect* it.
       */
      if(!res) {
        hd_t *hd;
        sys_info_t *st;

        if((hd = hd_list(hd_data, hw_sys, 0, NULL))) {
          if(
            hd->detail &&
            hd->detail->type == hd_detail_sys &&
            (st = hd->detail->sys.data) &&
            (
              !strcmp(st->system_type, "PReP") ||
              strstr(st->system_type, "CHRP") == st->system_type	/* CHRP && CHRP64 */
            )
          ) {
            always_ps2_mouse = 1;
          }
        }
      }
#endif

      PROGRESS(1, 1, "ps/2");

      /* open the mouse device... */
      if(hd_timeout(test_ps2_open, NULL, 2) > 0) {
        ADD2LOG("ps/2: open(%s) timed out\n", DEV_PSAUX);
        fd = -2;
      }
      else {
        fd = open(DEV_PSAUX, O_RDWR | O_NONBLOCK);
      }

      PROGRESS(1, 2, "ps/2");

      if(fd >= 0) {
        /* ...write the id command... */

        PROGRESS(1, 3, "ps/2");

        write(fd, intelli_init, sizeof intelli_init);
        usleep(25000);
        read_data(hd_data, fd, buf, sizeof buf);

        if(write(fd, &cmd_mouse_id, 1) == 1) {

          PROGRESS(1, 4, "ps/2");
          usleep(50000);        /* ...give it a chance to react... */

          /* ...read the response... */
          buf_len = read_data(hd_data, fd, buf, sizeof buf);

          if(buf_len >= 1) mouse_id = buf[buf_len - 1];

          // if we didn't get any response, try this
          if(buf_len == 0 || (hd_data->debug & HD_DEB_MOUSE)) {
            PROGRESS(1, 5, "ps/2");
            if(write(fd, &cmd_mouse_info, 1) == 1) {
              usleep(50000);
              buf_len = read_data(hd_data, fd, buf, sizeof buf);
              /*
               * Assume a mouse to be attached if at least 2 bytes are
               * returned.
               */
              if(mouse_id == -1u && buf_len >= 2) mouse_id = 0;
            }
          }

          PROGRESS(1, 6, "ps/2");
        }
        close(fd);

        PROGRESS(1, 7, "ps/2");

        /*
         * The following code is apparently necessary on some board/mouse
         * combinations. Otherwise the PS/2 mouse won't work.
         */
        if((fd = open(DEV_PSAUX, O_RDONLY | O_NONBLOCK)) >= 0) {
          PROGRESS(1, 8, "ps/2");

          FD_ZERO(&set);
          FD_SET(fd, &set);
          tv.tv_sec = 0; tv.tv_usec = 1;
          if(select(fd + 1, &set, NULL, NULL, &tv) == 1) {
            PROGRESS(1, 9, "ps/2");

            read(fd, buf, sizeof buf);

            PROGRESS(1, 10, "ps/2");
          }
          PROGRESS(1, 11, "ps/2");

          close(fd);

          PROGRESS(1, 12, "ps/2");
        }
      }
      else {
        ADD2LOG("open(" DEV_PSAUX "): %s\n", fd == -1 ? strerror(errno) : "timeout");
      }

      if(mouse_id == -1u) {

        /*
         * Assume a PS/2 mouse is attached if the ps/2 controller has
         * genetrated some events.
         */

        if(
          res
#ifdef __PPC__
          || always_ps2_mouse
#endif
        ) {
          PROGRESS(1, 13, "ps/2");
          mouse_id = 0;
        }
      }

      if(mouse_id != -1u) {
        PROGRESS(1, 14, "ps/2");

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_mouse;
        hd->sub_class.id = sc_mou_ps2;
        hd->bus.id = bus_ps2;
        hd->unix_dev_name = new_str(DEV_MICE);
        hd->attached_to = hd1->idx;

        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0200);
        switch(mouse_id) {
          case 3:		/* 3 buttons + wheel */
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0004);
            break;

          case 4:		/* 5 buttons + wheel */
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0005);
            break;

          default:	/* 0 */
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0002);
        }
      }

      /* there can only be one... */
      break;
    }
  }
}

void test_ps2_open(void *arg)
{
  open(DEV_PSAUX, O_RDWR | O_NONBLOCK);
}
#endif

#if 0
static void get_sunmouse(hd_data_t *hd_data)
{
  hd_t *hd;
  int fd;
  int found;

  found = 0;

  /* Only search for Sun mouse if we have a Sun keyboard */
  for(hd = hd_data->hd; hd; hd = hd->next)
    {
      if(hd->base_class.id == bc_keyboard &&
	 hd->sub_class.id == sc_keyboard_kbd &&
	 ID_TAG(hd->vendor.id) == TAG_SPECIAL && ID_VALUE(hd->vendor.id) == 0x0202)
	found = 1;
    }

  if (found)
    {
      if ((fd = open(DEV_SUNMOUSE, O_RDONLY)) != -1)
	{
	  /* FIXME: Should probably talk to the mouse to see
	     if the connector is not empty. */
	  close (fd);

	  PROGRESS(1, 1, "Sun Mouse");

	  hd = add_hd_entry (hd_data, __LINE__, 0);
	  hd->base_class.id = bc_mouse;
	  hd->sub_class.id = sc_mou_sun;
	  hd->bus.id = bus_serial;
	  hd->unix_dev_name = new_str(DEV_SUNMOUSE);

	  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0202);
	  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0000);
	}
    }
}
#endif

/*
 * Gather serial mouse data and put it into hd_data->ser_mouse.
 */
void get_serial_mouse(hd_data_t *hd_data)
{
  hd_t *hd;
  int j, fd, fd_max = 0, sel, max_len;
  unsigned modem_info;
  fd_set set, set0;
  struct timeval to;
  ser_device_t *sm;
  struct termios tio;

  FD_ZERO(&set);

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_comm &&
      hd->sub_class.id == sc_com_ser &&
      hd->unix_dev_name &&
      !hd->tag.ser_skip &&
      !has_something_attached(hd_data, hd)
    ) {
      if((fd = open(hd->unix_dev_name, O_RDWR | O_NONBLOCK)) >= 0) {
        if(tcgetattr(fd, &tio)) continue;
        sm = add_ser_mouse_entry(&hd_data->ser_mouse, new_mem(sizeof *sm));
        sm->dev_name = new_str(hd->unix_dev_name);
        sm->fd = fd;
        sm->tio = tio;
        sm->hd_idx = hd->idx;
        if(fd > fd_max) fd_max = fd;
        FD_SET(fd, &set);

        /*
         * PnP COM spec black magic...
         */
        setspeed(fd, 1200, 1, CS7);
        modem_info = TIOCM_DTR | TIOCM_RTS;
        ioctl(fd, TIOCMBIC, &modem_info);
      }
    }
  }

  if(!hd_data->ser_mouse) return;

  /*
   * 200 ms seems to be too fast for some mice...
   */
  usleep(300000);		/* PnP protocol */

  for(sm = hd_data->ser_mouse; sm; sm = sm->next) {
    modem_info = TIOCM_DTR | TIOCM_RTS;
    ioctl(sm->fd, TIOCMBIS, &modem_info);
  }

  /* smaller buffer size, otherwise we might wait really long... */
  max_len = sizeof sm->buf < 128 ? sizeof sm->buf : 128;

  to.tv_sec = 0; to.tv_usec = 300000;

  set0 = set;
  for(;;) {
   to.tv_sec = 0; to.tv_usec = 300000;
    set = set0;
    if((sel = select(fd_max + 1, &set, NULL, NULL, &to)) > 0) {
      for(sm = hd_data->ser_mouse; sm; sm = sm->next) {
        if(FD_ISSET(sm->fd, &set)) {
          if((j = read(sm->fd, sm->buf + sm->buf_len, max_len - sm->buf_len)) > 0)
            sm->buf_len += j;
          if(j <= 0) FD_CLR(sm->fd, &set0);	// #####
        }
      }
    }
    else {
      break;
    }
  }

  for(sm = hd_data->ser_mouse; sm; sm = sm->next) {
    chk4id(sm);
    /* reset serial lines */
    tcflush(sm->fd, TCIOFLUSH);
    tcsetattr(sm->fd, TCSAFLUSH, &sm->tio);
    close(sm->fd);
  }
}


/*
 * Go through serial mouse data and add hd entries.
 */
void add_serial_mouse(hd_data_t *hd_data)
{
  hd_t *hd;
  char buf[4];
  ser_device_t *sm;

  for(sm = hd_data->ser_mouse; sm; sm = sm->next) {
    if(sm->is_mouse) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_mouse;
      hd->sub_class.id = sc_mou_ser;
      hd->bus.id = bus_serial;
      hd->unix_dev_name = new_str(sm->dev_name);
      hd->attached_to = sm->hd_idx;
      if(*sm->pnp_id) {
        strncpy(buf, sm->pnp_id, 3);
        buf[3] = 0;
        hd->vendor.id = name2eisa_id(buf);
        if(!hd->vendor.id) {	/* in case it's a really strange one... */
          hd->vendor.name = new_str(buf);
        }
        hd->device.id = MAKE_ID(TAG_EISA, strtol(sm->pnp_id + 3, NULL, 16));

        hd->serial = new_str(sm->serial);
        if(sm->user_name) hd->device.name = new_str(sm->user_name);
        if(sm->vend) {
          free_mem(hd->vendor.name);
          hd->vendor.name = new_str(sm->vend);
        }

        if(sm->dev_id && strlen(sm->dev_id) >= 7) {
          char buf[5], *s;
          unsigned u1, u2;

          u1 = name2eisa_id(sm->dev_id);
          if(u1) {
            strncpy(buf, sm->dev_id + 3, 4);
            buf[4] = 0;
            u2 = strtol(sm->dev_id + 3, &s, 16);
            if(!*s) {
              hd->compat_vendor.id = u1;
              hd->compat_device.id = MAKE_ID(TAG_EISA, u2);
            }
          }
        }
      }
      else {
        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0200);
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0003);
      }
    }
  }
}


/*
 * Baud setting magic taken from gpm.
 */

int _setspeed(int fd, int old, int new, int needtowrite, unsigned short flags)
{
  struct termios tty;
  char *c;
  int err = 0;

  flags |= CREAD | CLOCAL | HUPCL;

  if(tcgetattr(fd, &tty)) return errno;

  tty.c_iflag = IGNBRK | IGNPAR;
  tty.c_oflag = 0;
  tty.c_lflag = 0;
  tty.c_line = 0;
  tty.c_cc[VTIME] = 0;
  tty.c_cc[VMIN] = 1;

  switch (old)
    {
    case 9600:  tty.c_cflag = flags | B9600; break;
    case 4800:  tty.c_cflag = flags | B4800; break;
    case 2400:  tty.c_cflag = flags | B2400; break;
    case 1200:
    default:    tty.c_cflag = flags | B1200; break;
    }

  if(tcsetattr(fd, TCSAFLUSH, &tty)) return errno;

  switch (new)
    {
    case 9600:  c = "*q";  tty.c_cflag = flags | B9600; break;
    case 4800:  c = "*p";  tty.c_cflag = flags | B4800; break;
    case 2400:  c = "*o";  tty.c_cflag = flags | B2400; break;
    case 1200:
    default:    c = "*n";  tty.c_cflag = flags | B1200; break;
    }

  if(needtowrite) {
    err = 2 - write(fd, c, 2);
  }

  usleep(100000);

  if(tcsetattr(fd, TCSAFLUSH, &tty)) return errno;

  return err;
}


void setspeed(int fd, int new, int needtowrite, unsigned short flags)
{
  int i, err;

  for(i = 9600; i >= 1200; i >>= 1) {
    err = _setspeed(fd, i, new, needtowrite, flags);
#if 0
    if(err) {
      fprintf(stderr, "%d, %d ", i, err);
      perror("");
    }
#endif
  }
}


#if 0
/*
 * Check for a PnP info field starting at ofs;
 * returns either the length of the field or 0 if none was found.
 *
 * the minfo_t struct is updated with the PnP data
 */
int is_pnpinfo(ser_device_t *mi, int ofs)
{
  int i;
  unsigned char *s = mi->buf + ofs;
  int len = mi->buf_len - ofs;

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

  if(
    (mi->bits == 6 && s[10] == 0x09) ||
    (mi->bits == 7 && s[10] == 0x29)
  ) {
    return 11;
  }

  if(
    (mi->bits != 6 || s[10] != 0x3c) &&
    (mi->bits != 7 || s[10] != 0x5c)
  ) {
    return 0;
  }

  /* skip extended info */
  for(i = 11; i < len; i++) {
    if(
      (mi->bits == 6 && s[i] == 0x09) ||
      (mi->bits == 7 && s[i] == 0x29)
    ) {
      return i + 1;
    }
  }

  /*
   * some mice have problems providing the extended info -> return ok in
   * these cases too
   */
  if(
    (mi->bits == 6 && s[10] == 0x3c) ||
    (mi->bits == 7 && s[10] == 0x5c)
  ) {
    return 11;
  }

  /* no end token... */

  return 0;
}
#endif

unsigned chk4id(ser_device_t *mi)
{
  int i;

#if 0
  unsigned char fake[] =
  {
    // fake pnp data
  };

  mi->buf_len = sizeof fake;
  memcpy(mi->buf, fake, mi->buf_len);
  // for(i = 0; i < mi->buf_len; i++) mi->buf[i] += ' ';
#endif

  if(!mi->buf_len) return 0;

  for(i = 0; i < mi->buf_len; i++) {
    if((mi->pnp = is_pnpinfo(mi, i))) break;
  }
  if(i == mi->buf_len) {
    /* non PnP, but MS compatible */
    if(*mi->buf == 'M')
      mi->non_pnp = mi->buf_len - 1;
    else
      return 0;
  }

  mi->garbage = i;

  for(i = 0; i < mi->garbage; i++) {
    if(mi->buf[i] == 'M') {
      mi->non_pnp = mi->garbage - i;
      mi->garbage = i;
      break;
    }
  }

  if(mi->non_pnp || mi->bits == 6) mi->is_mouse = 1;

  return mi->is_mouse;
}

ser_device_t *add_ser_mouse_entry(ser_device_t **sm, ser_device_t *new_sm)
{
  while(*sm) sm = &(*sm)->next;
  return *sm = new_sm;
}


void dump_ser_mouse_data(hd_data_t *hd_data)
{
  int j;
  ser_device_t *sm;

  if(!(sm = hd_data->ser_mouse)) return;

  ADD2LOG("----- serial mice -----\n");

  for(; sm; sm = sm->next) {
    ADD2LOG("%s\n", sm->dev_name);
    if(sm->serial) ADD2LOG("serial: \"%s\"\n", sm->serial);
    if(sm->class_name) ADD2LOG("class_name: \"%s\"\n", sm->class_name);
    if(sm->dev_id) ADD2LOG("dev_id: \"%s\"\n", sm->dev_id);
    if(sm->user_name) ADD2LOG("user_name: \"%s\"\n", sm->user_name);

    if(sm->garbage) {
      ADD2LOG("  garbage[%u]: ", sm->garbage);
      hexdump(&hd_data->log, 1, sm->garbage, sm->buf);
      ADD2LOG("\n");
    }

    if(sm->non_pnp) {
      ADD2LOG("  non-pnp[%u]: ", sm->non_pnp);
      hexdump(&hd_data->log, 1, sm->non_pnp, sm->buf + sm->garbage);
      ADD2LOG("\n");
    }

    if(sm->pnp) {
      ADD2LOG("  pnp[%u]: ", sm->pnp);
      hexdump(&hd_data->log, 1, sm->pnp, sm->buf + sm->garbage + sm->non_pnp);
      ADD2LOG("\n");
    }

    if((j = sm->buf_len - (sm->garbage + sm->non_pnp + sm->pnp))) {
      ADD2LOG("  moves[%u]: ", j);
      hexdump(&hd_data->log, 1, j, sm->buf + sm->garbage + sm->non_pnp + sm->pnp);
      ADD2LOG("\n");
    }

    if(sm->is_mouse) ADD2LOG("  is mouse\n");

    if(sm->pnp) {
      ADD2LOG("  bits: %u\n", sm->bits);
      ADD2LOG("  PnP Rev: %u.%02u\n", sm->pnp_rev / 100, sm->pnp_rev % 100);
      ADD2LOG("  PnP ID: \"%s\"\n", sm->pnp_id);
    }

    if(sm->next) ADD2LOG("\n");
  }

  ADD2LOG("----- serial mice end -----\n");
}

#endif		/* !defined(LIBHD_TINY) */

/** @} */


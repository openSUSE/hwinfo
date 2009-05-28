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

/**
 * @defgroup MODEMint Modem devices
 * @ingroup libhdDEVint
 * @brief Modem detection functions
 *
 * Note: what about modem speed?
 *
 * @{
 */

#ifndef LIBHD_TINY

static struct speeds_s {
  unsigned baud;
  speed_t mask;
} speeds[] = {
  {    1200, B1200    },
  {    1800, B1800    },
  {    2400, B2400    },
  {    4800, B4800    },
  {    9600, B9600    },
  {   19200, B19200   },
  {   38400, B38400   },
  {   57600, B57600   },
  {  115200, B115200  }
#if !defined(__sparc__)
  ,{  230400, B230400  }
  ,{  460800, B460800  }
  ,{  500000, B500000  }
  ,{ 1000000, B1000000 }
  ,{ 2000000, B2000000 }
  ,{ 4000000, B4000000 }
#endif
};

#define MAX_SPEED	(sizeof speeds / sizeof *speeds)

static char *init_strings[] = {
  "Q0 V1 E1",
  "S0=0",
  "&C1",
  "&D2",
  "+FCLASS=0"
};

#define MAX_INIT_STRING	(sizeof init_strings / sizeof *init_strings)

static void get_serial_modem(hd_data_t* hd_data);
static void add_serial_modem(hd_data_t* hd_data);
static int dev_name_duplicate(hd_data_t *hd_data, char *dev_name);
static void guess_modem_name(hd_data_t *hd_data, ser_device_t *sm);
static void at_cmd(hd_data_t *hd_data, char *at, int raw, int log_it);
static void write_modem(hd_data_t *hd_data, char *msg);
static void read_modem(hd_data_t *hd_data);
static ser_device_t *add_ser_modem_entry(ser_device_t **sm, ser_device_t *new_sm);
static int set_modem_speed(ser_device_t *sm, unsigned baud);    
static int init_modem(ser_device_t *mi);
static unsigned chk4id(ser_device_t *mi);
static void dump_ser_modem_data(hd_data_t *hd_data);

void hd_scan_modem(hd_data_t *hd_data)
{
  ser_device_t *sm, *sm_next;

  if(!hd_probe_feature(hd_data, pr_modem)) return;

  hd_data->module = mod_modem;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->ser_modem = NULL;

  PROGRESS(1, 0, "serial");

  hd_fork(hd_data, 15, 120);

  if(hd_data->flags.forked) {
    get_serial_modem(hd_data);
    hd_move_to_shm(hd_data);
    if((hd_data->debug & HD_DEB_MODEM)) dump_ser_modem_data(hd_data);
  }
  else {
    /* take data from shm */
    hd_data->ser_modem = ((hd_data_t *) (hd_data->shm.data))->ser_modem;
    if((hd_data->debug & HD_DEB_MODEM)) dump_ser_modem_data(hd_data);
  }

  hd_fork_done(hd_data);

  add_serial_modem(hd_data);

  hd_shm_clean(hd_data);

  for(sm = hd_data->ser_modem; sm; sm = sm_next) {
    sm_next = sm->next;

    free_str_list(sm->at_resp);

    free_mem(sm->dev_name);
    free_mem(sm->serial);
    free_mem(sm->class_name);
    free_mem(sm->dev_id);
    free_mem(sm->user_name);
    free_mem(sm->vend);
    free_mem(sm->init_string1);
    free_mem(sm->init_string2);

    free_mem(sm);
  }
  hd_data->ser_modem = NULL;

}

int check_for_responce(str_list_t *str_list, char *str, int len)
{
  for(; str_list != NULL; str_list = str_list->next) {
    if(!strncmp(str_list->str, str, len)) return 1;
  }

  return 0;
}

str_list_t *str_list_dup(str_list_t *orig)
{
  str_list_t *dup = NULL;

  for(; orig != NULL; orig = orig->next) {
    add_str_list(&dup, orig->str);
  }

  return dup;
}

void get_serial_modem(hd_data_t *hd_data)
{
  hd_t *hd;
  int i, j, fd;
  unsigned modem_info, baud;
  char *command;
  ser_device_t *sm;
  int chk_usb = hd_probe_feature(hd_data, pr_modem_usb);

  /* serial modems & usb modems */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      (
        (
          hd->base_class.id == bc_comm &&
          hd->sub_class.id == sc_com_ser &&
          !hd->tag.skip_modem &&
          hd->tag.ser_device != 2 &&		/* cf. serial.c */
          !has_something_attached(hd_data, hd)
        ) ||
        (
          chk_usb &&
          hd->bus.id == bus_usb &&
          hd->base_class.id == bc_modem
        )
      ) && hd->unix_dev_name
    ) {
      if(dev_name_duplicate(hd_data, hd->unix_dev_name)) continue;
      if((fd = open(hd->unix_dev_name, O_RDWR | O_NONBLOCK)) >= 0) {
        sm = add_ser_modem_entry(&hd_data->ser_modem, new_mem(sizeof *sm));
        sm->dev_name = new_str(hd->unix_dev_name);
        sm->fd = fd;
        sm->hd_idx = hd->idx;
        sm->do_io = 1;
        init_modem(sm);
      }
    }
  }

  if(!hd_data->ser_modem) return;

  PROGRESS(2, 0, "init");

  usleep(300000);		/* PnP protocol; 200ms seems to be too fast  */
  
  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    modem_info = TIOCM_DTR | TIOCM_RTS;
    ioctl(sm->fd, TIOCMBIS, &modem_info);
    ioctl(sm->fd, TIOCMGET, &modem_info);
    if(!(modem_info & (TIOCM_DSR | TIOCM_CD))) {
      sm->do_io = 0;
    }
  }

  /* just a quick test if we get a response to an AT command */

  for(i = 0; i < 4; i++) {
    PROGRESS(3, i + 1, "at test");

    for(sm = hd_data->ser_modem; sm; sm = sm->next) {
      if(!sm->is_modem)
        set_modem_speed(sm, i == 0 ? 115200 : i == 1 ? 38400 : i == 2 ? 9600 : 1200);
    }

    at_cmd(hd_data, "AT\r", 1, 1);

    for(sm = hd_data->ser_modem; sm; sm = sm->next) {
      if(strstr(sm->buf, "OK") || strstr(sm->buf, "0")) {
        sm->is_modem = 1;
        sm->do_io = 0;
      }
      sm->buf_len = 0;		/* clear buffer */
    }
  }

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if((sm->do_io = sm->is_modem)) {
      sm->max_baud = sm->cur_baud;
    }
  }

  /* check for init string */
  PROGRESS(4, 0, "init string");

  command = NULL;
  for(i = 0; (unsigned) i < MAX_INIT_STRING; i++) {
    str_printf(&command, 0, "AT %s\r", init_strings[i]);
    at_cmd(hd_data, command, 1, 1);

    for(sm = hd_data->ser_modem; sm; sm = sm->next) {
      if(strstr(sm->buf, "OK") || strstr(sm->buf, "0")) {
        str_printf(&sm->init_string2, -1,
          "%s %s", sm->init_string2 ? "" : "AT", init_strings[i]
        );
      }
    }
  }
  command = free_mem(command);

  for(sm = hd_data->ser_modem; sm; sm = sm->next)
    if(sm->is_modem)
      str_printf(&sm->init_string1, -1, "ATZ");

  {
    int cmds[] = { 1, 3, 4, 5, 6 };
    char at[10];
    int i, j, ModemsCount = 0;
    str_list_t **responces = NULL;
    for(sm = hd_data->ser_modem; sm; sm = sm->next)
      if(sm->is_modem)
	ModemsCount++;
    responces = new_mem(ModemsCount * sizeof *responces);
    
    at_cmd(hd_data, "ATI\r", 0, 1);
    for(j = 0, sm = hd_data->ser_modem; sm; sm = sm->next) {
      if(sm->is_modem)
	responces[j++] = str_list_dup(sm->at_resp);
    }

    for(i = 0; (unsigned) i < sizeof cmds / sizeof *cmds; i++) {
      int atx = cmds[i];
      sprintf(at, "ATI%d\r", atx);
      at_cmd(hd_data, at, 0, 1);
      for(j = 0, sm = hd_data->ser_modem; sm; sm = sm->next) {
	if(sm->is_modem) {
	  if(atx == 1 && check_for_responce(responces[j], "Hagenuk", 7) &&
	     (check_for_responce(sm->at_resp, "Speed Dragon", 12) ||
	      check_for_responce(sm->at_resp, "Power Dragon", 12))) {
	    free_mem(sm->init_string1);
	    free_mem(sm->init_string2);
	    sm->init_string1 = new_str("AT&F");
	    sm->init_string2 = new_str("ATB8");
	  }
	  if(atx == 3 && check_for_responce(responces[j], "346900", 6) &&
	     check_for_responce(sm->at_resp, "3Com U.S. Robotics ISDN", 23)) {
	    free_mem(sm->init_string1);
	    free_mem(sm->init_string2);
	    sm->init_string1 = new_str("AT&F");
	    sm->init_string2 = new_str("AT*PPP=1");
	  }
	  if(atx == 4 && check_for_responce(responces[j], "SP ISDN", 7) &&
	     check_for_responce(sm->at_resp, "Sportster ISDN TA", 17)) {
	    free_mem(sm->init_string1);
	    free_mem(sm->init_string2);
	    sm->init_string1 = new_str("AT&F");
	    sm->init_string2 = new_str("ATB3");
	  }
	  if(atx == 6 && check_for_responce(responces[j], "644", 3) &&
	     check_for_responce(sm->at_resp, "ELSA MicroLink ISDN", 19)) {
	    free_mem(sm->init_string1);
	    free_mem(sm->init_string2);
	    sm->init_string1 = new_str("AT&F");
	    sm->init_string2 = new_str("AT$IBP=HDLCP");
	    free_mem(sm->pppd_option);
	    sm->pppd_option = new_str("default-asyncmap");
	  }
	  if(atx == 6 && check_for_responce(responces[j], "643", 3) &&
	     check_for_responce(sm->at_resp, "MicroLink ISDN/TLV.34", 21)) {
	    free_mem(sm->init_string1);
	    free_mem(sm->init_string2);
	    sm->init_string1 = new_str("AT&F");
	    sm->init_string2 = new_str("AT\\N10%P1");
	  }
	  if(atx == 5 && check_for_responce(responces[j], "ISDN TA", 6) &&
	     check_for_responce(sm->at_resp, "ISDN TA;ASU", 4)) {
	    free_mem(sm->vend);
	    sm->vend = new_str("ASUS");
	    free_mem(sm->user_name);
	    sm->user_name = new_str("ISDNLink TA");
	    free_mem(sm->init_string1);
	    free_mem(sm->init_string2);
	    sm->init_string1 = new_str("AT&F");
	    sm->init_string2 = new_str("ATB40");
	  }
	  if(atx==3 && check_for_responce(responces[j], "128000", 6) &&
	     check_for_responce(sm->at_resp, "Lasat Speed", 11)) {
	    free_mem(sm->init_string1);
	    free_mem(sm->init_string2);
	    sm->init_string1 = new_str("AT&F");
	    sm->init_string2 = new_str("AT\\P1&B2X3");
	  }
	  if(atx == 1 &&
	     (check_for_responce(responces[j], "28642", 5) ||
	      check_for_responce(responces[j], "1281", 4) ||
	      check_for_responce(responces[j], "1282", 4) ||
	      check_for_responce(responces[j], "1283", 4) ||
	      check_for_responce(responces[j], "1291", 4) ||
	      check_for_responce(responces[j], "1292", 4) ||
	      check_for_responce(responces[j], "1293", 4)) &&
	     (check_for_responce(sm->at_resp, "Elite 2864I", 11) ||
	      check_for_responce(sm->at_resp, "ZyXEL omni", 10))) {
	    free_mem(sm->init_string1);
	    free_mem(sm->init_string2);
	    sm->init_string1 = new_str("AT&F");
	    sm->init_string2 = new_str("AT&O2B40");
	  }
	  j++;
	}
      }
    }

    for(i = 0; i < ModemsCount; i++) free_str_list(responces[i]);
    free_mem(responces);
  }

  /* now, go for the maximum speed... */
  PROGRESS(5, 0, "speed");

  for(i = MAX_SPEED - 1; i >= 0; i--) {
    baud = speeds[i].baud;
    for(j = 0, sm = hd_data->ser_modem; sm; sm = sm->next) {
      if(sm->is_modem) {
        if(baud > sm->max_baud) {
          sm->do_io = set_modem_speed(sm, baud) ? 0 : 1;
          if(sm->do_io) j++;
        }
      }
    }

    /* no modems */
    if(!j) continue;

    at_cmd(hd_data, "AT\r", 1, 0);

    for(sm = hd_data->ser_modem; sm; sm = sm->next) {
      if(strstr(sm->buf, "OK") || strstr(sm->buf, "0")) {
        sm->max_baud = sm->cur_baud;
      }
      else {
        sm->do_io = 0;
      }
      sm->buf_len = 0;		/* clear buffer */
    }
  }

  /* now, fix it all up... */
  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->is_modem) {
      set_modem_speed(sm, sm->max_baud);
      sm->do_io = 1;
    }
  }

#if 0
  /* just for testing */
  if((hd_data->debug & HD_DEB_MODEM)) {
    int i;
    int cmds[] = { 0, 1, 2, 3, 6 };
    char at[10];

    PROGRESS(8, 0, "testing");

    at_cmd(hd_data, "ATI\r", 0, 1);
    for(i = 0; (unsigned) i < sizeof cmds / sizeof *cmds; i++) {
      sprintf(at, "ATI%d\r", cmds[i]);
      at_cmd(hd_data, at, 0, 1);
    }
    at_cmd(hd_data, "AT\r", 0, 1);
  }
#endif

  PROGRESS(5, 0, "pnp id");

  at_cmd(hd_data, "ATI9\r", 1, 1);

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->is_modem) {
      chk4id(sm);

      if(!sm->user_name) guess_modem_name(hd_data, sm);
    }

    /* reset serial lines */
    tcflush(sm->fd, TCIOFLUSH);
    tcsetattr(sm->fd, TCSAFLUSH, &sm->tio);
    close(sm->fd);
  }
}


void add_serial_modem(hd_data_t *hd_data)
{
  hd_t *hd;
  char buf[4];
  ser_device_t *sm;
  hd_res_t *res;

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(!sm->is_modem) continue;

    hd = hd_get_device_by_idx(hd_data, sm->hd_idx);
    if(hd && hd->base_class.id == bc_modem) {
      /* just *add* info */

    }
    else {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_modem;
      hd->bus.id = bus_serial;
      hd->unix_dev_name = new_str(sm->dev_name);
      hd->attached_to = sm->hd_idx;
      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->baud.type = res_baud;
      res->baud.speed = sm->max_baud;
      if(sm->pppd_option) {
	res = add_res_entry(&hd->res, new_mem(sizeof *res));
	res->pppd_option.type = res_pppd_option;
	res->pppd_option.option = new_str(sm->pppd_option);
      }
      if(*sm->pnp_id) {
        strncpy(buf, sm->pnp_id, 3);
        buf[3] = 0;
        hd->vendor.id = name2eisa_id(buf);
        hd->device.id = MAKE_ID(TAG_EISA, strtol(sm->pnp_id + 3, NULL, 16));
      }
      hd->serial = new_str(sm->serial);
      if(sm->user_name) hd->device.name = new_str(sm->user_name);
      if(sm->vend) hd->vendor.name = new_str(sm->vend);

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

      if(!(hd->device.id || hd->device.name || hd->vendor.id || hd->vendor.name)) {
        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x2000);
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
      }
    }
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->init_strings.type = res_init_strings;
    res->init_strings.init1 = new_str(sm->init_string1);
    res->init_strings.init2 = new_str(sm->init_string2);
  }
}


int dev_name_duplicate(hd_data_t *hd_data, char *dev_name)
{
  ser_device_t *sm;

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(!strcmp(sm->dev_name, dev_name)) return 1;
  }

  return 0;
}

void guess_modem_name(hd_data_t *hd_data, ser_device_t *modem)
{
  ser_device_t *sm;
  str_list_t *sl;
  char *s;
#ifdef __PPC__
  char *s1, *s2;
  unsigned u;
#endif

  for(sm = hd_data->ser_modem; sm; sm = sm->next) sm->do_io = 0;

  (sm = modem)->do_io = 1;

#ifdef __PPC__
  at_cmd(hd_data, "ATI0\r", 0, 1);
  sl = sm->at_resp;
  if(sl && !strcmp(sl->str, "ATI0")) sl = sl->next;	/* skip AT cmd echo */

  s1 = NULL;
  if(sl) {
    if(strstr(sl->str, "PowerBook")) {
      sm->vend = new_str("Apple");
      sm->user_name = new_str(sl->str);

      return;
    }
    s1 = new_str(sl->str);
  }

  at_cmd(hd_data, "ATI1\r", 0, 1);
  sl = sm->at_resp;
  if(sl && !strcmp(sl->str, "ATI1")) sl = sl->next;	/* skip AT cmd echo */

  if(sl) {
    if(strstr(sl->str, "APPLE")) {
      sm->vend = new_str("Apple");
      str_printf(&sm->user_name, 0, "AT Modem");
      if(s1) {
        u = strtoul(s1, &s2, 10);
        if(u && !*s2 && !(u % 1000)) {
          str_printf(&sm->user_name, 0, "%uk AT Modem", u / 1000);
        }
      }
      s1 = free_mem(s1);

      return;
    }
  }
  s1 = free_mem(s1);

#endif
  
  /* ATI3 command */
  at_cmd(hd_data, "ATI3\r", 0, 1);
  sl = sm->at_resp;
  if(sl && !strcmp(sl->str, "ATI3")) sl = sl->next;	/* skip AT cmd echo */

  if(sl) {
    if(*sl->str == 'U' && strstr(sl->str, "Robotics ")) {
      /* looks like an U.S. Robotics... */

      sm->vend = new_str("U.S. Robotics, Inc.");
      /* strip revision code */
      if((s = strstr(sl->str, " Rev. "))) *s = 0;
      sm->user_name = canon_str(sl->str, strlen(sl->str));

      return;
    }

    if(strstr(sl->str, "3Com U.S. Robotics ") == sl->str) {
      /* looks like an 3Com U.S. Robotics... */

      sm->vend = new_str("3Com U.S. Robotics, Inc.");
      sm->user_name = canon_str(sl->str, strlen(sl->str));

      return;
    }

    if(strstr(sl->str, "-V34_DS -d Z201 2836")) {
      /* looks like a Zoom V34X */

      sm->vend = new_str("Zoom Telephonics, Inc.");
      sm->user_name = new_str("Zoom FaxModem V.34X Plus Model 2836");

      return;
    }

    if(strstr(sl->str, "FM560 VER 3.01 V.90")) {
      /* looks like a Microcom DeskPorte 56K Voice ... */

      sm->vend = new_str("Microcom");
      sm->user_name = new_str("TravelCard 56K");

      return;
    }

    if(strstr(sl->str, "Compaq Microcom 550 56K Modem")) {
      /* looks like a Microcom DeskPorte Pocket ... */

      sm->vend = new_str("Compaq");
      sm->user_name = new_str("Microcom 550 56K Modem");

      return;
    }
  }

  /* ATI0 command */
  at_cmd(hd_data, "ATI0\r", 0, 1);
  sl = sm->at_resp;
  if(sl && !strcmp(sl->str, "ATI0")) sl = sl->next;	/* skip AT cmd echo */

  if(sl) {
    if(strstr(sl->str, "DP Pocket")) {
      /* looks like a Microcom DeskPorte Pocket ... */

      sm->vend = new_str("Microcom");
      sm->user_name = new_str("DeskPorte Pocket");

      return;
    }
  }

  /* ATI6 command */
  at_cmd(hd_data, "ATI6\r", 0, 1);
  sl = sm->at_resp;
  if(sl && !strcmp(sl->str, "ATI6")) sl = sl->next;	/* skip AT cmd echo */

  if(sl) {
    if(strstr(sl->str, "RCV56DPF-PLL L8571A")) {
      /* looks like a Microcom DeskPorte 56K Voice ... */

      sm->vend = new_str("Microcom");
      sm->user_name = new_str("DeskPorte 56K Voice");

      return;
    }
  }

  /* ATI2 command */
  at_cmd(hd_data, "ATI2\r", 0, 1);
  sl = sm->at_resp;
  if(sl && !strcmp(sl->str, "ATI2")) sl = sl->next;	/* skip AT cmd echo */

  if(sl) {
    if(strstr(sl->str, "ZyXEL ")) {
      /* looks like a ZyXEL... */

      sm->vend = new_str("ZyXEL");

      at_cmd(hd_data, "ATI1\r", 0, 1);
      sl = sm->at_resp;
      if(sl && !strcmp(sl->str, "ATI1")) sl = sl->next;
      
      if(sl && sl->next) {
        sl = sl->next;
        if((s = strstr(sl->str, " V "))) *s = 0;
        sm->user_name = canon_str(sl->str, strlen(sl->str));
      }

      return;
    }
  }

}

void at_cmd(hd_data_t *hd_data, char *at, int raw, int log_it)
{
  static unsigned u = 1;
  char *s, *s0;
  ser_device_t *sm;
  str_list_t *sl;
  int modems = 0;

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->do_io) {
      sm->buf_len = 0;
      modems++;
    }
  }

  if(modems == 0) return;

  PROGRESS(9, u, "write at cmd");
  write_modem(hd_data, at);
  PROGRESS(9, u, "read at resp");
  usleep (200000);
  read_modem(hd_data);
  PROGRESS(9, u, "read ok");
  u++;

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->do_io) {
      sm->at_resp = free_str_list(sm->at_resp);
      if(sm->buf_len == 0 || raw) continue;
      s0 = sm->buf;
      while((s = strsep(&s0, "\r\n"))) {
        if(*s) add_str_list(&sm->at_resp, s);
      }
    }
  }

  if(!(hd_data->debug & HD_DEB_MODEM) || !log_it) return;

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->do_io) {
      ADD2LOG("%s@%u: %s\n", sm->dev_name, sm->cur_baud, at);
      if(raw) {
        ADD2LOG("  ");
        hd_log_hex(hd_data, 1, sm->buf_len, sm->buf);
        ADD2LOG("\n");
      }
      else {
        for(sl = sm->at_resp; sl; sl = sl->next) ADD2LOG("  %s\n", sl->str);
      }
    }
  }
}


void write_modem(hd_data_t *hd_data, char *msg)
{
  ser_device_t *sm;
  int i, len = strlen(msg);

  for(sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->do_io) {
      i = write(sm->fd, msg, len);
      if(i != len) {
        ADD2LOG("%s write oops: %d/%d (\"%s\")\n", sm->dev_name, i, len, msg);
      }
    }
  }
}

void read_modem(hd_data_t *hd_data)
{
  int i, sel, fd_max = -1;
  fd_set set, set0;
  struct timeval to;
  ser_device_t *sm;

  FD_ZERO(&set0);

  for(i = 0, sm = hd_data->ser_modem; sm; sm = sm->next) {
    if(sm->do_io) {
      FD_SET(sm->fd, &set0);
      if(sm->fd > fd_max) fd_max = sm->fd;
      i++;
    }
  }

  if(!i) return;	/* nothing selected */

  for(;;) {
    to.tv_sec = 0; to.tv_usec = 1000000;
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

int set_modem_speed(ser_device_t *sm, unsigned baud)
{
  int i;
  speed_t st;
  struct termios tio;

  for(i = 0; (unsigned) i < MAX_SPEED; i++) if(speeds[i].baud == baud) break;

  if(i == MAX_SPEED) return 1;

  if(tcgetattr(sm->fd, &tio)) return errno;

  cfsetospeed(&tio, speeds[i].mask);
  cfsetispeed(&tio, speeds[i].mask);

  if(tcsetattr(sm->fd, TCSAFLUSH, &tio)) return errno;

  /* tcsetattr() returns ok even if it couldn't set the speed... */

  if(tcgetattr(sm->fd, &tio)) return errno;

  st = cfgetospeed(&tio);

  for(i = 0; (unsigned) i < MAX_SPEED; i++) if(speeds[i].mask == st) break;

  if(i == MAX_SPEED) return 2;

  sm->cur_baud = speeds[i].baud;

  return baud == speeds[i].baud ? 0 : 3;
}


int init_modem(ser_device_t *sm)
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
int is_pnpinfo(ser_device_t *mi, int ofs)
{
  int i, j, k, l;
  unsigned char c, *s = mi->buf + ofs, *t;
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

  i = 1;

  /* six bit values */
  if((s[i] & ~0x3f) || (s[i + 1] & ~0x3f)) return 0;
  mi->pnp_rev = (s[i] << 6) + s[i + 1];

  /* pnp_rev may *optionally* be given as a string!!! (e.g. "1.0")*/
  if(mi->bits == 7) {
    j = 0;
    if(s[i + 2] < 'A') {
      j++;
      if(s[i + 3] < 'A') j++;
    }
    if(j) {
      if(s[i] < '0' || s[i] > '9') return 0;
      if(s[i + 1] != '.') return 0;
      for(k = 0; k < j; k++)
        if(s[i + 2 + k] < '0' || s[i + 2 + k] > '9') return 0;
      mi->pnp_rev = (s[i] - '0') * 100;
      mi->pnp_rev += s[i + 2] * 10;
      if(j == 2) mi->pnp_rev += s[i + 3];
      i += j;
    }
  }

  i += 2;

  /* the eisa id */
  for(j = 0; j < 7; j++) {
    mi->pnp_id[j] = s[i + j];
    if(mi->bits == 6) mi->pnp_id[j] += 0x20;
  }
  mi->pnp_id[7] = 0;

  i += 7;

  /* now check the id */
  for(j = 0; j < 3; j++) {
    if(
      /* numbers are not really allowed, but... */
      (mi->pnp_id[j] < '0' || mi->pnp_id[j] > '9') &&
      (mi->pnp_id[j] < 'A' || mi->pnp_id[j] > 'Z') &&
      mi->pnp_id[j] != '_'
    ) return 0;
  }

  for(j = 3; j < 7; j++) {
    if(
      (mi->pnp_id[j] < '0' || mi->pnp_id[j] > '9') &&
      (mi->pnp_id[j] < 'A' || mi->pnp_id[j] > 'F')
    ) return 0;
  }


  if((mi->bits == 6 && s[i] == 0x09) || (mi->bits == 7 && s[i] == 0x29)) {
    return i + 1;
  }

  if((mi->bits != 6 || s[i] != 0x3c) && (mi->bits != 7 || s[i] != 0x5c)) {
    return 0;
  }

  /* parse extended info */
  serial = class_name = dev_id = user_name = 0;
  for(j = 0; i < len; i++) {
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
          /* skip *optional*(!!!) 2 char checksum */
          t = mi->user_name + l - 2;
          if(
            ((t[0] >= '0' && t[0] <= '9') || (t[0] >= 'A' && t[0] <= 'F')) &&
            ((t[1] >= '0' && t[1] <= '9') || (t[1] >= 'A' && t[1] <= 'F'))
          ) {
            /* OK, *might* be a hex number... */
            mi->user_name[l - 2] = 0;

            /*
             * A better check would be to look for the complete name string
             * in the output from another AT command, e.g AT3, AT6 or AT11.
             * If it's there -> no checksum field.
             */
          }
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


unsigned chk4id(ser_device_t *mi)
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

ser_device_t *add_ser_modem_entry(ser_device_t **sm, ser_device_t *new_sm)
{
  while(*sm) sm = &(*sm)->next;
  return *sm = new_sm;
}

void dump_ser_modem_data(hd_data_t *hd_data)
{
  int j;
  ser_device_t *sm;

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
      hd_log_hex(hd_data, 1, sm->garbage, sm->buf);
      ADD2LOG("\n");  
    }

    if(sm->pnp) {
      ADD2LOG("  pnp[%u]: ", sm->pnp);
      hd_log_hex(hd_data, 1, sm->pnp, sm->buf + sm->garbage);
      ADD2LOG("\n");
    }

    if((j = sm->buf_len - (sm->garbage + sm->pnp))) {
      ADD2LOG("  post_garbage[%u]: ", j);
      hd_log_hex(hd_data, 1, j, sm->buf + sm->garbage + sm->pnp);
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

#endif	/* ifndef LIBHD_TINY */

/** @} */


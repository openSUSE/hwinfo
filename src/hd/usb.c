#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/usb.h>
#include <linux/usbdevice_fs.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "usb.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * usb info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static int get_next_device(char *dev, int idx);
static void get_usb_data(hd_data_t *hd_data);
static int usb_control_msg(int fd, unsigned requesttype, unsigned request, unsigned value, unsigned index, unsigned size, void *data);
static void set_class_entries(hd_data_t *hd_data, hd_t *hd, usb_t *usb);
static usb_t *find_usb_entry(hd_data_t *hd_data, int *dev_idx);
static usb_t *add_usb_entry(hd_data_t *hd_data, usb_t *new_usb);
static void dump_usb_data(hd_data_t *hd_data);
static void add_usb_guid(hd_t *hd);

#define USB_DT_CS_DEVICE	0x21
#define CTRL_RETRIES		50
#define CTRL_TIMEOUT		100	/* milliseconds */

void hd_scan_usb(hd_data_t *hd_data)
{
  hd_t *hd, *hd2;
  unsigned usb_ctrl[4];		/* up to 4 USB controllers; just for fun... */
  unsigned usb_ctrl_idx = 0;
  int usb_idx;
  usb_t *usb;
  hd_res_t *res;
  int kbd_cnt, mse_cnt, lp_cnt, acm_cnt;
  char *s;
#if 0
  char *mse_dev = "/dev/usbmouse";
#endif
  char *lp_dev = "/dev/usb/lp", *acm_dev = "/dev/ttyACM";

  if(!hd_probe_feature(hd_data, pr_usb)) return;

  hd_data->module = mod_usb;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->proc_usb = free_str_list(hd_data->proc_usb);
  hd_data->usb = NULL;

  PROGRESS(1, 0, "check");

  for(hd = hd_data->hd; hd && usb_ctrl_idx < sizeof usb_ctrl / sizeof *usb_ctrl; hd = hd->next) {
    if(hd->base_class.id == bc_serial && hd->sub_class.id == sc_ser_usb) {
      usb_ctrl[usb_ctrl_idx++] = hd->idx;
    }
  }

  if(usb_ctrl_idx && hd_probe_feature(hd_data, pr_usb_mods)) {
    /* load usb modules... */
    PROGRESS(2, 0, "mods");
    load_module(hd_data, "usbcore");
    hd2 = hd_get_device_by_idx(hd_data, usb_ctrl[0]);
    s = "usb-uhci";
    if(hd2 && hd2->prog_if.id == pif_usb_ohci) s = "usb-ohci";
    if(hd2 && hd2->prog_if.id == pif_usb_ehci) s = "usb-ehci";
    load_module(hd_data, s);
    load_module(hd_data, "input");
    load_module(hd_data, "hid");
    load_module(hd_data, "keybdev");
    load_module(hd_data, "mousedev");
    load_module(hd_data, "printer");
    load_module(hd_data, "acm");
  }

  PROGRESS(3, 0, "read info");

  get_usb_data(hd_data);
  if((hd_data->debug & HD_DEB_USB)) dump_usb_data(hd_data);

  PROGRESS(4, 0, "build list");

  /* as an alternative, we might just *sort* the usb list... */
  usb_idx = kbd_cnt = mse_cnt = lp_cnt = acm_cnt = 0;
  do {
    usb = find_usb_entry(hd_data, &usb_idx);
    if(usb) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus.id = bus_usb;
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_usb;
      hd->detail->usb.data = usb;

      usb->hd_idx = hd->idx;

      hd->slot = (usb->bus << 8) + usb->dev_nr;

      hd->func = usb->ifdescr;

      if(usb->vendor || usb->device) {
        hd->vendor.id = MAKE_ID(TAG_USB, usb->vendor);
        hd->device.id = MAKE_ID(TAG_USB, usb->device);
      }
      if(usb->rev) str_printf(&hd->revision.name, 0, "%x.%02x", usb->rev >> 8, usb->rev & 0xff);

      if(usb->manufact) {
        if(hd->vendor.id)
          hd->sub_vendor.name = new_str(usb->manufact);
        else
          hd->vendor.name = new_str(usb->manufact);
      }

      if(usb->product) {
        if(hd->device.id)
          hd->sub_device.name = new_str(usb->product);
        else
          hd->device.name = new_str(usb->product);
      }

      if(usb->serial) hd->serial = new_str(usb->serial);

      add_usb_guid(hd);

      if(usb->speed) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->baud.type = res_baud;
        res->baud.speed = usb->speed;
      }

      set_class_entries(hd_data, hd, usb);

      if (usb->driver && !strcmp(usb->driver,"usbscanner")) {
      	hd->base_class.id = bc_scanner;
      }

      if(hd->base_class.id == bc_mouse) {
#if 0
        mse_cnt = get_next_device(mse_dev, mse_cnt);
        if(mse_cnt >= 0) str_printf(&hd->unix_dev_name, 0, "%s%d", mse_dev, mse_cnt++);
#else
        /* new USB stack - new devices :-/ */
        hd->unix_dev_name = new_str(DEV_MICE);
#endif
      }

      if(hd->base_class.id == bc_keyboard) {
        // kbd_cnt = get_next_device(kbd_dev, kbd_cnt);
        // if(kbd_cnt >= 0) str_printf(&hd->unix_dev_name, 0, "%s%d", kbd_dev, kbd_cnt++);
      }

      if(hd->base_class.id == bc_printer) {
        lp_cnt = get_next_device(lp_dev, lp_cnt);
        if(lp_cnt >= 0 && lp_cnt < 8) str_printf(&hd->unix_dev_name, 0, "%s%d", lp_dev, lp_cnt++);
      }

      if(hd->base_class.id == bc_modem) {
        acm_cnt = get_next_device(acm_dev, acm_cnt);
        if(acm_cnt >= 0) str_printf(&hd->unix_dev_name, 0, "%s%d", acm_dev, acm_cnt++);
      }

    }
  }
  while(usb_idx);

  PROGRESS(5, 0, "tree");

  /* now, connect the usb devices to each other */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->bus.id == bus_usb &&
      hd->detail &&
      hd->detail->type == hd_detail_usb
    ) {
      usb_t *hd_usb = hd->detail->usb.data;

      if(hd_usb->parent) {
        usb_idx = (hd_usb->bus << 16) + (hd_usb->parent << 8);
        usb = find_usb_entry(hd_data, &usb_idx);
        if(usb && usb->hd_idx) {
          hd->attached_to = usb->hd_idx;
        }
      }
      else if(hd->base_class.id == bc_hub && (unsigned) hd_usb->bus < usb_ctrl_idx) {
        /* root hub */
        hd->attached_to = usb_ctrl[hd_usb->bus];

        hd2 = hd_get_device_by_idx(hd_data, hd->attached_to);

        if(hd2 && !(hd->vendor.id || hd->vendor.name)) {
          hd->vendor.id = hd2->vendor.id;
          hd->vendor.name = new_str(hd2->vendor.name);
        }

        if(!(hd->device.id || hd->device.name)) {
          hd->device.name = new_str("Root Hub");
        }
      }
    }
  }

  /* remove potentially dangling 'next' links */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->bus.id == bus_usb &&
      hd->detail &&
      hd->detail->type == hd_detail_usb
    ) {
      hd->detail->usb.data->next = NULL;
    }
  }

  hd_data->usb = NULL;

}


int get_next_device(char *dev, int idx)
{
  char *s = NULL;
  int dev_ok = 0;
  int fd;

  if(idx < 0 || idx > 15) return -1;

  for(; idx < 16; idx++) {
    dev_ok = 1;
    str_printf(&s, 0, "%s%d", dev, idx);
    fd = open(s, O_RDONLY | O_NONBLOCK);
    /*
     * note: basically, we treat missing or inacessible devices as ok;
     * we're looking only for devices without drivers attached
     */
    if(fd < 0 && (errno == ENXIO || errno == ENODEV)) dev_ok = 0;
    s = free_mem(s);
    close(fd);
    if(dev_ok) break;
  }

  return dev_ok ? idx : -1;
}


void get_usb_data(hd_data_t *hd_data)
{
  int i0, i1, i2, i3, i4, i5, i6;
  char buf[256];
  str_list_t *sl, *sl_next;
  usb_t *usb = NULL, *usb_next;
  char *s;
  int fd, cfg_descr_size, i, ifcnt;
  unsigned char *cfg_descr, *hid_descr;

  if(
    !(hd_data->proc_usb = read_file(PROC_USB_DEVICES, 0, 0)) &&
    !(hd_data->proc_usb = read_file(DEV_USB_DEVICES, 0, 0))
  ) return;

  for(sl = hd_data->proc_usb; sl; sl = sl->next) {
    if(*sl->str == 'T') {
      usb = add_usb_entry(hd_data, new_mem(sizeof *usb));
    }
    s = strlen(sl->str) > 4 ? sl->str + 4 : "";
    if(usb) switch(*sl->str) {
      case 'B':
        add_str_list(&usb->b, s); break;
      case 'C':
        if(*s && sl->str[2] == '*')
          add_str_list(&usb->c, s);
        else
          add_str_list(&usb->ci, s);
        break;
      case 'D':
        add_str_list(&usb->d, s); break;
      case 'E':
        add_str_list(&usb->e, s); break;
      case 'I':
        if(!usb->i || strstr(s, "Cls=03") /* HID only */) {
          add_str_list(&usb->i, s);
        }
        break;
      case 'P':
        add_str_list(&usb->p, s); break;
      case 'S':
        add_str_list(&usb->s, s); break;
      case 'T':
        add_str_list(&usb->t, s); break;
    }
  }

  /*
   * Create individual entries for every interface descriptor.
   */
  for(usb = hd_data->usb; usb; usb = usb_next) {
    usb_next = usb->next;

    if(usb->i && (sl = usb->i->next)) {
      usb->i->next = NULL;
      ifcnt = 1;
      for(; sl; sl = sl_next, ifcnt++) {
        sl_next = sl->next;
        if(!usb->cloned) usb->cloned = usb;
        usb->next = new_mem(sizeof *usb);
        memcpy(usb->next, usb, sizeof *usb);
        usb = usb->next;
        usb->next = usb_next;
        usb->i = sl;
        usb->i->next = NULL;
        usb->ifdescr = ifcnt;
      }
    }
  }

  for(usb = hd_data->usb; usb; usb = usb->next) {
    if(
      sscanf(usb->t->str,
        "Bus=%d Lev=%d Prnt=%d Port=%d Cnt=%d Dev#=%d Spd=%7s MxCh=%d",
        &i0, &i1, &i2, &i3, &i4, &i5, buf, &i6
      ) == 8 ||
      (/* old usb stack */ i0 = 0, sscanf(usb->t->str,
        "Lev=%d Prnt=%d Port=%d Cnt=%d Dev#=%d Spd=%7s MxCh=%d",
        &i1, &i2, &i3, &i4, &i5, buf, &i6
      )) == 7
    ) {
      usb->bus = i0;
      usb->dev_nr = i5;
      if(strcmp(buf, "12")) usb->speed = 12*1000000;
      if(strcmp(buf, "1.5")) usb->speed = 15*100000;
      usb->lev = i1;
      usb->parent = i2;
      usb->port = i3;
      usb->count = i4;
      usb->conns = i6;
    }

    for(sl = usb->s; sl; sl = sl->next) {
      if(sscanf(sl->str, "Manufacturer=%255[^\n]", buf) == 1) {
        usb->manufact = canon_str(buf, strlen(buf));
      }
      if(sscanf(sl->str, "Product=%255[^\n]", buf) == 1) {
        usb->product = canon_str(buf, strlen(buf));
      }
      if(sscanf(sl->str, "SerialNumber=%255[^\n]", buf) == 1) {
        usb->serial = canon_str(buf, strlen(buf));
      }
    }

    if(
      usb->p &&
      sscanf(usb->p->str, "Vendor=%x ProdID=%x Rev=%x.%x", &i0, &i1, &i2, &i3) == 4
    ) {
      usb->vendor = i0;
      usb->device = i1;
      usb->rev = (i2 << 8) + i3;
    }

    if(
      usb->d &&
      sscanf(usb->d->str,
        "Ver=%x.%x Cls=%x(%255[^)]) Sub=%x Prot=%x MxPS=%d #Cfgs=%d",
         &i0, &i1, &i2, buf, &i3, &i4, &i5, &i6
      ) == 8
    ) {
      usb->d_cls = i2;
      usb->d_sub = i3;
      usb->d_prot = i4;
    }

    /* we'll look just at the first interface for now */
    if(
      usb->i &&
      sscanf(usb->i->str,
        "If#=%d Alt=%d #EPs=%d Cls=%x(%*255[^)]) Sub=%x Prot=%x Driver=%255[^\n]",
         &i0, &i1, &i2, &i3, &i4, &i5, buf
      ) >= 7
    ) {
      usb->i_cls = i3;
      usb->i_sub = i4;
      usb->i_prot = i5;
      if(strcmp(buf, "(none)")) usb->driver = new_str(buf);
    }

    if(usb->i_cls == 3 && !hd_data->flags.fast) {	/* hid */
      /*
       * Check only the first interface for country info.
       */
      if(!usb->cloned) {
        s = NULL;
        str_printf(&s, 0, "/proc/bus/usb/%03u/%03u", usb->bus, usb->dev_nr);
        fd = open(s, O_RDWR);
        if(fd >= 0) {
          if(
            usb_control_msg(fd,
              USB_DIR_IN,
              USB_REQ_GET_DESCRIPTOR,
              (USB_DT_CONFIG << 8) + 0,
              0, USB_DT_CONFIG_SIZE, buf
            ) >= 0 &&
            buf[0] >= USB_DT_CONFIG_SIZE &&
            buf[1] == USB_DT_CONFIG
          ) {
            cfg_descr_size = buf[2] | buf[3] << 8;
            cfg_descr = new_mem(cfg_descr_size);
            if(
              usb_control_msg(fd,
                USB_DIR_IN,
                USB_REQ_GET_DESCRIPTOR,
                (USB_DT_CONFIG << 8) + 0,
                0, cfg_descr_size, cfg_descr
              ) >= 0
            ) {
              ADD2LOG("  got config descr for %03u/%03u (%u bytes):\n    ", usb->bus, usb->dev_nr, cfg_descr_size);
              hexdump(&hd_data->log, 0, cfg_descr_size, cfg_descr);
              ADD2LOG("\n");

              /* ok, we have it, now parse it */

              for(i = 0; i < cfg_descr_size; i += cfg_descr[i]) {
                if(cfg_descr[i] < 2 || cfg_descr[i] + i > cfg_descr_size) break;
                if(cfg_descr[i + 1] == USB_DT_CS_DEVICE) {
                  hid_descr = cfg_descr + i;
                  if(hid_descr[0] >= 6 + 3 * hid_descr[5]) {
                    ADD2LOG("    hid descr: ");
                    hexdump(&hd_data->log, 0, hid_descr[0], hid_descr);
                    ADD2LOG("\n");
                    usb->country = hid_descr[4];
                    // ADD2LOG("    country code: %u\n", usb->country);
                  }
                }
              }
            }
            cfg_descr = free_mem(cfg_descr);
          }
          close(fd);
        }
        s = free_mem(s);
      }
      else {
        usb->country = usb->cloned->country;
      }
    }
  }
}


/* taken from lsusb.c (usbutils-0.8) */
int usb_control_msg(int fd, unsigned requesttype, unsigned request, unsigned value, unsigned index, unsigned size, void *data)
{
  struct usbdevfs_ctrltransfer ctrl;
  int result, try;

  ctrl.requesttype = requesttype;
  ctrl.request = request;
  ctrl.value = value;
  ctrl.index = index;
  ctrl.length = size;
  ctrl.timeout = 1000;
  ctrl.data = data;
  ctrl.timeout = CTRL_TIMEOUT; 
  try = 0;

  do {
    result = ioctl(fd, USBDEVFS_CONTROL, &ctrl);
    try++;
  } while(try < CTRL_RETRIES && result == -1 && errno == ETIMEDOUT);

  return result;
}


void set_class_entries(hd_data_t *hd_data, hd_t *hd, usb_t *usb)
{
  int cls, sub, prot;
  unsigned u;

  if(usb->d_cls) {
    cls = usb->d_cls; sub = usb->d_sub; prot = usb->d_prot;
  }
  else {
    cls = usb->i_cls; sub = usb->i_sub; prot = usb->i_prot;
  }

  switch(cls) {
    case 2:
      hd->base_class.id = bc_modem;
      break;

    case 3:
      if(sub == 1 && prot == 1) {
        hd->base_class.id = bc_keyboard;
        hd->sub_class.id = sc_keyboard_kbd;
        break;
      }
      if(sub == 1 && prot == 2) {
        if(!(
          (usb->vendor == 0x056a && usb->device == 0x0022)	/* Wacom Tablet */
//          || (usb->vendor == 0x08ca && usb->device == 0x0020)	/* AIPTEK APT-6000U tablet */
        )) {
          hd->base_class.id = bc_mouse;
          hd->sub_class.id = sc_mou_usb;
          hd->compat_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0200);
          hd->compat_device.id = MAKE_ID(TAG_SPECIAL, 0x001);
        }
        break;
      }
      break;

    case 6:
      if(sub == 1 && prot == 1) { /* PTP camera */
        hd->base_class.id = bc_camera;
        hd->sub_class.id = sc_camera_digital;
        break;
      }
      break;

    case 7:
      hd->base_class.id = bc_printer;
      break;

    case 8:
      hd->base_class.id = bc_storage_device;
      switch(sub) {
        case 1:		/* flash devices & removable media */
        case 5:
        case 6:
          hd->sub_class.id = sc_sdev_disk;
          break;
        case 2:
          hd->sub_class.id = sc_sdev_cdrom;
          break;
        case 3:
          hd->sub_class.id = sc_sdev_tape;
          break;
        case 4:
          hd->sub_class.id = sc_sdev_floppy;
          break;
        default:
          hd->sub_class.id = sc_sdev_other;
      }
      break;

    case 9:
      hd->base_class.id = bc_hub;
      break;
  }

  if((u = device_class(hd_data, hd->vendor.id, hd->device.id))) {
    hd->base_class.id = u >> 8;
    hd->sub_class.id = u & 0xff;
  }

}


/*
 * look for an usb entry with dev_idx; dev_idx is set to the smallest
 * value bigger than dev_idx (is 0 if dev_idx is largest)
 */
usb_t *find_usb_entry(hd_data_t *hd_data, int *dev_idx)
{
  usb_t *usb, *found_usb = NULL;
  int search_idx = *dev_idx, cur_idx, next_idx = 0;

  for(usb = hd_data->usb; usb; usb = usb->next) {
    cur_idx = (usb->bus << 16) + (usb->dev_nr << 8) + usb->ifdescr;
    if(cur_idx == search_idx) found_usb = usb;
    if(
      cur_idx > search_idx &&
      (cur_idx < next_idx || !next_idx)
    ) {
      next_idx = cur_idx;
    }
  }

  *dev_idx = next_idx;

  return found_usb;
}

/*
 * Store a raw USB entry; just for convenience.
 */
usb_t *add_usb_entry(hd_data_t *hd_data, usb_t *new_usb)
{
  usb_t **usb = &hd_data->usb;

  while(*usb) usb = &(*usb)->next;

  return *usb = new_usb;
}


/*
 * Add some scsi data to the global log.
 */
void dump_usb_data(hd_data_t *hd_data)
{
  str_list_t *sl;
  usb_t *usb;

  ADD2LOG("----- /proc/bus/usb/devices -----\n");
  for(sl = hd_data->proc_usb; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/bus/usb/devices end -----\n");

  ADD2LOG("----- usb device info -----\n");
  for(usb = hd_data->usb; usb; usb = usb->next) {
    ADD2LOG("  %d:%d.%d (@%d:%d) %d %d\n", usb->bus, usb->dev_nr, usb->ifdescr, usb->bus, usb->parent, usb->conns, usb->speed);
    ADD2LOG("  vend 0x%04x, dev 0x%04x, rev 0x%04x\n", usb->vendor, usb->device, usb->rev);
    ADD2LOG(
      "  cls/sub/prot: 0x%02x/0x%02x/0x%02x  0x%02x/0x%02x/0x%02x\n",
      usb->d_cls, usb->d_sub, usb->d_prot, usb->i_cls, usb->i_sub, usb->i_prot
    );
    if(usb->driver) ADD2LOG("  driver \"%s\"\n", usb->driver);
    if(usb->manufact) ADD2LOG("  manufacturer \"%s\"\n", usb->manufact);
    if(usb->product) ADD2LOG("  product \"%s\"\n", usb->product);
    if(usb->serial) ADD2LOG("  serial \"%s\"\n", usb->serial);
    if(usb->country) ADD2LOG("  country %u\n", usb->country);
    if(usb->next) ADD2LOG("\n");
  }
  ADD2LOG("----- usb device info end -----\n");
}


void add_usb_guid(hd_t *hd)
{
  unsigned pg[3];
  char c, *s;

  pg[0] = ((hd->vendor.id & 0xffff) << 16) | (hd->device.id & 0xffff);
  pg[1] = pg[2] = 0;
  if(hd->serial) {
    for(s = hd->serial; *s; s++) {
      pg[1] <<= 4;
      pg[1] |= pg[2] >> 28;
      pg[2] <<= 4;
      c = toupper(*s);
      if(c > '9') c -= 'A' - '9' - 1;
      pg[2] |= c - '0';
    }
  }

  str_printf(&hd->usb_guid, 0, "%08x%08x%08x", pg[0], pg[1], pg[2]);
}


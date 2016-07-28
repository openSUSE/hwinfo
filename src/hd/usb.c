#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "usb.h"

/**
 * @defgroup USBint Universal Serial Bus (USB)
 * @ingroup libhdBUSint
 * @brief USB scan functions
 *
 * @{
 */

#define IOCNR_GET_DEVICE_ID             1
#define IOCNR_GET_BUS_ADDRESS           5
#define IOCNR_GET_VID_PID               6

/* Get device_id string: */
#define LPIOC_GET_DEVICE_ID(len) _IOC(_IOC_READ, 'P', IOCNR_GET_DEVICE_ID, len)
/* Get two-int array: [0]=bus number, [1]=device address: */
#define LPIOC_GET_BUS_ADDRESS(len) _IOC(_IOC_READ, 'P', IOCNR_GET_BUS_ADDRESS, len)
/* Get two-int array: [0]=vendor ID, [1]=product ID: */
#define LPIOC_GET_VID_PID(len) _IOC(_IOC_READ, 'P', IOCNR_GET_VID_PID, len)


static void get_usb_devs(hd_data_t *hd_data);
static void set_class_entries(hd_data_t *hd_data, hd_t *hd, usb_t *usb);
static void add_input_dev(hd_data_t *hd_data, char *name);
static void get_input_devs(hd_data_t *hd_data);
static void get_printer_devs(hd_data_t *hd_data);
static void read_usb_lp(hd_data_t *hd_data, hd_t *hd);
static void get_serial_devs(hd_data_t *hd_data);

void hd_scan_sysfs_usb(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_usb)) return;

  hd_data->module = mod_usb;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->proc_usb = free_str_list(hd_data->proc_usb);
  hd_data->usb = NULL;

  PROGRESS(1, 0, "sysfs drivers");
  
  hd_sysfs_driver_list(hd_data);

  PROGRESS(2, 0, "usb");

  get_usb_devs(hd_data);

  PROGRESS(3, 1, "joydev mod");
  load_module(hd_data, "joydev");
    
  PROGRESS(3, 2, "evdev mod");
  load_module(hd_data, "evdev");

  PROGRESS(3, 3, "input");
  get_input_devs(hd_data);

  PROGRESS(3, 4, "lp");
  get_printer_devs(hd_data);

  PROGRESS(3, 5, "serial");
  get_serial_devs(hd_data);

}


void get_usb_devs(hd_data_t *hd_data)
{
  uint64_t ul0;
  unsigned u1, u2, u3;
  hd_t *hd, *hd1;
  usb_t *usb;
  str_list_t *sl, *usb_devs = NULL;
  char *s, *s1, *t;
  hd_res_t *res;
  size_t l;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *sf_dev_2;

  sf_bus = read_dir("/sys/bus/usb/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: usb\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = hd_read_sysfs_link("/sys/bus/usb/devices", sf_bus_e->str);

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "bNumInterfaces"), &ul0, 0)) {
      add_str_list(&usb_devs, sf_dev);
      ADD2LOG("  usb dev: %s\n", hd_sysfs_id(sf_dev));
    }
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/usb/devices", sf_bus_e->str));

    ADD2LOG(
      "  usb device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    if(
      hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "bInterfaceNumber"), &ul0, 16)
    ) {
      hd = add_hd_entry(hd_data, __LINE__, 0);

      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_usb;
      hd->detail->usb.data = usb = new_mem(sizeof *usb);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);

      hd->bus.id = bus_usb;
      hd->func = ul0;

      usb->ifdescr = ul0;

      if((s = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
        s = canon_str(s, strlen(s));
        ADD2LOG("    modalias = \"%s\"\n", s);
        if(s && *s) {
          hd->modalias = s;
          s = NULL;
        }
        s = free_mem(s);
      }

      ADD2LOG("    bInterfaceNumber = %u\n", hd->func);

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "bInterfaceClass"), &ul0, 16)) {
        usb->i_cls = ul0;
        ADD2LOG("    bInterfaceClass = %u\n", usb->i_cls);
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "bInterfaceSubClass"), &ul0, 16)) {
        usb->i_sub = ul0;
        ADD2LOG("    bInterfaceSubClass = %u\n", usb->i_sub);
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "bInterfaceProtocol"), &ul0, 16)) {
        usb->i_prot = ul0;
        ADD2LOG("    bInterfaceProtocol = %u\n", usb->i_prot);
      }

      /* device has longest matching sysfs id */
      u2 = strlen(sf_dev);
      s = NULL;
      for(u3 = 0, sl = usb_devs; sl; sl = sl->next) {
        u1 = strlen(sl->str);
        if(u1 > u3 && u1 <= u2 && !strncmp(sf_dev, sl->str, u1)) {
          u3 = u1;
          s = sl->str;
        }
      }

      if(s) {
        ADD2LOG("    if: %s @ %s\n", hd->sysfs_bus_id, hd_sysfs_id(s));
        sf_dev_2 = new_str(s);
        if(sf_dev_2) {

          if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev_2, "bDeviceClass"), &ul0, 16)) {
            usb->d_cls = ul0;
            ADD2LOG("    bDeviceClass = %u\n", usb->d_cls);
          }

          if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev_2, "bDeviceSubClass"), &ul0, 16)) {
            usb->d_sub = ul0;
            ADD2LOG("    bDeviceSubClass = %u\n", usb->d_sub);
          }

          if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev_2, "bDeviceProtocol"), &ul0, 16)) {
            usb->d_prot = ul0;
            ADD2LOG("    bDeviceProtocol = %u\n", usb->d_prot);
          }

          if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev_2, "idVendor"), &ul0, 16)) {
            usb->vendor = ul0;
            ADD2LOG("    idVendor = 0x%04x\n", usb->vendor);
          }

          if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev_2, "idProduct"), &ul0, 16)) {
            usb->device = ul0;
            ADD2LOG("    idProduct = 0x%04x\n", usb->device);
          }

          if((s = get_sysfs_attr_by_path(sf_dev_2, "manufacturer"))) {
            usb->manufact = canon_str(s, strlen(s));
            ADD2LOG("    manufacturer = \"%s\"\n", usb->manufact);
          }

          if((s = get_sysfs_attr_by_path(sf_dev_2, "product"))) {
            usb->product = canon_str(s, strlen(s));
            ADD2LOG("    product = \"%s\"\n", usb->product);
          }

          if((s = get_sysfs_attr_by_path(sf_dev_2, "serial"))) {
            usb->serial = canon_str(s, strlen(s));
            ADD2LOG("    serial = \"%s\"\n", usb->serial);
          }

          if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev_2, "bcdDevice"), &ul0, 16)) {
            usb->rev = ul0;
            ADD2LOG("    bcdDevice = %04x\n", usb->rev);
          }

          if((s = get_sysfs_attr_by_path(sf_dev_2, "speed"))) {
            s = canon_str(s, strlen(s));
            if(!strcmp(s, "1.5")) usb->speed = 15*100000;
            else if(!strcmp(s, "12")) usb->speed = 12*1000000;
            else if(!strcmp(s, "480")) usb->speed = 480*1000000;
            ADD2LOG("    speed = \"%s\"\n", s);
            s = free_mem(s);
          }

          sf_dev_2 = free_mem(sf_dev_2);
        }
      }

      if(usb->vendor || usb->device) {
        hd->vendor.id = MAKE_ID(TAG_USB, usb->vendor);
        hd->device.id = MAKE_ID(TAG_USB, usb->device);
      }

      if(usb->manufact) hd->vendor.name = new_str(usb->manufact);
      if(usb->product) hd->device.name = new_str(usb->product);
      if(usb->serial) hd->serial = new_str(usb->serial);

      if(usb->rev) str_printf(&hd->revision.name, 0, "%x.%02x", usb->rev >> 8, usb->rev & 0xff);

      if(usb->speed) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->baud.type = res_baud;
        res->baud.speed = usb->speed;
      }

      s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
      if(s) add_str_list(&hd->drivers, s);

      set_class_entries(hd_data, hd, usb);

      if(!hd_data->scanner_db) {
        hd_data->scanner_db = hd_module_list(hd_data, 1);
      }

      if(
        hd->drivers &&
        search_str_list(hd_data->scanner_db, hd->drivers->str)
      ) {
        hd->base_class.id = bc_scanner;
      }

      // ###### FIXME
      if(hd->base_class.id == bc_modem) {
        hd->unix_dev_name = new_str("/dev/ttyACM0");
      }
    }

    sf_dev = free_mem(sf_dev);
  }

  sf_bus = free_str_list(sf_bus);

  /* connect usb devices to each other */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->module == hd_data->module && hd->sysfs_id) {

      s = new_str(hd->sysfs_id);
      t = strrchr(s, '/');
      if(t) *t = 0;

      /* parent has longest matching sysfs id */
      u2 = strlen(s);
      for(u3 = 0, hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
        if(hd1->sysfs_id) {
          s1 = new_str(hd1->sysfs_id);

          if(hd1->module == hd_data->module) {
            t = strrchr(s1, ':');
            if(t) *t = 0;
            l = strlen(s1);
            if(l > 2 && s1[l-2] == '-' && s1[l-1] == '0') {
              /* root hub */
              s1[l-2] = 0 ;
            }
          }

          u1 = strlen(s1);
          if(u1 > u3 && u1 <= u2 && !strncmp(s, s1, u1)) {
            u3 = u1;
            hd->attached_to = hd1->idx;
          }

          s1 = free_mem(s1);
        }
      }

      s = free_mem(s);
    }
  }

  /* remove some entries */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->sysfs_id &&
      !hd->tag.remove
    ) {

      s = new_str(hd->sysfs_id);
      t = strrchr(s, ':');
      if(t) *t = 0;

      for(hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
        if(
          hd1 != hd &&
          hd1->module == hd_data->module &&
          hd1->sysfs_id &&
          !hd1->tag.remove &&
          hd1->base_class.id == hd->base_class.id
        ) {

          s1 = new_str(hd1->sysfs_id);
          t = strrchr(s1, ':');
          if(t) *t = 0;

          /* same usb device */
          if(!strcmp(s, s1)) {
            hd1->tag.remove = 1;
            ADD2LOG("removed: %s\n", hd1->sysfs_id);
          }

          s1 = free_mem(s1);
        }
      }

      s = free_mem(s);
    }
  }

  remove_tagged_hd_entries(hd_data);


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
    case 1:
      hd->base_class.id = bc_multimedia;
      hd->sub_class.id = sc_multi_audio;
      break;
    case 2:
      if(usb->i_sub == 6 && usb->i_prot == 0) {
        hd->base_class.id = bc_network;
        hd->sub_class.id = 0x91;
      }
      else if(usb->i_sub == 2 && usb->i_prot >= 1 && usb->i_prot <= 6) {
        hd->base_class.id = bc_modem;
      }
      break;

    case 3:
      if(sub == 1 && prot == 1) {
        if(!(
          (usb->vendor == 0x05ac && usb->device == 0x1000)	/* MacBook, bnc #374101 */
        )) {
          hd->base_class.id = bc_keyboard;
          hd->sub_class.id = sc_keyboard_kbd;
        }
        break;
      }
      if(sub == 1 && prot == 2) {
        if(!(
          (usb->vendor == 0x056a && usb->device == 0x0022)	/* Wacom Tablet */
          || (usb->vendor == 0x147a && usb->device == 0xe001)	/* eDio USB Multi Remote Controlle */
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

    case 0x0b:
      hd->base_class.id = bc_chipcard;
      break;

    case 0xe0:
      if(sub == 1 && prot == 1) {
        hd->base_class.id = bc_bluetooth;
        hd->sub_class.id = 0;
      }
      break;

    case 0xff:
      /* hp psc 2100, 2200, 2150, officejet 6100 */
      if(
        sub == 0xcc &&
        (
          usb->vendor == 0x03f0 &&
          (
            usb->device == 0x2811 ||
            usb->device == 0x2911 ||
            usb->device == 0x2a11 ||
            usb->device == 0x2d11
          )
        )
      ) {
        hd->base_class.id = bc_scanner;
        hd->sub_class.id = 1;
      }
      break;
  }

  if((u = device_class(hd_data, hd->vendor.id, hd->device.id))) {
    hd->base_class.id = u >> 8;
    hd->sub_class.id = u & 0xff;
  }

  /* FIXME: hack for bt isdn box */
  if(
    hd->vendor.id == MAKE_ID(TAG_USB, 0x057c) &&
    hd->device.id == MAKE_ID(TAG_USB, 0x2200)
  ) {
    hd_set_hw_class(hd, hw_bluetooth);
  }

}


void add_input_dev(hd_data_t *hd_data, char *name)
{
  hd_t *hd;
  char *s, *t;
  hd_dev_num_t dev_num = { };
  unsigned u1, u2;
  char *sf_drv_name, *sf_drv, *bus_name, *bus_id;
  char *sf_cdev_name, *sf_dev;

  sf_cdev_name = name ? strrchr(name, '/') : NULL;
  if(sf_cdev_name) sf_cdev_name++;

  ADD2LOG(
    "  input: name = %s, path = %s\n",
    sf_cdev_name,
    hd_sysfs_id(name)
  );

  if(!sf_cdev_name || !strncmp(sf_cdev_name, "ts", sizeof "ts" - 1)) return;

  if((s = get_sysfs_attr_by_path(name, "dev"))) {
    if(sscanf(s, "%u:%u", &u1, &u2) == 2) {
      dev_num.type = 'c';
      dev_num.major = u1;
      dev_num.minor = u2;
      dev_num.range = 1;
    }
    ADD2LOG("    dev = %u:%u\n", u1, u2);
  }
  else {
    ADD2LOG("    no dev - ignored\n");
    return;
  }

  sf_dev = new_str(hd_read_sysfs_link(name, "device"));

  if(sf_dev) {
    /* new kernel (2.6.24): one more level */
    s = new_str(hd_read_sysfs_link(sf_dev, "device"));
    if(s) {
      free_mem(sf_dev);
      sf_dev = s;
    }

    bus_id = sf_dev ? strrchr(sf_dev, '/') : NULL;
    if(bus_id) bus_id++;

    sf_drv_name = NULL;
    if((sf_drv = hd_read_sysfs_link(sf_dev, "driver"))) {
      sf_drv_name = strrchr(sf_drv, '/');
      if(sf_drv_name) sf_drv_name++;
      sf_drv_name = new_str(sf_drv_name);
    }

    bus_name = NULL;
    if((s = hd_read_sysfs_link(sf_dev, "subsystem"))) {
      bus_name = strrchr(s, '/');
      if(bus_name) bus_name++;
      bus_name = new_str(bus_name);
    }

    s = hd_sysfs_id(sf_dev);

    ADD2LOG(
      "    input device: bus = %s, bus_id = %s driver = %s\n      path = %s\n",
      bus_name,
      bus_id,
      sf_drv_name,
      s
    );

    /* find device (matching sysfs path) */
    hd = hd_find_sysfs_id(hd_data, s);

    /* not found? - retry one level up */
    if(!hd) {
      char *ns = new_str(s), *nt;
      if((nt = strrchr(ns, '/'))) {
        *nt = 0;
        hd = hd_find_sysfs_id(hd_data, ns);
      }
      free_mem(ns);
    }

    /* when we have it, add input device name */
    if(hd) {
      t = NULL;
      str_printf(&t, 0, "/dev/input/%s", sf_cdev_name);

      if(strncmp(sf_cdev_name, "mouse", sizeof "mouse" - 1)) {
        if(!hd->unix_dev_name) {
          hd->unix_dev_name = t;
          hd->unix_dev_num = dev_num;
        }
      }
      else {
        free_mem(hd->unix_dev_name);
        free_mem(hd->unix_dev_name2);

        hd->unix_dev_name2 = t;
        hd->unix_dev_num2 = dev_num;

        dev_num.major = 13;
        dev_num.minor = 63;
        hd->unix_dev_name = new_str(DEV_MICE);
        hd->unix_dev_num = dev_num;

        // make it a mouse, #216091
        if(hd->base_class.id == bc_none) {
          hd->base_class.id = bc_mouse;
          hd->sub_class.id = sc_mou_usb;
          hd->compat_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0200);
          hd->compat_device.id = MAKE_ID(TAG_SPECIAL, 0x001);
        }
      }
    }

    bus_name = free_mem(bus_name);
    sf_drv_name = free_mem(sf_drv_name);
  }

  sf_dev = free_mem(sf_dev);
}


void get_input_devs(hd_data_t *hd_data)
{
  str_list_t *sf_dir, *sf_dir_e;
  char *sf_dev = NULL;
  int is_dir = 0;

  /*
   * A bit tricky: if there are links, assume newer sysfs layout with compat
   * symlinks; if not, assume old layout with directories.
   */
  sf_dir = read_dir("/sys/class/input", 'l');
  if(!sf_dir) {
    sf_dir = read_dir("/sys/class/input", 'd');
    is_dir = 1;
  }
  
  if(!sf_dir) {
    ADD2LOG("sysfs: no such class: input\n");
    return;
  }

  for(sf_dir_e = sf_dir; sf_dir_e; sf_dir_e = sf_dir_e->next) {
    if(is_dir) {
      str_printf(&sf_dev, 0, "/sys/class/input/%s", sf_dir_e->str);
    }
    else {
      sf_dev = new_str(hd_read_sysfs_link("/sys/class/input", sf_dir_e->str));
    }

    add_input_dev(hd_data, sf_dev);

    sf_dev = free_mem(sf_dev);
  }

  sf_dir = free_str_list(sf_dir);
}


void get_printer_devs(hd_data_t *hd_data)
{
  hd_t *hd;
  char *s, *t;
  hd_dev_num_t dev_num = { };
  unsigned u1, u2;
  str_list_t *sf_class, *sf_class_e;
  char *sf_cdev = NULL, *sf_dev;
  char *sf_drv_name, *sf_drv, *bus_id, *bus_name;

  sf_class = read_dir("/sys/class/usb", 'D');
  if(!sf_class) sf_class = read_dir("/sys/class/usb_endpoint", 'l');

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: usb\n");
    return;
  }

  for(sf_class_e = sf_class; sf_class_e; sf_class_e = sf_class_e->next) {
    if(strncmp(sf_class_e->str, "lp", 2)) continue;

    str_printf(&sf_cdev, 0, "/sys/class/usb/%s", sf_class_e->str);

    ADD2LOG(
      "  usb: name = %s, path = %s\n",
      sf_class_e->str,
      hd_sysfs_id(sf_cdev)
    );

    if((s = get_sysfs_attr_by_path(sf_cdev, "dev"))) {
      if(sscanf(s, "%u:%u", &u1, &u2) == 2) {
        dev_num.type = 'c';
        dev_num.major = u1;
        dev_num.minor = u2;
        dev_num.range = 1;
      }
      ADD2LOG("    dev = %u:%u\n", u1, u2);
    }

    sf_dev = new_str(hd_read_sysfs_link(sf_cdev, "device"));

    if(sf_dev) {
      bus_id = sf_dev ? strrchr(sf_dev, '/') : NULL;
      if(bus_id) bus_id++;

      sf_drv_name = NULL;
      if((sf_drv = hd_read_sysfs_link(sf_dev, "driver"))) {
        sf_drv_name = strrchr(sf_drv, '/');
        if(sf_drv_name) sf_drv_name++;
        sf_drv_name = new_str(sf_drv_name);
      }

      bus_name = NULL;
      if((s = hd_read_sysfs_link(sf_dev, "bus"))) {
        bus_name = strrchr(s, '/');
        if(bus_name) bus_name++;
        bus_name = new_str(bus_name);
      }

      s = hd_sysfs_id(sf_dev);

      ADD2LOG(
        "    usb device: bus = %s, bus_id = %s driver = %s\n      path = %s\n",
        bus_name,
        bus_id,
        sf_drv_name,
        s
      );

      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(
          hd->module == hd_data->module &&
          hd->sysfs_id &&
          s &&
          !strcmp(s, hd->sysfs_id)
        ) {
          t = NULL;
          str_printf(&t, 0, "/dev/usb/%s", sf_class_e->str);

          hd->unix_dev_name = t;
          hd->unix_dev_num = dev_num;

          read_usb_lp(hd_data, hd);
        }
      }

      bus_name = free_mem(bus_name);
      sf_drv_name = free_mem(sf_drv_name);
    }

    sf_dev = free_mem(sf_dev);
  }

  sf_cdev = free_mem(sf_cdev);
  sf_class = free_str_list(sf_class);
}


#define MATCH_FIELD(field, var) \
  if(!strncasecmp(sl->str, field, sizeof field - 1)) var = sl->str + sizeof field - 1

/*
 * assign /dev/usb/lp* to usb printers.
 */
void read_usb_lp(hd_data_t *hd_data, hd_t *hd)
{
  char *s;
  char buf[1024];
  int fd, two_ints[2];
  str_list_t *sl0, *sl;
  char *vend, *prod, *serial, *descr;

  if((fd = open(hd->unix_dev_name, O_RDWR)) < 0) return;

  if(ioctl(fd, LPIOC_GET_BUS_ADDRESS(sizeof two_ints), two_ints) == -1) {
    close(fd);
    return;
  }
  
  ADD2LOG("  usb/lp: bus = %d, dev_nr = %d\n", two_ints[0], two_ints[1]);

  if(ioctl(fd, LPIOC_GET_VID_PID(sizeof two_ints), two_ints) != -1) {
    /* just for the record */
    ADD2LOG("  usb/lp: vend = 0x%04x, prod = 0x%04x\n", two_ints[0], two_ints[1]);
  }

  memset(buf, 0, sizeof buf);
  if(!ioctl(fd, LPIOC_GET_DEVICE_ID(sizeof buf), buf)) {
    buf[sizeof buf - 1] = 0;
    s = canon_str(buf + 2, sizeof buf - 3);
    ADD2LOG("  usb/lp: \"%s\"\n", s);
    sl0 = hd_split(';', s);
    free_mem(s);
    vend = prod = serial = descr = NULL;
    for(sl = sl0; sl; sl = sl->next) {
      MATCH_FIELD("MFG:", vend);
      MATCH_FIELD("MANUFACTURER:", vend);
      MATCH_FIELD("MDL:", prod);
      MATCH_FIELD("MODEL:", prod);
      MATCH_FIELD("DES:", descr);
      MATCH_FIELD("DESCRIPTION:", descr);
      MATCH_FIELD("SERN:", serial);
      MATCH_FIELD("SERIALNUMBER:", serial);
    }
    ADD2LOG(
      "  usb/lp: vend = %s, prod = %s, descr = %s, serial = %s\n",
      vend ?: "", prod ?: "", descr ?: "", serial ?: ""
    );
    if(descr) {
      str_printf(&hd->model, 0, "%s", descr);
    }
    if(vend && prod) {
      str_printf(&hd->sub_vendor.name, 0, "%s", vend);
      str_printf(&hd->sub_device.name, 0, "%s", prod);
    }
    if(serial && !hd->serial) {
      hd->serial = new_str(serial);
    }

    free_str_list(sl0);
  }

  close(fd);
}
#undef MATCH_FIELD


void get_serial_devs(hd_data_t *hd_data)
{
  hd_t *hd;
  char *s, *t;
  hd_dev_num_t dev_num = { };
  unsigned u1, u2;
  str_list_t *sf_class, *sf_class_e;
  char *sf_cdev = NULL, *sf_dev;
  char *sf_drv_name, *sf_drv, *bus_id, *bus_name;

  sf_class = read_dir("/sys/class/tty", 'D');

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: tty\n");
    return;
  }

  for(sf_class_e = sf_class; sf_class_e; sf_class_e = sf_class_e->next) {
    if(strncmp(sf_class_e->str, "ttyUSB", 6)) continue;

    str_printf(&sf_cdev, 0, "/sys/class/tty/%s", sf_class_e->str);

    ADD2LOG(
      "  usb: name = %s, path = %s\n",
      sf_class_e->str,
      hd_sysfs_id(sf_cdev)
    );

    if((s = get_sysfs_attr_by_path(sf_cdev, "dev"))) {
      if(sscanf(s, "%u:%u", &u1, &u2) == 2) {
        dev_num.type = 'c';
        dev_num.major = u1;
        dev_num.minor = u2;
        dev_num.range = 1;
      }
      ADD2LOG("    dev = %u:%u\n", u1, u2);
    }

    sf_dev = new_str(hd_read_sysfs_link(sf_cdev, "device"));

    if(sf_dev) {
      bus_id = sf_dev ? strrchr(sf_dev, '/') : NULL;
      if(bus_id) bus_id++;

      sf_drv_name = NULL;
      if((sf_drv = hd_read_sysfs_link(sf_dev, "driver"))) {
        sf_drv_name = strrchr(sf_drv, '/');
        if(sf_drv_name) sf_drv_name++;
        sf_drv_name = new_str(sf_drv_name);
      }

      bus_name = NULL;
      if((s = hd_read_sysfs_link(sf_dev, "bus"))) {
        bus_name = strrchr(s, '/');
        if(bus_name) bus_name++;
        bus_name = new_str(bus_name);
      }

      s = hd_sysfs_id(sf_dev);

      if((t = strrchr(s, '/')) && !strncmp(t + 1, "ttyUSB", sizeof "ttyUSB" - 1)) *t = 0;

      ADD2LOG(
        "    usb device: bus = %s, bus_id = %s driver = %s\n      path = %s\n",
        bus_name,
        bus_id,
        sf_drv_name,
        s
      );

      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(
          hd->module == hd_data->module &&
          hd->sysfs_id &&
          s &&
          !strcmp(s, hd->sysfs_id)
        ) {
          t = NULL;
          str_printf(&t, 0, "/dev/%s", sf_class_e->str);

          hd->unix_dev_name = t;
          hd->unix_dev_num = dev_num;

          hd->base_class.id = bc_comm;
          hd->sub_class.id = sc_com_ser;
          hd->prog_if.id = 0x80;

          // bnc #408715 (T-Balancer BigNG)
          if(
            hd->vendor.id == MAKE_ID(TAG_USB, 0x0403) &&
            hd->device.id == MAKE_ID(TAG_USB, 0x6001)
          ) {
            hd->tag.skip_mouse = hd->tag.skip_modem = 1;
          }
        }
      }
    }

    sf_dev = free_mem(sf_dev);
  }

  sf_cdev = free_mem(sf_cdev);
  sf_class = free_str_list(sf_class);
}

/** @} */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "usb.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * usb
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void get_usb_devs(hd_data_t *hd_data);
static void set_class_entries(hd_data_t *hd_data, hd_t *hd, usb_t *usb);
static void get_input_devs(hd_data_t *hd_data);

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

}


void get_usb_devs(hd_data_t *hd_data)
{
  uint64_t ul0, ul1;
  unsigned u1, u2, u3;
  hd_t *hd, *hd1;
  usb_t *usb;
  str_list_t *sl, *usb_devs = NULL;
  char *s, *s1, *t;
  hd_res_t *res;
  size_t l;

  struct sysfs_bus *sf_bus;
  struct dlist *sf_dev_list;
  struct sysfs_device *sf_dev;
  struct sysfs_device *sf_dev_2;

  sf_bus = sysfs_open_bus("usb");

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: usb\n");
    return;
  }

  sf_dev_list = sysfs_get_bus_devices(sf_bus);

  if(sf_dev_list) dlist_for_each_data(sf_dev_list, sf_dev, struct sysfs_device) {
    if(hd_attr_uint(sysfs_get_device_attr(sf_dev, "bNumInterfaces"), &ul0, 0)) {
      add_str_list(&usb_devs, sf_dev->path);
      ADD2LOG("  usb dev: %s\n", hd_sysfs_id(sf_dev->path));
    }
  }

  if(sf_dev_list) dlist_for_each_data(sf_dev_list, sf_dev, struct sysfs_device) {
    ADD2LOG(
      "  usb device: name = %s, bus_id = %s, bus = %s\n    path = %s\n",
      sf_dev->name,
      sf_dev->bus_id,
      sf_dev->bus,
      hd_sysfs_id(sf_dev->path)
    );

    if(
      hd_attr_uint(sysfs_get_device_attr(sf_dev, "bInterfaceNumber"), &ul0, 16) &&
      hd_attr_uint(sysfs_get_device_attr(sf_dev, "bAlternateSetting"), &ul1, 0) &&
      ul1 == 0
    ) {
      hd = add_hd_entry(hd_data, __LINE__, 0);

      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_usb;
      hd->detail->usb.data = usb = new_mem(sizeof *usb);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev->path));
      hd->sysfs_bus_id = new_str(sf_dev->bus_id);

      hd->bus.id = bus_usb;
      hd->func = ul0;

      usb->ifdescr = ul0;

      ADD2LOG("    bInterfaceNumber = %u\n", hd->func);

      if(hd_attr_uint(sysfs_get_device_attr(sf_dev, "bInterfaceClass"), &ul0, 16)) {
        usb->i_cls = ul0;
        ADD2LOG("    bInterfaceClass = %u\n", usb->i_cls);
      }

      if(hd_attr_uint(sysfs_get_device_attr(sf_dev, "bInterfaceSubClass"), &ul0, 16)) {
        usb->i_sub = ul0;
        ADD2LOG("    bInterfaceSubClass = %u\n", usb->i_sub);
      }

      if(hd_attr_uint(sysfs_get_device_attr(sf_dev, "bInterfaceProtocol"), &ul0, 16)) {
        usb->i_prot = ul0;
        ADD2LOG("    bInterfaceProtocol = %u\n", usb->i_prot);
      }

      /* device has longest matching sysfs id */
      u2 = strlen(sf_dev->path);
      s = NULL;
      for(u3 = 0, sl = usb_devs; sl; sl = sl->next) {
        u1 = strlen(sl->str);
        if(u1 > u3 && u1 <= u2 && !strncmp(sf_dev->path, sl->str, u1)) {
          u3 = u1;
          s = sl->str;
        }
      }

      if(s) {
        ADD2LOG("    if: %s @ %s\n", hd->sysfs_bus_id, hd_sysfs_id(s));
        sf_dev_2 = sysfs_open_device_path(s);
        if(sf_dev_2) {

          if(hd_attr_uint(sysfs_get_device_attr(sf_dev_2, "bDeviceClass"), &ul0, 16)) {
            usb->d_cls = ul0;
            ADD2LOG("    bDeviceClass = %u\n", usb->d_cls);
          }

          if(hd_attr_uint(sysfs_get_device_attr(sf_dev_2, "bDeviceSubClass"), &ul0, 16)) {
            usb->d_sub = ul0;
            ADD2LOG("    bDeviceSubClass = %u\n", usb->d_sub);
          }

          if(hd_attr_uint(sysfs_get_device_attr(sf_dev_2, "bDeviceProtocol"), &ul0, 16)) {
            usb->d_prot = ul0;
            ADD2LOG("    bDeviceProtocol = %u\n", usb->d_prot);
          }

          if(hd_attr_uint(sysfs_get_device_attr(sf_dev_2, "idVendor"), &ul0, 16)) {
            usb->vendor = ul0;
            ADD2LOG("    idVendor = 0x%04x\n", usb->vendor);
          }

          if(hd_attr_uint(sysfs_get_device_attr(sf_dev_2, "idProduct"), &ul0, 16)) {
            usb->device = ul0;
            ADD2LOG("    idProduct = 0x%04x\n", usb->device);
          }

          if((s = hd_attr_str(sysfs_get_device_attr(sf_dev_2, "manufacturer")))) {
            usb->manufact = canon_str(s, strlen(s));
            ADD2LOG("    manufacturer = \"%s\"\n", usb->manufact);
          }

          if((s = hd_attr_str(sysfs_get_device_attr(sf_dev_2, "product")))) {
            usb->product = canon_str(s, strlen(s));
            ADD2LOG("    product = \"%s\"\n", usb->product);
          }

          if((s = hd_attr_str(sysfs_get_device_attr(sf_dev_2, "serial")))) {
            usb->serial = canon_str(s, strlen(s));
            ADD2LOG("    serial = \"%s\"\n", usb->serial);
          }

          if(hd_attr_uint(sysfs_get_device_attr(sf_dev_2, "bcdDevice"), &ul0, 16)) {
            usb->rev = ul0;
            ADD2LOG("    bcdDevice = %04x\n", usb->rev);
          }

          if((s = hd_attr_str(sysfs_get_device_attr(sf_dev_2, "speed")))) {
            s = canon_str(s, strlen(s));
            if(strcmp(s, "1.5")) usb->speed = 15*100000;
            else if(strcmp(s, "12")) usb->speed = 12*1000000;
            else if(strcmp(s, "240")) usb->speed = 240*1000000;
            ADD2LOG("    speed = \"%s\"\n", s);
            s = free_mem(s);
          }

          sysfs_close_device(sf_dev_2);
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

#if 0
      if(hd->base_class.id == bc_mouse) {
        hd->unix_dev_name = new_str(DEV_MICE);
      }
#endif

    }
  }

  sysfs_close_bus(sf_bus);

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
            fprintf(stderr, "removed: %s\n", hd1->sysfs_id);
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

}


void get_input_devs(hd_data_t *hd_data)
{
  hd_t *hd;
  char *s, *t;
  hd_dev_num_t dev_num;
  unsigned u1, u2;

  struct sysfs_class *sf_class;
  struct sysfs_class_device *sf_cdev;
  struct sysfs_device *sf_dev;
  struct dlist *sf_cdev_list;

  sf_class = sysfs_open_class("input");

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: input\n");
    return;
  }

  sf_cdev_list = sysfs_get_class_devices(sf_class);
  if(sf_cdev_list) dlist_for_each_data(sf_cdev_list, sf_cdev, struct sysfs_class_device) {
    ADD2LOG(
      "  input: name = %s, path = %s\n",
      sf_cdev->name,
      hd_sysfs_id(sf_cdev->path)
    );

    if((s = hd_attr_str(sysfs_get_classdev_attr(sf_cdev, "dev")))) {
      if(sscanf(s, "%u:%u", &u1, &u2) == 2) {
        dev_num.type = 'c';
        dev_num.major = u1;
        dev_num.minor = u2;
        dev_num.range = 1;
      }
      ADD2LOG("    dev = %u:%u\n", u1, u2);
    }

    sf_dev = sysfs_get_classdev_device(sf_cdev);
    if(sf_dev) {
      s = hd_sysfs_id(sf_dev->path);

      ADD2LOG(
        "    input device: bus = %s, bus_id = %s driver = %s\n      path = %s\n",
        sf_dev->bus,
        sf_dev->bus_id,
        sf_dev->driver_name,
        s
      );

      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(
          hd->module == hd_data->module &&
          hd->sysfs_id &&
          !strcmp(s, hd->sysfs_id)
        ) {
          t = NULL;
          str_printf(&t, 0, "/dev/input/%s", sf_cdev->name);

          if(strncmp(sf_cdev->name, "mouse", sizeof "mouse" - 1)) {
            hd->unix_dev_name = t;
            hd->unix_dev_num = dev_num;
          }
          else {
            hd->unix_dev_name2 = t;
            hd->unix_dev_num2 = dev_num;

            dev_num.major = 13;
            dev_num.minor = 63;
            hd->unix_dev_name = new_str(DEV_MICE);
            hd->unix_dev_num = dev_num;
          }
        }
      }
    }
  }

  sysfs_close_class(sf_class);
}



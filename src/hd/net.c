#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>

#include "hd.h"
#include "hd_int.h"
#include "net.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gather network interface info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void get_driverinfo(hd_data_t *hd_data, hd_t *hd);
static void add_xpnet(hd_data_t *hdata);
static void add_iseries(hd_data_t *hdata);

/*
 * This is independent of the other scans.
 */

void hd_scan_net(hd_data_t *hd_data)
{
  unsigned u;
  hd_t *hd, *hd2;
#if 0
#if defined(__s390__) || defined(__s390x__)
  hd_t *hd0;
#endif
#endif
  char *s, *hw_addr;
  hd_res_t *res, *res1;

  struct sysfs_class *sf_class;
  struct sysfs_class_device *sf_cdev;
  struct sysfs_device *sf_dev;
  struct sysfs_driver *sf_drv;
  struct dlist *sf_cdev_list;

  if(!hd_probe_feature(hd_data, pr_net)) return;

  hd_data->module = mod_net;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->net = free_str_list(hd_data->net);

  PROGRESS(1, 0, "get network data");

  sf_class = sysfs_open_class("net");

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: net\n");
    return;
  }

  sf_cdev_list = sysfs_get_class_devices(sf_class);
  if(sf_cdev_list) dlist_for_each_data(sf_cdev_list, sf_cdev, struct sysfs_class_device) {
    ADD2LOG(
      "  net interface: name = %s, classname = %s, path = %s\n",
      sf_cdev->name,
      sf_cdev->classname,
      hd_sysfs_id(sf_cdev->path)
    );

    hw_addr = NULL;
    if((s = hd_attr_str(sysfs_get_classdev_attr(sf_cdev, "address")))) {
      hw_addr = canon_str(s, strlen(s));
      ADD2LOG("    hw_addr = %s\n", hw_addr);
    }

    sf_dev = sysfs_get_classdev_device(sf_cdev);
    if(sf_dev) {
      ADD2LOG("    net device: path = %s\n", hd_sysfs_id(sf_dev->path));
    }

    sf_drv = sysfs_get_classdev_driver(sf_cdev);
    if(sf_drv) {
      ADD2LOG(
        "    net driver: name = %s, path = %s\n",
        sf_drv->name,
        hd_sysfs_id(sf_drv->path)
      );
    }

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_network_interface;

    res1 = NULL;
    if(hw_addr && strspn(hw_addr, "0:") != strlen(hw_addr)) {
      res1 = new_mem(sizeof *res1);
      res1->hwaddr.type = res_hwaddr;
      res1->hwaddr.addr = new_str(hw_addr);
      add_res_entry(&hd->res, res1);
    }

    hw_addr = free_mem(hw_addr);

    hd->unix_dev_name = new_str(sf_cdev->name);
    hd->sysfs_id = new_str(hd_sysfs_id(sf_cdev->path));

    if(sf_drv) {
      add_str_list(&hd->drivers, sf_drv->name);
    }
    else if(hd->res) {
      get_driverinfo(hd_data, hd);
    }

    if(sf_dev) {
      hd->sysfs_device_link = new_str(hd_sysfs_id(sf_dev->path)); 

      hd2 = hd_find_sysfs_id(hd_data, hd_sysfs_id(sf_dev->path));
      if(hd2) {
        hd->attached_to = hd2->idx;
        /* add hw addr to network card */
        if(res1) {
          u = 0;
          for(res = hd2->res; res; res = res->next) {
            if(
              res->any.type == res_hwaddr &&
              !strcmp(res->hwaddr.addr, res1->hwaddr.addr)
            ) {
              u = 1;
              break;
            }
          }
          if(!u) {
            res = new_mem(sizeof *res);
            res->hwaddr.type = res_hwaddr;
            res->hwaddr.addr = new_str(res1->hwaddr.addr);
            add_res_entry(&hd2->res, res);
          }
        }
      }
    }

    if(!strcmp(hd->unix_dev_name, "lo")) {
      hd->sub_class.id = sc_nif_loopback;
    }
    else if(sscanf(hd->unix_dev_name, "eth%u", &u) == 1) {
      hd->sub_class.id = sc_nif_ethernet;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "tr%u", &u) == 1) {
      hd->sub_class.id = sc_nif_tokenring;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "fddi%u", &u) == 1) {
      hd->sub_class.id = sc_nif_fddi;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "ctc%u", &u) == 1) {
      hd->sub_class.id = sc_nif_ctc;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "iucv%u", &u) == 1) {
      hd->sub_class.id = sc_nif_iucv;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "hsi%u", &u) == 1) {
      hd->sub_class.id = sc_nif_hsi;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "qeth%u", &u) == 1) {
      hd->sub_class.id = sc_nif_qeth;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "escon%u", &u) == 1) {
      hd->sub_class.id = sc_nif_escon;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "myri%u", &u) == 1) {
      hd->sub_class.id = sc_nif_myrinet;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "sit%u", &u) == 1) {
      hd->sub_class.id = sc_nif_sit;	/* ipv6 over ipv4 tunnel */
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "wlan%u", &u) == 1) {
      hd->sub_class.id = sc_nif_wlan;
      hd->slot = u;
    }
    else if(sscanf(hd->unix_dev_name, "xp%u", &u) == 1) {
      hd->sub_class.id = sc_nif_xp;
      hd->slot = u;
    }
    /* ##### add more interface names here */
    else {
      hd->sub_class.id = sc_nif_other;
    }

    hd->bus.id = bus_none;
  }

  sysfs_close_class(sf_class);

  if(hd_is_sgi_altix(hd_data)) add_xpnet(hd_data);
  if(hd_is_iseries(hd_data)) add_iseries(hd_data);

#if 0

#if defined(__s390__) || defined(__s390x__)
      if(
        hd->sub_class.id != sc_nif_loopback &&
        hd->sub_class.id != sc_nif_sit && hd->sub_class.id != sc_nif_ethernet && hd->sub_class.id != sc_nif_qeth &&
	hd->sub_class.id != sc_nif_ctc
      ) {
        hd0 = hd;
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_network;
        hd->unix_dev_name = new_str(hd0->unix_dev_name);
        hd->slot = hd0->slot;
        hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);	// IBM
        switch(hd0->sub_class.id) {
          case sc_nif_tokenring:
            hd->sub_class.id = 1;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
            str_printf(&hd->device.name, 0, "Token ring card %d", hd->slot);
            break;
          case sc_nif_ctc:
            hd->sub_class.id = 0x04;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0004);
            str_printf(&hd->device.name, 0, "CTC %d", hd->slot);
            break;
          case sc_nif_iucv:
            hd->sub_class.id = 0x05;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0005);
            str_printf(&hd->device.name, 0, "IUCV %d", hd->slot);
            break;
          case sc_nif_hsi:
            hd->sub_class.id = 0x06;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0006);
            str_printf(&hd->device.name, 0, "HSI %d", hd->slot);
            break;
          case sc_nif_escon:
            hd->sub_class.id = 0x08;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0008);
            str_printf(&hd->device.name, 0, "ESCON %d", hd->slot);
            break;
          default:
            hd->sub_class.id = 0x80;
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0080);
        }
      }
#endif

#endif


}


/*
 * Get it the classical way, for drivers that don't support sysfs (veth).
 */
void get_driverinfo(hd_data_t *hd_data, hd_t *hd)
{
  int fd;
  struct ethtool_drvinfo drvinfo = { cmd:ETHTOOL_GDRVINFO };
  struct ifreq ifr;

  if(!hd->unix_dev_name) return;

  if(strlen(hd->unix_dev_name) > sizeof ifr.ifr_name - 1) return;

  if((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) return;

  /* get driver info */
  memset(&ifr, 0, sizeof ifr);
  strcpy(ifr.ifr_name, hd->unix_dev_name);
  ifr.ifr_data = (caddr_t) &drvinfo;
  if(ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
    ADD2LOG("    ethtool driver: %s\n", drvinfo.driver);
    ADD2LOG("    ethtool    bus: %s\n", drvinfo.bus_info);

    add_str_list(&hd->drivers, drvinfo.driver);
  }
  else {
    ADD2LOG("    ethtool error: %s\n", strerror(errno));
  }

  close(fd);
}


/*
 * SGI Altix cross partition network.
 */
void add_xpnet(hd_data_t *hd_data)
{
  hd_t *hd, *hd_card;
  hd_res_t *res, *res2;

  hd_card = add_hd_entry(hd_data, __LINE__, 0);
  hd_card->base_class.id = bc_network;
  hd_card->sub_class.id = 0x83;

  hd_card->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4002);
  hd_card->device.id = MAKE_ID(TAG_SPECIAL, 1);

  if(hd_module_is_active(hd_data, "xpnet")) {
    add_str_list(&hd_card->drivers, "xpnet");
  }

  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface &&
      hd->sub_class.id == sc_nif_xp
    ) {
      hd->attached_to = hd_card->idx;

      for(res = hd->res; res; res = res->next) {
        if(res->any.type == res_hwaddr) break;
      }

      if(res) {
        res2 = new_mem(sizeof *res2);
        res2->hwaddr.type = res_hwaddr;
        res2->hwaddr.addr = new_str(res->hwaddr.addr);
        add_res_entry(&hd_card->res, res2);
      }

      break;
    }
  }
}


/*
 * iSeries veth devices.
 */
void add_iseries(hd_data_t *hd_data)
{
  hd_t *hd, *hd_card;
  hd_res_t *res, *res2;
  unsigned card_cnt = 0;

  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface &&
      search_str_list(hd->drivers, "veth")
    ) {
      hd_card = add_hd_entry(hd_data, __LINE__, 0);
      hd_card->base_class.id = bc_network;
      hd_card->sub_class.id = 0x00;
      hd_card->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);	// IBM
      hd_card->device.id = MAKE_ID(TAG_SPECIAL, 0x1000);
      hd_card->slot = card_cnt++;
      str_printf(&hd_card->device.name, 0, "Virtual Ethernet card %d", hd_card->slot);
      add_str_list(&hd_card->drivers, "veth");

      hd->attached_to = hd_card->idx;

      for(res = hd->res; res; res = res->next) {
        if(res->any.type == res_hwaddr) break;
      }

      if(res) {
        res2 = new_mem(sizeof *res2);
        res2->hwaddr.type = res_hwaddr;
        res2->hwaddr.addr = new_str(res->hwaddr.addr);
        add_res_entry(&hd_card->res, res2);
      }
    }
  }
}



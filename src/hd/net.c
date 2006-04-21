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
#include <linux/if_arp.h>

#include "hd.h"
#include "hd_int.h"
#include "net.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gather network interface info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void get_driverinfo(hd_data_t *hd_data, hd_t *hd);
static void get_linkstate(hd_data_t *hd_data, hd_t *hd);
static void add_xpnet(hd_data_t *hdata);
static void add_mv(hd_data_t *hdata);
static void add_iseries(hd_data_t *hdata);
static void add_uml(hd_data_t *hdata);
static void add_kma(hd_data_t *hdata);
static void add_xen(hd_data_t *hdata);
static void add_if_name(hd_t *hd_card, hd_t *hd);

/*
 * This is independent of the other scans.
 */

void hd_scan_net(hd_data_t *hd_data)
{
  unsigned u;
  int if_type;
  hd_t *hd, *hd_card;
  char *s, *hw_addr;
  hd_res_t *res, *res1;
  uint64_t ul0;

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
    hd_card = NULL;

    ADD2LOG(
      "  net interface: name = %s, classname = %s, path = %s\n",
      sf_cdev->name,
      sf_cdev->classname,
      hd_sysfs_id(sf_cdev->path)
    );

    if_type = -1;
    if(hd_attr_uint(sysfs_get_classdev_attr(sf_cdev, "type"), &ul0, 0)) {
      if_type = ul0;
      ADD2LOG("    type = %d\n", if_type);
    }

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
    hd->sub_class.id = sc_nif_other;

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

      hd_card = hd_find_sysfs_id(hd_data, hd_sysfs_id(sf_dev->path));
      if(hd_card) {
        hd->attached_to = hd_card->idx;

        /* for cards with strange pci classes */
        hd_set_hw_class(hd_card, hw_network_ctrl);

        /* add hw addr to network card */
        if(res1) {
          u = 0;
          for(res = hd_card->res; res; res = res->next) {
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
            add_res_entry(&hd_card->res, res);
          }
        }
        /* add interface names */
        add_if_name(hd_card, hd);
      }
    }

#if 0
    "ctc"	sc_nif_ctc
    "iucv"	sc_nif_iucv
    "hsi"	sc_nif_hsi
    "qeth"	sc_nif_qeth
    "escon"	sc_nif_escon
    "myri"	sc_nif_myrinet
    "wlan"	sc_nif_wlan
    "xp"	sc_nif_xp
    "usb"	sc_nif_usb
#endif
    switch(if_type) {
      case ARPHRD_ETHER:	/* eth */
        hd->sub_class.id = sc_nif_ethernet;
        break;
      case ARPHRD_LOOPBACK:	/* lo */
        hd->sub_class.id = sc_nif_loopback;
        break;
      case ARPHRD_SIT:		/* sit */
        hd->sub_class.id = sc_nif_sit;
        break;
      case ARPHRD_FDDI:		/* fddi */
        hd->sub_class.id = sc_nif_fddi;
        break;
      case ARPHRD_IEEE802_TR:	/* tr */
        hd->sub_class.id = sc_nif_tokenring;
        break;
#if 0
      case ARPHRD_IEEE802:	/* fc */
        hd->sub_class.id = sc_nif_fc;
        break;
#endif
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
    else if(sscanf(hd->unix_dev_name, "usb%u", &u) == 1) {
      hd->sub_class.id = sc_nif_usb;
      hd->slot = u;
    }
    /* ##### add more interface names here */

    hd->bus.id = bus_none;

    /* fix card type */
    if(hd_card) {
      if(
        (hd_card->base_class.id == 0 && hd_card->sub_class.id == 0) ||
        (hd_card->base_class.id == bc_network && hd_card->sub_class.id == 0x80)
      ) {
        switch(hd->sub_class.id) {
          case sc_nif_ethernet:
            hd_card->base_class.id = bc_network;
            hd_card->sub_class.id = 0;
            break;

          case sc_nif_usb:
            hd_card->base_class.id = bc_network;
            hd_card->sub_class.id = 0x91;
            break;
        }
      }
    }
  }

  sysfs_close_class(sf_class);

  if(hd_is_sgi_altix(hd_data)) add_xpnet(hd_data);
  if(hd_is_iseries(hd_data)) add_iseries(hd_data);
  add_uml(hd_data);
  add_xen(hd_data);
  add_kma(hd_data);
  add_mv(hd_data);

  /* add link status info */
  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface
    ) {
      get_linkstate(hd_data, hd);

      if(!(hd_card = hd_get_device_by_idx(hd_data, hd->attached_to))) continue;

      for(res = hd->res; res; res = res->next) {
        if(res->any.type == res_link) break;
      }

      if(res) {
        for(res1 = hd_card->res; res1; res1 = res1->next) {
          if(res1->any.type == res_link) break;
        }
        if(res && !res1) {
          res1 = new_mem(sizeof *res1);
          res1->link.type = res_link;
          res1->link.state = res->link.state;
          add_res_entry(&hd_card->res, res1);
        }
      }
    }
  }
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
    ADD2LOG("    GDRVINFO ethtool error: %s\n", strerror(errno));
  }

  close(fd);
}


/*
 * Check network link status.
 */
void get_linkstate(hd_data_t *hd_data, hd_t *hd)
{
  int fd;
  struct ethtool_value linkstatus = { cmd:ETHTOOL_GLINK };
  struct ifreq ifr;
  hd_res_t *res;

  if(!hd->unix_dev_name) return;

  if(strlen(hd->unix_dev_name) > sizeof ifr.ifr_name - 1) return;

  if((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) return;

  /* get driver info */
  memset(&ifr, 0, sizeof ifr);
  strcpy(ifr.ifr_name, hd->unix_dev_name);
  ifr.ifr_data = (caddr_t) &linkstatus;
  if(ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
    ADD2LOG("  %s: ethtool link state: %d\n", hd->unix_dev_name, linkstatus.data);
    res = new_mem(sizeof *res);
    res->link.type = res_link;
    res->link.state = linkstatus.data ? 1 : 0;
    add_res_entry(&hd->res, res);
  }
  else {
    ADD2LOG("  %s: GLINK ethtool error: %s\n", hd->unix_dev_name, strerror(errno));
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

      add_if_name(hd_card, hd);

      break;
    }
  }
}


/*
 * Marvell Gigabit Ethernet thing
 */
void add_mv(hd_data_t *hd_data)
{
  hd_t *hd, *hd_card;
  hd_res_t *res, *res2;
  struct stat sbuf;

  /*
   * Actually there are two (.0 & .1), but only one seems to be used - so we
   * don't care.
   */
  if(stat("/sys/devices/platform/mv643xx_eth.0", &sbuf) || !S_ISDIR(sbuf.st_mode)) return;

  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->vendor.id == MAKE_ID(TAG_PCI, 0x11ab) &&
      hd->device.id == MAKE_ID(TAG_PCI, 0x6460)
    ) break;
  }

  hd_card = add_hd_entry(hd_data, __LINE__, 0);
  hd_card->base_class.id = bc_network;
  hd_card->sub_class.id = 0;

  hd_card->sysfs_id = new_str("/devices/platform/mv643xx_eth.0");
  hd_card->sysfs_bus_id = new_str("mv643xx_eth.0");

  if(hd) {
    hd_card->attached_to = hd->idx;
    hd_card->vendor.id = hd->vendor.id;
    hd_card->device.id = hd->device.id;
  }
  else {
    hd_card->vendor.name = new_str("Marvell");
    hd_card->device.name = new_str("Gigabit Ethernet");
  }

  if(hd_module_is_active(hd_data, "mv643xx_eth")) {
    add_str_list(&hd_card->drivers, "mv643xx_eth");
  }

  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface &&
      hd->sub_class.id == sc_nif_ethernet &&
      search_str_list(hd->drivers, "mv643xx_eth")
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

      add_if_name(hd_card, hd);

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
  unsigned i, cardmask = 0, card_cnt = 0;
  str_list_t *sl0, *sl;

  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface &&
      (
        search_str_list(hd->drivers, "veth") ||
        search_str_list(hd->drivers, "iseries_veth")
      )
    ) {
      hd_card = add_hd_entry(hd_data, __LINE__, 0);
      hd_card->base_class.id = bc_network;
      hd_card->sub_class.id = 0x00;
      hd_card->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);	// IBM
      hd_card->device.id = MAKE_ID(TAG_SPECIAL, 0x1000);
      add_str_list(&hd_card->drivers, "iseries_veth");
      hd_card->slot = card_cnt++;
      str_printf(&hd_card->device.name, 0, "Virtual Ethernet card");
      hd->attached_to = hd_card->idx;

      for(res = hd->res; res; res = res->next) {
        if(res->any.type == res_hwaddr) break;
      }

      if(res) {
        unsigned int slotno;

        res2 = new_mem(sizeof *res2);
        res2->hwaddr.type = res_hwaddr;
        res2->hwaddr.addr = new_str(res->hwaddr.addr);
        add_res_entry(&hd_card->res, res2);
        if (sscanf(res->hwaddr.addr, "02:01:ff:%x:ff:", &slotno)) {
          hd_card->slot = slotno;
          str_printf(&hd_card->device.name, 0, "Virtual Ethernet card %d", hd_card->slot);
	}
      }

      add_if_name(hd_card, hd);
    }
  }

  if(!card_cnt) {
    sl0 = read_file("/proc/iSeries/config", 0, 0);
    for(sl = sl0; sl; sl = sl->next) {
      if(sscanf(sl->str, "AVAILABLE_VETH=%x", &cardmask) == 1)
     	 break;
    }
    free_str_list(sl0);

    for (i = 0; i < 16; i++) {
      if ((0x8000 >> i) & cardmask) {
	hd_card = add_hd_entry(hd_data, __LINE__, 0);
	hd_card->base_class.id = bc_network;
	hd_card->sub_class.id = 0x00;
	hd_card->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);	// IBM
	hd_card->device.id = MAKE_ID(TAG_SPECIAL, 0x1000);
	hd_card->slot = i;
	str_printf(&hd_card->device.name, 0, "Virtual Ethernet card %d", i);
      }
    }
  }
}


/*
 * UML veth devices.
 */
void add_uml(hd_data_t *hd_data)
{
  hd_t *hd, *hd_card;
  hd_res_t *res, *res2;
  unsigned card_cnt = 0;

  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface &&
      search_str_list(hd->drivers, "uml virtual ethernet")
    ) {
      hd_card = add_hd_entry(hd_data, __LINE__, 0);
      hd_card->base_class.id = bc_network;
      hd_card->sub_class.id = 0x00;
      hd_card->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6010);	// UML
      hd_card->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
      hd_card->slot = card_cnt++;
      str_printf(&hd_card->device.name, 0, "Virtual Ethernet card %d", hd_card->slot);

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

      add_if_name(hd_card, hd);
    }
  }
}


/*
 * KMA veth devices.
 */
void add_kma(hd_data_t *hd_data)
{
  hd_t *hd, *hd_card;
  hd_res_t *res, *res2;
  unsigned card_cnt = 0;

  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface &&
      search_str_list(hd->drivers, "kveth2")
    ) {
      hd_card = add_hd_entry(hd_data, __LINE__, 0);
      hd_card->base_class.id = bc_network;
      hd_card->sub_class.id = 0x00;
      hd_card->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6012);	// VirtualIron
      hd_card->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
      hd_card->slot = card_cnt++;
      str_printf(&hd_card->device.name, 0, "Ethernet card %d", hd_card->slot);

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

      add_if_name(hd_card, hd);
    }
  }
}


/*
 * XEN veth devices.
 */
void add_xen(hd_data_t *hd_data)
{
  hd_t *hd, *hd_card;
  hd_res_t *res, *res2;
  unsigned card_cnt = 0;
  char *s = NULL;
  struct stat sbuf;

  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface
    ) {
      str_printf(&s, 0, "/proc/xen/net/%s", hd->unix_dev_name);
      if(stat(s, &sbuf) || !S_ISDIR(sbuf.st_mode)) continue;

      hd_card = add_hd_entry(hd_data, __LINE__, 0);
      hd_card->base_class.id = bc_network;
      hd_card->sub_class.id = 0x00;
      hd_card->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6011);	// Xen
      hd_card->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
      hd_card->slot = card_cnt++;
      str_printf(&hd_card->device.name, 0, "Virtual Ethernet card %d", hd_card->slot);

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

      add_if_name(hd_card, hd);
    }
  }

  free_mem(s);
}


/*
 * add interface name to card
 */
void add_if_name(hd_t *hd_card, hd_t *hd)
{
  if(hd->unix_dev_name) {
    if(!search_str_list(hd_card->unix_dev_names, hd->unix_dev_name)) {
      add_str_list(&hd_card->unix_dev_names, hd->unix_dev_name);
    }
    if(!hd_card->unix_dev_name) {
      hd_card->unix_dev_name = new_str(hd->unix_dev_name);
    }
  }
}


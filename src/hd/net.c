#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

/**
 * @defgroup NETint Network devices
 * @ingroup libhdDEVint
 * @brief Network device scan functions
 *
 * Gather network interface info
 *
 * @{
 */

static void get_ethtool_priv(hd_data_t *hd_data, hd_t *hd);
static void get_driverinfo(hd_data_t *hd_data, hd_t *hd);
static void get_linkstate(hd_data_t *hd_data, hd_t *hd);
static hd_res_t *get_phwaddr(hd_data_t *hd_data, hd_t *hd);
static void add_xpnet(hd_data_t *hdata);
static void add_uml(hd_data_t *hdata);
static void add_kma(hd_data_t *hdata);
static void add_if_name(hd_t *hd_card, hd_t *hd);

/*
 * This is independent of the other scans.
 */

void hd_scan_net(hd_data_t *hd_data)
{
  unsigned u;
  int if_type, if_carrier;
  hd_t *hd, *hd_card;
  char *s, *t, *hw_addr;
  hd_res_t *res, *res_hw, *res_phw, *res_lnk;
  uint64_t ul0;
  str_list_t *sf_class, *sf_class_e;
  char *sf_cdev = NULL, *sf_dev = NULL;
  char *sf_drv_name, *sf_drv;

  if(!hd_probe_feature(hd_data, pr_net)) return;

  hd_data->module = mod_net;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->net = free_str_list(hd_data->net);

  PROGRESS(1, 0, "get network data");

  sf_class = read_dir("/sys/class/net", 'l');
  if(!sf_class) sf_class = read_dir("/sys/class/net", 'd');

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: net\n");
    return;
  }

  for(sf_class_e = sf_class; sf_class_e; sf_class_e = sf_class_e->next) {
    str_printf(&sf_cdev, 0, "/sys/class/net/%s", sf_class_e->str);

    hd_card = NULL;

    ADD2LOG(
      "  net interface: name = %s, path = %s\n",
      sf_class_e->str,
      hd_sysfs_id(sf_cdev)
    );

    if_type = -1;
    if(hd_attr_uint(get_sysfs_attr_by_path(sf_cdev, "type"), &ul0, 0)) {
      if_type = ul0;
      ADD2LOG("    type = %d\n", if_type);
    }

    if_carrier = -1;
    if(hd_attr_uint(get_sysfs_attr_by_path(sf_cdev, "carrier"), &ul0, 0)) {
      if_carrier = ul0;
      ADD2LOG("    carrier = %d\n", if_carrier);
    }

    hw_addr = NULL;
    if((s = get_sysfs_attr_by_path(sf_cdev, "address"))) {
      hw_addr = canon_str(s, strlen(s));
      ADD2LOG("    hw_addr = %s\n", hw_addr);
    }

    sf_dev = new_str(hd_read_sysfs_link(sf_cdev, "device"));
    if(sf_dev) {
      ADD2LOG("    net device: path = %s\n", hd_sysfs_id(sf_dev));
    }

    sf_drv_name = NULL;
    sf_drv = hd_read_sysfs_link(sf_dev, "driver");
    if(sf_drv) {
      sf_drv_name = strrchr(sf_drv, '/');
      if(sf_drv_name) sf_drv_name++;
      ADD2LOG(
        "    net driver: name = %s, path = %s\n",
        sf_drv_name,
        hd_sysfs_id(sf_drv)
      );
    }

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_network_interface;
    hd->sub_class.id = sc_nif_other;

    hd->unix_dev_name = new_str(sf_class_e->str);
    hd->sysfs_id = new_str(hd_sysfs_id(sf_cdev));

    res_hw = NULL;
    if(hw_addr && strspn(hw_addr, "0:") != strlen(hw_addr)) {
      res_hw = new_mem(sizeof *res_hw);
      res_hw->hwaddr.type = res_hwaddr;
      res_hw->hwaddr.addr = new_str(hw_addr);
      add_res_entry(&hd->res, res_hw);
    }

    res_phw = get_phwaddr(hd_data, hd);

    if(if_carrier >= 0) {
      res = new_mem(sizeof *res);
      res->link.type = res_link;
      res->link.state = if_carrier ? 1 : 0;
      add_res_entry(&hd->res, res);
    }

    if(sf_drv_name) {
      add_str_list(&hd->drivers, sf_drv_name);
    }
    else if(hd->res) {
      get_driverinfo(hd_data, hd);
    }

    get_ethtool_priv(hd_data, hd);

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
      default:
        hd->sub_class.id = sc_nif_other;
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
    else {
      for(s = hd->unix_dev_name; *s; s++) if(isdigit(*s)) break;
      if(*s && (u = strtoul(s, &s, 10), !*s)) {
        hd->slot = u;
      }
    }

    hd->bus.id = bus_none;

    hd_card = NULL;

    if(sf_dev) {
      s = new_str(hd_sysfs_id(sf_dev));

      hd->sysfs_device_link = new_str(s);

      hd_card = hd_find_sysfs_id(hd_data, s);

      // try one above, if not found
      if(!hd_card) {
        t = strrchr(s, '/');
        if(t) {
          *t = 0;
          hd_card = hd_find_sysfs_id(hd_data, s);
        }
      }

      /* if one card has several interfaces (as with PS3), check interface names, too */
      if(
        hd_card &&
        hd_card->unix_dev_name &&
        hd->unix_dev_name &&
        strcmp(hd->unix_dev_name, hd_card->unix_dev_name)
      ) {
        hd_card = hd_find_sysfs_id_devname(hd_data, s, hd->unix_dev_name);
      }

      s = free_mem(s);

      if(hd_card) {
        hd->attached_to = hd_card->idx;

        /* for cards with strange pci classes */
        hd_set_hw_class(hd_card, hw_network_ctrl);

        /* add hw addr to network card */
        if(res_hw) {
          u = 0;
          for(res = hd_card->res; res; res = res->next) {
            if(
              res->any.type == res_hwaddr &&
              !strcmp(res->hwaddr.addr, res_hw->hwaddr.addr)
            ) {
              u = 1;
              break;
            }
          }
          if(!u) {
            res = new_mem(sizeof *res);
            res->hwaddr.type = res_hwaddr;
            res->hwaddr.addr = new_str(res_hw->hwaddr.addr);
            add_res_entry(&hd_card->res, res);
          }
        }

        /* add permanent hw addr to network card */
        if(res_phw) {
          u = 0;
          for(res = hd_card->res; res; res = res->next) {
            if(
              res->any.type == res_phwaddr &&
              !strcmp(res->hwaddr.addr, res_phw->hwaddr.addr)
            ) {
              u = 1;
              break;
            }
          }
          if(!u) {
            res = new_mem(sizeof *res);
            res->hwaddr.type = res_phwaddr;
            res->hwaddr.addr = new_str(res_phw->hwaddr.addr);
            add_res_entry(&hd_card->res, res);
          }
        }

        /*
         * add interface names...
         * but not wmasterX (bnc #441778)
         */
        if(if_type != 801) add_if_name(hd_card, hd);
      }
    }

    if(!hd_card && hw_addr) {
      /* try to find card based on hwaddr (for prom-based cards) */

      for(hd_card = hd_data->hd; hd_card; hd_card = hd_card->next) {
        if(
          hd_card->base_class.id != bc_network ||
          hd_card->sub_class.id != 0
        ) continue;
        for(res = hd_card->res; res; res = res->next) {
          if(
            res->any.type == res_hwaddr &&
            !strcmp(hw_addr, res->hwaddr.addr)
          ) break;
        }
        if(res) {
          hd->attached_to = hd_card->idx;
          break;
        }
      }
    }

    hw_addr = free_mem(hw_addr);

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

    sf_dev = free_mem(sf_dev);
  }

  sf_cdev = free_mem(sf_cdev);
  sf_class = free_str_list(sf_class);

  if(hd_is_sgi_altix(hd_data)) add_xpnet(hd_data);
  add_uml(hd_data);
  add_kma(hd_data);

  /* add link status info & dump eeprom */
  for(hd = hd_data->hd ; hd; hd = hd->next) {
    if(
      hd->module == hd_data->module &&
      hd->base_class.id == bc_network_interface
    ) {
      char *buf = NULL;
      str_list_t *sl0, *sl;

      if(hd_probe_feature(hd_data, pr_net_eeprom) && hd->unix_dev_name) {
        PROGRESS(2, 0, "eeprom dump");

        str_printf(&buf, 0, "|/usr/sbin/ethtool -e %s 2>/dev/null", hd->unix_dev_name);
        if((sl0 = read_file(buf, 0, 0))) {
          ADD2LOG("----- %s %s -----\n", hd->unix_dev_name, "EEPROM dump");
          for(sl = sl0; sl; sl = sl->next) {
            ADD2LOG("%s", sl->str);
          }
          ADD2LOG("----- %s end -----\n", "EEPROM dump");
          free_str_list(sl0);
        }
        free(buf);
      }

      for(res = hd->res; res; res = res->next) {
        if(res->any.type == res_link) break;
      }

      if(!res) get_linkstate(hd_data, hd);

      if(!(hd_card = hd_get_device_by_idx(hd_data, hd->attached_to))) continue;

      for(res = hd->res; res; res = res->next) {
        if(res->any.type == res_link) break;
      }

      if(res) {
        for(res_lnk = hd_card->res; res_lnk; res_lnk = res_lnk->next) {
          if(res_lnk->any.type == res_link) break;
        }
        if(res && !res_lnk) {
          res_lnk = new_mem(sizeof *res_lnk);
          res_lnk->link.type = res_link;
          res_lnk->link.state = res->link.state;
          add_res_entry(&hd_card->res, res_lnk);
        }
      }

      hd_card->is.fcoe_offload = hd->is.fcoe_offload;
      hd_card->is.iscsi_offload = hd->is.iscsi_offload;
      hd_card->is.storage_only = hd->is.storage_only;
    }
  }
}


/*
 * Get private flags via ethtool.
 */
void get_ethtool_priv(hd_data_t *hd_data, hd_t *hd)
{
  int fd, err = 0;
  unsigned u, len = 0;
  struct ifreq ifr = {};
  struct {
    struct ethtool_sset_info hdr;
    uint32_t buf[1];
  } sset_info = { hdr:{ cmd:ETHTOOL_GSSET_INFO, sset_mask:1ULL << ETH_SS_PRIV_FLAGS } };
  struct ethtool_gstrings *strings = NULL;
  struct ethtool_value flags = { cmd:ETHTOOL_GPFLAGS };

  if(!hd->unix_dev_name) return;

  if(strlen(hd->unix_dev_name) > sizeof ifr.ifr_name - 1) return;
  strcpy(ifr.ifr_name, hd->unix_dev_name);

  if((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) return;

  ifr.ifr_data = &sset_info;
  if(ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
    len = sset_info.hdr.sset_mask ? sset_info.hdr.data[0] : 0;
    ADD2LOG("    ethtool private flags: %u\n", len);
  }
  else {
    ADD2LOG("    GSSET_INFO ethtool error: %s\n", strerror(errno));
    err = 1;
  }

  if(len) strings = calloc(1, sizeof *strings + len * ETH_GSTRING_LEN);

  if(!strings) err = 1;

  if(!err) {
    strings->cmd = ETHTOOL_GSTRINGS;
    strings->string_set = ETH_SS_PRIV_FLAGS;
    strings->len = len;

    ifr.ifr_data = strings;
    if(ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
      for(u = 0; u < len; u++) strings->data[(u + 1) * ETH_GSTRING_LEN - 1] = 0;
    }
    else {
      ADD2LOG("    GSTRINGS ethtool error: %s\n", strerror(errno));
      err = 1;
    }
  }

  if(len > 32) len = 32;

  if(!err) {
    ifr.ifr_data = &flags;
    if(ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
      for(u = 0; u < len; u++) {
        char *key = strings->data + u * ETH_GSTRING_LEN;
        unsigned val = (flags.data >> u) & 1;
        ADD2LOG("    %s = %u\n", key, val);
        // add 1 to get tri-state flags: 0 = unset, 1 = false, 2 = true
        if(!strcmp(key, "FCoE offload support")) hd->is.fcoe_offload = val + 1;
        if(!strcmp(key, "iSCSI offload support")) hd->is.iscsi_offload = val + 1;
        if(!strcmp(key, "Storage only interface")) hd->is.storage_only = val + 1;
      }
    }
    else {
      ADD2LOG("    GPFLAGS ethtool error: %s\n", strerror(errno));
      err = 1;
    }
  }

  free(strings);

  close(fd);
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
 * Get permanent hardware address (it's not in sysfs).
 */
hd_res_t *get_phwaddr(hd_data_t *hd_data, hd_t *hd)
{
  int fd;
  struct ethtool_perm_addr *phwaddr = new_mem(sizeof (struct ethtool_perm_addr) + MAX_ADDR_LEN);
  struct ifreq ifr;
  hd_res_t *res = NULL;

  phwaddr->cmd = ETHTOOL_GPERMADDR;
  phwaddr->size = MAX_ADDR_LEN;

  if(!hd->unix_dev_name) return res;

  if(strlen(hd->unix_dev_name) > sizeof ifr.ifr_name - 1) return res;

  if((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) return res;

  /* get permanent hardware addr */
  memset(&ifr, 0, sizeof ifr);
  strcpy(ifr.ifr_name, hd->unix_dev_name);
  ifr.ifr_data = (caddr_t) phwaddr;
  if(ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
    int i;
    char *addr = NULL;
    if(phwaddr->size > 0) {
      addr = new_mem(phwaddr->size * 3 + 1);	// yes, we need an extra byte
      for(i = 0; i < phwaddr->size; i++) {
        sprintf(addr + 3 * i, "%02x:", phwaddr->data[i]);
      }
      addr[3 * i - 1] = 0;
    }

    ADD2LOG("  %s: ethtool permanent hw address[%d]: %s\n", hd->unix_dev_name, phwaddr->size, addr ?: "");

    if(addr && strspn(addr, "0:") != strlen(addr)) {
      res = new_mem(sizeof *res);
      res->hwaddr.type = res_phwaddr;
      res->hwaddr.addr = new_str(addr);
      add_res_entry(&hd->res, res);
    }

    free_mem(addr);
  }
  else {
    ADD2LOG("  %s: GLINK ethtool error: %s\n", hd->unix_dev_name, strerror(errno));
  }

  close(fd);

  return res;
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
 * add interface name to card
 */
void add_if_name(hd_t *hd_card, hd_t *hd)
{
  str_list_t *sl0;

  if(hd->unix_dev_name) {
    if(!search_str_list(hd_card->unix_dev_names, hd->unix_dev_name)) {
      if(hd->sub_class.id == sc_nif_other) {
        /* add at end */
        add_str_list(&hd_card->unix_dev_names, hd->unix_dev_name);
      }
      else {
        /* add at top */
        sl0 = new_mem(sizeof *sl0);
        sl0->next = hd_card->unix_dev_names;
        sl0->str = new_str(hd->unix_dev_name);
        hd_card->unix_dev_names = sl0;
      }
      free_mem(hd_card->unix_dev_name);
      hd_card->unix_dev_name = new_str(hd_card->unix_dev_names->str);
    }
  }
}

/** @} */


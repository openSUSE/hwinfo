#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include "hd.h"
#include "hd_int.h"
#include "pcmcia.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * pcmcia via sysfs
 *
 *
 * expects pci devs to be probed already to assign bridge
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void pcmcia_read_data(hd_data_t *hd_data);
static void pcmcia_ctrl_read_data(hd_data_t *hd_data);


void hd_scan_pcmcia(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_pcmcia)) return;

  hd_data->module = mod_pcmcia;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "sysfs drivers");
    
  hd_sysfs_driver_list(hd_data);

  PROGRESS(2, 0, "pcmcia");

  pcmcia_read_data(hd_data);

  PROGRESS(3, 0, "pcmcia ctrl");

  pcmcia_ctrl_read_data(hd_data);

}



void pcmcia_read_data(hd_data_t *hd_data)
{
  hd_t *hd, *hd2;
  unsigned u0, u1, func_id;
  uint64_t ul0;
  char *s, *t;
  char *prod1, *prod2, *prod3, *prod4;
  str_list_t *sl;

  struct sysfs_bus *sf_bus;
  struct dlist *sf_dev_list;
  struct sysfs_device *sf_dev;

  sf_bus = sysfs_open_bus("pcmcia");

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: pcmcia\n");
    return;
  }

  sf_dev_list = sysfs_get_bus_devices(sf_bus);
  if(sf_dev_list) dlist_for_each_data(sf_dev_list, sf_dev, struct sysfs_device) {
    ADD2LOG(
      "  pcmcia device: name = %s, bus_id = %s, bus = %s\n    path = %s\n",
      sf_dev->name,  
      sf_dev->bus_id,
      sf_dev->bus,
      hd_sysfs_id(sf_dev->path)
    );

    if(sscanf(sf_dev->bus_id, "%x.%x", &u0, &u1) != 2) continue;

    hd = add_hd_entry(hd_data, __LINE__, 0);

    hd->sysfs_id = new_str(hd_sysfs_id(sf_dev->path));
    hd->sysfs_bus_id = new_str(sf_dev->bus_id);
    hd->bus.id = bus_pcmcia;

    hd->slot = u0;
    hd->func = u1;
    hd->hotplug_slot = u0 + 1;
    hd->hotplug = hp_pcmcia;

    s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
    if(s) add_str_list(&hd->drivers, s);

    if((s = hd_attr_str(sysfs_get_device_attr(sf_dev, "modalias")))) {
      hd->modalias = canon_str(s, strlen(s));
      ADD2LOG("    modalias = \"%s\"\n", s);
    }

    if(hd_attr_uint(sysfs_get_device_attr(sf_dev, "manf_id"), &ul0, 0)) {
      ADD2LOG("    manf_id = 0x%04x\n", (unsigned) ul0);
      hd->vendor.id = MAKE_ID(TAG_PCMCIA, ul0);
    }

    if(hd_attr_uint(sysfs_get_device_attr(sf_dev, "card_id"), &ul0, 0)) {
      ADD2LOG("    card_id = 0x%04x\n", (unsigned) ul0);
      hd->device.id = MAKE_ID(TAG_PCMCIA, ul0);
    }

    /*
     * "multifunction", "memory", "serial", "parallel",
     * "fixed disk", "video", "network", "AIMS",
     * "SCSI"
     */
    func_id = 0;
    if(hd_attr_uint(sysfs_get_device_attr(sf_dev, "func_id"), &ul0, 0)) {
      func_id = ul0;
      ADD2LOG("    func_id = 0x%04x\n", func_id);
    }

    prod1 = NULL;
    if((s = hd_attr_str(sysfs_get_device_attr(sf_dev, "prod_id1")))) {
      prod1 = canon_str(s, strlen(s));
      ADD2LOG("    prod_id1 = \"%s\"\n", prod1);
    }

    prod2 = NULL;
    if((s = hd_attr_str(sysfs_get_device_attr(sf_dev, "prod_id2")))) {
      prod2 = canon_str(s, strlen(s));
      ADD2LOG("    prod_id2 = \"%s\"\n", prod2);
    }

    prod3 = NULL;
    if((s = hd_attr_str(sysfs_get_device_attr(sf_dev, "prod_id3")))) {
      prod3 = canon_str(s, strlen(s));
      ADD2LOG("    prod_id3 = \"%s\"\n", prod3);
    }

    prod4 = NULL;
    if((s = hd_attr_str(sysfs_get_device_attr(sf_dev, "prod_id4")))) {
      prod4 = canon_str(s, strlen(s));
      ADD2LOG("    prod_id4 = \"%s\"\n", prod4);
    }

    if(func_id == 6) {
      hd->base_class.id = bc_network;
      hd->sub_class.id = 0x80;		/* other */
    }

    if(prod1 && *prod1) {
      add_str_list(&hd->extra_info, prod1);
      hd->vendor.name = prod1;
      prod1 = NULL;
    }
    if(prod2 && *prod2) {
      add_str_list(&hd->extra_info, prod2);
      hd->device.name = prod2;
      prod2 = NULL;
    }
    if(prod3 && *prod3) add_str_list(&hd->extra_info, prod3);
    if(prod4 && *prod4) add_str_list(&hd->extra_info, prod4);

    for(sl = hd->extra_info; sl ; sl = sl->next) {
      if(strstr(sl->str, "Ethernet")) hd->sub_class.id = 0;	/* ethernet */
      if(
        !hd->revision.name &&
        !sl->next &&
        (
          !strncasecmp(sl->str, "rev.", sizeof "rev." - 1) ||
          (
            (sl->str[0] == 'V' || sl->str[0] == 'v') &&
            (sl->str[1] >= '0' && sl->str[1] <= '9')
          )
        )
      ) {
        hd->revision.name = new_str(sl->str);
      }
    }

    prod1 = free_mem(prod1);
    prod2 = free_mem(prod2);
    prod3 = free_mem(prod3);
    prod4 = free_mem(prod4);


    s = new_str(hd->sysfs_id);

    if((t = strrchr(s, '/'))) {
      *t = 0;
      if((hd2 = hd_find_sysfs_id(hd_data, s))) {
        hd->attached_to = hd2->idx;
      }
    }
    free_mem(s);

  }

  sysfs_close_bus(sf_bus);
}


void pcmcia_ctrl_read_data(hd_data_t *hd_data)
{
  hd_t *hd, *bridge_hd;
  char *s;
  unsigned u;
  unsigned sockets[16 /* just large enough */] = { };

  struct sysfs_class *sf_class;
  struct sysfs_class_device *sf_cdev;
  struct sysfs_device *sf_dev;
  struct dlist *sf_cdev_list;

  sf_class = sysfs_open_class("pcmcia_socket");

  if(!sf_class) {
    ADD2LOG("sysfs: no such class: pcmcia_socket\n");
  }
  else {
    sf_cdev_list = sysfs_get_class_devices(sf_class);

    if(sf_cdev_list) dlist_for_each_data(sf_cdev_list, sf_cdev, struct sysfs_class_device) {
      if(
        sscanf(sf_cdev->name, "pcmcia_socket%u", &u) == 1 &&
        (sf_dev = sysfs_get_classdev_device(sf_cdev))
      ) {
        s = hd_sysfs_id(sf_dev->path);
        hd = hd_find_sysfs_id(hd_data, s);
        if(hd && u < sizeof sockets / sizeof *sockets) sockets[u] = hd->idx;

        ADD2LOG("  pcmcia socket %u: %s\n", u, s);
      }
    }

    sysfs_close_class(sf_class);
  }


  /* find card bus devices & assign them socket numbers */

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->bus.id != bus_pci) continue;
    if((bridge_hd = hd_get_device_by_idx(hd_data, hd->attached_to))) {
      if(
        bridge_hd->base_class.id == bc_bridge &&
        bridge_hd->sub_class.id == sc_bridge_cardbus
      ) {
        hd->hotplug = hp_cardbus;
      }
     else if(
        bridge_hd->base_class.id == bc_bridge &&
        bridge_hd->sub_class.id == sc_bridge_pcmcia
      ) {
        hd->hotplug = hp_pcmcia;
      }

      for(u = 0; u < sizeof sockets / sizeof *sockets; u++) {
        if(sockets[u] == bridge_hd->idx) {
          hd->hotplug_slot = u + 1;
        }
      }
    }
  }
}



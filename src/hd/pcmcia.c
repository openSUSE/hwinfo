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
#include <linux/hdreg.h>

#include "hd.h"
#include "hd_int.h"
#include "pcmcia.h"

static void read_cardinfo(hd_data_t *hd_data);
static void assign_bridges(hd_data_t *hd_data);
static void add_sysfs_stuff(hd_data_t *hd_data, hd_t *hd);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * PCMCIA info via cardctl
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


void hd_scan_pcmcia(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_pcmcia)) return;

  hd_data->module = mod_pcmcia;

  /* some clean-up */
  remove_hd_entries(hd_data);

  read_cardinfo(hd_data);

  assign_bridges(hd_data);

}


void read_cardinfo(hd_data_t *hd_data)
{
  str_list_t *sl, *sl0, *sl1;
  int i0, i1, pcmcia_sock, manf_id0, manf_id1, func, prod_info;
  char buf0[256], buf1[256], buf2[256], buf3[256];
  hd_t *hd;
  unsigned cardbus = 0;		/* bitmask: cardbus vs. pc-card */

  sl0 = read_file("| /sbin/cardctl status 2>/dev/null", 0, 0);

  ADD2LOG("-----  cardctl status -----\n");
  for(sl = sl0; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("-----  cardctl status end -----\n");

  for(pcmcia_sock = -1, sl = sl0; sl; sl = sl->next) {
    if(sscanf(sl->str, " Socket %d:", &i0) == 1) {
      pcmcia_sock = i0;
      continue;
    }

    if(strstr(sl->str, " CardBus card")) {
      if(pcmcia_sock >= 0 && pcmcia_sock < 8 * (int) sizeof cardbus) {
        cardbus |= 1 << pcmcia_sock;
      }
      pcmcia_sock = -1;
      continue;
    }
  }

  free_str_list(sl0);

  sl0 = read_file("| /sbin/cardctl ident 2>/dev/null", 0, 0);

  ADD2LOG("-----  cardctl ident -----\n");
  for(sl = sl0; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("-----  cardctl ident end -----\n");

  for(
    pcmcia_sock = manf_id0 = manf_id1 = func = prod_info = -1, sl = sl0;
    sl;
    sl = sl->next
  ) {
    if(sscanf(sl->str, " manfid: %i, %i", &i0, &i1) == 2) {
      manf_id0 = i0;
      manf_id1 = i1;
    }

    if(sscanf(sl->str, " function: %d", &i0) == 1) {
      /*
       * "multifunction", "memory", "serial", "parallel",
       * "fixed disk", "video", "network", "AIMS",
       * "SCSI"
       */
      func = i0;
    }

    if(
      (i0 = sscanf(
        sl->str,
        " product info: \"%255[^\"]\", \"%255[^\"]\", \"%255[^\"]\", \"%255[^\"]\"",
        buf0, buf1, buf2, buf3
      )) >= 1
    ) {
      prod_info = i0;
    }

    if(sscanf(sl->str, " Socket %d:", &i0) == 1) {
      i1 = 1;
    }
    else {
      i1 = 0;
    }

    if(i1 || !sl->next) {
      if(pcmcia_sock >= 0 && (prod_info >= 1 || manf_id0 != -1)) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->bus.id = bus_pcmcia;
        hd->slot = pcmcia_sock;
        hd->hotplug_slot = pcmcia_sock + 1;
        if(manf_id0 != -1 && manf_id1 != -1) {
          hd->vendor.id = MAKE_ID(TAG_PCMCIA, manf_id0);
          hd->device.id = MAKE_ID(TAG_PCMCIA, manf_id1);
        }
        if(pcmcia_sock < 8 * (int) sizeof cardbus && (cardbus & (1 << pcmcia_sock))) {
          hd->hotplug = hp_cardbus;
        }
        else {
          hd->hotplug = hp_pcmcia;
        }

        if(func == 6) {
          hd->base_class.id = bc_network;
          hd->sub_class.id = 0x80;		/* other */
        }
        if(prod_info >= 1) add_str_list(&hd->extra_info, buf0);
        if(prod_info >= 2) add_str_list(&hd->extra_info, buf1);
        if(prod_info >= 3) add_str_list(&hd->extra_info, buf2);
        if(prod_info >= 4) add_str_list(&hd->extra_info, buf3);
        if(prod_info >= 2) {
          hd->vendor.name = new_str(buf0);
          hd->device.name = new_str(buf1);
        }
        for(sl1 = hd->extra_info; sl1 ; sl1 = sl1->next) {
          if(strstr(sl1->str, "Ethernet")) hd->sub_class.id = 0;	/* ethernet */
          if(
            !hd->revision.name &&
            !sl1->next &&
            (
              !strncasecmp(sl1->str, "rev.", sizeof "rev." - 1) ||
              (
                (sl1->str[0] == 'V' || sl1->str[0] == 'v') &&
                (sl1->str[1] >= '0' && sl1->str[1] <= '9')
              )
            )
          ) {
            hd->revision.name = new_str(sl1->str);
          }
        }
      }

      manf_id0 = manf_id1 = func = prod_info = -1;
    }

    if(i1) pcmcia_sock = i0;

  }

  free_str_list(sl0);
}


/*
 * Identify hotpluggable devices.
 */
void assign_bridges(hd_data_t *hd_data)
{
  hd_t *hd, *hd1, *bridge_hd;
  unsigned p_sock[8], p_socks, u = 0;

  for(hd = hd_data->hd; hd; hd = hd->next) {
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
    }
  }

  for(p_socks = 0, hd = hd_data->hd; hd; hd = hd->next) {
    if(
      u < sizeof p_sock / sizeof *p_sock &&
      is_pcmcia_ctrl(hd_data, hd)
    ) {
      p_sock[p_socks++] = hd->idx;
    }
  }

  if(p_socks) {
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        !hd->tag.remove &&
        hd->bus.id == bus_pcmcia &&
        hd->slot < p_socks &&
        p_sock[hd->slot]
      ) {
        for(u = p_sock[hd->slot], hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
          if(hd1->tag.remove) continue;
          if(hd1->status.available == status_no) continue;
          if(hd1->attached_to == u) break;
        }
        if(hd1) {
          hd1->hotplug = hd->hotplug;
          hd1->hotplug_slot = hd->hotplug_slot;
          if(!hd1->extra_info) {
            hd1->extra_info = hd->extra_info;
            hd->extra_info = NULL;
          }
          hd->tag.remove = 1;
        }
        else {
          hd->attached_to = p_sock[hd->slot];
          add_sysfs_stuff(hd_data, hd);
        }
        p_sock[hd->slot] = 0;
      }
    }

    remove_tagged_hd_entries(hd_data);
  }
}


void add_sysfs_stuff(hd_data_t *hd_data, hd_t *hd)
{
  hd_t *hd_par;
  char *s = NULL, *s1;
  struct sysfs_device *sf_dev;

  hd_par = hd_get_device_by_idx(hd_data, hd->attached_to);

  if(!hd_par || !hd_par->sysfs_id || hd->sysfs_id) return;

  str_printf(&s, 0, "/sys%s/%d.0", hd_par->sysfs_id, hd->slot);

  sf_dev = sysfs_open_device_path(s);

  if(sf_dev) {
    hd->sysfs_id = new_str(hd_sysfs_id(s));
    s1 = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
    if(s1) add_str_list(&hd->drivers, s1);
  }

  sysfs_close_device(sf_dev);

  s = free_mem(s);
}


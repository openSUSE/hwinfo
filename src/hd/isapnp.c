#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "isapnp.h"


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * isapnp stuff
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__i386__) || defined(__alpha__)

static void get_pnp_devs(hd_data_t *hd_data);

#if 0
static void get_read_port(hd_data_t *hd_data, isapnp_t *);
static void build_list(hd_data_t *hd_data, str_list_t *isapnp_list);
#endif

void hd_scan_isapnp(hd_data_t *hd_data)
{
#if 0
  hd_t *hd;
  hd_res_t *res;
  int isapnp_ok;
  str_list_t *isapnp_list = NULL, *sl;
#endif
  
  if(!hd_probe_feature(hd_data, pr_isapnp)) return;

  hd_data->module = mod_isapnp;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "pnp devices");

  get_pnp_devs(hd_data);

#if 0
  PROGRESS(1, 0, "read port");

  if(!hd_data->isapnp) {
    hd_data->isapnp = new_mem(sizeof *hd_data->isapnp);
  }
  else {
    hd_data->isapnp->cards = 0;
    /* just in case... */
    hd_data->isapnp->card = free_mem(hd_data->isapnp->card);
    /* keep the port */
  }

  if(!hd_data->isapnp->read_port) get_read_port(hd_data, hd_data->isapnp);

  PROGRESS(3, 0, "get pnp data");

  isapnp_list = read_file(PROC_ISAPNP, 0, 0);

  if((hd_data->debug & HD_DEB_ISAPNP)) {
    ADD2LOG("----- %s -----\n", PROC_ISAPNP);
    for(sl = isapnp_list; sl; sl = sl->next) {
      ADD2LOG("  %s", sl->str);
    }
    ADD2LOG("----- %s end -----\n", PROC_ISAPNP);
  }

  isapnp_ok = isapnp_list && hd_data->isapnp->read_port ? 1 : 1;

  PROGRESS(4, 0, "build list");

  if(isapnp_ok) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_internal;
    hd->sub_class.id = sc_int_isapnp_if;

    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = ISAPNP_ADDR_PORT;
    res->io.range = 1;
    res->io.access = acc_wo;

    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = ISAPNP_DATA_PORT;
    res->io.range = 1;
    res->io.access = acc_wo;

    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = hd_data->isapnp->read_port;
    res->io.range = 1;
    res->io.access = acc_ro;
  }

  build_list(hd_data, isapnp_list);

  free_str_list(isapnp_list);
#endif

}


void get_pnp_devs(hd_data_t *hd_data)
{
  hd_t *hd;
  char *s, *t, buf[4];
  unsigned u1, u2, u3;

  struct sysfs_bus *sf_bus;
  struct dlist *sf_dev_list;
  struct sysfs_device *sf_dev;
  struct sysfs_device *sf_dev_2;

  sf_bus = sysfs_open_bus("pnp");

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: pnp\n");
    return;
  }

  sf_dev_list = sysfs_get_bus_devices(sf_bus);


  if(sf_dev_list) dlist_for_each_data(sf_dev_list, sf_dev, struct sysfs_device) {
    ADD2LOG(
      "  pnp device: name = %s, bus_id = %s, bus = %s\n    path = %s\n",
      sf_dev->name,
      sf_dev->bus_id,
      sf_dev->bus,
      hd_sysfs_id(sf_dev->path)
    );

    if((s = hd_attr_str(sysfs_get_device_attr(sf_dev, "id")))) {
      if(sscanf(s, "%3s%4x", buf, &u1) == 2 && (u2 = name2eisa_id(buf))) {
        ADD2LOG("    id = %s %04x\n", eisa_vendor_str(u2), u1);

        hd = add_hd_entry(hd_data, __LINE__, 0);

        hd->sysfs_id = new_str(hd_sysfs_id(sf_dev->path));
        hd->sysfs_bus_id = new_str(sf_dev->bus_id);

        hd->bus.id = bus_isa;
        hd->is.isapnp = 1;

        hd->sub_vendor.id = u2;
        hd->sub_device.id = MAKE_ID(TAG_EISA, u1);

        if(sscanf(hd->sysfs_bus_id, "%2x:%2x.%2x", &u1, &u2, &u3) == 3) {
          hd->slot = u2;
          hd->func = u3;
        }

        s = new_str(sf_dev->path);
        if((t = strrchr(s, '/'))) *t = 0;

        sf_dev_2 = sysfs_open_device_path(s);
        if(sf_dev_2) {
          if((t = hd_attr_str(sysfs_get_device_attr(sf_dev_2, "card_id")))) {
            if(sscanf(t, "%3s%4x", buf, &u1) == 2 && (u2 = name2eisa_id(buf))) {
              ADD2LOG("    card id = %s %04x\n", eisa_vendor_str(u2), u1);

              hd->vendor.id = u2;
              hd->device.id = MAKE_ID(TAG_EISA, u1);
            }
          }
          if((t = hd_attr_str(sysfs_get_device_attr(sf_dev_2, "name")))) {
             hd->device.name = canon_str(t, strlen(t));
             if(!strcasecmp(hd->device.name, "unknown")) {
               hd->device.name = free_mem(hd->device.name);
             }
          }

          sysfs_close_device(sf_dev_2);
        }


        free_mem(s);


        if(hd->sub_vendor.id == hd->vendor.id && hd->sub_device.id == hd->device.id) {
          hd->sub_vendor.id = hd->sub_device.id = 0;
        }

      }
    }

  }


  sysfs_close_bus(sf_bus);

}


#if 0
unsigned char *add_isapnp_card_res(isapnp_card_t *ic, int len, int type)
{
  ic->res = add_mem(ic->res, sizeof *ic->res, ic->res_len);

  ic->res[ic->res_len].len = len;
  ic->res[ic->res_len].type = type;
  ic->res[ic->res_len].data = new_mem(len);

  if(type == RES_LOG_DEV_ID) {	/* logical device id */
    ic->log_devs++;
  }

  return ic->res[ic->res_len++].data;
}


isapnp_card_t *add_isapnp_card(isapnp_t *ip, int csn)
{
  isapnp_card_t *c;

  ip->card = add_mem(ip->card, sizeof *ip->card, ip->cards);
  c = ip->card + ip->cards++;

  c->csn = csn;
  c->serial = new_mem(sizeof *c->serial * 8);
  c->card_regs = new_mem(sizeof *c->card_regs * 0x30);

  return c;
}


void get_read_port(hd_data_t *hd_data, isapnp_t *p)
{
  hd_res_t *res;

  p->read_port = 0;

  res = NULL;
  gather_resources(hd_data->misc, &res, "ISAPnP", W_IO);
  if(res && res->any.type == res_io) p->read_port = res->io.base;
  free_res_list(res);
}


void build_list(hd_data_t *hd_data, str_list_t *isapnp_list)
{
  hd_t *hd = NULL;
  str_list_t *sl;
  char s1[4], s2[100];
  int card, ldev, cdev_id, ldev_active = 0;
  char *dev_name = NULL, *ldev_name = NULL;
  unsigned dev_id = 0, vend_id = 0, base_class = 0, sub_class = 0, ldev_id;
  unsigned u, ux[5];
  int i, j;
  hd_res_t *res;

  for(sl = isapnp_list; sl; sl = sl->next) {

    if(sscanf(sl->str, "Card %d '%3s%4x:%99[^']", &card, s1, &dev_id, s2) == 4) {
//      ADD2LOG("\n\n** card %d >%s< %04x >%s<**\n", card, s1, dev_id, s2);

      dev_name = free_mem(dev_name);
      if(strcmp(s2, "Unknown")) dev_name = new_str(s2);

      dev_id = MAKE_ID(TAG_EISA, dev_id);
      vend_id = name2eisa_id(s1);

      base_class  = sub_class = 0;
      if((u = device_class(hd_data, vend_id, dev_id))) {
        base_class = u >> 8;
        sub_class = u & 0xff;
      }
      
#if 0
// ########## FIXME
      if(
        (ID_VALUE(vend_id) || ID_VALUE(dev_id)) &&
        !((db_name = hd_device_name(hd_data, vend_id, dev_id)) && *db_name)
      ) {
        if(dev_name) {
          add_device_name(hd_data, vend_id, dev_id, dev_name);
        }
      }
#endif

      continue;
    }

    if(sscanf(sl->str, " Logical device %d '%3s%4x:%99[^']", &ldev, s1, &ldev_id, s2) == 4) {
//      ADD2LOG("\n\n** ldev %d >%s< %04x >%s<**\n", ldev, s1, ldev_id, s2);

      ldev_name = free_mem(ldev_name);
      if(strcmp(s2, "Unknown")) ldev_name = new_str(s2);

      hd = add_hd_entry(hd_data, __LINE__, 0);

      hd->bus.id = bus_isa;
      hd->is.isapnp = 1;
      hd->slot = card;
      hd->func = ldev;

      hd->vendor.id = vend_id;
      hd->device.id = dev_id;

      hd->base_class.id = base_class;
      hd->sub_class.id = sub_class;

      hd->sub_device.id = MAKE_ID(TAG_EISA, ldev_id);
      hd->sub_vendor.id = name2eisa_id(s1);

      if(hd->sub_vendor.id == hd->vendor.id && hd->sub_device.id == hd->device.id) {
        hd->sub_vendor.id = hd->sub_device.id = 0;
      }

      if((u = sub_device_class(hd_data, hd->vendor.id, hd->device.id, hd->sub_vendor.id, hd->sub_device.id))) {
        hd->base_class.id = u >> 8;
        hd->sub_class.id = u & 0xff;
      }

#if 0
# ############# FIXME
      if(
        (ID_VALUE(hd->sub_vendor.id) || ID_VALUE(hd->sub_device.id)) &&
        !hd_sub_device_name(hd_data, hd->vend, hd->dev, hd->sub_vend, hd->sub_device.id)
      ) {
        if(ldev_name) {
          add_sub_device_name(hd_data, hd->vend, hd->dev, hd->sub_vend, hd->sub_device.id, ldev_name);
        }
      }
#endif

      continue;
    }

    if(strstr(sl->str, "Device is not active")) {
      ldev_active = 0;
      continue;
    }

    if(strstr(sl->str, "Device is active")) {
      ldev_active = 1;
      continue;
    }

    if(hd && sscanf(sl->str, " Compatible device %3s%4x", s1, &cdev_id) == 2) {
//      ADD2LOG("\n\n** cdev >%s< %04x **\n", s1, cdev_id);

      hd->compat_device.id = MAKE_ID(TAG_EISA, cdev_id);
      hd->compat_vendor.id = name2eisa_id(s1);

      if(!(hd->base_class.id || hd->sub_class.id)) {
        if((u = device_class(hd_data, hd->compat_vendor.id, hd->compat_device.id))) {
          hd->base_class.id = u >> 8;
          hd->sub_class.id = u & 0xff;
        }
        else if(hd->compat_vendor.id == MAKE_ID(TAG_EISA, 0x41d0)) {
          /* 0x41d0 is 'PNP' */
          switch((hd->compat_device.id >> 12) & 0xf) {
            case   8:
              hd->base_class.id = bc_network;
              hd->sub_class.id = 0x80;
              break;
            case 0xa:
              hd->base_class.id = bc_storage;
              hd->sub_class.id = 0x80;
              break;
            case 0xb:
              hd->base_class.id = bc_multimedia;
              hd->sub_class.id = 0x80;
              break;
            case 0xc:
            case 0xd:
              hd->base_class.id = bc_modem;
              break;
          }
        }
      }

      continue;
    }

    if(
      hd &&
      (j = sscanf(sl->str,
        " Active port %x, %x, %x, %x, %x, %x",
        ux, ux + 1, ux + 2, ux + 3, ux + 4, ux + 5
      )) >= 1
    ) {

      for(i = 0; i < j; i++) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->io.type = res_io;
        res->io.enabled = ldev_active ? 1 : 0;
        res->io.base = ux[i];
        res->io.access = acc_rw;
      }

      continue;
    }

    if(hd && (j = sscanf(sl->str, " Active IRQ %d [%x], %d [%x]", ux, ux + 1, ux + 2, ux + 3)) >= 1) {
      for(i = 0; i < j; i += 2) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->irq.type = res_irq;
        res->irq.enabled = ldev_active ? 1 : 0;
        res->irq.base = ux[i];
      }

      continue;
    }

    if(hd && (j = sscanf(sl->str, " Active DMA %d, %d", ux, ux + 1)) >= 1) {
      for(i = 0; i < j; i++) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->dma.type = res_dma;
        res->dma.enabled = ldev_active ? 1 : 0;
        res->dma.base = ux[i];
      }

      continue;
    }


  }

  free_mem(dev_name);
  free_mem(ldev_name);
}
#endif


#endif /* defined(__i386__) || defined(__alpha__) */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "isdn.h"

#undef ISDN_TEST

/**
 * @defgroup ISDNint ISDN devices
 * @ingroup libhdDEVint
 * @brief ISDN identify functions
 *
 * @{
 */

#ifndef LIBHD_TINY

#if !defined(__s390__) && !defined(__s390x__) && !defined(__alpha__)

void hd_scan_isdn(hd_data_t *hd_data)
{
  hd_t *hd;
  cdb_isdn_card *cic;

  if(!hd_probe_feature(hd_data, pr_isdn)) return;

  hd_data->module = mod_isdn;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "list");

#ifdef ISDN_TEST
  {
    hd_res_t *res;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x3005);
    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0500);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x0300;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor.id = MAKE_ID(TAG_EISA, 0x1593);
    hd->device.id = MAKE_ID(TAG_EISA, 0x0133);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x0240;
    res->io.access = acc_rw;
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->irq.type = res_irq;
    res->irq.enabled = 1;
    res->irq.base = 99;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor.id = MAKE_ID(TAG_EISA, 0x0e98);
    hd->device.id = MAKE_ID(TAG_EISA, 0x0000);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x0180;
    res->io.access = acc_rw;
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x0540;
    res->io.access = acc_rw;
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->irq.type = res_irq;
    res->irq.enabled = 1;
    res->irq.base = 77;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_pci;
    hd->base_class.id = bc_isdn;
    hd->vendor.id = MAKE_ID(TAG_PCI, 0x1244);
    hd->device.id = MAKE_ID(TAG_PCI, 0x0a00);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0xe000;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x3001);
    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0100);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0xe80;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x3000);
    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1a00);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x400;
    res->io.access = acc_rw;


  }
#endif

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if((cic = get_isdn_info(hd))) {
      hd->base_class.id = bc_isdn;
      hd->sub_class.id = 0;
      free_mem(cic);
    }
  }


}

cdb_isdn_card *get_isdn_info(hd_t *hd)
{
  cdb_isdn_card *cic0, *cic;
  unsigned u0, u1;

  if(hd->bus.id == bus_pci ||
    hd->bus.id == bus_isa ||
    hd->bus.id == bus_usb ||
    hd->bus.id == bus_pcmcia ||
    hd->bus.id == bus_cardbus) {

    cic = NULL;
    u0 = ID_VALUE(hd->vendor.id);
    if(
      hd->bus.id == bus_isa &&
      ID_TAG(hd->vendor.id) == TAG_SPECIAL &&
      u0 >= 0x3000 && u0 <= 0x3006 &&
      ID_TAG(hd->device.id) == TAG_SPECIAL
    ) {
      u0 = ID_VALUE(hd->device.id);
      cic = hd_cdbisdn_get_card_from_type(u0 >> 8, u0 & 0xff);
    }

    if(
      hd->bus.id == bus_isa &&
      ID_TAG(hd->vendor.id) == TAG_EISA &&
      ID_TAG(hd->device.id) == TAG_EISA
    ) {
      u0 = ID_VALUE(hd->vendor.id);
      u1 = ID_VALUE(hd->device.id);
      cic = hd_cdbisdn_get_card_from_id(((u0 & 0xff) << 8) + ((u0 >> 8) & 0xff),
                                   ((u1 & 0xff) << 8) + ((u1 >> 8) & 0xff),
                                   0xffff,0xffff);
    }

    if(hd->bus.id == bus_pci) {
      cic = hd_cdbisdn_get_card_from_id(ID_VALUE(hd->vendor.id), ID_VALUE(hd->device.id),
                                 ID_VALUE(hd->sub_vendor.id), ID_VALUE(hd->sub_device.id));
    }

    if(hd->bus.id == bus_usb &&
    	ID_TAG(hd->vendor.id) == TAG_USB &&
    	ID_TAG(hd->device.id) == TAG_USB) {
      
      if (hd->revision.id == 0 && hd->revision.name) {
        /* the revision is usually saved as string (1.00) */
      	sscanf(hd->revision.name, "%x.%x", &u1, &u0);
      	u0 = u0 | u1 << 8;
      } else
      	u0 = ID_VALUE(hd->revision.id);

      cic = hd_cdbisdn_get_card_from_id(ID_VALUE(hd->vendor.id), ID_VALUE(hd->device.id),
                                 u0, 0xffff);
      if (!cic) /* to get cards without revision info in database */
      	cic = hd_cdbisdn_get_card_from_id(ID_VALUE(hd->vendor.id), ID_VALUE(hd->device.id),
      				0xffff, 0xffff);
    }
    
    if((hd->bus.id == bus_pcmcia || hd->bus.id == bus_cardbus) &&
    	(hd->base_class.id == bc_network || hd->base_class.id == bc_isdn)) {
    	if (hd->drivers && hd->drivers->str) {
    		if (0 == strcmp(hd->drivers->str, "teles_cs")) {
    			cic = hd_cdbisdn_get_card_from_type(8, 0);
    		} else if (0 == strcmp(hd->drivers->str, "sedlbauer_cs")) {
    			cic = hd_cdbisdn_get_card_from_type(22, 2);
    		} else if (0 == strcmp(hd->drivers->str, "avma1_cs")) {
    			cic = hd_cdbisdn_get_card_from_type(26, 0);
    		} else if (0 == strcmp(hd->drivers->str, "fcpcmcia_cs")) {
    			cic = hd_cdbisdn_get_card_from_type(8002, 5);
    		} else if (0 == strcmp(hd->drivers->str, "elsa_cs")) {
    			cic = hd_cdbisdn_get_card_from_type(10, 11);
    		} else if (0 == strcmp(hd->drivers->str, "avm_cs")) {
    			cic = hd_cdbisdn_get_card_from_type(8001, 2);
    		}
        }
    }

    if (cic && cic->Class && strcmp(cic->Class, "DSL")) {
      cic0 = new_mem(sizeof *cic0);
      memcpy(cic0, cic, sizeof *cic0);
      return cic0;
    }
  }
  return NULL;
}

void hd_scan_dsl(hd_data_t *hd_data)
{
  hd_t *hd;
  cdb_isdn_card *cic;

  if(!hd_probe_feature(hd_data, pr_isdn)) return;

  hd_data->module = mod_dsl;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "list");

#ifdef DSL_TEST
  {
    hd_res_t *res;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_pci;
    hd->base_class.id = bc_dsl;
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x1244);
    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x2700);	// type, subtype

  }
#endif

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if((cic = get_dsl_info(hd))) {
      free_mem(cic);
    }
  }


}

cdb_isdn_card *get_dsl_info(hd_t *hd)
{
  cdb_isdn_card *cic0, *cic;
  cdb_isdn_vario *civ;
  unsigned u0, u1;

  if(hd->bus.id == bus_pci ||
    hd->bus.id == bus_usb) {

    cic = NULL;

    if(hd->bus.id == bus_pci) {
      cic = hd_cdbisdn_get_card_from_id(ID_VALUE(hd->vendor.id), ID_VALUE(hd->device.id),
                                 ID_VALUE(hd->sub_vendor.id), ID_VALUE(hd->sub_device.id));
    }

    if(hd->bus.id == bus_usb &&
    	ID_TAG(hd->vendor.id) == TAG_USB &&
    	ID_TAG(hd->device.id) == TAG_USB) {
      
      if (hd->revision.id == 0 && hd->revision.name) {
        /* the revision is usually saved as string (1.00) */
      	sscanf(hd->revision.name, "%x.%x", &u1, &u0);
      	u0 = u0 | u1 << 8;
      } else
      	u0 = ID_VALUE(hd->revision.id);

      cic = hd_cdbisdn_get_card_from_id(ID_VALUE(hd->vendor.id), ID_VALUE(hd->device.id),
                                 u0, 0xffff);
      if (!cic) /* to get cards without revision info in database */
      	cic = hd_cdbisdn_get_card_from_id(ID_VALUE(hd->vendor.id), ID_VALUE(hd->device.id),
      				0xffff, 0xffff);
    }

    if (cic && cic->Class && !strcmp(cic->Class, "DSL")) {
      hd->base_class.id = bc_dsl;
      hd->sub_class.id = sc_dsl_unknown;
      civ = hd_cdbisdn_get_vario(cic->vario);
      if (civ && civ->interface) {
        if (0 == strncmp(civ->interface, "CAPI20", 6)) {
          hd->sub_class.id = sc_dsl_capi;
        } else if (0 == strncmp(civ->interface, "pppoe", 5)) {
          hd->sub_class.id = sc_dsl_pppoe;
        }
      }
      cic0 = new_mem(sizeof *cic0);
      memcpy(cic0, cic, sizeof *cic0);
      return cic0;
    }
  }
  return NULL;
}

#endif		/* !defined(__s390__) && !defined(__s390x__) && !defined(__alpha__) */

#endif		/* !defined(LIBHD_TINY) */

/** @} */


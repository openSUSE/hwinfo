#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "isdn.h"

#undef ISDN_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * identify isdn cards
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifndef LIBHD_TINY

#if !defined(__s390__) && !defined(__s390x__) && !defined(__alpha__)

void hd_scan_isdn(hd_data_t *hd_data)
{
  hd_t *hd;
  ihw_card_info *ici;

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
    hd->vendor3.id = MAKE_ID(TAG_SPECIAL, 0x3005);
    hd->device3.id = MAKE_ID(TAG_SPECIAL, 0x0500);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x0300;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor3.id = MAKE_ID(TAG_EISA, 0x1593);
    hd->device3.id = MAKE_ID(TAG_EISA, 0x0133);	// type, subtype
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
    hd->vendor3.id = MAKE_ID(TAG_EISA, 0x0e98);
    hd->device3.id = MAKE_ID(TAG_EISA, 0x0000);	// type, subtype
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
    hd->vendor3.id = MAKE_ID(TAG_PCI, 0x1244);
    hd->device3.id = MAKE_ID(TAG_PCI, 0x0a00);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0xe000;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor3.id = MAKE_ID(TAG_SPECIAL, 0x3001);
    hd->device3.id = MAKE_ID(TAG_SPECIAL, 0x0100);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0xe80;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor3.id = MAKE_ID(TAG_SPECIAL, 0x3000);
    hd->device3.id = MAKE_ID(TAG_SPECIAL, 0x1a00);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x400;
    res->io.access = acc_rw;


  }
#endif

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if((ici = get_isdn_info(hd))) {
      hd->base_class.id = bc_isdn;
      hd->sub_class.id = 0;
      free_mem(ici);
    }
  }


}

ihw_card_info *get_isdn_info(hd_t *hd)
{
  ihw_card_info *ici0, *ici;
  unsigned u0, u1;

  if(hd->bus.id == bus_pci ||
    hd->bus.id == bus_isa ||
    hd->bus.id == bus_usb) {

    ici = NULL;
    u0 = ID_VALUE(hd->vendor3.id);
    if(
      hd->bus.id == bus_isa &&
      ID_TAG(hd->vendor3.id) == TAG_SPECIAL &&
      u0 >= 0x3000 && u0 <= 0x3006 &&
      ID_TAG(hd->device3.id) == TAG_SPECIAL
    ) {
      u0 = ID_VALUE(hd->device3.id);
      ici = hd_ihw_get_card_from_type(u0 >> 8, u0 & 0xff);
    }

    if(
      hd->bus.id == bus_isa &&
      ID_TAG(hd->vendor3.id) == TAG_EISA &&
      ID_TAG(hd->device3.id) == TAG_EISA
    ) {
      u0 = ID_VALUE(hd->vendor3.id);
      u1 = ID_VALUE(hd->device3.id);
      ici = hd_ihw_get_card_from_id(((u0 & 0xff) << 8) + ((u0 >> 8) & 0xff),
                                   ((u1 & 0xff) << 8) + ((u1 >> 8) & 0xff),
                                   0xffff,0xffff);
    }

    if(hd->bus.id == bus_pci) {
      ici = hd_ihw_get_card_from_id(ID_VALUE(hd->vendor3.id), ID_VALUE(hd->device3.id),
                                 ID_VALUE(hd->sub_vendor3.id),ID_VALUE(hd->sub_dev));
    }

    if(hd->bus.id == bus_usb) {
      ici = hd_ihw_get_card_from_id(ID_VALUE(hd->vendor3.id), ID_VALUE(hd->device3.id),
                                 0xffff,0xffff);
    }

    if(ici) {
      ici0 = new_mem(sizeof *ici0);
      memcpy(ici0, ici, sizeof *ici0);
      return ici0;
    }
  }
  return NULL;
}

#endif		/* !defined(__s390__) && !defined(__s390x__) && !defined(__alpha__) */

#endif		/* !defined(LIBHD_TINY) */


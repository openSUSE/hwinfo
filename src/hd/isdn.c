#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "isdn.h"

#define ISDN_TEST

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * identify isdn cards
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

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
    hd->bus = bus_isa;
    hd->base_class = bc_isdn;
    hd->vend = MAKE_ID(TAG_SPECIAL, 0x3000);
    hd->dev = MAKE_ID(TAG_SPECIAL, 0x0500);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x0300;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus = bus_isa;
    hd->base_class = bc_isdn;
    hd->vend = MAKE_ID(TAG_EISA, 0x1593);
    hd->dev = MAKE_ID(TAG_EISA, 0x0133);	// type, subtype
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
    hd->bus = bus_isa;
    hd->base_class = bc_isdn;
    hd->vend = MAKE_ID(TAG_EISA, 0x0e98);
    hd->dev = MAKE_ID(TAG_EISA, 0x0000);	// type, subtype
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
    hd->bus = bus_pci;
    hd->base_class = bc_isdn;
    hd->vend = MAKE_ID(TAG_PCI, 0x1244);
    hd->dev = MAKE_ID(TAG_PCI, 0x0a00);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0xe000;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus = bus_isa;
    hd->base_class = bc_isdn;
    hd->vend = MAKE_ID(TAG_SPECIAL, 0x3000);
    hd->dev = MAKE_ID(TAG_SPECIAL, 0x0100);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0xe80;
    res->io.access = acc_rw;

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus = bus_isa;
    hd->base_class = bc_isdn;
    hd->vend = MAKE_ID(TAG_SPECIAL, 0x3000);
    hd->dev = MAKE_ID(TAG_SPECIAL, 0x1a00);	// type, subtype
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = 0x400;
    res->io.access = acc_rw;


  }
#endif

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if((ici = get_isdn_info(hd))) {
      hd->base_class = bc_isdn;
      hd->sub_class = 0;
      free_mem(ici);
    }
  }


}

ihw_card_info *get_isdn_info(hd_t *hd)
{
  ihw_card_info *ici0, *ici;
  unsigned u0, u1;

//#if defined(__i386)
/*
 * libihw currently breaks on non-Intel machines
 */

  if(hd->bus == bus_pci || hd->bus == bus_isa) {
    ici0 = new_mem(sizeof *ici0);
    ici = NULL;
    if(
      hd->bus == bus_isa &&
      hd->vend == MAKE_ID(TAG_SPECIAL, 0x3000) &&
      ID_TAG(hd->dev) == TAG_SPECIAL
    ) {
      u0 = ID_VALUE(hd->dev);
      ici0->type = u0 >> 8;
      ici0->subtype = u0 & 0xff;
      ici = ihw_get_device_from_type(ici0);
    }

    if(
      hd->bus == bus_isa &&
      ID_TAG(hd->vend) == TAG_EISA &&
      ID_TAG(hd->dev) == TAG_EISA
    ) {
      u0 = ID_VALUE(hd->vend);
      u1 = ID_VALUE(hd->dev);
      ici0->Class = CLASS_ISAPNP;
      ici0->vendor = ((u0 & 0xff) << 8) + ((u0 >> 8) & 0xff);
      ici0->device = ((u1 & 0xff) << 8) + ((u1 >> 8) & 0xff);
      ici0->subvendor = 0xffff;
      ici0->subdevice = 0xffff;
      ici = ihw_get_device_from_id(ici0);
    }

    if(hd->bus == bus_pci) {
      ici0->Class = CLASS_PCI;
      ici0->vendor = ID_VALUE(hd->vend);
      ici0->device = ID_VALUE(hd->dev);
      ici0->subvendor = ID_VALUE(hd->sub_vend);
      ici0->subdevice = ID_VALUE(hd->sub_dev);
      ici = ihw_get_device_from_id(ici0);
    }

    if(ici) return ici;

    ici0 = free_mem(ici0);
  }

//#endif		/* i386 */

  return NULL;
}


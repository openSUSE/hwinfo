#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "isdn.h"

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

    // ######## byte-order !!!!
    if(
      hd->bus == bus_isa &&
      ID_TAG(hd->vend) == TAG_EISA &&
      ID_TAG(hd->dev) == TAG_EISA
    ) {
      u0 = ID_VALUE(hd->vend);
      u1 = ID_VALUE(hd->dev);
      ici0->vendor = ((u0 & 0xff) << 8) + ((u0 >> 8) & 0xff);
      ici0->device = ((u1 & 0xff) << 8) + ((u1 >> 8) & 0xff);
      ici0->subvendor = 0xffff;
      ici0->subdevice = 0xffff;
      ici = ihw_get_device_from_id(ici0);
    }

    if(hd->bus == bus_pci) {
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


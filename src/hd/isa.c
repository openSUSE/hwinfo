#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "isa.h"

/**
 * @defgroup ISAint ISA
 * @ingroup libhdBUSint
 * @brief ISA bus scan functions
 *
 * @{
 */

#if defined(__i386__)

static void scan_isa_isdn(hd_data_t *hd_data);
static isa_isdn_t *free_isa_isdn(isa_isdn_t *ii);

static void dump_isa_isdn_data(hd_data_t *hd_data, isa_isdn_t *ii);

void hd_scan_isa(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_isa)) return;

  hd_data->module = mod_isa;

  /* some clean-up */
  remove_hd_entries(hd_data);
  // hd_data->isa = NULL;

  if(hd_probe_feature(hd_data, pr_isa_isdn)) {
    scan_isa_isdn(hd_data);
  }

}

void scan_isa_isdn(hd_data_t *hd_data)
{
  isa_isdn_t *ii0, *ii;
  hd_t *hd;
  hd_res_t *res;

  PROGRESS(1, 0, "isdn");

  ii0 = isdn_detect();

  dump_isa_isdn_data(hd_data, ii0);

  PROGRESS(1, 1, "isdn");

  for(ii = ii0; ii; ii = ii->next) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_isa;
    hd->base_class.id = bc_isdn;
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x3000 + ii->type);
    hd->device.id = MAKE_ID(TAG_SPECIAL, ((ii->type << 8) + (ii->subtype & 0xff)) & 0xffff);

    if(ii->has_io) {
      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->io.type = res_io;
      res->io.enabled = 1;
      res->io.base = ii->io;
      res->io.access = acc_rw;
    }

    if(ii->has_irq) {
      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->irq.type = res_irq;
      res->irq.enabled = 1;
      res->irq.base = ii->irq;
    }

    // #### ask libihw? -> isdn.c

  }

  free_isa_isdn(ii0);
}

isa_isdn_t *new_isa_isdn(isa_isdn_t **ii)
{
  while(*ii) ii = &(*ii)->next;

  return *ii = new_mem(sizeof **ii);
}

isa_isdn_t *free_isa_isdn(isa_isdn_t *ii)
{
  isa_isdn_t *l;

  for(; ii; ii = (l = ii)->next, free_mem(l));

  return NULL;
}

void dump_isa_isdn_data(hd_data_t *hd_data, isa_isdn_t *ii)
{
  ADD2LOG("---------- ISA ISDN raw data ----------\n");

  for(; ii; ii = ii->next) {
    ADD2LOG("  type %d, subtype %d", ii->type, ii->subtype);
    if(ii->has_mem) ADD2LOG(", mem 0x%04x", ii->mem);
    if(ii->has_io) ADD2LOG(", io 0x%04x", ii->io);
    if(ii->has_irq) ADD2LOG(", irq %d", ii->irq);
    ADD2LOG("\n");
  }

  ADD2LOG("---------- ISA ISDN raw data end ----------\n");
}


#endif /* defined(__i386__) */

/** @} */


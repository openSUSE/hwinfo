#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "braille.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * braille displays
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if 0
static void scan_braille_alva(hd_data_t *hd_data);
static void scan_braille_fhp(hd_data_t *hd_data);
static void scan_braille_ht(hd_data_t *hd_data);
#endif

//static void dump_isa_isdn_data(hd_data_t *hd_data, isa_isdn_t *ii);

void hd_scan_braille(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_braille)) return;

  hd_data->module = mod_braille;

  /* some clean-up */
  remove_hd_entries(hd_data);
  // hd_data->braille = NULL;

#if 0
  if(hd_probe_feature(hd_data, pr_braille_alva)) {
    scan_braille_alva(hd_data);
  }

  if(hd_probe_feature(hd_data, pr_braille_fhp)) {
    scan_braille_fhp(hd_data);
  }

  if(hd_probe_feature(hd_data, pr_braille_ht)) {
    scan_braille_ht(hd_data);
  }
#endif
}

#if 0
void scan_braille_alva(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res;

  PROGRESS(1, 0, "alva");

  ii0 = isdn_detect();

  dump_isa_isdn_data(hd_data, ii0);

  PROGRESS(1, 1, "isdn");

  for(ii = ii0; ii; ii = ii->next) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus = bus_isa;
    hd->base_class = bc_isdn;
    hd->vend = MAKE_ID(TAG_SPECIAL, 0x3000 + ii->type);
    hd->dev = MAKE_ID(TAG_SPECIAL, ((ii->type << 8) + (ii->subtype & 0xff)) & 0xffff);

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

  free_isa_isdn(ii);
}
#endif

#if 0
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
#endif


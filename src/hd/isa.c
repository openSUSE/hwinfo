#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "isa.h"


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * isa cards
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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
  isa_isdn_t *ii;

  PROGRESS(1, 0, "isdn");

  ii = isdn_detect();

  dump_isa_isdn_data(hd_data, ii);

  free_isa_isdn(ii);
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


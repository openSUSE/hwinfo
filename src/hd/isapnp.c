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
 * TODO:
 *   - memory range decoding
 *   - port range 'guessing' is wrong; cf. SB16 cards
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__i386__) || defined(__alpha__)

static void dump_raw_isapnp(hd_data_t *hd_data);
static void get_read_port(isapnp_t *);
static isapnp_res_t *get_isapnp_res(isapnp_card_t *, int, int);    
static unsigned find_io_range(isapnp_card_t *, unsigned);
static void dump_pnp_res(hd_data_t *hd_data, isapnp_card_t *);

void hd_scan_isapnp(hd_data_t *hd_data)
{
  hd_t *hd;
  int i, j, k;
  unsigned u, u2, ser;
  static unsigned mem32_cfgs[4] = { CFG_MEM32_0, CFG_MEM32_1, CFG_MEM32_2, CFG_MEM32_3 };
  unsigned char *t, *v, *s;
  isapnp_card_t *c;
  isapnp_res_t *r;
  isapnp_dev_t *dev;
  hd_res_t *res;
  
  if(!hd_probe_feature(hd_data, pr_isapnp)) return;

  hd_data->module = mod_isapnp;

  /* some clean-up */
  remove_hd_entries(hd_data);

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

  if(!hd_data->isapnp->read_port) get_read_port(hd_data->isapnp);

  PROGRESS(2, 0, "get pnp data");
  
  hd_data->module = mod_pnpdump;
  pnpdump(hd_data, 0);
  hd_data->module = mod_isapnp;

  if(!hd_data->isapnp->cards) hd_data->isapnp->read_port = 0;

  if((hd_data->debug & HD_DEB_ISAPNP)) dump_raw_isapnp(hd_data);

  PROGRESS(3, 0, "build list");

  if(hd_data->isapnp->cards) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus = bus_isa;
    hd->base_class = bc_internal;
    hd->sub_class = sc_int_isapnp_if;

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

  for(i = 0; i < hd_data->isapnp->cards; i++) {
    c = hd_data->isapnp->card + i;
    t = c->serial;

    for(j = 0; j < c->log_devs; j++) {
      hd = add_hd_entry(hd_data, __LINE__, 0);

      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_isapnp;
      dev = hd->detail->isapnp.data = new_mem(sizeof *hd->detail->isapnp.data);
      dev->card = new_mem(sizeof *dev->card);
      *dev->card = *c;
      dev->dev = j;

      if(c->broken) hd->broken = 1;

      hd->bus = bus_isa;
      hd->slot = c->csn;
      hd->func = j;

      hd->vend = MAKE_ID(TAG_EISA, (t[0] << 8) + t[1]);
      hd->dev = MAKE_ID(TAG_EISA, (t[2] << 8) + t[3]);

      if((u = device_class(hd_data, hd->vend, hd->dev))) {
        hd->base_class = u >> 8;
        hd->sub_class = u & 0xff;
      }

      if(
        (ID_VALUE(hd->vend) || ID_VALUE(hd->dev)) &&
        !hd_device_name(hd_data, hd->vend, hd->dev)
      ) {
        if((r = get_isapnp_res(c, 0, RES_ANSI_NAME))) {
          s = canon_str(r->data, r->len);
          add_device_name(hd_data, hd->vend, hd->dev, s);
          free_mem(s);
        }
      }

      ser = (t[7] << 24) + (t[6] << 16) + (t[5] << 8)+ t[4];
      if(ser != -1) {
        str_printf(&hd->serial, 0, "%u", ser);
      }

      if((c->ldev_regs[j][0] & 1)) {
        dev->flags |= (1 << isapnp_flag_act);
      }

      if((r = get_isapnp_res(c, j + 1, RES_LOG_DEV_ID))) {
        v = r->data;
        hd->sub_vend = MAKE_ID(TAG_EISA, (v[0] << 8) + v[1]);
        hd->sub_dev = MAKE_ID(TAG_EISA, (v[2] << 8) + v[3]);
        if(
          c->log_devs == 1 &&
          hd->sub_vend == hd->vend &&
          hd->sub_dev == hd->dev
        ) {
          hd->sub_vend = hd->sub_dev = 0;
        }

        if((u = sub_device_class(hd_data, hd->vend, hd->dev, hd->sub_vend, hd->sub_dev))) {
          hd->base_class = u >> 8;
          hd->sub_class = u & 0xff;
        }

        if(
          (ID_VALUE(hd->sub_vend) || ID_VALUE(hd->sub_dev)) &&
          !hd_sub_device_name(hd_data, hd->vend, hd->dev, hd->sub_vend, hd->sub_dev)
        ) {
          if((r = get_isapnp_res(c, j + 1, RES_ANSI_NAME))) {
            s = canon_str(r->data, r->len);
            add_sub_device_name(hd_data, hd->vend, hd->dev, hd->sub_vend, hd->sub_dev, s);
            free_mem(s);
          }
        }
      }

      if((r = get_isapnp_res(c, j + 1, RES_COMPAT_DEV_ID))) {
        v = r->data;

        hd->compat_vend = MAKE_ID(TAG_EISA, (v[0] << 8) + v[1]);
        hd->compat_dev = MAKE_ID(TAG_EISA, (v[2] << 8) + v[3]);

        if(!(hd->base_class || hd->sub_class)) {
          if((u = device_class(hd_data, hd->compat_vend, hd->compat_dev))) {
            hd->base_class = u >> 8;
            hd->sub_class = u & 0xff;
          }
          else if(hd->compat_vend == MAKE_ID(TAG_EISA, 0x41d0)) {
            /* 0x41d0 is 'PNP' */
            switch((hd->compat_dev >> 12) & 0xf) {
              case   8:
                hd->base_class = bc_network;
                hd->sub_class = 0x80;
                break;
              case 0xa:
                hd->base_class = bc_storage;
                hd->sub_class = 0x80;
                break;
              case 0xb:
                hd->base_class = bc_multimedia;
                hd->sub_class = 0x80;
                break;
              case 0xc:
              case 0xd:
                hd->base_class = bc_modem;
                break;
            }
          }
        }
      }


      v = c->ldev_regs[j];

      for(k = 0; k < 4; k++) {
        u = (v[CFG_MEM24     - 0x30 + 8 * k] << 16) +
            (v[CFG_MEM24 + 1 - 0x30 + 8 * k] << 8) +
             v[CFG_MEM24 + 2 - 0x30 + 8 * k];
        if(u) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->mem.type = res_mem;
          res->mem.enabled = dev->flags & (1 << isapnp_flag_act) ? 1 : 0;
          res->mem.access = acc_rw;
          res->mem.base = u & ~0xff;
          u2 = (v[CFG_MEM24 + 3 - 0x30 + 8 * k] << 16) +
               (v[CFG_MEM24 + 4 - 0x30 + 8 * k] << 8);
          if(u & 1) {
            if(u2 >= res->mem.base)
              res->mem.range = u2 - res->mem.base;
          }
          else {
            res->mem.range = u2 + 0x100;
          }
        }
      }

      for(k = 0; k < 4; k++) {
        u = (v[mem32_cfgs[k]     - 0x30] << 24) +
            (v[mem32_cfgs[k] + 1 - 0x30] << 16) +
            (v[mem32_cfgs[k] + 2 - 0x30] << 8) +
             v[mem32_cfgs[k] + 3 - 0x30];
        if(u) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->mem.type = res_mem;
          res->mem.enabled = dev->flags & (1 << isapnp_flag_act) ? 1 : 0;
          res->mem.access = acc_rw;
          res->mem.base = u;
          u2 = (v[mem32_cfgs[k] + 5 - 0x30] << 24) +
               (v[mem32_cfgs[k] + 6 - 0x30] << 16) +
               (v[mem32_cfgs[k] + 7 - 0x30] << 8) +
                v[mem32_cfgs[k] + 8 - 0x30];
          if(v[mem32_cfgs[k] + 4 - 0x30] & 1) {
            if(u2 >= res->mem.base)
              res->mem.range = u2 - res->mem.base;
          }
          else {
            res->mem.range = u2;
            res->mem.range++;
          }
        }
      }

      for(k = 0; k < 8; k++) {
        u = (v[CFG_IO_HI_BASE - 0x30 + 2 * k] << 8) +
             v[CFG_IO_LO_BASE - 0x30 + 2 * k];
        if(u) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->io.type = res_io;
          res->io.enabled = dev->flags & (1 << isapnp_flag_act) ? 1 : 0;
          res->io.base = u;
          res->io.range = find_io_range(c, u);
          res->io.access = acc_rw;
        }
      }

      for(k = 0; k < 2; k++) {
        u = v[CFG_IRQ - 0x30 + 2 * k] & 15;
        if(u) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->irq.type = res_irq;
          res->irq.enabled = dev->flags & (1 << isapnp_flag_act) ? 1 : 0;
          res->irq.base = u;
        }
      }

      for(k = 0; k < 2; k++) {
        u = v[CFG_DMA - 0x30 + k] & 7;
        if(u != 4) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->dma.type = res_dma;
          res->dma.enabled = dev->flags & (1 << isapnp_flag_act) ? 1 : 0;
          res->dma.base = u;
        }
      }

    }
  }

  hd_data->isapnp->card = free_mem(hd_data->isapnp->card);

}

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


void dump_raw_isapnp(hd_data_t *hd_data)
{
  int i, j, k;
  isapnp_t *p;
  isapnp_card_t *c;

  p = hd_data->isapnp;

  ADD2LOG("---------- ISA PnP raw data ----------\n");
  ADD2LOG("isapnp read port: 0x%x\n", p->read_port);
  for(k = 0; k < p->cards; k++) {
    c = p->card + k;

    ADD2LOG("card %d (csn %d, %d logical devices)\n", k, c->csn, c->log_devs);

    ADD2LOG("  serial: ");
    hexdump(&hd_data->log, 0, 8, c->serial);
    ADD2LOG("\n");

    ADD2LOG("  card_regs: 00: ");
    for(i = 0; i < 0x30; i += 0x10) {
      if(i) ADD2LOG("\n             %02x: ", i);
      hexdump(&hd_data->log, 1, 0x10, c->card_regs + i);
    }
    ADD2LOG("\n");

    for(j = 0; j < c->log_devs; j++) {
      ADD2LOG("  log dev %d: 30: ", j);
      for(i = 0; i < 0xd0; i += 0x10) {
        if(i) ADD2LOG("\n             %02x: ", i + 0x30);
        hexdump(&hd_data->log, 1, 0x10, c->ldev_regs[j] + i);
      }
      ADD2LOG("\n");
    }

    for(i = 0; i < c->res_len; i++) {
      ADD2LOG("  type 0x%02x, len %2d: ", c->res[i].type, c->res[i].len);
      hexdump(&hd_data->log, 1, c->res[i].len, c->res[i].data);
      ADD2LOG("\n");
    }
    dump_pnp_res(hd_data, c);
    if(k != p->cards - 1) ADD2LOG("\n");
  }
  ADD2LOG("---------- ISA PnP raw data end ----------\n");
}


void get_read_port(isapnp_t *p)
{
  FILE *f;
  char buf[200];
  int i = 0;

  p->read_port = 0;

  if(!(f = fopen(ISAPNP_CONF, "r"))) return;

  while(fgets(buf, sizeof buf, f)) {
    if(sscanf(buf, " ( READPORT %x )", &i) == 1) break;
  }

  fclose(f);

  p->read_port = i;
}


isapnp_res_t *get_isapnp_res(isapnp_card_t *c, int log_dev, int type)
{
  int i;
  isapnp_res_t *r;

  if(!c->res_len) return NULL;

  for(r = c->res, i = 0; i < c->res_len; i++, r++) {
    if(r->type == RES_LOG_DEV_ID) log_dev--;
    if(r->type == type && !log_dev) return r;
  }

  return NULL;
}


unsigned find_io_range(isapnp_card_t *ic, unsigned io_base)
{
  int i, ranges = 0;
  unsigned range = 0, u0, u1, u2, u3;	/* range = 0 for gcc -Wall */
  unsigned char *t;

  for(i = 0; i < ic->res_len; i++) {
    t = ic->res[i].data;

    if(ic->res[i].type == RES_FIXED_IO && ic->res[i].len == 3) {
      u0 = ((t[1] & 3) << 8) + t[0];
      u1 = t[2];
      if(u0 == io_base) {
        if(!ranges) { range = u1; ranges++; }
        if(range != u1) ranges++;
      }
    }

    if(ic->res[i].type == RES_IO && ic->res[i].len == 7) {
      u0 = (t[2] << 8) + t[1];
      u1 = (t[4] << 8) + t[3];
      u2 = t[5]; u3 = t[6];
      if(
        io_base == u0 ||
        io_base == u1 ||
        (io_base >= u0 && io_base <= u1 && u2 && !((io_base - u0) % u2))
      ) {
        if(!ranges) { range = u3; ranges++; }
        if(range != u3) ranges++;
      }
//      printf(">>io: %4x-%4x+%2x(%2x)<<\n", u0, u1, u2, t[6]);
    }
  }

  return ranges == 1 ? range : 0;
}


void dump_pnp_res(hd_data_t *hd_data, isapnp_card_t *c)
{
  int i, j;
  unsigned char *t;
  char *s, *df = "";
  unsigned u0, u1, u2, u3;

  ADD2LOG("  ----\n");
  for(i = 0; i < c->res_len; i++) {
    t = c->res[i].data;

    switch(c->res[i].type) {
      case RES_PNP_VERSION:
        ADD2LOG("%s  pnp ver %u.%u, %u\n", df, t[0] >> 4, t[0] & 5, t[1]);
        break;

      case RES_LOG_DEV_ID:
        s = isa_id2str((t[3] << 24) + (t[2] << 16) + (t[1] << 8)+ t[0]);
        ADD2LOG("%s  id %s\n", df, s);
        free_mem(s);
        break;

      case RES_COMPAT_DEV_ID:
        s = isa_id2str((t[3] << 24) + (t[2] << 16) + (t[1] << 8)+ t[0]);
        ADD2LOG("%s  compat id %s\n", df, s);
        free_mem(s);
        break;

      case RES_IRQ:
        u0 = (t[1] << 8) + t[0];
        ADD2LOG("%s  irq mask", df);
        for(j = 0; j < 16; j++) if(u0 & (1 << j)) ADD2LOG(" %u", j);
        ADD2LOG("\n");
        break;

      case RES_DMA:
        u0 = t[0];
        ADD2LOG("%s  dma mask", df);
        for(j = 0; j < 8; j++) if(u0 & (1 << j)) ADD2LOG(" %u", j);
        ADD2LOG("\n");
        break;

      case RES_START_DEP:
        df = "  ";
        ADD2LOG("  start DF\n");
        break;

      case RES_END_DEP:
        df = "";
        ADD2LOG("  end DF\n");
        break;

      case RES_IO:
        u0 = (t[2] << 8) + t[1];
        u1 = (t[4] << 8) + t[3];
        u2 = t[5]; u3 = t[6];
        ADD2LOG("%s  i/o ports 0x%x-0x%x(0x%x), step 0x%x\n", df, u0, u1, u3, u2);
        break;

      case RES_FIXED_IO:
        u0 = ((t[1] & 3) << 8) + t[0];
        u1 = t[2];
        ADD2LOG("%s  fixed i/o ports 0x%x(0x%x)\n", df, u0, u1);
        break;

      case RES_END:
        df = 0;
        ADD2LOG("  end\n");
        break;

      case RES_ANSI_NAME:
        s = new_mem(c->res[i].len + 1);
        memcpy(s, t, c->res[i].len);
        ADD2LOG("%s  name \"%s\"\n", df, s);
        free_mem(s);
        break;

      default:
        ADD2LOG("%s  type 0x%02x, len %2d: ", df, c->res[i].type, c->res[i].len);
        hexdump(&hd_data->log, 1, c->res[i].len, t);
        ADD2LOG("\n");
    }
  }
}

#endif /* defined(__i386__) || defined(__alpha__) */


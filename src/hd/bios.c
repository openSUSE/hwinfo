#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "bios.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * bios info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__i386__)

static void read_memory(memory_range_t *mem);
static void dump_memory(hd_data_t *hd_data, memory_range_t *mem, int sparse, char *label);
static void get_pnp_support_status(memory_range_t *mem, bios_info_t *bt);
static void get_smbios_info(hd_data_t *hd_data, memory_range_t *mem, bios_info_t *bt);
static unsigned char crc(unsigned char *mem, unsigned len);
static int get_smp_info(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp);
static void parse_mpconfig(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp);

int detect_smp(hd_data_t *hd_data)
{
  bios_info_t *bt;
  hd_t *hd;

  if(!hd_data->bios_ram.data) return -1;	/* hd_scan_bios() not called */

  for(bt = NULL, hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_internal &&
      hd->sub_class == sc_int_bios &&
      hd->detail &&
      hd->detail->type == hd_detail_bios &&
      (bt = hd->detail->bios.data)
    ) {
      break;
    }
  }

  if(!bt) return -1;

//  return bt->smp.ok ? bt->smp.cpus_en ? bt->smp.cpus_en : 1 : 0;
// Dell Dimension 8100 has a MP table with 0 cpus!

  return bt->smp.ok ? bt->smp.cpus_en : 0;

  return 0;
}


void hd_scan_bios(hd_data_t *hd_data)
{
  hd_t *hd;
  char *s;
  bios_info_t *bt;
  unsigned char *bios_ram;
  unsigned u, u1;
  memory_range_t mem;
  unsigned smp_ok;
  vbe_info_t *vbe;
  vbe_mode_info_t *mi;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_bios)) return;

  hd_data->module = mod_bios;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "cmdline");

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class = bc_internal;
  hd->sub_class = sc_int_bios;
  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_bios;
  hd->detail->bios.data = bt = new_mem(sizeof *bt);

  /*
   * first, look for APM support
   */
  if((s = get_cmd_param(hd_data, 1))) {
    if(strlen(s) >= 10) {
      bt->apm_supported = 1;
      bt->apm_ver = hex(s, 1);
      bt->apm_subver = hex(s + 1, 1);
      bt->apm_bios_flags = hex(s + 2, 2);
      /*
       * Bitfields for APM flags (from Ralf Brown's list):
       * Bit(s)  Description
       *  0      16-bit protected mode interface supported
       *  1      32-bit protected mode interface supported
       *  2      CPU idle call reduces processor speed
       *  3      BIOS power management disabled
       *  4      BIOS power management disengaged (APM v1.1)
       *  5-7    reserved
       */
      bt->apm_enabled = (bt->apm_bios_flags & 8) ? 0 : 1;

      bt->vbe_ver = hex(s + 4, 2);
      bt->vbe_ver = (((bt->vbe_ver >> 4) & 0xf) << 8) + (bt->vbe_ver & 0xf);
      bt->vbe_video_mem = hex(s + 6, 4) << 16;
    }

    s = free_mem(s);
  }

  if((s = get_cmd_param(hd_data, 2))) {
    if(strlen(s) > 8) {
      if(s[8] == '.') bt->lba_support = 1;
    }

    s = free_mem(s);
  }

  /*
   * get the i/o ports for the parallel & serial interfaces from the BIOS
   * memory area starting at 0x40:0
   */
  PROGRESS(2, 0, "ram");

  hd_data->bios_ram.start = BIOS_RAM_START;
  hd_data->bios_ram.size = BIOS_RAM_SIZE;
  read_memory(&hd_data->bios_ram);

  hd_data->bios_rom.start = BIOS_ROM_START;
  hd_data->bios_rom.size = BIOS_ROM_SIZE;
  read_memory(&hd_data->bios_rom);

  if(hd_data->bios_ram.data) {
    bios_ram = hd_data->bios_ram.data;

    bt->ser_port0 = (bios_ram[1] << 8) + bios_ram[0];
    bt->ser_port1 = (bios_ram[3] << 8) + bios_ram[2];
    bt->ser_port2 = (bios_ram[5] << 8) + bios_ram[4];
    bt->ser_port3 = (bios_ram[7] << 8) + bios_ram[6];

    bt->par_port0 = (bios_ram[  9] << 8) + bios_ram[  8];
    bt->par_port1 = (bios_ram[0xb] << 8) + bios_ram[0xa];
    bt->par_port2 = (bios_ram[0xd] << 8) + bios_ram[0xc];

    ADD2LOG("  bios: %u disks\n", bios_ram[0x75]);

    bt->low_mem_size = ((bios_ram[0x14] << 8) + bios_ram[0x13]) << 10;

    hd_data->bios_ebda.start = hd_data->bios_ebda.size = 0;
    hd_data->bios_ebda.data = free_mem(hd_data->bios_ebda.data);
    u = ((bios_ram[0x0f] << 8) + bios_ram[0x0e]) << 4;
    if(u) {
      hd_data->bios_ebda.start = u;
      hd_data->bios_ebda.size = 1;	/* just one byte */
      read_memory(&hd_data->bios_ebda);
      if(hd_data->bios_ebda.data) {
        u1 = hd_data->bios_ebda.data[0];
        if(u1 > 0 && u1 <= 64) {	/* be sensible, typically only 1k */
          u1 <<= 10;
          if(u + u1 <= (1 << 20)) {
            hd_data->bios_ebda.size = u1;
            read_memory(&hd_data->bios_ebda);
          }
        }
      }
    }

    if(hd_data->bios_ebda.data) {
      ADD2LOG(
        "  bios: EBDA 0x%05x bytes at 0x%05x\n",
        hd_data->bios_ebda.size, hd_data->bios_ebda.start
      );
    }
  }

  /*
   * read the bios rom and look for useful things there...
   */
  PROGRESS(2, 0, "rom");

  if(hd_data->bios_rom.data) {
    get_pnp_support_status(&hd_data->bios_rom, bt);
    get_smbios_info(hd_data, &hd_data->bios_rom, bt);
  }

  PROGRESS(3, 0, "smp");

  smp_ok = 0;

  mem = hd_data->bios_ebda;
  smp_ok = get_smp_info(hd_data, &mem, &bt->smp);

  if(!smp_ok) {
    mem = hd_data->bios_rom;
    if(mem.data) {
      mem.size -= 0xf0000 - mem.start;
      mem.data += 0xf0000 - mem.start;
      mem.start = 0xf0000;
      if(mem.size < (1 << 20)) smp_ok = get_smp_info(hd_data, &mem, &bt->smp);
    }
  }

  if(!smp_ok) {
    mem.size = 1 << 10;
    mem.start = 639 << 10;
    mem.data = NULL;
    read_memory(&mem);
    if(mem.data) smp_ok = get_smp_info(hd_data, &mem, &bt->smp);
    mem.data = free_mem(mem.data);
  }

  if(bt->smp.ok && bt->smp.mpconfig) {
    mem.start = bt->smp.mpconfig;
    mem.size = 1 << 10;
    mem.data = NULL;
    read_memory(&mem);
    parse_mpconfig(hd_data, &mem, &bt->smp);
    mem.data = free_mem(mem.data);
  }
  
  if((hd_data->debug & HD_DEB_BIOS)) {
    dump_memory(hd_data, &hd_data->bios_ram, 0, "BIOS data");
    dump_memory(hd_data, &hd_data->bios_ebda, hd_data->bios_ebda.size <= (8 << 10) ? 0 : 1, "EBDA");
    // dump_memory(hd_data, &hd_data->bios_rom, 1, "BIOS ROM");

    if(bt->smp.ok && bt->smp.mpfp) {
      mem.start = bt->smp.mpfp;
      mem.size = 0x10;
      mem.data = NULL;
      read_memory(&mem);
      dump_memory(hd_data, &mem, 0, "MP FP");
      mem.data = free_mem(mem.data);
    }

    if(bt->smp.ok && bt->smp.mpconfig && bt->smp.mpconfig_size) {
      mem.start = bt->smp.mpconfig;
      mem.size = bt->smp.mpconfig_size;
      mem.data = NULL;
      read_memory(&mem);
      dump_memory(hd_data, &mem, 0, "MP config table");
      mem.data = free_mem(mem.data);
    }
  }

  if(hd_probe_feature(hd_data, pr_bios_vbe)) {
    PROGRESS(4, 0, "vbe");

    vbe = &bt->vbe;
    vbe->ok = 0;


    get_vbe_info(hd_data, vbe);

    if(vbe->ok) {
      bt->vbe_ver = vbe->version;
    }

    if(vbe->ok && vbe->fb_start) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_framebuffer;
      hd->sub_class = sc_fb_vesa;

#if 0
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_bios;
      hd->detail->bios.data = bt = new_mem(sizeof *bt);
#endif

      hd->vend_name = new_str(vbe->vendor_name);
      hd->dev_name = new_str(vbe->product_name);
      hd->sub_vend_name = new_str(vbe->oem_name);
      hd->rev_name = new_str(vbe->product_revision);

      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->phys_mem.type = res_phys_mem;
      res->phys_mem.range = vbe->memory;

      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->mem.type = res_mem;
      res->mem.base = vbe->fb_start;
      res->mem.range = vbe->memory;
      res->mem.access = acc_rw;
      res->mem.enabled = 1;

      if(vbe->mode) {
        for(u = 0; u < vbe->modes; u++) {
          mi = vbe->mode + u;
          if(
            (mi->attributes & 1) &&	/* mode supported */
            mi->fb_start &&
            mi->pixel_size != -1	/* text mode */
          ) {
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->framebuffer.type = res_framebuffer;
            res->framebuffer.width = mi->width;
            res->framebuffer.bytes_p_line = mi->bytes_p_line;
            res->framebuffer.height = mi->height;
            res->framebuffer.colorbits = mi->pixel_size;
            res->framebuffer.mode = mi->number + 0x200;
          }
        }
      }

#if 0
      if(
        hd->vend_name &&
        !strcmp(hd->vend_name, "Matrox") &&
        hd->dev_name &&
        (
          strstr(hd->dev_name, "G200") ||
          strstr(hd->dev_name, "G400") ||
          strstr(hd->dev_name, "G450")
        )
      ) {
        hd->broken = 1;
      }
#endif

    }

  }

}


void read_memory(memory_range_t *mem)
{
  int fd;

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log) fprintf(libhd_log, ">%p\n", CALLED_FROM(read_memory, mem));
  }
#endif

  if(mem->data) free_mem(mem->data);
  mem->data = new_mem(mem->size);
  fd = -1;
  if(
    !(
      (fd = open(DEV_MEM, O_RDWR)) >= 0 &&
      lseek(fd, mem->start, SEEK_SET) >= 0 &&
      read(fd, mem->data, mem->size) == mem->size
    )
  ) {
    mem->data = free_mem(mem->data);
    mem->size = mem->start = 0;
  }

  if(fd >= 0) close(fd);

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log) fprintf(libhd_log, "<%p\n", CALLED_FROM(read_memory, mem));
  }
#endif
}


void dump_memory(hd_data_t *hd_data, memory_range_t *mem, int sparse, char *label)
{
  unsigned u, step;

  if(!mem->size || !mem->data) return;

#if 1
  step = sparse ? 0x1000 : 0x10;
#else
  step = 0x10;
#endif

  ADD2LOG("----- %s 0x%05x - 0x%05x -----\n", label, mem->start, mem->start + mem->size - 1);
  for(u = 0; u < mem->size; u += step) {
    ADD2LOG("  %03x  ", u + mem->start);
    hexdump(&hd_data->log, 1, 0x10, mem->data + u);
    ADD2LOG("\n");
  }
  ADD2LOG("----- %s end -----\n", label);
}


void get_pnp_support_status(memory_range_t *mem, bios_info_t *bt)
{
  int i;
  unsigned char pnp[4] = { '$', 'P', 'n', 'P' };
  unsigned char *t;
  unsigned l, cs;

  if(!mem->data) return;

  for(i = 0xf0000 - mem->start; i < mem->size; i += 0x10) {
    t = mem->data + i;
    if(t[0] == pnp[0] && t[1] == pnp[1] && t[2] == pnp[2] && t[3] == pnp[3]) {
      for(l = cs = 0; l < t[5]; l++) { cs += t[l]; }
      if((cs & 0xff) == 0) {    	// checksum ok
        bt->is_pnp_bios = 1;
//        printf("0x%x bytes at 0x%x, cs = 0x%x\n", t[5], i, cs);
        bt->pnp_id = t[0x17] + (t[0x18] << 8) + (t[0x19] << 16) + (t[0x20] << 24);
      }
    }
  }
}

unsigned char crc(unsigned char *mem, unsigned len)
{
  unsigned char uc = 0;

  while(len--) uc += *mem++;

  return uc;
}

void get_smbios_info(hd_data_t *hd_data, memory_range_t *mem, bios_info_t *bt)
{
  unsigned u, u1, u2, ok, hlen = 0, ofs;
  unsigned addr = 0, len = 0;
  unsigned structs = 0, type, slen;
  memory_range_t memory;

  if(!mem->data || mem->size < 0x100) return;

  for(u = ok = 0; u <= mem->size - 0x100; u += 0x10) {
    if(*(unsigned *) (mem->data + u) == 0x5f4d535f) {	/* "_MP_" */
      hlen = mem->data[u + 5];
      addr = *(unsigned *) (mem->data + u + 0x18);
      len = *(unsigned short *) (mem->data + u + 0x16);
      structs = *(unsigned short *) (mem->data + u + 0x1c);
      if(hlen < 0x1e) continue;
      ok = crc(mem->data + u, hlen) == 0 && addr < (1 << 20) && len;
      if(ok) break;
    }
  }

  if(!ok) return;

  memory.start = mem->start + u;
  memory.size = hlen;
  memory.data = mem->data + u;
  dump_memory(hd_data, &memory, 0, "SMBIOS Entry Point");

  memory.start = addr;
  memory.size = len;
  memory.data = NULL;
  read_memory(&memory);
  if(len >= 0x4000) {
    ADD2LOG(
      "  SMBIOS Structure Table at 0x%05x (size 0x%x)\n",
      addr, len
    );
  }
  else {
    dump_memory(hd_data, &memory, 0, "SMBIOS Structure Table");
  }

  for(u = 0, ofs = 0; u < structs && ofs + 3 < len; u++) {
    type = memory.data[ofs];
    slen = memory.data[ofs + 1];
    if(ofs + slen > len || slen < 4) break;
    slen -= 4;
    ofs += 4;
    ADD2LOG("  %2u [type 0x%02x, ofs 0x%03x]: ", u, type, ofs);
    if(slen) hexdump(&hd_data->log, 0, slen, memory.data + ofs);
    ADD2LOG("\n");
    ofs += slen;
    u1 = ofs;
    u2 = 1;
    while(ofs + 1 < len) {
      if(!memory.data[ofs]) {
        if(ofs > u1) {
          ADD2LOG("     str%u: \"%s\"\n", u2, memory.data + u1);
          u1 = ofs + 1;
          u2++;
        }
        if(!memory.data[ofs + 1]) {
          ofs += 2;
          break;
        }
      }
      ofs++;
    }
  }

  if(u != structs) ADD2LOG("  oops: only %d of %d structs found\n", u, structs);

  memory.data = free_mem(memory.data);

}



int get_smp_info(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp)
{
  unsigned u, ok;
  unsigned addr = 0, len;

  if(mem->size < 0x10) return 0;

  for(u = ok = 0; u <= mem->size - 0x10; u++) {
    if(*(unsigned *) (mem->data + u) == 0x5f504d5f) {	/* "_MP_" */
      addr = *(unsigned *) (mem->data + u + 4);
      len = mem->data[u + 8];
      ok = len == 1 && crc(mem->data + u, 0x10) == 0 && addr < (1 << 20) ? 1 : 0;
      ADD2LOG(
        "  smp: %svalid MP FP at 0x%05x (size 0x%x, rev %u), MP config at 0x%05x\n",
        ok ? "" : "in", u + mem->start, len << 4, mem->data[u + 9], addr
      );
      if(ok) break;
    }
  }

  if(ok) {
    smp->ok = 1;
    smp->mpfp = mem->start + u;
    smp->rev = mem->data[u + 9];
    smp->mpconfig = addr;
    memcpy(smp->feature, mem->data + u + 11, 5);
  }

  return ok;
}


void parse_mpconfig(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp)
{
  unsigned cfg_len, xcfg_len;
  unsigned char u0, ux0;
  unsigned u, type, len, entries, entry_cnt;
  char *s;

  cfg_len = xcfg_len = 0;

  if(*(unsigned *) (mem->data) == 0x504d4350) {		/* "PCMP" */
    cfg_len = mem->data[0x04] + (mem->data[0x05] << 8);
    smp->mpconfig_size = cfg_len;
    u0 = crc(mem->data, cfg_len);
    if(u0) return;
    smp->mpconfig_ok = 1;
    smp->cpus = smp->cpus_en = 0;
    xcfg_len = mem->data[0x28] + (mem->data[0x29] << 8);
    ux0 = crc(mem->data + cfg_len, xcfg_len) + mem->data[0x2a];
    if(!ux0) {
      smp->mpconfig_size += xcfg_len;
    }
    else {
      xcfg_len = 0;
    }
  }

  if(cfg_len) {
    s = canon_str(mem->data + 8, 8);
    strcpy(smp->oem_id, s);
    free_mem(s);
    s = canon_str(mem->data + 0x10, 12);
    strcpy(smp->prod_id, s);
    s = free_mem(s);

    entries = mem->data[0x22] + (mem->data[0x23] << 8);
    ADD2LOG("  base MP config table (%u entries):\n", entries);
    entry_cnt = 0;
    for(u = 0x2c; u < cfg_len - 1; u += len, entry_cnt++) {
      type = mem->data[u];
      len = type == 0 ? 20 : type <= 4 ? 8 : 16;
      ADD2LOG("  %stype %u, len %u\n    ", type > 4 ? "unknown ": "", type, len);
      if(len + u > cfg_len) len = cfg_len - u;
      hexdump(&hd_data->log, 1, len, mem->data + u);
      ADD2LOG("\n");
      if(type > 4) break;
      if(type == 0) {
        smp->cpus++;
        if((mem->data[u + 3] & 1)) smp->cpus_en++;
      }
    }
    if(entry_cnt != entries) {
      ADD2LOG("  oops: %u entries instead of %u found\n", entry_cnt, entries);
    }
  }

  if(xcfg_len) {
    ADD2LOG("  extended MP config table:\n");
    for(u = 0; u < xcfg_len - 2; u += len) {
      type = mem->data[u + cfg_len];
      len = mem->data[u + cfg_len + 1];
      ADD2LOG("  type %u, len %u\n    ", type, len);
      if(len + u > xcfg_len) len = xcfg_len - u;
      hexdump(&hd_data->log, 1, len, mem->data + cfg_len + u);
      ADD2LOG("\n");
    }
  }


}


#endif /* defined(__i386__) */

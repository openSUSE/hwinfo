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
static unsigned char crc(unsigned char *mem, unsigned len);
static int get_smp_info(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp);
static void parse_mpconfig(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp);

static memory_range_t bios_rom, bios_ram, bios_ebda;
static smp_info_t smp;

int detect_smp(hd_data_t *hd_data)
{
  if(!hd_data->bios_ram) return -1;	/* hd_scan_bios() not called */

  return smp.ok ? smp.cpus_en ? smp.cpus_en : 1 : 0;
}

void hd_scan_bios(hd_data_t *hd_data)
{
  hd_t *hd;
  char *s;
  bios_info_t *bt;
  unsigned u, u1;
  memory_range_t mem;
  unsigned smp_ok;
  unsigned low_mem_size;

  if(!hd_probe_feature(hd_data, pr_bios)) return;

  hd_data->module = mod_bios;

  /* some clean-up */
  remove_hd_entries(hd_data);
  memset(&bios_rom, 0, sizeof bios_rom);
  memset(&bios_ram, 0, sizeof bios_ram);
  memset(&bios_ebda, 0, sizeof bios_ebda);
  memset(&smp, 0, sizeof smp);

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

  bios_ram.start = BIOS_RAM_START;
  bios_ram.size = BIOS_RAM_SIZE;
  read_memory(&bios_ram);
  hd_data->bios_ram = bios_ram.data;

  bios_rom.start = BIOS_ROM_START;
  bios_rom.size = BIOS_ROM_SIZE;
  read_memory(&bios_rom);
  hd_data->bios_rom = bios_rom.data;

  if(bios_ram.data) {
    bt->ser_port0 = (bios_ram.data[1] << 8) + bios_ram.data[0];
    bt->ser_port1 = (bios_ram.data[3] << 8) + bios_ram.data[2];
    bt->ser_port2 = (bios_ram.data[5] << 8) + bios_ram.data[4];
    bt->ser_port3 = (bios_ram.data[7] << 8) + bios_ram.data[6];

    bt->par_port0 = (bios_ram.data[  9] << 8) + bios_ram.data[  8];
    bt->par_port1 = (bios_ram.data[0xb] << 8) + bios_ram.data[0xa];
    bt->par_port2 = (bios_ram.data[0xd] << 8) + bios_ram.data[0xc];

    ADD2LOG("  bios: %u disks\n", bios_ram.data[0x75]);

    low_mem_size = ((bios_ram.data[0x14] << 8) + bios_ram.data[0x13]) << 10;
    ADD2LOG("  bios: low mem = %uk\n", low_mem_size >> 10);

    bios_ebda.start = bios_ebda.size = 0;
    bios_ebda.data = free_mem(bios_ebda.data);
    u = ((bios_ram.data[0x0f] << 8) + bios_ram.data[0x0e]) << 4;
    if(u) {
      bios_ebda.start = u;
      bios_ebda.size = 1;	/* just one byte */
      read_memory(&bios_ebda);
      if(bios_ebda.data) {
        u1 = bios_ebda.data[0];
        if(u1 > 0 && u1 <= 64) {	/* be sensible, typically only 1k */
          u1 <<= 10;
          if(u + u1 <= (1 << 20)) {
            bios_ebda.size = u1;
            read_memory(&bios_ebda);
          }
        }
      }
    }

    if(bios_ebda.data) {
      ADD2LOG(
        "  bios: EBDA 0x%05x at 0x%05x\n",
        bios_ebda.size, bios_ebda.start
      );
    }
  }

  /*
   * read the bios rom and look for useful things there...
   */
  PROGRESS(2, 0, "rom");

  if(bios_rom.data) {
    get_pnp_support_status(&bios_rom, bt);
  }

  PROGRESS(3, 0, "smp");

  smp_ok = 0;

  mem = bios_ebda;
  smp_ok = get_smp_info(hd_data, &mem, &smp);

  if(!smp_ok) {
    mem = bios_rom;
    if(mem.data) {
      mem.size -= 0xf0000 - mem.start;
      mem.data += 0xf0000 - mem.start;
      mem.start = 0xf0000;
      if(mem.size < (1 << 20)) smp_ok = get_smp_info(hd_data, &mem, &smp);
    }
  }

  if(!smp_ok) {
    mem.size = 1 << 10;
    mem.start = 639 << 10;
    mem.data = NULL;
    read_memory(&mem);
    if(mem.data) smp_ok = get_smp_info(hd_data, &mem, &smp);
    mem.data = free_mem(mem.data);
  }

  if(smp.ok && smp.mpconfig) {
    mem.start = smp.mpconfig;
    mem.size = 1 << 10;
    mem.data = NULL;
    read_memory(&mem);
    parse_mpconfig(hd_data, &mem, &smp);
    mem.data = free_mem(mem.data);
  }
  
  if((hd_data->debug & HD_DEB_BIOS)) {
    dump_memory(hd_data, &bios_ram, 0, "BIOS data");
    dump_memory(hd_data, &bios_ebda, bios_ebda.size <= (4 << 10) ? 0 : 1, "EBDA");
    dump_memory(hd_data, &bios_rom, 1, "BIOS ROM");

    if(smp.ok && smp.mpfp) {
      mem.start = smp.mpfp;
      mem.size = 0x10;
      mem.data = NULL;
      read_memory(&mem);
      dump_memory(hd_data, &mem, 0, "MP FP");
      mem.data = free_mem(mem.data);
    }

    if(smp.ok && smp.mpconfig && smp.mpconfig_size) {
      mem.start = smp.mpconfig;
      mem.size = smp.mpconfig_size;
      mem.data = NULL;
      read_memory(&mem);
      dump_memory(hd_data, &mem, 0, "MP config table");
      mem.data = free_mem(mem.data);
    }
  }

  free_mem(bios_ebda.data);
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


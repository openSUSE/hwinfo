#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <byteswap.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__i386__) || defined (__x86_64__) || defined(__ia64__)
#include <sys/io.h>
#endif
#include <sys/pci.h>

#include "hd.h"
#include "hd_int.h"
#include "bios.h"
#include "klog.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * bios info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__i386__) || defined (__x86_64__) || defined(__ia64__)

#ifndef LIBHD_TINY
static struct {
  int width;
  int height;
  char *vendor;
  char *name;
  char *version;
} panel_data[] = {
  {  800,  600, "Fujitsu Siemens", "LiteLine", "LF6" },
  { 1024,  768, "ASUSTEK", "L2000D", NULL },
  { 1024,  768, "ASUSTeK Computer Inc.", "L8400C series Notebook PC", NULL },
  { 1024,  768, "Acer", "TravelMate 720", NULL },
  { 1024,  768, "COMPAL", "N30T5", NULL },
  { 1024,  768, "Dell Computer Corporation", "Inspiron 5000", NULL },
  { 1024,  768, "Dell Computer Corporation", "Latitude C400", NULL },
  { 1024,  768, "Dell Computer Corporation", "Latitude CPt C400GT", NULL },
  { 1024,  768, "Hewlett-Packard", "HP OmniBook PC", "HP OmniBook 4150 B" },
  { 1024,  768, "IBM", "2373G1U", NULL },
  { 1024,  768, "IBM", "26284UG", NULL },
  { 1024,  768, "IBM", "26446AG", NULL },
  { 1024,  768, "IBM", "26455BG", NULL },
  { 1024,  768, "IBM", "264721G", NULL },
  { 1024,  768, "KDST", "KDS6KSUMO", NULL  },
  { 1024,  768, "Sony Corporation", "PCG-F370(UC)", NULL },
  { 1024,  768, "Sony Corporation", "PCG-N505SN", NULL },
  { 1024,  768, "TOSHIBA", "S2400-103", NULL },
  { 1400, 1050, "Dell Computer Corporation", "Inspiron 8000", NULL },
  { 1600, 1200, "Dell Computer Corporation", "Inspiron 8200", NULL },
  { 1600, 1200, "Dell Computer Corporation", "Latitude C840", NULL }
};
#endif

#define BIOS_TEST

typedef struct {
  unsigned eax, ebx, ecx, edx, esi, edi, eip, es, iret, cli;
} bios32_regs_t;

static void read_memory(memory_range_t *mem);
#ifndef LIBHD_TINY
static void dump_memory(hd_data_t *hd_data, memory_range_t *mem, int sparse, char *label);
static void get_pnp_support_status(memory_range_t *mem, bios_info_t *bt);
static void get_smbios_info(hd_data_t *hd_data, memory_range_t *mem, bios_info_t *bt);
static char *get_string(str_list_t *sl, int index);
static void parse_smbios(hd_data_t *hd_data, bios_info_t *bt);
static void get_fsc_info(hd_data_t *hd_data, memory_range_t *mem, bios_info_t *bt);
static void add_panel_info(hd_data_t *hd_data, bios_info_t *bt);
static void add_mouse_info(hd_data_t *hd_data, bios_info_t *bt);
static unsigned char crc(unsigned char *mem, unsigned len);
static int get_smp_info(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp);
static void parse_mpconfig(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp);
static int get_bios32_info(hd_data_t *hd_data, memory_range_t *mem, bios32_info_t *bios32);
#endif

int detect_smp(hd_data_t *hd_data)
{
  bios_info_t *bt;
  hd_t *hd;

  if(!hd_data->bios_ram.data) return -1;	/* hd_scan_bios() not called */

  for(bt = NULL, hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_internal &&
      hd->sub_class.id == sc_int_bios &&
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
  bios_info_t *bt;
#ifndef LIBHD_TINY
  char *s;
  unsigned char *bios_ram;
  unsigned u, u1;
  memory_range_t mem;
  unsigned smp_ok;
  vbe_info_t *vbe;
  vbe_mode_info_t *mi;
  hd_res_t *res;
#endif
  struct stat sbuf;
  str_list_t *sl;

  if(!hd_probe_feature(hd_data, pr_bios)) return;

  /* we better do nothing on a SGI Altix machine */
  if(!stat("/proc/sgi_sn", &sbuf)) return;

  hd_data->module = mod_bios;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "cmdline");

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class.id = bc_internal;
  hd->sub_class.id = sc_int_bios;
  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_bios;
  hd->detail->bios.data = bt = new_mem(sizeof *bt);

#ifndef LIBHD_TINY

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

  PROGRESS(1, 1, "apm");

  if(!bt->apm_ver) {
    str_list_t *sl0, *sl;

    sl0 = read_file(PROC_APM, 0, 0);
    if(sl0) {
      bt->apm_supported = 1;
      bt->apm_enabled = 1;
      ADD2LOG("----- %s -----\n", PROC_APM);
      for(sl = sl0; sl; sl = sl->next) {
        ADD2LOG("  %s", sl->str);
      }
      ADD2LOG("----- %s end -----\n", PROC_APM);
    }
    free_str_list(sl0);
  }

#endif		/* !defined(LIBHD_TINY) */

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

#ifndef LIBHD_TINY

  if(hd_data->bios_ram.data) {
    bios_ram = hd_data->bios_ram.data;

    bt->ser_port0 = (bios_ram[1] << 8) + bios_ram[0];
    bt->ser_port1 = (bios_ram[3] << 8) + bios_ram[2];
    bt->ser_port2 = (bios_ram[5] << 8) + bios_ram[4];
    bt->ser_port3 = (bios_ram[7] << 8) + bios_ram[6];

    bt->par_port0 = (bios_ram[  9] << 8) + bios_ram[  8];
    bt->par_port1 = (bios_ram[0xb] << 8) + bios_ram[0xa];
    bt->par_port2 = (bios_ram[0xd] << 8) + bios_ram[0xc];

    bt->led.scroll_lock = bios_ram[0x97] & 1;
    bt->led.num_lock = (bios_ram[0x97] >> 1) & 1;
    bt->led.caps_lock = (bios_ram[0x97] >> 2) & 1;
    bt->led.ok = 1;

    /*
     * do some consistency checks:
     *
     * ports must be < 0x1000 and not appear twice
     */
    if(bt->ser_port0 >= 0x1000) bt->ser_port0 = 0;

    if(
      bt->ser_port1 >= 0x1000 ||
      bt->ser_port1 == bt->ser_port0
    ) bt->ser_port1 = 0;

    if(
      bt->ser_port2 >= 0x1000 ||
      bt->ser_port2 == bt->ser_port0 ||
      bt->ser_port2 == bt->ser_port1
    ) bt->ser_port2 = 0;

    if(
      bt->ser_port3 >= 0x1000 ||
      bt->ser_port3 == bt->ser_port0 ||
      bt->ser_port3 == bt->ser_port1 ||
      bt->ser_port3 == bt->ser_port2
    ) bt->ser_port3 = 0;

    if(bt->par_port0 >= 0x1000) bt->par_port0 = 0;

    if(
      bt->par_port1 >= 0x1000 ||
      bt->par_port1 == bt->par_port0
    ) bt->par_port1 = 0;

    if(
      bt->par_port2 >= 0x1000 ||
      bt->par_port2 == bt->par_port0 ||
      bt->par_port2 == bt->par_port1
    ) bt->par_port2 = 0;

    ADD2LOG("  bios: %u disks\n", bios_ram[0x75]);

    bt->low_mem_size = ((bios_ram[0x14] << 8) + bios_ram[0x13]) << 10;

    if(bt->low_mem_size >= 768 || bt->low_mem_size < 500) {
      bt->low_mem_size = 0;
    }

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
    get_fsc_info(hd_data, &hd_data->bios_rom, bt);
    add_panel_info(hd_data, bt);
    add_mouse_info(hd_data, bt);
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

    if(!hd_data->klog) read_klog(hd_data);
    for(sl = hd_data->klog; sl; sl = sl->next) {
      if(sscanf(sl->str, "<6>PCI: Using configuration type %u", &u) == 1) {
        hd_data->pci_config_type = u;
        ADD2LOG("  klog: pci config type %u\n", hd_data->pci_config_type);
      }
    }

    get_vbe_info(hd_data, vbe);

    if(vbe->ok) {
      bt->vbe_ver = vbe->version;
    }

    if(vbe->ok && vbe->fb_start) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_framebuffer;
      hd->sub_class.id = sc_fb_vesa;

#if 0
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_bios;
      hd->detail->bios.data = bt = new_mem(sizeof *bt);
#endif

      hd->vendor.name = new_str(vbe->vendor_name);
      hd->device.name = new_str(vbe->product_name);
      hd->sub_vendor.name = new_str(vbe->oem_name);
      hd->revision.name = new_str(vbe->product_revision);

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
            mi->pixel_size != -1u	/* text mode */
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
        hd->device.name &&
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

#if 0
  if(hd_probe_feature(hd_data, pr_bios_32)) {
  }
#endif

  PROGRESS(5, 0, "32");

  mem = hd_data->bios_rom;
  if(mem.data) {
    mem.size -= 0xe0000 - mem.start;
    mem.data += 0xe0000 - mem.start;
    mem.start = 0xe0000;
    if(mem.size < (1 << 20)) get_bios32_info(hd_data, &mem, &bt->bios32);
  }

  if(bt->bios32.ok) {
    mem = hd_data->bios_rom;

    if(
      mem.start <= 0xfffea &&
      mem.start + mem.size >= 0xfffea + 6 &&
      !memcmp(mem.data + 0xfffea - mem.start, "COMPAQ", 6)
    ) {
      bt->bios32.compaq = 1;
      ADD2LOG("  bios32: compaq machine\n");
    }
  }


#endif		/* !defined(LIBHD_TINY) */

}


void read_memory(memory_range_t *mem)
{
  int fd;
#ifdef BIOS_TEST
  char *s = getenv("LIBHD_MEM");
#endif

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
#ifdef BIOS_TEST
      (fd = open(s ? s : DEV_MEM, O_RDONLY)) >= 0 &&
#else
      (fd = open(DEV_MEM, O_RDONLY)) >= 0 &&
#endif
      lseek(fd, mem->start, SEEK_SET) >= 0 &&
      (unsigned) read(fd, mem->data, mem->size) == mem->size
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


#ifndef LIBHD_TINY
void get_pnp_support_status(memory_range_t *mem, bios_info_t *bt)
{
  int i;
  unsigned char pnp[4] = { '$', 'P', 'n', 'P' };
  unsigned char *t;
  unsigned l, cs;

  if(!mem->data) return;

  for(i = 0xf0000 - mem->start; (unsigned) i < mem->size; i += 0x10) {
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
  unsigned addr = 0, len = 0, scnt;
  unsigned structs = 0, type, slen;
  char *s;
  memory_range_t memory;
  hd_smbios_t *sm;

  if(!mem->data || mem->size < 0x100) return;

  for(u = ok = 0; u <= mem->size - 0x100; u += 0x10) {
    if(*(unsigned *) (mem->data + u) == 0x5f4d535f) {	/* "_SM_" */
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

  bt->smbios_ver = (mem->data[u + 6] << 8) + mem->data[u + 7];

  hd_data->smbios = free_smbios_list(hd_data->smbios);

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

  for(type = 0, u = 0, ofs = 0; u < structs && ofs + 3 < len; u++) {
    type = memory.data[ofs];
    slen = memory.data[ofs + 1];
    if(ofs + slen > len || slen < 4) break;
    sm = add_smbios_entry(&hd_data->smbios, new_mem(sizeof *sm));
    sm->any.type = type;
    sm->any.data_len = slen;
    sm->any.data = new_mem(slen);
    memcpy(sm->any.data, memory.data + ofs, slen);
    sm->any.handle = memory.data[ofs + 2] + (memory.data[ofs + 3] << 8);
    ADD2LOG("  type 0x%02x [0x%04x]: ", type, sm->any.handle);
    if(slen) hexdump(&hd_data->log, 0, slen, sm->any.data);
    ADD2LOG("\n");
    if(type == sm_end) break;
    ofs += slen;
    u1 = ofs;
    u2 = 1;
    scnt = 0;
    while(ofs + 1 < len) {
      if(!memory.data[ofs]) {
        if(ofs > u1) {
          s = canon_str(memory.data + u1, strlen(memory.data + u1));
          add_str_list(&sm->any.strings, s);
          scnt++;
          if(*s) ADD2LOG("       str%d: \"%s\"\n", scnt, s);
          free_mem(s);
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

  if(u != structs) {
    if(type == sm_end) {
      ADD2LOG("  smbios: stopped at end tag\n");
    }
    else {
      ADD2LOG("  smbios oops: only %d of %d structs found\n", u, structs);
    }
  }

  memory.data = free_mem(memory.data);

  parse_smbios(hd_data, bt);
}


char *get_string(str_list_t *sl, int index)
{
  if(!sl || !index) return NULL;

  for(; sl; sl = sl->next, index--) {
    if(index == 1) return new_str(sl->str && *sl->str ? sl->str : NULL);
  }

  return NULL;
}

static struct {
  unsigned bit;
  char *name;
} smbios_feature_name1[] = {
  {  4, "ISA supported" },
  {  5, "MCA supported" },
  {  6, "EISA supported" },
  {  7, "PCI supported" },
  {  8, "PCMCIA supported" },
  {  9, "PnP supported" },
  { 10, "APM supported" },
  { 11, "BIOS flashable" },
  { 12, "BIOS shadowing allowed" },
  { 13, "VL-VESA supported" },
  { 14, "ESCD supported" },
  { 15, "CD boot supported" },
  { 16, "Selectable boot supported" },
  { 17, "BIOS ROM socketed" },
  { 18, "PCMCIA boot supported" },
  { 19, "EDD spec supported" },
  { 20, "1.2MB NEC 9800 Japanese Floppy supported" },
  { 21, "1.2MB Toshiba Japanese Floppy supported" },
  { 22, "360kB Floppy supported" },
  { 23, "1.2MB Floppy supported" },
  { 24, "720kB Floppy supported" },
  { 25, "2.88MB Floppy supported" },
  { 26, "Print Screen supported" },
  { 27, "8042 Keyboard Services supported" },
  { 28, "Serial Services supported" },
  { 29, "Printer Services supported" },
  { 30, "CGA/Mono Video supported" },
  { 31, "NEC PC-98" },
  { 64 + 0, "ACPI supported" },
  { 64 + 1, "USB Legacy supported" },
  { 64 + 2, "AGP supported" },
  { 64 + 3, "I2O boot supported" },
  { 64 + 4, "LS-120 boot supported" },
  { 64 + 5, "ATAPI ZIP boot supported" },
  { 64 + 6, "IEEE1394 boot supported" },
  { 64 + 7, "Smart Battery supported" },
  { 64 + 8, "BIOS Boot Spec supported" },
  { 64 + 9, "F12 Network boot supported" }
};


static struct {
  unsigned bit;
  char *name;
} smbios_feature_name2[] = {
  {  0, "hosting board" },
  {  1, "needs one daughter board" },
  {  2, "removable" },
  {  3, "replaceable" },
  {  4, "hot swappable" }
};


static char *smbios_wakeups[] = {
  "Reserved", "Other", "Unknown", "APM Timer",
  "Modem Ring", "LAN Remote", "Power Switch", "PCI PME#",
  "AC Power Restored"
};


static char *smbios_board_types[] = {
  NULL, "Other", "Unknown", "Server Blade",
  "Connectivity Switch", "System Management Module", "Processor Module", "I/O Module",
  "Memory Module", "Daughter Board", "Motherboard", "Processor/Memory Module",
  "Processor/IO Module", "Interconnect Board"
};


static char *smbios_chassis_types[] = {
  NULL, "Other", "Unknown", "Desktop",
  "Low Profile Desktop", "Pizza Box", "Mini Tower", "Tower",
  "Portable", "LapTop", "Notebook", "Hand Held",
  "Docking Station", "All in One", "Sub Notebook", "Space Saving",
  "Lunch Box", "Main Server Chassis", "Expansion Chassis", "SubChassis",
  "Bus Expansion Chassis", "Peripheral Chassis", "RAID Chassis", "Rack Mount Chassis",
  "Sealed-case PC", "Multi-System Chassis"
};


static char *smbios_ch_states[] = {
  NULL, "Other", "Unknown", "Safe",
  "Warning", "Critical", "Non-recoverable"
};


static char *smbios_ch_sec_states[] = {
  NULL, "Other", "Unknown", "None",
  "External interface locked out", "External interface enabled"
};


static char *smbios_upgrades[] = {
  NULL, "Other", "Unknown", "Daughter Board",
  "ZIF Socket", "Replaceable Piggy Back", "None", "LIF Socket",
  "Slot 1", "Slot 2", "370-Pin Socket", "Slot A",
  "Slot M", "Socket 423", "Socket A (Socket 462)", "Socket 478",
  "Socket 754", "Socket 940"
};


static char *smbios_cpu_status[8] = {
  "Unknown", "Enabled", "Disabled by User", "Disabled by BIOS",
  "Idle", "Reserved", "Reserved", "Other"
};


static char *smbios_proc_types[] = {
  NULL, "Other", "Unknown", "CPU",
  "Math", "DSP", "Video"
};


static char *smbios_proc_families[] = {
  NULL, "Other", "Unknown", "8086",
  "80286", "Intel386", "Intel486", "8087",
  "80287", "80387", "80487", "Pentium",
  "Pentium Pro", "Pentium II", "Pentium MMX", "Celeron",
  "Pentium II Xeon", "Pentium III",  "M1", "M2",
  /* 0x14 */
  [0x18] = "Duron", "K5", "K6", "K6-2",
  "K6-3", "Athlon", "AMD2900", "K6-2+",
  /* 0x20 */
  [0x78] = "Crusoe TM5000", "Crusoe TM3000",
  /* 0x7a */
  [0x82] = "Itanium", "Athlon 64", "Opteron Processor",
  /* 0x85 */
  [ 0xb0] = "Pentium III Xeon", "Pentium III with SpeedStep", "Pentium 4", "Xeon",
  "AS400", "Xeon MP", "Athlon XP", "Athlon MP",
  "Itanium 2",
  /* 0xb9 */
};


static char *smbios_cache_mode[] = {
  "Write Through", "Write Back", "Varies with Memory Address", "Unknown"
};


static char *smbios_cache_location[] = {
  "Internal", "External", "Reserved", "Unknown"
};


static char *smbios_cache_ecc[] = {
  NULL, "Other", "Unknown", "None",
  "Parity", "Single-bit", "Multi-bit"
};


static char *smbios_cache_cache_type[] = {
  NULL, "Other", "Unknown", "Instruction",
  "Data", "Unified"
};


static char *smbios_cache_assoc[] = {
  NULL, "Other", "Unknown", "Direct Mapped",
  "2-way Set-Associative", "4-way Set-Associative", "Fully Associative", "8-way Set-Associative",
  "16-way Set-Associative"
};


static struct {
  unsigned bit;
  char *name;
} smbios_cache_sram[] = {
  {  0, "Other" },
  {  1, "Unknown" },
  {  2, "Non-Burst" },
  {  3, "Burst" },
  {  4, "Pipeline Burst" },
  {  5, "Synchronous" },
  {  6, "Asynchronous" }
};


static char *smbios_connect_conn_type[] = {
  NULL, "Centronics", "Mini Centronics", "Proprietary",
  "DB-25 pin male", "DB-25 pin female", "DB-15 pin male", "DB-15 pin female",
  "DB-9 pin male", "DB-9 pin female", "RJ-11", "RJ-45",
  "50 Pin MiniSCSI", "Mini-DIN", "Micro-DIN", "PS/2",
  "Infrared", "HP-HIL", "Access Bus [USB]", "SSA SCSI",
  "Circular DIN-8 male", "Circular DIN-8 female", "On Board IDE", "On Board Floppy",
  "9 Pin Dual Inline [pin 10 cut]", "25 Pin Dual Inline [pin 26 cut]", "50 Pin Dual Inline", "68 Pin Dual Inline",
  "On Board Sound Input from CD-ROM", "Mini-Centronics Type-14", "Mini-Centronics Type-26", "Mini-jack [headphones]",
  "BNC", "1394",
  /* 0x22 */
  [ 0xa0 ] = "PC-98", "PC-98Hireso", "PC-H98", "PC-98Note",
  "PC-98Full",
  /* 0xa5 */
  [ 0xff ] = "Other"
};


static char *smbios_connect_port_type[] = {
  NULL, "Parallel Port XT/AT Compatible", "Parallel Port PS/2", "Parallel Port ECP",
  "Parallel Port EPP", "Parallel Port ECP/EPP", "Serial Port XT/AT Compatible", "Serial Port 16450 Compatible",
  "Serial Port 16550 Compatible", "Serial Port 16550A Compatible", "SCSI Port", "MIDI Port",
  "Joy Stick Port", "Keyboard Port", "Mouse Port", "SSA SCSI",
  "USB", "FireWire [IEEE P1394]", "PCMCIA Type I", "PCMCIA Type II",
  "PCMCIA Type III", "Cardbus", "Access Bus Port", "SCSI II",
  "SCSI Wide", "PC-98", "PC-98-Hireso", "PC-H98",
  "Video Port", "Audio Port", "Modem Port", "Network Port",
  /* 0x20 */
  [ 0xa0 ] = "8251 Compatible", "8251 FIFO Compatible",
  /* 0xa2 */
  [ 0xff ] = "Other"
};


static struct {
  unsigned bit;
  char *name;
} smbios_slot_feature[] = {
  {  0, "Unknown" },
  {  1, "5.0 V" },
  {  2, "3.3 V" },
  {  3, "Shared" },
  {  4, "PC Card-16" },
  {  5, "CardBus" },
  {  6, "Zoom Video" },
  {  7, "Modem Ring Resume" },
  {  8, "PME#" },
  {  9, "Hot-Plug" }
};


static char *smbios_slot_type[] = {
  NULL, "Other", "Unknown", "ISA",
  "MCA", "EISA", "PCI", "PC Card [PCMCIA]",
  "VL-VESA", "Proprietary", "Processor Card", "Proprietary Memory Card",
  "I/O Riser Card", "NuBus", "PCI - 66MHz Capable", "AGP",
  "AGP 2X", "AGP 4X", "PCI-X", "AGP 8X",
  /* 0x14 */
  [ 0xa0 ] = "PC-98/C20", "PC-98/C24", "PC-98/E", "PC-98/Local Bus",
  "PC-98/Card"
  /* 0xa5 */
};


static char *smbios_slot_bus_width[] = {
  NULL, "Other", "Unknown", "8 bit",
  "16 bit", "32 bit", "64 bit", "128 bit"
};


static char *smbios_slot_usage[] = {
  NULL, "Other", "Unknown", "Available",
  "In Use"
};


static char *smbios_slot_length[] = {
  NULL, "Other", "Unknown", "Short",
  "Long"
};


#define SMBIOS_ID2NAME(list, id, default) new_str(list[id < sizeof list / sizeof *list ? id : default])

void parse_smbios(hd_data_t *hd_data, bios_info_t *bt)
{
  hd_smbios_t *sm;
  str_list_t *sl;
  int cnt, data_len;
  unsigned char *sm_data;
  unsigned u, v;

  if(!hd_data->smbios) return;

  for(cnt = 0, sm = hd_data->smbios; sm; sm = sm->next, cnt++) {
    sm_data = sm->any.data;
    data_len = sm->any.data_len;
    sl = sm->any.strings;
    switch(sm->any.type) {
      case sm_biosinfo:
        if(data_len >= 0x12) {
          sm->biosinfo.start = (*(uint16_t *) (sm_data + 6)) << 4;
          sm->biosinfo.rom_size = (sm_data[9] + 1) << 16;
          sm->biosinfo.vendor = get_string(sl, sm_data[4]);
          sm->biosinfo.version = get_string(sl, sm_data[5]);
          sm->biosinfo.date = get_string(sl, sm_data[8]);
          memcpy(&sm->biosinfo.features, sm_data + 0xa, 8);
        }
        if(data_len >= 0x13) {
          sm->biosinfo.xfeatures = sm_data[0x12];
        }
        if(data_len >= 0x14) {
          sm->biosinfo.xfeatures |= sm_data[0x13] << 8;
        }
        for(u = 0; u < sizeof smbios_feature_name1 / sizeof *smbios_feature_name1; u++) {
          v = smbios_feature_name1[u].bit;
          if(v < 64 ? sm->biosinfo.features & (1L << v) : sm->biosinfo.xfeatures & (1 << (v - 64))) {
            add_str_list(&sm->biosinfo.feature_str, smbios_feature_name1[u].name);
          }
        }
        break;

      case sm_sysinfo:
        if(data_len >= 8) {
          sm->sysinfo.manuf = get_string(sl, sm_data[4]);
          sm->sysinfo.product = get_string(sl, sm_data[5]);
          sm->sysinfo.version = get_string(sl, sm_data[6]);
          sm->sysinfo.serial = get_string(sl, sm_data[7]);
        }
        if(data_len >= 0x19) {
          memcpy(sm->sysinfo.uuid, sm_data + 8, 16);
          sm->sysinfo.wake_up.id = sm_data[0x18];
          sm->sysinfo.wake_up.name = SMBIOS_ID2NAME(smbios_wakeups, sm->sysinfo.wake_up.id, 1);
        }
        break;

      case sm_boardinfo:
        if(data_len >= 8) {
          sm->boardinfo.manuf = get_string(sl, sm_data[4]);
          sm->boardinfo.product = get_string(sl, sm_data[5]);
          sm->boardinfo.version = get_string(sl, sm_data[6]);
          sm->boardinfo.serial = get_string(sl, sm_data[7]);
        }
        if(data_len >= 9) {
          sm->boardinfo.asset = get_string(sl, sm_data[8]);
        }
        if(data_len >= 0x0e) {
          sm->boardinfo.features = sm_data[9];
          sm->boardinfo.location = get_string(sl, sm_data[0x0a]);
          sm->boardinfo.chassis = *(uint16_t *) (sm_data + 0x0b);
          sm->boardinfo.board_type.id = sm_data[0x0d];
          sm->boardinfo.board_type.name = SMBIOS_ID2NAME(smbios_board_types, sm->boardinfo.board_type.id, 1);
          for(u = 0; u < sizeof smbios_feature_name2 / sizeof *smbios_feature_name2; u++) {
            if(sm->boardinfo.features & (1 << smbios_feature_name2[u].bit)) {
              add_str_list(&sm->boardinfo.feature_str, smbios_feature_name2[u].name);
            }
          }
        }
        if(data_len >= 0x0f) {
          u = sm_data[0x0e];
          if(u && data_len >= 0x0f + 2 * u) {
            sm->boardinfo.objects_len = u;
            sm->boardinfo.objects = new_mem(u * sizeof *sm->boardinfo.objects);
            for(u = 0; u < sm->boardinfo.objects_len; u++) {
              sm->boardinfo.objects[u] = *(uint16_t *) (sm_data + 0x0f + 2 * u);
            }
          }
        }
        break;

      case sm_chassis:
        if(data_len >= 6) {
          sm->chassis.manuf = get_string(sl, sm_data[4]);
          sm->chassis.lock = sm_data[5] >> 7;
          sm->chassis.ch_type.id = sm_data[5] & 0x7f;
          sm->chassis.ch_type.name = SMBIOS_ID2NAME(smbios_chassis_types, sm->chassis.ch_type.id, 1);
        }
        if(data_len >= 9) {
          sm->chassis.version = get_string(sl, sm_data[6]);
          sm->chassis.serial = get_string(sl, sm_data[7]);
          sm->chassis.asset = get_string(sl, sm_data[8]);
        }
        if(data_len >= 0x0d) {
          sm->chassis.bootup.id = sm_data[9];
          sm->chassis.power.id = sm_data[0x0a];
          sm->chassis.thermal.id = sm_data[0x0b];
          sm->chassis.security.id = sm_data[0x0c];
          sm->chassis.bootup.name = SMBIOS_ID2NAME(smbios_ch_states, sm->chassis.bootup.id, 1);
          sm->chassis.power.name = SMBIOS_ID2NAME(smbios_ch_states, sm->chassis.power.id, 1);
          sm->chassis.thermal.name = SMBIOS_ID2NAME(smbios_ch_states, sm->chassis.thermal.id, 1);
          sm->chassis.security.name = SMBIOS_ID2NAME(smbios_ch_sec_states, sm->chassis.security.id, 1);
        }
        if(data_len >= 0x11) {
          sm->chassis.oem = *(uint32_t *) (sm_data + 0x0d);
        }
        break;

      case sm_processor:
        if(data_len >= 0x1a) {
          sm->processor.socket = get_string(sl, sm_data[4]);
          sm->processor.manuf = get_string(sl, sm_data[7]);
          sm->processor.version = get_string(sl, sm_data[0x10]);
          sm->processor.voltage = sm_data[0x11];
          if(sm->processor.voltage & 0x80) {
            sm->processor.voltage &= 0x7f;
          }
          else {
            switch(sm->processor.voltage) {
              case 0x01:
                sm->processor.voltage = 50;
                break;
              case 0x02:
                sm->processor.voltage = 33;
                break;
              case 0x04:
                sm->processor.voltage = 29;
                break;
              default:
                sm->processor.voltage = 0;
            }
          }
          sm->processor.pr_type.id = sm_data[5];
          sm->processor.family.id = sm_data[6];
          sm->processor.cpu_id = *(uint64_t *) (sm_data + 8);
          sm->processor.ext_clock = *(uint16_t *) (sm_data + 0x12);
          sm->processor.max_speed = *(uint16_t *) (sm_data + 0x14);
          sm->processor.current_speed = *(uint16_t *) (sm_data + 0x16);
          sm->processor.sock_status = (sm_data[0x18] >> 6) & 1;
          sm->processor.cpu_status.id = sm_data[0x18] & 7;
          sm->processor.upgrade.id = sm_data[0x19];
          sm->processor.pr_type.name = SMBIOS_ID2NAME(smbios_proc_types, sm->processor.pr_type.id, 1);
          sm->processor.family.name = SMBIOS_ID2NAME(smbios_proc_families, sm->processor.family.id, 1);
          if(!sm->processor.family.name && sm->processor.family.id) sm->processor.family.name = new_str("Other");
          sm->processor.cpu_status.name = SMBIOS_ID2NAME(smbios_cpu_status, sm->processor.cpu_status.id, 0);
          sm->processor.upgrade.name = SMBIOS_ID2NAME(smbios_upgrades, sm->processor.upgrade.id, 1);
        }
        if(data_len >= 0x20) {
          sm->processor.l1_cache = *(uint16_t *) (sm_data + 0x1a);
          sm->processor.l2_cache = *(uint16_t *) (sm_data + 0x1c);
          sm->processor.l3_cache = *(uint16_t *) (sm_data + 0x1e);
          if(sm->processor.l1_cache == 0xffff) sm->processor.l1_cache = 0;
          if(sm->processor.l2_cache == 0xffff) sm->processor.l2_cache = 0;
          if(sm->processor.l3_cache == 0xffff) sm->processor.l3_cache = 0;
        }
        if(data_len >= 0x21) {
          sm->processor.serial = get_string(sl, sm_data[0x20]);
        }
        if(data_len >= 0x22) {
          sm->processor.asset = get_string(sl, sm_data[0x21]);
          sm->processor.part = get_string(sl, sm_data[0x22]);
        }
        break;

      case sm_cache:
        if(data_len >= 0x0f) {
          sm->cache.socket = get_string(sl, sm_data[4]);
          u = *(uint16_t *) (sm_data + 7);
          if((u & 0x8000)) u = (u & 0x7fff) << 6;
          sm->cache.max_size = u;
          u = *(uint16_t *) (sm_data + 9);
          if((u & 0x8000)) u = (u & 0x7fff) << 6;
          sm->cache.current_size = u;
          u = *(uint16_t *) (sm_data + 5);
          sm->cache.mode.id = (u >> 8) & 3;
          sm->cache.state = (u >> 7) & 1;
          sm->cache.location.id = (u >> 5) & 3;
          sm->cache.socketed = (u >> 3) & 1;
          sm->cache.level = u & 7;
          sm->cache.mode.name = SMBIOS_ID2NAME(smbios_cache_mode, sm->cache.mode.id, 0);
          sm->cache.location.name = SMBIOS_ID2NAME(smbios_cache_location, sm->cache.location.id, 0);
          sm->cache.supp_sram = *(uint16_t *) (sm_data + 0x0b);
          for(u = 0; u < sizeof smbios_cache_sram / sizeof *smbios_cache_sram; u++) {
            if(sm->cache.supp_sram & (1 << smbios_cache_sram[u].bit)) {
              add_str_list(&sm->cache.supp_sram_str, smbios_cache_sram[u].name);
            }
          }
          sm->cache.sram = *(uint16_t *) (sm_data + 0x0d);
          for(u = 0; u < sizeof smbios_cache_sram / sizeof *smbios_cache_sram; u++) {
            if(sm->cache.sram & (1 << smbios_cache_sram[u].bit)) {
              add_str_list(&sm->cache.sram_str, smbios_cache_sram[u].name);
            }
          }
        }
        if(data_len >= 0x13) {
          sm->cache.speed = sm_data[0x0f];
          sm->cache.ecc.id = sm_data[0x10];
          sm->cache.cache_type.id = sm_data[0x11];
          sm->cache.assoc.id = sm_data[0x12];
          sm->cache.ecc.name = SMBIOS_ID2NAME(smbios_cache_ecc, sm->cache.ecc.id, 1);
          sm->cache.cache_type.name = SMBIOS_ID2NAME(smbios_cache_cache_type, sm->cache.cache_type.id, 1);
          sm->cache.assoc.name = SMBIOS_ID2NAME(smbios_cache_assoc, sm->cache.assoc.id, 1);
        }
        break;

      case sm_connect:
        if(data_len >= 9) {
          sm->connect.i_des = get_string(sl, sm_data[4]);
          sm->connect.x_des = get_string(sl, sm_data[6]);
          sm->connect.i_type.id = sm_data[5];
          sm->connect.x_type.id = sm_data[7];
          sm->connect.port_type.id = sm_data[8];
          sm->connect.i_type.name = SMBIOS_ID2NAME(smbios_connect_conn_type, sm->connect.i_type.id, 0xff);
          sm->connect.x_type.name = SMBIOS_ID2NAME(smbios_connect_conn_type, sm->connect.x_type.id, 0xff);
          sm->connect.port_type.name = SMBIOS_ID2NAME(smbios_connect_port_type, sm->connect.port_type.id, 0xff);
          if(!sm->connect.i_type.name && sm->connect.i_type.id) sm->connect.i_type.name = new_str("Other");
          if(!sm->connect.x_type.name && sm->connect.x_type.id) sm->connect.x_type.name = new_str("Other");
          if(!sm->connect.port_type.name && sm->connect.port_type.id) sm->connect.port_type.name = new_str("Other");
        }
        break;

      case sm_slot:
        if(data_len >= 0x0c) {
          sm->slot.desig = get_string(sl, sm_data[4]);
          sm->slot.slot_type.id = sm_data[5];
          sm->slot.bus_width.id = sm_data[6];
          sm->slot.usage.id = sm_data[7];
          sm->slot.length.id = sm_data[8];
          sm->slot.id = *(uint16_t *) (sm_data + 9);
          sm->slot.features = sm_data[0x0b];
        }
        if(data_len >= 0x0d) {
          sm->slot.features += sm_data[0x0c] << 8;
        }
        sm->slot.slot_type.name = SMBIOS_ID2NAME(smbios_slot_type, sm->slot.slot_type.id, 1);
        sm->slot.bus_width.name = SMBIOS_ID2NAME(smbios_slot_bus_width, sm->slot.bus_width.id, 1);
        sm->slot.usage.name = SMBIOS_ID2NAME(smbios_slot_usage, sm->slot.usage.id, 1);
        sm->slot.length.name = SMBIOS_ID2NAME(smbios_slot_length, sm->slot.length.id, 1);
        for(u = 0; u < sizeof smbios_slot_feature / sizeof *smbios_slot_feature; u++) {
          if(sm->slot.features & (1 << smbios_slot_feature[u].bit)) {
            add_str_list(&sm->slot.feature_str, smbios_slot_feature[u].name);
          }
        }
        break;

      case sm_onboard:
        if(data_len >= 4) {
          u = data_len - 4;
          if(!(u & 1)) {
            u >>= 1;
            if(u > sizeof sm->onboard.descr / sizeof *sm->onboard.descr) {
              u = sizeof sm->onboard.descr / sizeof *sm->onboard.descr;
            }
            for(v = 0; v < u; v++) {
              sm->onboard.descr[v] = get_string(sl, sm_data[4 + (v << 1) + 1]);
              sm->onboard.dtype[v] = sm_data[4 + (v << 1)];
            }
          }
        }
        break;

      case sm_lang:
        if(data_len >= 0x16) {
          sm->lang.current = get_string(sl, sm_data[0x15]);
        }
        break;

      case sm_memarray:
        if(data_len >= 0x0b) {
          sm->memarray.ecc = sm_data[6];
          memcpy(&sm->memarray.max_size, sm_data + 0x7, 4);
        }
        break;

      case sm_memdevice:
        if(data_len >= 0x15) {
          sm->memdevice.eccbits = *(uint16_t *) (sm_data + 8);
          sm->memdevice.width = *(uint16_t *) (sm_data + 0xa);
          if(sm->memdevice.width == 0xffff) sm->memdevice.width = 0;
          if(sm->memdevice.eccbits == 0xffff) sm->memdevice.eccbits = 0;
          if(sm->memdevice.eccbits >= sm->memdevice.width) {
            sm->memdevice.eccbits -= sm->memdevice.width;
          }
          else {
            sm->memdevice.eccbits = 0;
          }
          sm->memdevice.size = *(uint16_t *) (sm_data + 0xc);
          if(sm->memdevice.size == 0xffff) sm->memdevice.size = 0;
          if((sm->memdevice.size & 0x8000)) {
            sm->memdevice.size &= 0x7fff;
          }
          else {
            sm->memdevice.size <<= 10;
          }
          sm->memdevice.form = sm_data[0xe];
          sm->memdevice.location = get_string(sl, sm_data[0x10]);
          sm->memdevice.bank = get_string(sl, sm_data[0x11]);
          sm->memdevice.type1 = sm_data[0x12];
          sm->memdevice.type2 = sm_data[0x13] + (sm_data[0x14] << 8);
        }
        if(data_len >= 0x17) {
          sm->memdevice.speed = sm_data[0x15] + (sm_data[0x16] << 8);
        }
        break;

      case sm_mouse:
        if(data_len >= 7) {
          sm->mouse.mtype = sm_data[4];
          sm->mouse.interface = sm_data[5];
          sm->mouse.buttons = sm_data[6];
        }
        break;

      default:
	break;
    }
  }
}

#undef SMBIOS_ID2NAME


void get_fsc_info(hd_data_t *hd_data, memory_range_t *mem, bios_info_t *bt)
{
  unsigned u, mtype, fsc_id;
  unsigned x, y;
  hd_smbios_t *sm;
  char *vendor = NULL;

  if(!mem->data || mem->size < 0x20) return;

  for(sm = hd_data->smbios; sm; sm = sm->next) {
    if(sm->any.type == sm_sysinfo) {
      vendor = sm->sysinfo.manuf;
      break;
    }
  }

  vendor = vendor && !strcasecmp(vendor, "Fujitsu") ? "Fujitsu" : "Fujitsu Siemens";

  for(u = 0; u <= mem->size - 0x20; u += 0x10) {
    if(
      *(unsigned *) (mem->data + u) == 0x696a7546 &&
      *(unsigned *) (mem->data + u + 4) == 0x20757374
    ) {
      mtype = *(unsigned *) (mem->data + u + 0x14);
      if(!crc(mem->data + u, 0x20) && !(mtype & 0xf0000000)) {
        fsc_id = (mtype >> 12) & 0xf;

        switch(fsc_id) {
          case 1:
            x = 640; y = 480;
            break;

          case 2:
            x = 800; y = 600;
            break;

          case 3:
            x = 1024; y = 768;
            break;

          case 4:
            x = 1280; y = 1024;
            break;

          case 5:
            x = 1400; y = 1050;
            break;

          case 6:
            x = 1024; y = 512;
            break;

          case 7:
            x = 1280; y = 600;
            break;

          case 8:
            x = 1600; y = 1200;
            break;

          default:
            x = 0; y = 0;
        }

        if(x) {
          bt->lcd.vendor = new_str(vendor);
          bt->lcd.name = new_str("Notebook LCD");
          bt->lcd.width = x;
          bt->lcd.height = y;
        }

        ADD2LOG("  found FSC LCD: %d (%ux%u)\n", fsc_id, x, y);
        break;
      }
    }
  }
}


void add_panel_info(hd_data_t *hd_data, bios_info_t *bt)
{
  unsigned width, height;
  char *vendor, *name, *version;
  hd_smbios_t *sm;
  unsigned u;

  if(bt->lcd.width || !hd_data->smbios) return;

  vendor = name = version = NULL;
  width = height = 0;

  for(sm = hd_data->smbios; sm; sm = sm->next) {
    if(sm->any.type == sm_sysinfo) {
      vendor = sm->sysinfo.manuf;
      name = sm->sysinfo.product;
      version = sm->sysinfo.version;
      break;
    }
  }

  if(!vendor || !name) return;

  for(u = 0; u < sizeof panel_data / sizeof *panel_data; u++) {
    if(
      !strcmp(vendor, panel_data[u].vendor) &&
      !strcmp(name, panel_data[u].name) &&
      (version || !panel_data[u].version) &&
      (!version || !panel_data[u].version || !strcmp(version, panel_data[u].version))
    ) {
      bt->lcd.vendor = new_str(vendor);
      bt->lcd.name = new_str("Notebook LCD");
      bt->lcd.width = panel_data[u].width;
      bt->lcd.height = panel_data[u].height;
      break;
    }
  }
}


void add_mouse_info(hd_data_t *hd_data, bios_info_t *bt)
{
  unsigned compat_vend, compat_dev, bus;
  char *vendor, *name, *type;
  hd_smbios_t *sm;
  static char *mice[] = {
    NULL, "Touch Pad" /* normally "Other" */, NULL, "Mouse",
    "Track Ball", "Track Point", "Glide Point", "Touch Pad"
  };

  if(bt->mouse.compat_vend || !hd_data->smbios) return;

  vendor = name = type = NULL;
  compat_vend = compat_dev = bus = 0;

  for(sm = hd_data->smbios; sm; sm = sm->next) {
    if(sm->any.type == sm_sysinfo) {
      vendor = sm->sysinfo.manuf;
      name = sm->sysinfo.product;
    }
    if(
      sm->any.type == sm_mouse &&
      !compat_vend	/* take the first entry */
    ) {
      compat_vend = compat_dev = bus = 0;
      type = NULL;
      
      switch(sm->mouse.interface) {
        case 4:	/* ps/2 */
        case 7:	/* bus mouse (dell notebooks report this) */
          bus = bus_ps2;
          compat_vend = MAKE_ID(TAG_SPECIAL, 0x0200);
          compat_dev = MAKE_ID(TAG_SPECIAL, sm->mouse.buttons == 3 ? 0x0007 : 0x0006);
          break;
      }
      type = mice[sm->mouse.mtype < sizeof mice / sizeof *mice ? sm->mouse.mtype : 0];
    }
  }

  if(!vendor || !name) return;

  if(!type) {
    if(!strcmp(vendor, "Sony Corporation") && strstr(name, "PCG-") == name) {
      bus = bus_ps2;
      type = mice[7];
      compat_vend = MAKE_ID(TAG_SPECIAL, 0x0200);
      compat_dev = MAKE_ID(TAG_SPECIAL, 0x0006);
    }
  }

  if(!type) return;

  bt->mouse.vendor = new_str(vendor);
  bt->mouse.type = new_str(type);
  bt->mouse.bus = bus;
  bt->mouse.compat_vend = compat_vend;
  bt->mouse.compat_dev = compat_dev;
}


int get_smp_info(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp)
{
#ifndef __ia64__
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
#else
  return 0;
#endif
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


int get_bios32_info(hd_data_t *hd_data, memory_range_t *mem, bios32_info_t *bios32)
{
  unsigned u, ok;
  unsigned addr = 0, len;

  if(mem->size < 0x10) return 0;

  for(u = ok = 0; u <= mem->size - 0x10; u++) {
    if(*(unsigned *) (mem->data + u) == 0x5f32335f) {	/* "_32_" */
      addr = *(unsigned *) (mem->data + u + 4);
      len = mem->data[u + 9];
      ok = len == 1 && crc(mem->data + u, 0x10) == 0 && addr < (1 << 20) ? 1 : 0;
      ADD2LOG(
        "  bios32: %svalid SD header at 0x%05x (size 0x%x, rev %u), SD at 0x%05x\n",
        ok ? "" : "in", u + mem->start, len << 4, mem->data[u + 8], addr
      );
      if(ok) break;
    }
  }

  if(ok) {
    bios32->ok = 1;
    bios32->entry = addr;
  }

  return ok;
}


#endif		/* !defined(LIBHD_TINY) */


#endif /* defined(__i386__) || defined (__x86_64__) || defined (__ia64__) */

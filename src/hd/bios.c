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
#include "smbios.h"
#include "klog.h"

/**
 * @defgroup BIOSint BIOS information
 * @ingroup  libhdINFOint
 * @brief BIOS information scan
 *
 * @{
 */

#if defined(__i386__) || defined (__x86_64__) || defined (__ia64__)

#include "ibm-notebooks.h"
static int tp_lookup(char *key, unsigned *width, unsigned *height, unsigned *xsize, unsigned *ysize);

static struct {
  unsigned short xsize;	/* mm */
  unsigned short ysize;	/* mm */
  unsigned short width;
  unsigned short height;
  char *vendor;
  char *name;
  char *version;
} __attribute((packed)) panel_data[] = {
  {   0,   0, 1400, 1050, "IBM", "73geu99", NULL },
  {   0,   0,  800,  600, "Fujitsu Siemens", "LiteLine", "LF6" },
  {   0,   0, 1024,  768, "ASUSTEK", "L2000D", NULL },
  {   0,   0, 1024,  768, "ASUSTeK Computer Inc.", "L8400C series Notebook PC", NULL },
  {   0,   0, 1024,  768, "ASUSTeK Computer Inc.", "S5N", NULL },
  {   0,   0, 1024,  768, "Acer", "TravelMate 720", NULL },
  {   0,   0, 1024,  768, "COMPAL", "N30T5", NULL },
  {   0,   0, 1024,  768, "Dell Computer Corporation", "Inspiron 5000", NULL },
  {   0,   0, 1024,  768, "Dell Computer Corporation", "Latitude C400", NULL },
  {   0,   0, 1024,  768, "Dell Computer Corporation", "Latitude C600", NULL },
  {   0,   0, 1024,  768, "Dell Computer Corporation", "Latitude CPt C400GT", NULL },
  {   0,   0, 1024,  768, "Dell Computer Corporation", "Latitude CPx J650GT", NULL },
  {   0,   0, 1024,  768, "Hewlett-Packard", "HP OmniBook PC", "HP OmniBook 4150 B" },
  {   0,   0, 1280,  800, "Hewlett-Packard", "hp compaq nx9105 (DU367T#ABD)", "03" },
  { 330, 210, 1280,  800, "Hewlett-Packard", "hp compaq nx9105 (DU367T#ABD)", "F.21" },
  {   0,   0, 1280,  800, "Hewlett-Packard", "Pavilion zv5000 (PA456EA#ABD)", "F.11" },
  {   0,   0, 1024,  768, "KDST", "KDS6KSUMO", NULL  },
  {   0,   0, 1024,  768, "Sony Corporation", "PCG-F370(UC)", NULL },
  {   0,   0, 1024,  768, "Sony Corporation", "PCG-N505SN", NULL },
  {   0,   0, 1024,  768, "TOSHIBA", "S2400-103", NULL },
  {   0,   0, 1280,  800, "Acer", "Aspire 1520", NULL },
  {   0,   0, 1440,  900, "FUJITSU SIEMENS", "Amilo M3438 Series", NULL },
  {   0,   0, 1400, 1050, "Acer", "TravelMate 660", NULL },
  {   0,   0, 1400, 1050, "Dell Computer Corporation", "Inspiron 8000", NULL },
  { 286, 214, 1400, 1050, "Dell Computer Corporation", "Latitude D600", NULL },
  {   0,   0, 1400, 1050, "TOSHIBA", "TECRA 9100", NULL },
  {   0,   0, 1600, 1200, "Dell Computer Corporation", "Inspiron 8200", NULL },
  {   0,   0, 1600, 1200, "Dell Computer Corporation", "Latitude C840", NULL },
  { 190, 110, 1024,  600, "FUJITSU SIEMENS", "LIFEBOOK P1510", NULL },
  { 285, 215, 1024,  768, "FUJITSU SIEMENS", "LIFEBOOK S7020", NULL },
  { 305, 230, 1400, 1050, "Samsung Electronics", "SX20S", "Revision PR" },
  { 290, 200, 1280,  800, "TOSHIBA", "Satellite M30X", "PSA72E-00J020GR" },
  { 305, 230, 1400, 1050, "ASUSTeK Computer Inc.", "M6V", "1.0" },
  { 246, 185, 1024,  768, "FUJITSU SIEMENS", "LifeBook T Series", NULL },
  { 304, 190, 1400, 1050, "Hewlett-Packard", "HP Compaq nc6000 (PN887PA#ABG)", "F.0F" },
  { 332, 208, 1280,  800, "MICRO-STAR INT'L CO.,LTD.", "MS-1011", "0101" },
  { 304, 189, 1440,  900, "Dell Inc.", "Latitude D630C", NULL },
  { 305, 229, 1024,  768, "IBM CORPORATION", "4840563", NULL },
  { 337, 270, 1280, 1024, "IBM CORPORATION", "4840573", NULL },
  { 305, 229, 1024,  768, "IBM CORPORATION", "4836135", NULL },
  { 305, 229, 1024,  768, "IBM CORPORATION", "48361Z5", NULL },
  { 305, 229, 1024,  768, "IBM CORPORATION", "48361z5", NULL },
  { 305, 229, 1024,  768, "IBM CORPORATION", "48381Z5", NULL },
  { 305, 229, 1024,  768, "IBM CORPORATION", "48381z5", NULL },
  { 337, 270, 1280, 1024, "IBM CORPORATION", "4836137", NULL },
  { 337, 270, 1280, 1024, "IBM CORPORATION", "48361Z7", NULL },
  { 337, 270, 1280, 1024, "IBM CORPORATION", "48361z7", NULL },
  { 337, 270, 1280, 1024, "IBM CORPORATION", "4838137", NULL },
  { 337, 270, 1280, 1024, "IBM CORPORATION", "48381Z7", NULL },
  { 337, 270, 1280, 1024, "IBM CORPORATION", "48381z7", NULL },
  { 367, 230, 1440,  900, "Hewlett-Packard", "HP Pavilion dv7 Notebook PC", "F.12" },
};

#define BIOS_TEST

typedef struct {
  unsigned eax, ebx, ecx, edx, esi, edi, eip, es, iret, cli;
} bios32_regs_t;

static void read_memory(hd_data_t *hd_data, memory_range_t *mem);
static void dump_memory(hd_data_t *hd_data, memory_range_t *mem, int sparse, char *label);
static void get_pnp_support_status(memory_range_t *mem, bios_info_t *bt);
static void smbios_get_info(hd_data_t *hd_data, memory_range_t *mem, bios_info_t *bt);
static void get_fsc_info(hd_data_t *hd_data, memory_range_t *mem, bios_info_t *bt);
#ifndef LIBHD_TINY
static void add_panel_info(hd_data_t *hd_data, bios_info_t *bt);
#endif
static void add_mouse_info(hd_data_t *hd_data, bios_info_t *bt);
static void chk_vbox(hd_data_t *hd_data);
static unsigned char crc(unsigned char *mem, unsigned len);
static int get_smp_info(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp);
static unsigned parse_mpconfig_len(hd_data_t *hd_data, memory_range_t *mem);
static void parse_mpconfig(hd_data_t *hd_data, memory_range_t *mem, smp_info_t *smp);
static int get_bios32_info(hd_data_t *hd_data, memory_range_t *mem, bios32_info_t *bios32);

int detect_smp_bios(hd_data_t *hd_data)
{
  bios_info_t *bt;
  hd_smbios_t *sm;
  hd_t *hd;
  int cpus;

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

  cpus = 0;

  /* look at smbios data in case there's no mp table */
  if(hd_data->smbios) {
    for(sm = hd_data->smbios; sm; sm = sm->next) {
      if(
        sm->any.type == sm_processor &&
        sm->processor.pr_type.id == 3 &&	/* cpu */
        sm->processor.cpu_status.id == 1	/* enabled */
      ) {
        cpus++;
      }
    }
    ADD2LOG("  smp detect: mp %d cpus, smbios %d cpus\n", bt->smp.ok ? bt->smp.cpus_en : 0, cpus);
  }

  if(bt->smp.ok && bt->smp.cpus_en) cpus = bt->smp.cpus_en;

  return cpus;
}


void hd_scan_bios(hd_data_t *hd_data)
{
  hd_t *hd;
  bios_info_t *bt;
  char *s;
  unsigned char *bios_ram;
  unsigned u, u1;
  memory_range_t mem;
  unsigned smp_ok;
#ifndef LIBHD_TINY
  vbe_info_t *vbe;
  vbe_mode_info_t *mi;
  hd_res_t *res;
  str_list_t *sl, *sl0;
#endif

  if(!hd_probe_feature(hd_data, pr_bios)) return;

  /* we better do nothing on a SGI Altix machine */
  if(hd_is_sgi_altix(hd_data)) return;

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

  /*
   * get the i/o ports for the parallel & serial interfaces from the BIOS
   * memory area starting at 0x40:0
   */
  PROGRESS(2, 0, "ram");

  hd_data->bios_ram.start = BIOS_RAM_START;
  hd_data->bios_ram.size = BIOS_RAM_SIZE;
  read_memory(hd_data, &hd_data->bios_ram);

  hd_data->bios_rom.start = BIOS_ROM_START;
  hd_data->bios_rom.size = BIOS_ROM_SIZE;
  read_memory(hd_data, &hd_data->bios_rom);

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

    if(bt->low_mem_size) {
      ADD2LOG("  bios: %uk low mem\n", bt->low_mem_size >> 10);
    }

    /* too unusual */
    if(bt->low_mem_size >= (768 << 10) || bt->low_mem_size < (384 << 10)) {
      bt->low_mem_size = 0;
    }

    hd_data->bios_ebda.start = hd_data->bios_ebda.size = 0;
    hd_data->bios_ebda.data = free_mem(hd_data->bios_ebda.data);
    u = ((bios_ram[0x0f] << 8) + bios_ram[0x0e]) << 4;
    if(u) {
      hd_data->bios_ebda.start = u;
      hd_data->bios_ebda.size = 1;	/* just one byte */
      read_memory(hd_data, &hd_data->bios_ebda);
      if(hd_data->bios_ebda.data) {
        u1 = hd_data->bios_ebda.data[0];
        if(u1 > 0 && u1 <= 64) {	/* be sensible, typically only 1k */
          u1 <<= 10;
          if(u + u1 <= (1 << 20)) {
            hd_data->bios_ebda.size = u1;
            read_memory(hd_data, &hd_data->bios_ebda);
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
    smbios_get_info(hd_data, &hd_data->bios_rom, bt);
    get_fsc_info(hd_data, &hd_data->bios_rom, bt);
#ifndef LIBHD_TINY
    add_panel_info(hd_data, bt);
#endif
    add_mouse_info(hd_data, bt);
    chk_vbox(hd_data);
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
    read_memory(hd_data, &mem);
    if(mem.data) smp_ok = get_smp_info(hd_data, &mem, &bt->smp);
    mem.data = free_mem(mem.data);
  }

  if(bt->smp.ok && bt->smp.mpconfig) {
    mem.start = bt->smp.mpconfig;
    mem.size = 0x40;
    mem.data = NULL;
    read_memory(hd_data, &mem);
    u = parse_mpconfig_len(hd_data, &mem);
    ADD2LOG("  MP config table size: %u\n", u);
    if(u > 0x40) {
      mem.size = u < (1 << 16) ? u : (1 << 16);
      read_memory(hd_data, &mem);
    }
    if(u) parse_mpconfig(hd_data, &mem, &bt->smp);
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
      read_memory(hd_data, &mem);
      dump_memory(hd_data, &mem, 0, "MP FP");
      mem.data = free_mem(mem.data);
    }

    if(bt->smp.ok && bt->smp.mpconfig && bt->smp.mpconfig_size) {
      mem.start = bt->smp.mpconfig;
      mem.size = bt->smp.mpconfig_size;
      mem.data = NULL;
      read_memory(hd_data, &mem);
      dump_memory(hd_data, &mem, 0, "MP config table");
      mem.data = free_mem(mem.data);
    }
  }

#ifndef LIBHD_TINY
  if(hd_probe_feature(hd_data, pr_bios_vesa)) {
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

      hd_set_hw_class(hd, hw_vbe);

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
#endif	/* LIBHD_TINY */

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

  if(hd_probe_feature(hd_data, pr_bios_acpi)) {
    PROGRESS(6, 0, "acpi");

    if((sl0 = read_file("|/usr/sbin/acpidump 2>/dev/null", 0, 0))) {
      ADD2LOG("----- %s -----\n", "ACPI dump");
      for(sl = sl0; sl; sl = sl->next) {
        ADD2LOG("%s", sl->str);
      }
      ADD2LOG("----- %s end -----\n", "ACPI dump");
      free_str_list(sl0);
    }
  }
}


void read_memory(hd_data_t *hd_data, memory_range_t *mem)
{
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
#ifdef BIOS_TEST
  hd_read_mmap(hd_data, s ?: DEV_MEM, mem->data, mem->start, mem->size);
#else
  hd_read_mmap(hd_data, DEV_MEM, mem->data, mem->start, mem->size);
#endif

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
    hexdump(&hd_data->log, 1, mem->size - u > 0x10 ? 0x10 : mem->size - u, mem->data + u);
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


void smbios_get_info(hd_data_t *hd_data, memory_range_t *mem, bios_info_t *bt)
{
  unsigned u, u1, u2, ok, hlen = 0, ofs;
  unsigned addr = 0, len = 0, scnt;
  unsigned structs = 0, type, slen;
  unsigned use_sysfs = 0;
  char *s;
  memory_range_t memory, memory_sysfs;
  hd_smbios_t *sm;

  // looking for smbios data in 3 places:

  // 1st try: look it up in sysfs

  memory_sysfs.data = get_sysfs_attr_by_path2("/sys/firmware/dmi/tables", "smbios_entry_point", &memory_sysfs.size);

  if(memory_sysfs.data) {
    // get_sysfs_attr_by_path2 returns static buffer; make a copy
    unsigned char *buf = memory_sysfs.data;
    memory_sysfs.data = new_mem(memory_sysfs.size);
    memcpy(memory_sysfs.data, buf, memory_sysfs.size);
    memory_sysfs.start = 0;
    dump_memory(hd_data, &memory_sysfs, 0, "SMBIOS Entry Point (sysfs)");
    if(memory_sysfs.size >= 0x10) {
      use_sysfs = 1;
      mem = &memory_sysfs;
    }
  }
  else {
    // 2nd try: look entry point up in EFI variables

    char *t;
    char *s = get_sysfs_attr_by_path("/sys/firmware/efi", "systab");
    if(s && (t = strstr(s, "SMBIOS="))) {
      unsigned start_ofs = strtoul(t + sizeof "SMBIOS=" - 1, NULL, 0);
      if(start_ofs) {
        memory_sysfs.size = 0x20;
        memory_sysfs.start = start_ofs;
        read_memory(hd_data, &memory_sysfs);
        dump_memory(hd_data, &memory_sysfs, 0, "SMBIOS Entry Point (efi)");
        mem = &memory_sysfs;
      }
    }
  }

  // 3rd try: scan legacy BIOS

  if(!mem->data || mem->size < 0x10) return;

  for(u = ok = 0; u <= mem->size - 0x10; u += 0x10) {
    if(*(unsigned *) (mem->data + u) == 0x5f4d535f) {	/* "_SM_" */
      hlen = mem->data[u + 5];
      if(hlen < 0x1e || u + hlen > mem->size) continue;
      addr = *(unsigned *) (mem->data + u + 0x18);
      len = *(unsigned short *) (mem->data + u + 0x16);
      structs = *(unsigned short *) (mem->data + u + 0x1c);
      ok = crc(mem->data + u, hlen) == 0 && len;
      if(ok) {
        bt->smbios_ver = (mem->data[u + 6] << 8) + mem->data[u + 7];
        break;
      }
    }
    /* Also look for legacy DMI entry point */
    if(memcmp(mem->data + u, "_DMI_", 5) == 0) {
      hlen = 0x0f;
      addr = *(unsigned *) (mem->data + u + 0x08);
      len = *(unsigned short *) (mem->data + u + 0x06);
      structs = *(unsigned short *) (mem->data + u + 0x0c);
      ok = crc(mem->data + u, hlen) == 0 && len;
      if(ok) {
        bt->smbios_ver = ((mem->data[u + 0x0e] & 0xf0) << 4) + (mem->data[u + 0x0e] & 0x0f);
        break;
      }
    }
  }

  if(!ok) return;

  hd_data->smbios = smbios_free(hd_data->smbios);

  ADD2LOG("  Found DMI table at 0x%08x (0x%04x bytes)\n", addr, len);

  memory.start = mem->start + u;
  memory.size = hlen;
  memory.data = mem->data + u;
  if(!use_sysfs) dump_memory(hd_data, &memory, 0, "SMBIOS Entry Point");

  memory.data = NULL;
  memory.start = addr;

  if(use_sysfs) {
    memory.data = get_sysfs_attr_by_path2("/sys/firmware/dmi/tables", "DMI", &memory.size);
    if(memory.data) {
      // get_sysfs_attr_by_path2 returns static buffer; make a copy
      unsigned char *buf = memory.data;
      memory.data = new_mem(memory.size);
      memcpy(memory.data, buf, memory.size);
      ADD2LOG("  Got DMI table from sysfs (0x%04x bytes)\n", memory.size);
      if(memory.size != len) {
        ADD2LOG("  Oops: DMI table size mismatch; expected 0x%04x bytes!\n", len);
      }
    }
  }

  if(!memory.data) {
    memory.size = len;
    read_memory(hd_data, &memory);
  }

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
    sm = smbios_add_entry(&hd_data->smbios, new_mem(sizeof *sm));
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
  memory_sysfs.data = free_mem(memory_sysfs.data);

  smbios_parse(hd_data);
}


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


#ifndef LIBHD_TINY
void add_panel_info(hd_data_t *hd_data, bios_info_t *bt)
{
  unsigned width, height, xsize = 0, ysize = 0;
  char *vendor, *name, *version;
  hd_smbios_t *sm;
  unsigned u;

  if(!hd_data->smbios) return;

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

  if(
    !strcmp(vendor, "IBM") &&
    tp_lookup(name, &width, &height, &xsize, &ysize)
  ) {
    bt->lcd.vendor = new_str(vendor);
    bt->lcd.name = new_str("Notebook LCD");
    bt->lcd.width = width;
    bt->lcd.height = height;
    bt->lcd.xsize = xsize;
    bt->lcd.ysize = ysize;

    return;
  }

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
      bt->lcd.xsize = panel_data[u].xsize;
      bt->lcd.ysize = panel_data[u].ysize;
      break;
    }
  }
}
#endif

void add_mouse_info(hd_data_t *hd_data, bios_info_t *bt)
{
  unsigned compat_vend, compat_dev, bus;
  char *vendor, *name, *type;
  hd_smbios_t *sm;

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
      
      switch(sm->mouse.interface.id) {
        case 4:	/* ps/2 */
        case 7:	/* bus mouse (dell notebooks report this) */
          bus = bus_ps2;
          compat_vend = MAKE_ID(TAG_SPECIAL, 0x0200);
          compat_dev = MAKE_ID(TAG_SPECIAL, sm->mouse.buttons == 3 ? 0x0007 : 0x0006);
          break;
      }
      type = sm->mouse.mtype.name;
      if(sm->mouse.mtype.id == 1) type = "Touch Pad";	/* Why??? */
      if(sm->mouse.mtype.id == 2) type = NULL;		/* "Other" */
    }
  }

  if(!vendor || !name) return;

  if(!type) {
    if(!strcmp(vendor, "Sony Corporation") && strstr(name, "PCG-") == name) {
      bus = bus_ps2;
      type = "Touch Pad";
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


void chk_vbox(hd_data_t *hd_data)
{
  hd_smbios_t *sm;

  for(sm = hd_data->smbios; sm; sm = sm->next) {
    if(
      sm->any.type == sm_sysinfo &&
      sm->sysinfo.product &&
      !strcmp(sm->sysinfo.product, "VirtualBox")
    ) {
      hd_data->flags.vbox = 1;
    }
  }
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
      ok = len == 1 && crc(mem->data + u, 0x10) == 0 ? 1 : 0;
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


unsigned parse_mpconfig_len(hd_data_t *hd_data, memory_range_t *mem)
{
  unsigned len = 0;

  if(*(unsigned *) (mem->data) == 0x504d4350) {		/* "PCMP" */
    len = mem->data[0x04] + (mem->data[0x05] << 8) +
          mem->data[0x28] + (mem->data[0x29] << 8);
  }

  return len;
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
      if(len < 2) {
        ADD2LOG("  oops: invalid record lenght\n");
        break;
      }
    }
  }
}


int get_bios32_info(hd_data_t *hd_data, memory_range_t *mem, bios32_info_t *bios32)
{
  unsigned u, ok;
  unsigned addr = 0, len;

  if(mem->size < 0x10) return 0;

  for(u = ok = 0; u <= mem->size - 0x10; u += 0x10) {
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

/**
 * db format (32 bits):
 * leaf: 1, last: 1, key: 6, range: 4, ofs: 20
 */
int tp_lookup(char *key_str, unsigned *width, unsigned *height, unsigned *xsize, unsigned *ysize)
{
  unsigned u;
  unsigned key, range, ofs, last, leaf = 0;

  if(strlen(key_str) != 7) return 0;

  for(u = 0; u < 7; u++) {
    if(key_str[u] < '0' || key_str[u] >= '0' + 64) return 0;
  }

  for(u = 0; u < sizeof tp_db / sizeof *tp_db; u++) {
    key = (tp_db[u] >> 24) & 0x3f;
    range = (tp_db[u] >> 20) & 0xf;
    ofs = tp_db[u] & ((1 << 20) - 1);
    // printf("key = %d, range = %d, ofs = %d, str = %d\n", key, range, ofs, *key_str - '0');
    if(*key_str - '0' >= key && *key_str - '0' <= key + range) {
      // printf("match\n");
      leaf = (tp_db[u] >> 31) & 1;
      if(leaf) break;
      key_str++;
      // printf("%d\n", *key_str);
      if(ofs >= sizeof tp_db / sizeof *tp_db || !*key_str) return 0;
      u = ofs - 1;
      // printf("next1 = %u\n", ofs);
      continue;
    }
    else {
      last = (tp_db[u] >> 30) & 1;
      if(last) return 0;
      // printf("next2 = %u\n", u + 1);
    }
  }

  if(leaf) {
    if(ofs >= sizeof tp_values / sizeof *tp_values) return 0;
    *width = tp_values[ofs].width;
    *height = tp_values[ofs].height;
    *xsize = tp_values[ofs].xsize;
    *ysize = tp_values[ofs].ysize;
  }

  return 1;
}

#endif /* defined(__i386__) || defined (__x86_64__) */

/** @} */


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

static void get_pnp_support_status(unsigned char *, bios_info_t *);

void hd_scan_bios(hd_data_t *hd_data)
{
  hd_t *hd;
  char *s;
  int fd, i;
  bios_info_t *bt;
  unsigned char *bios_ram;

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
  if((s = get_cmd_param(1))) {
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

  /*
   * get the i/o ports for the parallel & serial interfaces from the BIOS
   * memory area starting at 0x40:0
   */
  PROGRESS(2, 0, "ram");

  hd_data->bios_ram = new_mem(BIOS_RAM_SIZE);
  fd = -1;
  if(
    (fd = open(DEV_MEM, O_RDWR)) >= 0 && 
    lseek(fd, BIOS_RAM_START, SEEK_SET) >= 0 &&
    read(fd, hd_data->bios_ram, BIOS_RAM_SIZE) == BIOS_RAM_SIZE
  ) {
    bios_ram = hd_data->bios_ram;
    bt->ser_port0 = (bios_ram[1] << 8) + bios_ram[0];
    bt->ser_port1 = (bios_ram[3] << 8) + bios_ram[2];
    bt->ser_port2 = (bios_ram[5] << 8) + bios_ram[4];
    bt->ser_port3 = (bios_ram[7] << 8) + bios_ram[6];

    bt->par_port0 = (bios_ram[  9] << 8) + bios_ram[  8];
    bt->par_port1 = (bios_ram[0xb] << 8) + bios_ram[0xa];
    bt->par_port2 = (bios_ram[0xd] << 8) + bios_ram[0xc];
  }
  else {
    hd_data->bios_ram = free_mem(hd_data->bios_ram);
  }
  if(fd >= 0) close(fd);


  /*
   * read the bios rom and look for useful things there...
   */
  PROGRESS(2, 0, "rom");

  hd_data->bios_rom = new_mem(BIOS_ROM_SIZE);
  fd = -1;
  if(
    (fd = open(DEV_MEM, O_RDWR)) >= 0 && 
    lseek(fd, BIOS_ROM_START, SEEK_SET) >= 0 &&
    read(fd, hd_data->bios_rom, BIOS_ROM_SIZE) == BIOS_ROM_SIZE
  ) {
    get_pnp_support_status(hd_data->bios_rom, bt);
  }
  else {
    hd_data->bios_rom = free_mem(hd_data->bios_rom);
  }

  if(fd >= 0) close(fd);

  if((hd_data->debug & HD_DEB_BIOS)) {
    if(hd_data->bios_ram) {
      ADD2LOG("----- BIOS vars -----\n");
      for(i = 0; i < BIOS_RAM_SIZE; i += 0x10) {
        ADD2LOG("  %03x  ", i + BIOS_RAM_START);
        hexdump(&hd_data->log, 1, 0x10, hd_data->bios_ram + i);
        ADD2LOG("\n");
      }
      ADD2LOG("----- BIOS vars end -----\n");
    }

    if(hd_data->bios_rom) {
      ADD2LOG("----- BIOS ROM -----\n");
      for(i = 0; i < BIOS_ROM_SIZE; i += 0x1000) {
        ADD2LOG("  %03x  ", i + BIOS_ROM_START);
        hexdump(&hd_data->log, 1, 0x10, hd_data->bios_rom + i);
        ADD2LOG("\n");
      }
      ADD2LOG("----- BIOS ROM end -----\n");
    }
  }

}


void get_pnp_support_status(unsigned char *bios, bios_info_t *bt)
{
  int i;
  unsigned char pnp[4] = { '$', 'P', 'n', 'P' };
  unsigned char *t;
  unsigned l, cs;

  for(i = 0xf0000 - BIOS_ROM_START; i < BIOS_ROM_SIZE; i += 0x10) {
    t = bios + i;
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

#endif /* defined(__i386__) */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <endian.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "edd.h"

/**
 * @defgroup EDDint EDD partition information
 * @ingroup  libhdINFOint
 * @brief EDD disks layout / partition functions
 *
 * @{
 */

#if defined(__i386__) || defined(__x86_64__)

static void read_edd_info(hd_data_t *hd_data);
static int does_match(edd_info_t *ei, hd_t *hd, unsigned type);
static int does_match0(edd_info_t *ei, edd_info_t *ei0, unsigned type);
static int is_disk(hd_t *hd);
static uint64_t disk_size(hd_t *hd);
static int identical_disks(hd_t *hd1, hd_t *hd2);

void hd_scan_sysfs_edd(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_edd)) return;

  hd_data->module = mod_edd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  hd_data->flags.edd_used = 0;

  if(hd_probe_feature(hd_data, pr_edd_mod)) {
    PROGRESS(1, 0, "edd mod");
    load_module(hd_data, "edd");
  }

  PROGRESS(2, 0, "edd info");

  read_edd_info(hd_data);
}


void read_edd_info(hd_data_t *hd_data)
{
  unsigned u, u1;
  uint64_t ul0, edd_dev_path1, edd_dev_path2;
  edd_info_t *ei;
  str_list_t *sl, *sf_dir, *sf_dir_e, *sf_dir2;
  char *sf_edd = NULL, *net_link = NULL;
  unsigned char *edd_raw;
  char *edd_bus, *edd_interface, *edd_link;

  for(u = 0; u < sizeof hd_data->edd / sizeof *hd_data->edd; u++) {
    free_mem(hd_data->edd[u].sysfs_id);
  }

  memset(hd_data->edd, 0, sizeof hd_data->edd);

  sf_dir = read_dir("/sys/firmware/edd", 'd');

  for(sf_dir_e = sf_dir; sf_dir_e; sf_dir_e = sf_dir_e->next) {
    str_printf(&sf_edd, 0, "/sys/firmware/edd/%s", sf_dir_e->str);

    if(
      sscanf(sf_dir_e->str, "int13_dev%02x", &u) == 1 &&
      u >= 0x80 &&
      u <= 0xff
    ) {
      u -= 0x80;

      ei = hd_data->edd + u;

      ei->valid = 1;

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_edd, "sectors"), &ul0, 0)) {
        ei->sectors = ul0;
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_edd, "default_cylinders"), &ul0, 0)) {
        ei->edd.cyls = ul0;
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_edd, "default_heads"), &ul0, 0)) {
        ei->edd.heads = ul0;
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_edd, "default_sectors_per_track"), &ul0, 0)) {
        ei->edd.sectors = ul0;
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_edd, "legacy_max_cylinder"), &ul0, 0)) {
        ei->legacy.cyls = ul0 + 1;
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_edd, "legacy_max_head"), &ul0, 0)) {
        ei->legacy.heads = ul0 + 1;
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_edd, "legacy_sectors_per_track"), &ul0, 0)) {
        ei->legacy.sectors = ul0;
      }

      if(ei->sectors && ei->edd.heads && ei->edd.sectors) {
        ei->edd.cyls = ei->sectors / (ei->edd.heads * ei->edd.sectors);
      }

      if(hd_attr_uint(get_sysfs_attr_by_path(sf_edd, "mbr_signature"), &ul0, 0)) {
        ei->signature = ul0;
      }

      sl = hd_attr_list(get_sysfs_attr_by_path(sf_edd, "extensions"));
      if(search_str_list(sl, "Fixed disk access")) hd_data->edd[u].ext_fixed_disk = 1;
      if(search_str_list(sl, "Device locking and ejecting")) hd_data->edd[u].ext_lock_eject = 1;
      if(search_str_list(sl, "Enhanced Disk Drive support")) hd_data->edd[u].ext_edd = 1;
      if(search_str_list(sl, "64-bit extensions")) hd_data->edd[u].ext_64bit = 1;

      edd_bus = edd_interface = NULL;
      edd_dev_path1 = edd_dev_path2 = 0;

      edd_raw = get_sysfs_attr_by_path2(sf_edd, "raw_data", &u1);
      if(u1 >= 40) edd_bus = canon_str(edd_raw + 36, 4);
      if(u1 >= 48) {
        edd_interface = canon_str(edd_raw + 40, 8);
        if(!strcmp(edd_interface, "FIBRE")) ei->ext_fibre = 1;
      }
      if(u1 >= 72) {
        memcpy(&edd_dev_path1, edd_raw + 56, 8);
        memcpy(&edd_dev_path2, edd_raw + 64, 8);

        edd_dev_path1 = be64toh(edd_dev_path1);		// wwid for fc
      }

      edd_link = hd_read_sysfs_link(sf_edd, "pci_dev");
      if(edd_link) {
        str_printf(&net_link, 0, "%s/net", edd_link);
        sf_dir2 =  read_dir("/sys/firmware/edd", 'D');
        if(sf_dir2) ei->ext_net = 1;
        free_str_list(sf_dir2);
      }

      ADD2LOG(
        "edd: 0x%02x\n  mbr sig: 0x%08x\n  size: %"PRIu64"\n  chs default: %u/%u/%u\n  chs legacy: %u/%u/%u\n  caps: %s%s%s%s%s%s\n",
        u + 0x80,
        ei->signature,
        ei->sectors,
        ei->edd.cyls,
        ei->edd.heads,
        ei->edd.sectors,
        ei->legacy.cyls,
        ei->legacy.heads,
        ei->legacy.sectors,
        ei->ext_fixed_disk ? "fixed " : "",
        ei->ext_lock_eject ? "lock " : "",
        ei->ext_edd ? "edd " : "",
        ei->ext_64bit ? "64bit " : "",
        ei->ext_fibre ? "fibre " : "",
        ei->ext_net ? "net " : ""
      );
      ADD2LOG("  bus: %s\n  interface: %s\n  dev path: %016"PRIx64" %016"PRIx64"\n", edd_bus, edd_interface, edd_dev_path1, edd_dev_path2);

      free_mem(edd_bus);
      free_mem(edd_interface);
    }
  }

  free_mem(sf_edd);
  free_mem(net_link);
  free_str_list(sf_dir);
}


void assign_edd_info(hd_data_t *hd_data)
{
  hd_t *hd, *match_hd;
  unsigned u, u1, u2, lba, matches, real_matches, match_edd, type;
  bios_info_t *bt;
  edd_info_t *ei;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(is_disk(hd)) hd->rom_id = free_mem(hd->rom_id);
  }

  /* add BIOS drive ids to disks */
  for(type = 0; type < 4; type++) {
    for(u = 0; u < sizeof hd_data->edd / sizeof *hd_data->edd; u++) {
      ei = hd_data->edd + u;
      if(!ei->valid || ei->assigned) continue;

      for(u1 = u2 = 0; u1 < sizeof hd_data->edd / sizeof *hd_data->edd; u1++) {
        if(does_match0(ei, hd_data->edd + u1, type)) u2++;
      }

      /* not unique */
      if(u2 != 1) continue;

      matches = real_matches = 0;
      match_hd = 0;
      match_edd = 0;

      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(!is_disk(hd) || hd->rom_id) continue;

        if(does_match(ei, hd, type)) {
          if(!matches) {
            match_edd = u;
            match_hd = hd;
          }
          else {
            if(identical_disks(hd, match_hd)) real_matches--;
          }
          matches++;
          real_matches++;
        }
      }

      ADD2LOG("  %02x: matches %d (%d)\n", u + 0x80, real_matches, matches);

      if(real_matches == 1) {
        hd_data->flags.edd_used = 1;
        hd_data->edd[match_edd].assigned = 1;

        if(matches == 1) {
          str_printf(&match_hd->rom_id, 0, "0x%02x", match_edd + 0x80);
          ADD2LOG("  %s = %s (match %d)\n", match_hd->unix_dev_name, match_hd->rom_id, type);
        }
        else {
          for(hd = hd_data->hd; hd; hd = hd->next) {
            if(!is_disk(hd) || hd->rom_id) continue;
            if(does_match(ei, hd, type)) {
              str_printf(&hd->rom_id, 0, "0x%02x", match_edd + 0x80);
              ADD2LOG("  %s = %s (match %d)\n", hd->unix_dev_name, hd->rom_id, type);
            }
          }
        }
      }
    }
  }

  /* set lba support flag in BIOS data */
  for(lba = u = 0; u < sizeof hd_data->edd / sizeof *hd_data->edd; u++) {
    if(hd_data->edd[u].ext_fixed_disk) {
      lba = 1;
      break;
    }
  }

  if(lba) {
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_internal &&
        hd->sub_class.id == sc_int_bios &&
        hd->detail &&
        hd->detail->type == hd_detail_bios &&
        (bt = hd->detail->bios.data)
      ) {
        bt->lba_support = lba;
      }
    }
  }
}


int does_match(edd_info_t *ei, hd_t *hd, unsigned type)
{
  int i = 0;
  uint64_t u64;

  switch(type) {
    case 0:
      i = ei->signature == edd_disk_signature(hd) && ei->sectors == disk_size(hd);
      break;

    case 1:
      i = ei->signature == edd_disk_signature(hd);
      break;

    case 2:
      i = ei->sectors == disk_size(hd);
      break;

    case 3:
      u64 = ei->edd.heads * ei->edd.sectors;
      if(u64) {
        i = ei->edd.cyls == disk_size(hd) / u64;
      }
      break;
  }

  return i;
}


int does_match0(edd_info_t *ei, edd_info_t *ei0, unsigned type)
{
  int i = 0;

  switch(type) {
    case 0:
      i = ei->signature == ei0->signature && ei->sectors == ei0->sectors;
      break;

    case 1:
      i = ei->signature == ei0->signature;
      break;

    case 2:
      i = ei->sectors == ei0->sectors;
      break;

    case 3:
      i = ei->edd.cyls == ei0->edd.cyls;
      break;
  }

  return i;
}


int is_disk(hd_t *hd)
{
  return
    hd->base_class.id == bc_storage_device &&
    hd->sub_class.id == sc_sdev_disk;
}


uint64_t disk_size(hd_t *hd)
{
  hd_res_t *res;
  uint64_t ul = 0;

  for(res = hd->res; res; res = res->next) {
    if(
      res->any.type == res_size &&
      res->size.unit == size_unit_sectors
    ) {
      ul = res->size.val1;
      break;
    }
  }

  return ul;
}


unsigned edd_disk_signature(hd_t *hd)
{
  unsigned char *s = hd->block0;

  if(!s) return 0;

  return s[0x1b8] + (s[0x1b9] << 8) + (s[0x1ba] << 16) + (s[0x1bb] << 24);
}


int identical_disks(hd_t *hd1, hd_t *hd2)
{
  char *s1 = NULL, *s2 = NULL;
  str_list_t *sl;

  for(sl = hd1->unix_dev_names; sl; sl = sl->next) {
    if(!strncmp(sl->str, "/dev/disk/by-id/", sizeof "/dev/disk/by-id/" - 1)) {
      s1 = sl->str;
      break;
    }
  }

  for(sl = hd2->unix_dev_names; sl; sl = sl->next) {
    if(!strncmp(sl->str, "/dev/disk/by-id/", sizeof "/dev/disk/by-id/" - 1)) {
      s2 = sl->str;
      break;
    }
  }

  if(!s1) s1 = hd1->serial;
  if(!s2) s2 = hd1->serial;

  if(s1 && s2 && !strcmp(s1, s2)) return 1;

  return 0;
}


#endif /* defined(__i386__) || defined(__x86_64__) */

/** @} */


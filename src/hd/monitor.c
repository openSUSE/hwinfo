#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "monitor.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * monitor info
 *
 * Read the info out of the 'SuSE=' entry in /proc/cmdline. It contains
 * (among others) info from the EDID record got by our syslinux extension.
 *
 * We will try to look up our monitor id in the id file to get additional
 * info.
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifdef __PPC__
static void add_old_mac_monitor(hd_data_t *hd_data);
static void add_monitor(hd_data_t *hd_data, devtree_t *dt);
#endif
static int chk_edid_info(hd_data_t *hd_data, unsigned char *edid);
#if !defined(__PPC__)
static void add_lcd_info(hd_data_t *hd_data, hd_t *hd, bios_info_t *bt);
#endif
static void add_edid_info(hd_data_t *hd_data, hd_t *hd, unsigned char *edid);
static void add_monitor_res(hd_t *hd, unsigned x, unsigned y, unsigned hz, unsigned il);
static void fix_edid_info(hd_data_t *hd_data, unsigned char *edid);

#if !defined(__PPC__)
void hd_scan_monitor(hd_data_t *hd_data)
{
  hd_t *hd;
  int i, j, k;
  char *s, *s0, *s1, *se, m[8], *t;
  unsigned u;
  hd_res_t *res;
  monitor_info_t *mi = NULL;
  bios_info_t *bt;

  if(!hd_probe_feature(hd_data, pr_monitor)) return;

  hd_data->module = mod_monitor;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "ddc");

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class.id == bc_internal && hd->sub_class.id == sc_int_bios) break;
  }

  /* first, see if we got the full edid record from bios */
  bt = NULL;

#if 0
  /* for testing: LIBHD_EDID points to a file with valid edid record */
  {
    char *s = getenv("LIBHD_EDID");
    unsigned char edid[0x80];
    FILE *f;

    if(s && (f = fopen(s, "r"))) {
      if(fread(edid, sizeof edid, 1, f) == 1) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_monitor;
        add_edid_info(hd_data, hd, edid);
      }
      fclose(f);
      return;
    }
  }
#endif

  if(
    hd &&
    hd->detail &&
    hd->detail->type == hd_detail_bios &&
    (bt = hd->detail->bios.data) &&
    bt->vbe.ok
  ) {
    if(chk_edid_info(hd_data, bt->vbe.ddc)) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_monitor;

      hd_set_hw_class(hd, hw_vbe);

      hd->func = bt->vbe.port;

      add_edid_info(hd_data, hd, bt->vbe.ddc);

      return;
    }
  }

  /* Maybe a LCD panel? */
  if(bt && bt->lcd.width) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_monitor;
    hd->sub_class.id = sc_mon_lcd;

    hd_set_hw_class(hd, hw_vbe);

    add_lcd_info(hd_data, hd, bt);

    return;
  }

  /* Maybe we have hidden edid info here? */
  if(!(s = s0 = t = get_cmd_param(hd_data, 0))) return;		/* no :-( */

  s = strsep(&t, "^");

  se = s + strlen(s);

  if(se - s < 7 + 2 * 4) {
    free_mem(s0);
    return;
  }

  /* Ok, we've got it. Now we split the fields. */

  memcpy(m, s, 7); m[7] = 0; s += 7;

  hd = add_hd_entry(hd_data, __LINE__, 0);

  hd->base_class.id = bc_monitor;
  hd->vendor.id = name2eisa_id(m);
  if(sscanf(m + 3, "%x", &u) == 1) hd->device.id = MAKE_ID(TAG_EISA, u);
  if((u = device_class(hd_data, hd->vendor.id, hd->device.id))) {
    if((u >> 8) == bc_monitor) hd->sub_class.id = u & 0xff;
  }

  i = hex(s, 2); j = hex(s + 2, 2); s += 4;
  if(i > 0 && j > 0) {
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->size.type = res_size;
    res->size.unit = size_unit_cm;
    res->size.val1 = i;		/* width */
    res->size.val2 = j;		/* height */
  }

  i = hex(s, 2); s+= 2;
  if(i & (1 << 7)) add_monitor_res(hd, 720, 400, 70, 0);
  if(i & (1 << 6)) add_monitor_res(hd, 720, 400, 88, 0);
  if(i & (1 << 5)) add_monitor_res(hd, 640, 480, 60, 0);
  if(i & (1 << 4)) add_monitor_res(hd, 640, 480, 67, 0);
  if(i & (1 << 3)) add_monitor_res(hd, 640, 480, 72, 0);
  if(i & (1 << 2)) add_monitor_res(hd, 640, 480, 75, 0);
  if(i & (1 << 1)) add_monitor_res(hd, 800, 600, 56, 0);
  if(i & (1 << 0)) add_monitor_res(hd, 800, 600, 60, 0);

  i = hex(s, 2); s+= 2;
  if(i & (1 << 7)) add_monitor_res(hd,  800,  600, 72, 0);
  if(i & (1 << 6)) add_monitor_res(hd,  800,  600, 75, 0);
  if(i & (1 << 5)) add_monitor_res(hd,  832,  624, 75, 0);
  if(i & (1 << 4)) add_monitor_res(hd, 1024,  768, 87, 1);
  if(i & (1 << 3)) add_monitor_res(hd, 1024,  768, 60, 0);
  if(i & (1 << 2)) add_monitor_res(hd, 1024,  768, 70, 0);
  if(i & (1 << 1)) add_monitor_res(hd, 1024,  768, 75, 0);
  if(i & (1 << 0)) add_monitor_res(hd, 1280, 1024, 75, 0);

  if(((se - s) & 1) || se - s > 8 * 4 + 2) {
    ADD2LOG("  ddc oops: %d bytes left?\n", (int) (se - s));
    free_mem(s0);
    return;
  }

  while(s + 4 <= se) {
    i = (hex(s, 2) + 31) * 8; j = hex(s + 2, 2); s += 4;
    k = 0;
    switch((j >> 6) & 3) {
      case 1: k = (i * 3) / 4; break;
      case 2: k = (i * 4) / 5; break;
      case 3: k = (i * 9) / 16; break;
    }
    if(k) add_monitor_res(hd, i, k, (j & 0x3f) + 60, 0);
  }

  u = 0;
  if(se - s == 2) u = hex(s, 2) + 1990;

  if(u || t) {
    mi = new_mem(sizeof *mi);
    if(u) mi->manu_year = u;
    while((s = strsep(&t, "^"))) {
      for(s1 = s; *s1++; ) if(*s1 == '_') *s1 = ' ';
      switch(*s) {
        case '0':
          if(!mi->name && s[1]) mi->name = canon_str(s + 1, strlen(s + 1));
          break;
        case '1':
          u = 0;
          if(strlen(s) == 9) {
            i = hex(s + 1, 2);
            j = hex(s + 3, 2);
            if(i > j || !i) u = 1;
            mi->min_vsync = i;
            mi->max_vsync = j;
            i = hex(s + 5, 2);
            j = hex(s + 7, 2);
            if(i > j || !i) u = 1;
            mi->min_hsync = i;
            mi->max_hsync = j;
          }
          else {
            u = 1;
          }
          if(u) {
            mi->min_vsync = mi->max_vsync = mi->min_hsync = mi->max_hsync = 0;
            ADD2LOG("  ddc oops: invalid freq data\n");
          }
          break;
        case '2':
          if(!mi->vendor && s[1]) mi->vendor = canon_str(s + 1, strlen(s + 1));
          break;
        case '3':
          if(!mi->serial && s[1]) mi->serial = canon_str(s + 1, strlen(s + 1));
          break;
        default:
          ADD2LOG("  ddc oops: invalid tag 0x%02x\n", *s);
      }
    }
  }

  if(mi) {
    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_monitor;
    hd->detail->monitor.data = mi;

    hd->serial = new_str(mi->serial);

#if 0
// ########### FIXME
    if(
      mi->vendor &&
      ID_VALUE(hd->vendor.id) &&
      !hd_vendor_name(hd_data, hd->vendor.id)
    ) {
      add_vendor_name(hd_data, hd->vend, mi->vendor);
    }
#endif

#if 0
// ########### FIXME 
    if(
      mi->name &&
      (ID_VALUE(hd->vendor.id) || ID_VALUE(hd->device.id)) &&
      !hd_device_name(hd_data, hd->vend, hd->device.id)
    ) {
      add_device_name(hd_data, hd->vend, hd->dev, mi->name);
    }
#endif

    if(hd_data->debug) {
      ADD2LOG("----- DDC info -----\n");
      if(mi->vendor) {
        ADD2LOG("  vendor: \"%s\"\n", mi->vendor);
      }
      if(mi->name) {
        ADD2LOG("  model: \"%s\"\n", mi->name);
      }
      if(mi->serial) {
        ADD2LOG("  serial: \"%s\"\n", mi->serial);
      }
      if(mi->min_hsync) {
        ADD2LOG("  hsync: %u-%u kHz\n", mi->min_hsync, mi->max_hsync);
      }
      if(mi->min_vsync) {
        ADD2LOG("  vsync: %u-%u Hz\n", mi->min_vsync, mi->max_vsync);
      }
      if(mi->manu_year) {
        ADD2LOG("  manu. year: %u\n", mi->manu_year);
      }
      ADD2LOG("----- DDC info end -----\n");
    }
  }

  free_mem(s0);
}
#endif	/* !defined(__PPC__) */

#if defined(__PPC__)
void hd_scan_monitor(hd_data_t *hd_data)
{
  devtree_t *dt;
  int found;

  if(!hd_probe_feature(hd_data, pr_monitor)) return;

  hd_data->module = mod_monitor;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "prom");

  found = 0;
  for(dt = hd_data->devtree; dt; dt = dt->next) {
    if(dt->edid) {
      add_monitor(hd_data, dt);
      found = 1;
    }
  }

  if(!found) {
    add_old_mac_monitor(hd_data);
  }
}

void add_old_mac_monitor(hd_data_t *hd_data)
{
  hd_t *hd;
  unsigned u1, u2;
  str_list_t *sl;
  static struct {
    unsigned width, height, vfreq, interlaced;
  } mode_list[20] = {
    {  512,  384, 60, 1 },
    {  512,  384, 60, 0 },
    {  640,  480, 50, 1 },
    {  640,  480, 60, 1 },
    {  640,  480, 60, 0 },
    {  640,  480, 67, 0 },
    {  640,  870, 75, 0 },
    {  768,  576, 50, 1 },
    {  800,  600, 56, 0 },
    {  800,  600, 60, 0 },
    {  800,  600, 72, 0 },
    {  800,  600, 75, 0 },
    {  832,  624, 75, 0 },
    { 1024,  768, 60, 0 },
    { 1024,  768, 70, 0 },
    { 1024,  768, 75, 0 },
    { 1024,  768, 75, 0 },
    { 1152,  870, 75, 0 },
    { 1280,  960, 75, 0 },
    { 1280, 1024, 75, 0 }
  };

  for(sl = hd_data->klog; sl; sl = sl->next) {
    if(sscanf(sl->str, "<%*d>Monitor sense value = %i, using video mode %i", &u1, &u2) == 2) {
      u2--;
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_monitor;

      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0401);
      hd->device.id = MAKE_ID(TAG_SPECIAL, (u1 & 0xfff) + 0x1000);

      if((u1 = hd_display_adapter(hd_data))) {
        hd->attached_to = u1;
      }

      if(u2 < sizeof mode_list / sizeof *mode_list) {
        add_monitor_res(hd, mode_list[u2].width, mode_list[u2].height, mode_list[u2].vfreq, mode_list[u2].interlaced);
      }

      break;
    }
  }

}

void add_monitor(hd_data_t *hd_data, devtree_t *dt)
{
  hd_t *hd, *hd2;
  unsigned char *edid = dt->edid;

  if(!chk_edid_info(hd_data, edid)) return;

  hd = add_hd_entry(hd_data, __LINE__, 0);

  hd->base_class.id = bc_monitor;

  for(hd2 = hd_data->hd; hd2; hd2 = hd2->next) {
    if(
      hd2->detail &&
      hd2->detail->type == hd_detail_devtree &&
      hd2->detail->devtree.data == dt
    ) {
      hd->attached_to = hd2->idx;
      break;
    }
  }

  add_edid_info(hd_data, hd, edid);
}

#endif	/* defined(__PPC__) */

/* do some checks to ensure we got a reasonable block */
int chk_edid_info(hd_data_t *hd_data, unsigned char *edid)
{
  // no vendor or model info
  if(!(edid[0x08] || edid[0x09] || edid[0x0a] || edid[0x0b])) return 0;

  // no edid version or revision
  if(!(edid[0x12] || edid[0x13])) return 0;

  return 1;
}

#if !defined(__PPC__)
void add_lcd_info(hd_data_t *hd_data, hd_t *hd, bios_info_t *bt)
{
  monitor_info_t *mi = NULL;
  hd_res_t *res = NULL;

  hd->vendor.name = new_str(bt->lcd.vendor);
  hd->device.name = new_str(bt->lcd.name);

  add_monitor_res(hd, bt->lcd.width, bt->lcd.height, 60, 0);

  mi = new_mem(sizeof *mi);
  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_monitor;
  hd->detail->monitor.data = mi;

  mi->min_vsync = 50;
  mi->min_hsync = 31;
  mi->max_vsync = 75;
  mi->max_hsync = (mi->max_vsync * bt->lcd.height * 12) / 10000;

  if (bt->lcd.xsize) {
     res = add_res_entry(&hd->res, new_mem(sizeof *res));
     res->size.type = res_size;
     res->size.unit = size_unit_cm;
     res->size.val1 = bt->lcd.xsize;
     res->size.val2 = bt->lcd.ysize;
  }
}
#endif

void add_edid_info(hd_data_t *hd_data, hd_t *hd, unsigned char *edid)
{
  hd_res_t *res;
  monitor_info_t *mi = NULL;
  int i;
  unsigned u, u1, u2;

  fix_edid_info(hd_data, edid);

  if(edid[0x14] & 0x80) {
    /* digital signal -> assume lcd */
    hd->sub_class.id = 2;
  }

  u = (edid[8] << 8) + edid[9];
  hd->vendor.id = MAKE_ID(TAG_EISA, u);
  u = (edid[0xb] << 8) + edid[0xa];
  hd->device.id = MAKE_ID(TAG_EISA, u);
  if((u = device_class(hd_data, hd->vendor.id, hd->device.id))) {
    if((u >> 8) == bc_monitor) hd->sub_class.id = u & 0xff;
  }

  if(edid[0x15] > 0 && edid[0x16] > 0) {
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->size.type = res_size;
    res->size.unit = size_unit_cm;
    res->size.val1 = edid[0x15];	/* width */
    res->size.val2 = edid[0x16];	/* height */
  }

  u = edid[0x23];
  if(u & (1 << 7)) add_monitor_res(hd, 720, 400, 70, 0);
  if(u & (1 << 6)) add_monitor_res(hd, 720, 400, 88, 0);
  if(u & (1 << 5)) add_monitor_res(hd, 640, 480, 60, 0);
  if(u & (1 << 4)) add_monitor_res(hd, 640, 480, 67, 0);
  if(u & (1 << 3)) add_monitor_res(hd, 640, 480, 72, 0);
  if(u & (1 << 2)) add_monitor_res(hd, 640, 480, 75, 0);
  if(u & (1 << 1)) add_monitor_res(hd, 800, 600, 56, 0);
  if(u & (1 << 0)) add_monitor_res(hd, 800, 600, 60, 0);

  u = edid[0x24];
  if(u & (1 << 7)) add_monitor_res(hd,  800,  600, 72, 0);
  if(u & (1 << 6)) add_monitor_res(hd,  800,  600, 75, 0);
  if(u & (1 << 5)) add_monitor_res(hd,  832,  624, 75, 0);
  if(u & (1 << 4)) add_monitor_res(hd, 1024,  768, 87, 1);
  if(u & (1 << 3)) add_monitor_res(hd, 1024,  768, 60, 0);
  if(u & (1 << 2)) add_monitor_res(hd, 1024,  768, 70, 0);
  if(u & (1 << 1)) add_monitor_res(hd, 1024,  768, 75, 0);
  if(u & (1 << 0)) add_monitor_res(hd, 1280, 1024, 75, 0);

  for(i = 0; i < 4; i++) {
    u1 = (edid[0x26 + 2 * i] + 31) * 8;
    u2 = edid[0x27 + 2 * i];
    u = 0;
    switch((u2 >> 6) & 3) {
      case 1: u = (u1 * 3) / 4; break;
      case 2: u = (u1 * 4) / 5; break;
      case 3: u = (u1 * 9) / 16; break;
    }
    if(u) add_monitor_res(hd, u1, u, (u2 & 0x3f) + 60, 0);
  }

  mi = new_mem(sizeof *mi);
  mi->manu_year = 1990 + edid[0x11];

  for(i = 0x36; i < 0x36 + 4 * 0x12; i += 0x12) {
    if(!(edid[i] || edid[i + 1] || edid[i + 2])) {
      switch(edid[i + 3]) {
        case 0xfc:
          if(edid[i + 5]) {
            /* the name entry is splitted some times */
            str_printf(&mi->name, -1, "%s%s", mi->name ? " " : "", canon_str(edid + i + 5, 0xd));
          }
          break;

        case 0xfd:
          u = 0;
          u1 = edid[i + 5];
          u2 = edid[i + 6];
          if(u1 > u2 || !u1) u = 1;
          mi->min_vsync = u1;
          mi->max_vsync = u2;
          u1 = edid[i + 7];
          u2 = edid[i + 8];
          if(u1 > u2 || !u1) u = 1;
          mi->min_hsync = u1;
          mi->max_hsync = u2;
          if(u) {
            mi->min_vsync = mi->max_vsync = mi->min_hsync = mi->max_hsync = 0;
            ADD2LOG("  ddc oops: invalid freq data\n");
          }
          break;

        case 0xfe:
          if(!mi->vendor && edid[i + 5]) mi->vendor = canon_str(edid + i + 5, 0xd);
          break;

        case 0xff:
          if(!mi->serial && edid[i + 5]) mi->serial = canon_str(edid + i + 5, 0xd);
          break;

        default:
          ADD2LOG("  ddc oops: invalid tag 0x%02x\n", edid[i + 3]);
      }
    }
  }

  if(mi) {
    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_monitor;
    hd->detail->monitor.data = mi;

    hd->serial = new_str(mi->serial);
    hd->vendor.name = new_str(mi->vendor);
    hd->device.name = new_str(mi->name);

    if(hd_data->debug) {
      ADD2LOG("----- DDC info -----\n");
      if(mi->vendor) {
        ADD2LOG("  vendor: \"%s\"\n", mi->vendor);
      }
      if(mi->name) {
        ADD2LOG("  model: \"%s\"\n", mi->name);
      }
      if(mi->serial) {
        ADD2LOG("  serial: \"%s\"\n", mi->serial);
      }
      if(mi->min_hsync) {
        ADD2LOG("  hsync: %u-%u kHz\n", mi->min_hsync, mi->max_hsync);
      }
      if(mi->min_vsync) {
        ADD2LOG("  vsync: %u-%u Hz\n", mi->min_vsync, mi->max_vsync);
      }
      if(mi->manu_year) {
        ADD2LOG("  manu. year: %u\n", mi->manu_year);
      }
      ADD2LOG("----- DDC info end -----\n");
    }
  }
}

void add_monitor_res(hd_t *hd, unsigned width, unsigned height, unsigned vfreq, unsigned il)
{
  hd_res_t *res;

  res = add_res_entry(&hd->res, new_mem(sizeof *res));
  res->monitor.type = res_monitor;
  res->monitor.width = width;
  res->monitor.height = height;
  res->monitor.vfreq = vfreq;
  res->monitor.interlaced = il;
}

/*
 * This looks evil, but some Mac displays really lie at us.
 */
void fix_edid_info(hd_data_t *hd_data, unsigned char *edid)
{
  unsigned vend, dev;
  unsigned timing;
  int fix = 0;

  vend = (edid[8] << 8) + edid[9];
  dev = (edid[0xb] << 8) + edid[0xa];

  timing = (edid[0x24] << 8) + edid[0x23];

  /* APP9214: Apple Studio Display */
  if(vend == 0x0610 && dev == 0x9214 && timing == 0x0800) {
    timing = 0x1000;
    fix = 1;
  }

  if(fix) {
    edid[0x23] = timing & 0xff;
    edid[0x24] = (timing >> 8) & 0xff;
  }
}


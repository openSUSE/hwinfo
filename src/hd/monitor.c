#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "monitor.h"

/**
 * @defgroup MONITORint Monitor (DDC) information
 * @ingroup libhdINFOint
 * @brief Monitor information functions
 *
 * @{
 */

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
#endif
static void add_monitor(hd_data_t *hd_data, devtree_t *dt);
static int chk_edid_info(hd_data_t *hd_data, unsigned char *edid);
static void add_lcd_info(hd_data_t *hd_data, hd_t *hd, bios_info_t *bt);
static void add_edid_info(hd_data_t *hd_data, hd_t *hd, unsigned char *edid);
static void add_monitor_res(hd_t *hd, unsigned x, unsigned y, unsigned hz, unsigned il);
static void fix_edid_info(hd_data_t *hd_data, unsigned char *edid);

void hd_scan_monitor(hd_data_t *hd_data)
{
  hd_t *hd, *hd2;
  bios_info_t *bt;
  devtree_t *dt;
  pci_t *pci;
  int found;

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

  PROGRESS(2, 0, "bios");

  if(
    hd &&
    hd->detail &&
    hd->detail->type == hd_detail_bios &&
    (bt = hd->detail->bios.data) &&
    bt->vbe.ok
  ) {
    int pid = 0;
    int got_ddc_data = 0;
    for(pid = 0; pid < sizeof bt->vbe.ddc_port / sizeof *bt->vbe.ddc_port; pid++) {
      if(chk_edid_info(hd_data, bt->vbe.ddc_port[pid])) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_monitor;

        hd_set_hw_class(hd, hw_vbe);

        hd->func = pid;

        add_edid_info(hd_data, hd, bt->vbe.ddc_port[pid]);

        got_ddc_data = 1;
      }
    }
    if(got_ddc_data) {
      return;
    }
  }

  PROGRESS(3, 0, "pci");
  found = 0;
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd &&
      hd->detail &&
      hd->detail->type == hd_detail_pci &&
      (pci = hd->detail->pci.data) &&
      pci->edid_len >= 0x80 &&
      chk_edid_info(hd_data, pci->edid)
    ) {
      hd2 = add_hd_entry(hd_data, __LINE__, 0);
      hd2->base_class.id = bc_monitor;
      hd2->attached_to = hd->idx;
      add_edid_info(hd_data, hd2, pci->edid);
      found = 1;
    }
  }

  if(found) return;

  PROGRESS(4, 0, "internal db");

  /* Maybe a LCD panel? */
  if(bt && bt->lcd.width) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_monitor;
    hd->sub_class.id = sc_mon_lcd;

    hd_set_hw_class(hd, hw_vbe);

    add_lcd_info(hd_data, hd, bt);

    return;
  }

  PROGRESS(5, 0, "prom");

  found = 0;
  for(dt = hd_data->devtree; dt; dt = dt->next) {
    if(dt->edid) {
      add_monitor(hd_data, dt);
      found = 1;
    }
  }

#if defined(__PPC__)
  PROGRESS(6, 0, "old mac");

  if(!found) {
    add_old_mac_monitor(hd_data);
  }
#endif

}


#if defined(__PPC__)
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
#endif	/* defined(__PPC__) */


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


/* do some checks to ensure we got a reasonable block */
int chk_edid_info(hd_data_t *hd_data, unsigned char *edid)
{
  // no vendor or model info
  if(!(edid[0x08] || edid[0x09] || edid[0x0a] || edid[0x0b])) return 0;

  // no edid version or revision
  if(!(edid[0x12] || edid[0x13])) return 0;

  return 1;
}


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
     res->size.unit = size_unit_mm;
     res->size.val1 = bt->lcd.xsize;
     res->size.val2 = bt->lcd.ysize;
  }
}


void add_edid_info(hd_data_t *hd_data, hd_t *hd, unsigned char *edid)
{
  hd_res_t *res;
  monitor_info_t *mi = NULL;
  int i;
  unsigned u, u1, u2, tag;
  char *s;
  unsigned width_mm = 0, height_mm = 0;
  unsigned hblank, hsync_ofs, hsync, vblank, vsync_ofs, vsync;

  fix_edid_info(hd_data, edid);

  mi = new_mem(sizeof *mi);

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
    mi->width_mm = width_mm = edid[0x15] * 10;
    mi->height_mm = height_mm = edid[0x16] * 10;
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

  mi->manu_year = 1990 + edid[0x11];

  ADD2LOG("  detailed timings:\n");

  for(i = 0x36; i < 0x36 + 4 * 0x12; i += 0x12) {
    tag = (edid[i] << 24) + (edid[i + 1] << 16) + (edid[i + 2] << 8) + edid[i + 3];

    ADD2LOG("  #%d: ", (i - 0x36)/0x12);
    hexdump(&hd_data->log, 1, 0x12, edid + i);
    ADD2LOG("\n");

    switch(tag) {
      case 0xfc:
        if(edid[i + 5]) {
          /* name entry is splitted some times */
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
        if(!mi->vendor && edid[i + 5]) {
          mi->vendor = canon_str(edid + i + 5, 0xd);
          for(s = mi->vendor; *s; s++) if(*s < ' ') *s = ' ';
        }
        break;

      case 0xff:
        if(!mi->serial && edid[i + 5]) {
          mi->serial = canon_str(edid + i + 5, 0xd);
          for(s = mi->serial; *s; s++) if(*s < ' ') *s = ' ';
        }
        break;

      default:
        if(tag < 0x100) {
          ADD2LOG("  unknown tag 0x%02x\n", tag);
        }
        else {
          u = (edid[i + 0] + (edid[i + 1] << 8)) * 10;	/* pixel clock in kHz */
          if(!u) break;
          mi->clock = u;

          u1 = edid[i + 2] + ((edid[i + 4] & 0xf0) << 4);
          u2 = edid[i + 5] + ((edid[i + 7] & 0xf0) << 4);
          if(!u1 || !u2) break;
          mi->width = u1;
          mi->height = u2;

          u1 = edid[i + 12] + ((edid[i + 14] & 0xf0) << 4);
          u2 = edid[i + 13] + ((edid[i + 14] & 0xf) << 8);
          if(!u1 || !u2) break;
          mi->width_mm = u1;
          mi->height_mm = u2;


          hblank = edid[i + 3] + ((edid[i + 4] & 0xf) << 8);
          hsync_ofs = edid[i + 8] + ((edid[i + 11] & 0xc0) << 2);
          hsync = edid[i + 9] + ((edid[i + 11] & 0x30) << 4);

          vblank = edid[i + 6] + ((edid[i + 7] & 0xf) << 8);
          vsync_ofs = ((edid[i + 10] & 0xf0) >> 4) + ((edid[i + 11] & 0x0c) << 2);
          vsync = (edid[i + 10] & 0xf) + ((edid[i + 11] & 0x03) << 4);

          mi->hdisp       = mi->width;
          mi->hsyncstart  = mi->width + hsync_ofs;
          mi->hsyncend    = mi->width + hsync_ofs + hsync;
          mi->htotal      = mi->width + hblank;
          ADD2LOG(
            "    h: %4u %4u %4u %4u (+%u +%u +%u)\n",
            mi->hdisp, mi->hsyncstart, mi->hsyncend, mi->htotal,
            hsync_ofs, hsync_ofs + hsync, hblank
          );

          mi->vdisp       = mi->height;
          mi->vsyncstart  = mi->height + vsync_ofs;
          mi->vsyncend    = mi->height + vsync_ofs + vsync;
          mi->vtotal      = mi->height + vblank;
          ADD2LOG(
            "    v: %4u %4u %4u %4u (+%u +%u +%u)\n",
            mi->vdisp, mi->vsyncstart, mi->vsyncend, mi->vtotal,
            vsync_ofs, vsync_ofs + vsync, vblank
          );

          u = edid[i + 17];

          if(((u >> 3) & 3) == 3) {
            mi->hflag = (u & 4) ? '+' : '-';
            mi->vflag = (u & 2) ? '+' : '-';
            ADD2LOG("    %chsync %cvsync\n", mi->hflag, mi->vflag);
          }

          u1 = mi->width  + hblank;
          u2 = mi->height + vblank;

          if(u1 && u2) {
            ADD2LOG(
              "    %.1f MHz, %.1f kHz, %.1f Hz\n",
              (double) mi->clock / 1000,
              (double) mi->clock / u1,
              (double) mi->clock / u1 / u2 * 1000
            );
          }

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

    if(mi->width && mi->height) {
      for(res = hd->res; res; res = res->next) {
        if(
          res->any.type == res_monitor &&
          res->monitor.width == mi->width &&
          res->monitor.height == mi->height
        ) break;
      }
      /* actually we could calculate the vsync value */
      if(!res) add_monitor_res(hd, mi->width, mi->height, 60, 0);

      /* do some sanity checks on display size, see bug 155096, 186096 */
      if(mi->width_mm && mi->height_mm) {
        u = (mi->width_mm * mi->height * 16) / (mi->height_mm * mi->width);
        u1 = width_mm ? (width_mm * 16) / mi->width_mm : 16;
        u2 = height_mm ? (height_mm * 16) / mi->height_mm : 16;
        if(
          u <= 8 || u >= 32 ||		/* allow 1:2 distortion */
          u1 <= 8 || u1 >= 32 ||	/* width cm & mm values disagree by factor >2 --> use cm values */
          u2 <= 8 || u2 >= 32		/* dto, height */
        ) {
          ADD2LOG("  ddc: strange size data (%ux%u mm^2), trying cm values\n", mi->width_mm, mi->height_mm);
          /* ok, try cm values */
          if(width_mm && height_mm) {
            u = (width_mm * mi->height * 16) / (height_mm * mi->width);
            if(u > 8 && u < 32) {
              mi->width_mm = width_mm;
              mi->height_mm = height_mm;
            }
          }
          /* could not fix, clear */
          if(u <= 8 || u >= 32) {
            ADD2LOG("  ddc: cm values (%ux%u mm^2) didn't work either - giving up\n", width_mm, height_mm);
            mi->width_mm = mi->height_mm = 0;
          }
        }
      }
    }

    if(mi->width_mm && mi->height_mm) {
      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->size.type = res_size;
      res->size.unit = size_unit_mm;
      res->size.val1 = mi->width_mm;	/* width */
      res->size.val2 = mi->height_mm;	/* height */
    }

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
      if(mi->width || mi->height) {
        ADD2LOG("  size: %u x %u\n", mi->width, mi->height);
      }
      if(mi->width_mm || mi->height_mm) {
        ADD2LOG("  size (mm): %u x %u\n", mi->width_mm, mi->height_mm);
      }
      if(mi->clock) {
        ADD2LOG("  clock: %u kHz\n", mi->clock);
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

/** @} */


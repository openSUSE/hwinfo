#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <linux/fb.h>

#include "hd.h"
#include "hd_int.h"
#include "fb.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * scan framebuffer devices
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

typedef struct {
  unsigned width;
  unsigned height;
  double pix_clock;
  double h_freq;
  double v_freq;
} fb_info_t;

static fb_info_t *fb_get_info(hd_data_t *hd_data);

void hd_scan_fb(hd_data_t *hd_data)
{
  fb_info_t *fb;
  hd_t *hd;
  hd_res_t *res;
  unsigned imac_dev, imac_vend;
  unsigned imac = 0;
  monitor_info_t *mi = NULL;

  if(!hd_probe_feature(hd_data, pr_fb)) return;

  hd_data->module = mod_fb;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "read info");

  fb = fb_get_info(hd_data);

  if(fb) {
    imac_dev = MAKE_ID(TAG_EISA, 0x9d03);
    imac_vend = name2eisa_id("APP");

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->base_class.id == bc_monitor) break;
    }

    if(hd && hd->device.id == imac_dev && hd->vendor.id == imac_vend) {
      hd->tag.remove = 1;
      remove_tagged_hd_entries(hd_data);
      imac = 1;
      hd = NULL;
    }

    /* add monitor entry based on fb data if we have no other info */
    if(!hd) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_monitor;
      if(imac) {
        hd->vendor.id = imac_vend;
        hd->device.id = imac_dev;
      }
      else {
        hd->vendor.name = new_str("Generic");
        hd->device.name = new_str("Monitor");
      }

      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->monitor.type = res_monitor;
      res->monitor.width = fb->width;
      res->monitor.height = fb->height;
      res->monitor.vfreq = fb->v_freq + 0.5;

      if(!hd->detail) {
        mi = new_mem(sizeof *mi);
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_monitor;
        hd->detail->monitor.data = mi;

        mi->min_vsync = 50;
        mi->min_hsync = 31;
        mi->max_vsync = fb->v_freq * 1.11 + 0.9;
        mi->max_hsync = fb->h_freq / 1000.0 + 1.9;
        if(mi->max_vsync <= mi->min_vsync) mi->max_vsync = mi->min_vsync + 10;
        if(mi->max_hsync <= mi->min_hsync) mi->max_hsync = mi->min_hsync + 5;
        /* round up */
        mi->max_vsync = ((mi->max_vsync + 9) / 10) * 10;
      }
    }
  }
}

fb_info_t *fb_get_info(hd_data_t *hd_data)
{
  int fd;
  struct fb_var_screeninfo fbv_info;
  static fb_info_t fb_info;
  fb_info_t *fb = NULL;
  int h, v;

  fd = open(DEV_FB, O_RDONLY);
  if(fd < 0) fd = open(DEV_FB0, O_RDONLY);
  if(fd < 0) return fb;

  if(!ioctl(fd, FBIOGET_VSCREENINFO, &fbv_info)) {
    h = fbv_info.left_margin + fbv_info.xres + fbv_info.right_margin + fbv_info.hsync_len;
    v = fbv_info.upper_margin + fbv_info.yres + fbv_info.lower_margin + fbv_info.vsync_len;
    if(fbv_info.pixclock && h && v) {
      fb_info.width = fbv_info.xres;
      fb_info.height = fbv_info.yres;
      fb_info.pix_clock = 1e12 / fbv_info.pixclock;
      fb_info.h_freq = fb_info.pix_clock / h;
      fb_info.v_freq = fb_info.h_freq / v;
      fb = &fb_info;
      ADD2LOG("fb: size %d x %d\n", fb_info.width, fb_info.height);
      ADD2LOG("fb: timing %.2f MHz, %.2f kHz, %.2f Hz\n", fb_info.pix_clock * 1e-6, fb_info.h_freq * 1e-3, fb_info.v_freq);
    }
  }

  close(fd);

  return fb;
}

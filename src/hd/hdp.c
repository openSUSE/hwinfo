#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hdp.h"
#include "hdx.h"
#include "util.h"


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * This module provides a function that prints a hardware entry to stdout. 
 * This is useful for debugging or to provide the user with some fancy info.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


#define dump_line(x0, x1...) fprintf(f, "%*s" x0, ind, "", x1)
#define dump_line_str(x0...) fprintf(f, "%*s%s", ind, "", x0)
#define dump_line0(x0...) fprintf(f, x0)

static int ind = 0;		// output indentation

static void dump_normal(hd_t *, unsigned, FILE *);
static void dump_cpu(hd_t *, unsigned, FILE *);
static void dump_bios(hd_t *, unsigned, FILE *);

static char *make_device_name_str(hd_t *h, char *buf, int buf_size);
static char *make_sub_device_name_str(hd_t *h, char *buf, int buf_size);
static char *make_vend_name_str(hd_t *h, char *buf, int buf_size);
static char *make_sub_vend_name_str(hd_t *h, char *buf, int buf_size);


/*
 * Dump a hardware entry to FILE *f.
 */
void hd_dump_entry(hd_data_t *hd_data, hd_t *h, FILE *f)
{
  char *s, *a0, *a1;
  char buf1[32], buf2[32];
  hd_t *hd_tmp;

  s = "";
  if(
    h->detail &&
    h->detail->type == hd_detail_pci &&
    (h->detail->pci.data->flags & (1 << pci_flag_agp))
  ) s = "(AGP)";

  if(h->detail && h->detail->type == hd_detail_isapnp) s = "(PnP)";

  a0 = bus_name(h->bus);
  a1 = sub_class_name(h->base_class, h->sub_class);
  dump_line(
    "%02d: %s%s %02x.%x: %02x%02x %s\n",
    h->idx, a0 ? a0 : "?", s, h->slot, h->func,
    h->base_class, h->sub_class, a1 ? a1 : "?"
  );

//  if(h->bus == bus_pci && (h->ext_flags & (1 << pci_flag_pm))) dump_line0(", supports PM");
//  dump_line0("\n");

  ind += 2;

  if((hd_data->debug & HD_DEB_CREATION)) {
    s = mod_name_by_idx(h->module);
    if(!s) sprintf(s = buf1, "%u", h->module);
    if(h->count)
      sprintf(buf2, ".%u", h->count);
    else
      *buf2 = 0;
    dump_line("[Created at %s.%u%s]\n", s, h->line, buf2);
  }

  if(h->base_class == bc_internal && h->sub_class == sc_int_cpu)
    dump_cpu(h, hd_data->debug, f);
  else if(h->base_class == bc_internal && h->sub_class == sc_int_bios)
    dump_bios(h, hd_data->debug, f);
  else
    dump_normal(h, hd_data->debug, f);

  if(
    h->attached_to &&
    (hd_tmp = get_device_by_idx(hd_data, h->attached_to))
  ) {
    s = sub_class_name(hd_tmp->base_class, hd_tmp->sub_class);
    s = s ? s : "?";
    dump_line("Attached to: #%u (%s)\n", h->attached_to, s);
  }

  if(
    h->base_class == bc_storage_device &&
    h->sub_class == sc_sdev_cdrom &&
    h->detail &&
    h->detail->type == hd_detail_cdrom
  ) {
    cdrom_info_t *ci = h->detail->cdrom.data;

    if(ci) {
      if(ci->volume) dump_line("Volume ID: \"%s\"\n", ci->volume);
      if(ci->application) dump_line("Application: \"%s\"\n", ci->application);
      if(ci->publisher) dump_line("Publisher: \"%s\"\n", ci->publisher);
      if(ci->preparer) dump_line("Preparer: \"%s\"\n", ci->preparer);
      if(ci->creation_date) dump_line("Creation date: \"%s\"\n", ci->creation_date);
    }
    else {
      dump_line_str("Drive status: no cdrom found\n");
    }
  }

  ind -= 2;

  if(h->next) dump_line_str("\n");
}


/*
 * print 'normal' hardware entries
 */
void dump_normal(hd_t *h, unsigned debug, FILE *f)
{
  int i;
  char *s, *a0;
  uint64 u64;
  hd_res_t *res;
  char buf[256];
  driver_info_t *mi;

  if(h->vend || h->dev || h->dev_name || h->vend_name) {
    if(h->vend || h->vend_name || h->dev)
      dump_line("Vendor: %s\n", make_vend_name_str(h, buf, sizeof buf));
    dump_line("Model: %s\n", make_device_name_str(h, buf, sizeof buf));
  }

  if(h->sub_vend || h->sub_dev || h->sub_dev_name || h->sub_vend_name) {
    if(h->sub_vend || h->sub_vend_name || h->sub_dev)
      dump_line("SubVendor: %s\n", make_sub_vend_name_str(h, buf, sizeof buf));
    dump_line("SubDevice: %s\n", make_sub_device_name_str(h, buf, sizeof buf));
  }

  if(h->rev_name)
    dump_line("Revision: \"%s\"\n", h->rev_name);
  else if(h->rev)
    dump_line("Revision: 0x%02x\n", h->rev);

  if(h->serial)
    dump_line("Serial ID: \"%s\"\n", h->serial);

  if(h->compat_vend || h->compat_dev) {
    a0 = device_name(h->compat_vend, h->compat_dev);
    if(IS_EISA_ID(h->vend)) {
      dump_line(
        "Comaptible to: %s%04x \"%s\"\n",
        eisa_vendor_str(h->compat_vend), ID_VALUE(h->compat_dev), a0 ? a0 : "?"
      );
    }
    else {
      dump_line(
        "Comaptible to: %04x %04x \"%s\"\n",
        h->compat_vend, ID_VALUE(h->compat_dev), a0 ? a0 : "?"
      );
    }
  }

  if((debug & HD_DEB_DRIVER_INFO)) {
    if(h->vend || h->dev) {
      a0 = device_drv_name(h->vend, h->dev);
      if(a0) {
        dump_line("Device Driver Info: \"%s\"\n", a0);
      }
    }

    if(h->sub_vend || h->sub_dev) {
      a0 = sub_device_drv_name(h->vend, h->dev, h->sub_vend, h->sub_dev);
      if(a0) {
        dump_line("SubDevice Driver Info: \"%s\"\n", a0);
      }
    }
  }

  if((mi = get_driver_info(h))) {
    switch(mi->type) {
      case di_module:
        dump_line(
          "Driver Status: %s is %sactive\n",
          mi->module.name, mi->module.is_active ? "" : "not "
        );

        if(mi->module.load_cmd)
          dump_line(
            "Driver Activation Cmd: \"%s\"%s\n",
            mi->module.load_cmd, mi->module.autoload ? " (required)" : ""
          );

        if(mi->module.conf)
          dump_line("Driver \"conf.modules\" Entry: \"%s\"\n", mi->module.conf);
      break;

      case di_mouse:
        if(mi->mouse.xf86) dump_line("XFree Protocol: %s\n", mi->mouse.xf86);
        if(mi->mouse.gpm) dump_line("GPM Protocol: %s\n", mi->mouse.gpm);
        break;

      case di_x11:
        if(mi->x11.server) dump_line("XFree Server: %s\n", mi->x11.server);
        if(mi->x11.x3d) dump_line("3D-Accel: %s\n", mi->x11.x3d);
        if(mi->x11.colors.all) {
          dump_line_str("Color Depths: ");
          i = 0;
          if(mi->x11.colors.c8) { dump_line0("8"); i++; }
          if(mi->x11.colors.c15) { if(i) dump_line0(", "); dump_line0("15"); i++; }
          if(mi->x11.colors.c16) { if(i) dump_line0(", "); dump_line0("16"); i++; }
          if(mi->x11.colors.c24) { if(i) dump_line0(", "); dump_line0("24"); i++; }
          if(mi->x11.colors.c32) { if(i) dump_line0(", "); dump_line0("32"); i++; }
          dump_line0("\n");
        }
        if(mi->x11.dacspeed) dump_line("Max. DAC Clock: %u MHz\n", mi->x11.dacspeed);
        break;

      case di_display:
        if(mi->display.width)
          dump_line("Max. Resolution: %ux%u\n", mi->display.width, mi->display.height);
        if(mi->display.min_vsync)
           dump_line("Vert. Sync Range: %u-%u Hz\n", mi->display.min_vsync, mi->display.max_vsync);
        if(mi->display.min_hsync)
           dump_line("Hor. Sync Range: %u-%u kHz\n", mi->display.min_hsync, mi->display.max_hsync);
        if(mi->display.bandwidth)
           dump_line("Bandwidth: %u MHz\n", mi->display.bandwidth);
        break;

      default:
        dump_line_str("Driver Status: cannot handle driver info\n");
    }
  }


  if(h->unix_dev_name) {
    dump_line("Device File: %s\n", h->unix_dev_name);
  }

  for(res = h->res; res; res = res->next) {
    switch(res->any.type) {
      case res_phys_mem:
        if((u64 = (res->phys_mem.range >> 10) & ~(-1 << 10))) {
          dump_line("Memory Size: %"HD_LL"dM + %"HD_LL"dk\n", res->phys_mem.range >> 20, u64);
        }
        else {
          dump_line("Memory Size: %"HD_LL"dM\n", res->phys_mem.range >> 20);
        }
        break;

      case res_mem:
        *(s = buf) = 0;
        strcat(buf, res->mem.access == acc_rw ? "rw" : res->mem.access == acc_ro ? "ro" : "wo");
        strcat(buf, res->mem.prefetch == flag_yes ? ",prefetchable" : res->mem.prefetch == flag_no ? ",non-prefetchable" : "");
        if(!res->mem.enabled) strcat(buf, ",disabled");
        if(*s == ',') s++;
        if(res->mem.range) {
          dump_line(
            "Memory Range: 0x%08"HD_LL"x-0x%08"HD_LL"x (%s)\n",
            res->mem.base, res->mem.base + res->mem.range - 1, s
          );
        }
        else {
          dump_line("Memory Range: 0x%08"HD_LL"x-??? (%s)\n", res->mem.base, s);
        }
        break;

      case res_io:
        *(s = buf) = 0;
        strcat(buf, res->io.access == acc_rw ? "rw" : res->io.access == acc_ro ? "ro" : "wo");
        if(!res->io.enabled) strcat(buf, ",disabled");
        if(*s == ',') s++;
        if(res->io.range == 0) {
          dump_line("I/O Ports: 0x%02"HD_LL"x-??? (%s)\n", res->io.base, s);
        }
        else if(res->io.range == 1) {
          dump_line("I/O Port: 0x%02"HD_LL"x (%s)\n", res->io.base, s);
        }
        else {
          dump_line("I/O Ports: 0x%02"HD_LL"x-0x%02"HD_LL"x (%s)\n", res->io.base, res->io.base + res->io.range -1, s);
        }
        break;

      case res_irq:
        *(s = buf) = 0;
        switch(res->irq.triggered) {
          case 0:
            strcpy(s, "no events");
            break;

          case 1:
            strcpy(s, "1 event");
            break;

          default:
            sprintf(s, "%u events", res->irq.triggered);
        }
        if(!res->irq.enabled) {
          if(res->irq.triggered)
            strcat(s, ",");
          else
            *s = 0;
          strcat(s, "disabled");
        }
        dump_line("IRQ: %u (%s)\n", res->irq.base, s);
        break;

      case res_dma:
        *(s = buf) = 0;
        if(!res->dma.enabled) strcpy(buf, " (disabled)");
        dump_line("DMA: %u%s\n", res->dma.base, s);
        break;

      case res_monitor:
        dump_line(
          "Resolution: %ux%u@%uHz\n",
          res->monitor.width, res->monitor.height, res->monitor.vfreq
        );
        break;

      case res_size:
        {
          char *s, b0[32], b1[32];

          switch(res->size.unit) {
            case size_unit_cinch:
              s = "''";
              snprintf(b0, sizeof b0 - 1, "%s", float2str(res->size.val1, 2));
              snprintf(b1, sizeof b1 - 1, "%s", float2str(res->size.val2, 2));
              break;

            default:
              switch(res->size.unit) {
                case size_unit_cm:
                  s = "cm";
                  break;
                case size_unit_sectors:
                  s = "sectors";
                  break;
                case size_unit_kbyte:
                  s = "kByte";
                  break;
                default:
                  s = "some unit";
              }
              snprintf(b0, sizeof b0 - 1, "%u", res->size.val1);
              snprintf(b1, sizeof b1 - 1, "%u", res->size.val2);
          }
          if(!res->size.val2)
            dump_line("Size: %s %s\n", b0, s);
          else if(res->size.unit == size_unit_sectors)
            dump_line("Size: %s %s a %s bytes\n", b0, s, b1);
          else
            dump_line("Size: %sx%s %s\n", b0, b1, s);
        }
        break;

      case res_disk_geo:
        s = res->disk_geo.logical ? "Logical" : "Physical";
        dump_line(
          "Geometry (%s): CHS %u/%u/%u\n", s,
          res->disk_geo.cyls, res->disk_geo.heads, res->disk_geo.sectors
        );
        break;

      case res_cache:
        dump_line("Cache: %u kb\n", res->cache.size);
        break;

      case res_baud:
        dump_line("Speed: %u baud\n", res->baud.speed);
        break;

      default:
        dump_line("Unkown resource type %d\n", res->any.type);
    }
  }

}

/*
 * print CPU entries
 */
void dump_cpu(hd_t *hd, unsigned debug, FILE *f)
{
  cpu_info_t *ct;

  if(!hd->detail || hd->detail->type != hd_detail_cpu) return;
  if(!(ct = hd->detail->cpu.data)) return;

  dump_line0 ("  Arch: ");
  switch (ct->architecture) {
      case arch_intel:
	dump_line0 ("Intel\n");
	break;
      case arch_alpha:
	dump_line0 ("Alpha\n");
	break;
      case arch_sparc:
	dump_line0 ("Sparc (32)\n");
	break;
      case arch_sparc64:
	dump_line0 ("UltraSparc (64)\n");
	break;
      case arch_ppc:
	dump_line0 ("PowerPC\n");
	break;
      case arch_68k:
	dump_line0 ("68k\n");
	break;
      default:
	dump_line0 ("**UNKNWON**\n");
	break;
  }

  dump_line0 ("  Boot: ");
  switch (ct->boot) {
      case boot_lilo:
	dump_line0 ("LILO\n");
	break;
      case boot_milo:
	dump_line0 ("MILO\n");
	break;
      case boot_aboot:
	dump_line0 ("aboot\n");
	break;
      case boot_silo:
	dump_line0 ("SILO\n");
	break;
      default:
	dump_line0 ("**unknown**\n");
	break;
  }

  if(ct->vend_name) dump_line("Vendor: \"%s\"\n", ct->vend_name);
 
  if(ct->model_name)
    dump_line(
      "Model: %u.%u.%u \"%s\"\n",
      ct->family, ct->model, ct->stepping, ct->model_name
    );

  if(ct->clock) dump_line("Clock: %u MHz\n", ct->clock);

  if(ct->cache) dump_line("Cache: %u kb\n", ct->cache);
}


/*
 * print BIOS entries
 */
void dump_bios(hd_t *hd, unsigned debug, FILE *f)
{
  bios_info_t *bt;

  if(!hd->detail || hd->detail->type != hd_detail_bios) return;
  if(!(bt = hd->detail->bios.data)) return;

  if(bt->vbe_ver) {
    dump_line("VESA BIOS Version: %u.%u\n", bt->vbe_ver >> 4, bt->vbe_ver & 0x0f);
  }

  if(bt->vbe_video_mem) {
    dump_line("Video Memory: %u kb\n", bt->vbe_video_mem >> 10);
  }

  if(bt->apm_supported) {
    dump_line("APM Version: %u.%u\n", bt->apm_ver, bt->apm_subver);
    dump_line("APM Status: %s\n", bt->apm_enabled ? "on" : "off");
    dump_line("APM BIOS Flags: 0x%x\n", bt->apm_bios_flags);
  }

  if(bt->ser_port0) dump_line("Serial Port 0: 0x%x\n", bt->ser_port0);
  if(bt->ser_port1) dump_line("Serial Port 1: 0x%x\n", bt->ser_port1);
  if(bt->ser_port2) dump_line("Serial Port 2: 0x%x\n", bt->ser_port2);
  if(bt->ser_port3) dump_line("Serial Port 3: 0x%x\n", bt->ser_port3);

  if(bt->par_port0) dump_line("Parallel Port 0: 0x%x\n", bt->par_port0);
  if(bt->par_port1) dump_line("Parallel Port 1: 0x%x\n", bt->par_port1);
  if(bt->par_port2) dump_line("Parallel Port 2: 0x%x\n", bt->par_port2);

  if(bt->is_pnp_bios) dump_line("PnP BIOS: %s\n", isa_id2str(bt->pnp_id));
}


char *make_device_name_str(hd_t *h, char *buf, int buf_size)
{
  char *s;

  if(h->dev_name) {
    snprintf(buf, buf_size - 1, "\"%s\"", h->dev_name);
  }
  else {
    s = device_name(h->vend, h->dev);
    snprintf(buf, buf_size - 1, "%04x \"%s\"", ID_VALUE(h->dev), s ? s : "?");
  }

  return buf;
}


char *make_sub_device_name_str(hd_t *h, char *buf, int buf_size)
{
  char *s;

  if(h->sub_dev_name) {
    snprintf(buf, buf_size - 1, "\"%s\"", h->sub_dev_name);
  }
  else {
    s = sub_device_name(h->vend, h->dev, h->sub_vend, h->sub_dev);
    snprintf(buf, buf_size - 1, "%04x \"%s\"", ID_VALUE(h->sub_dev), s ? s : "?");
  }

  return buf;
}


char *make_vend_name_str(hd_t *h, char *buf, int buf_size)
{
  char *s;

  if(h->vend_name) {
    snprintf(buf, buf_size - 1, "\"%s\"", h->vend_name);
  }
  else {
    s = vendor_name(h->vend);
    if(IS_EISA_ID(h->vend)) {
      snprintf(buf, buf_size - 1, "%s \"%s\"", eisa_vendor_str(h->vend), s ? s : "?");
    }
    else {
      snprintf(buf, buf_size - 1, "%04x \"%s\"", h->vend, s ? s : "?");
    }
  }

  return buf;
}


char *make_sub_vend_name_str(hd_t *h, char *buf, int buf_size)
{
  char *s;

  if(h->sub_vend_name) {
    snprintf(buf, buf_size - 1, "\"%s\"", h->sub_vend_name);
  }
  else {
    s = vendor_name(h->sub_vend);
    if(IS_EISA_ID(h->sub_vend)) {
      snprintf(buf, buf_size - 1, "%s \"%s\"", eisa_vendor_str(h->sub_vend), s ? s : "?");
    }
    else {
      snprintf(buf, buf_size - 1, "%04x \"%s\"", h->sub_vend, s ? s : "?");
    }
  }

  return buf;
}



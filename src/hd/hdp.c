#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hd.h"
#include "hd_int.h"
#include "hdp.h"
#include "hddb.h"


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * This module provides a function that prints a hardware entry. 
 * This is useful for debugging or to provide the user with some fancy info.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifndef LIBHD_TINY

#define dump_line(x0, x1...) fprintf(f, "%*s" x0, ind, "", x1)
#define dump_line_str(x0...) fprintf(f, "%*s%s", ind, "", x0)
#define dump_line0(x0...) fprintf(f, x0)

static int ind = 0;		/* output indentation */

static void dump_normal(hd_data_t *, hd_t *, FILE *);
static void dump_cpu(hd_data_t *, hd_t *, FILE *);
static void dump_bios(hd_data_t *, hd_t *, FILE *);
static void dump_prom(hd_data_t *, hd_t *, FILE *);
static void dump_sys(hd_data_t *, hd_t *, FILE *);

static char *make_device_name_str(hd_data_t *hd_data, hd_t *h, char *buf, int buf_size);
static char *make_sub_device_name_str(hd_data_t *hd_data, hd_t *h, char *buf, int buf_size);
static char *make_vend_name_str(hd_data_t *hd_data, hd_t *h, char *buf, int buf_size);
static char *make_sub_vend_name_str(hd_data_t *hd_data, hd_t *h, char *buf, int buf_size);

static char *vend_id2str(unsigned vend);

/*
 * Dump a hardware entry to FILE *f.
 */
void hd_dump_entry(hd_data_t *hd_data, hd_t *h, FILE *f)
{
  char *s, *a0, *a1;
  char buf1[32], buf2[32];
  hd_t *hd_tmp;

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log)
      fprintf(libhd_log, "; %s\t%p\t%p\n", __FUNCTION__, CALLED_FROM(hd_dump_entry, hd_data), hd_data);
  }
#endif

  s = "";
  if(h->is.agp) s = "(AGP)";
  //  pci_flag_pm: dump_line0(", supports PM");
  if(h->is.isapnp) s = "(PnP)";

  a0 = hd_bus_name(hd_data, h->bus);
  a1 = hd_class_name(hd_data, 3, h->base_class, h->sub_class, h->prog_if);
  dump_line(
    "%02d: %s%s %02x.%x: %02x%02x %s\n",
    h->idx, a0 ? a0 : "?", s, h->slot, h->func,
    h->base_class, h->sub_class, a1 ? a1 : "?"
  );

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

  if((hd_data->debug & HD_DEB_CREATION) && h->unique_id) {
    dump_line("Unique ID: %s\n", h->unique_id);
  }

  if(h->base_class == bc_internal && h->sub_class == sc_int_cpu)
    dump_cpu(hd_data, h, f);
  else if(h->base_class == bc_internal && h->sub_class == sc_int_bios)
    dump_bios(hd_data, h, f);
  else if(h->base_class == bc_internal && h->sub_class == sc_int_prom)
    dump_prom(hd_data, h, f);
  else if(h->base_class == bc_internal && h->sub_class == sc_int_sys)
    dump_sys(hd_data, h, f);
  else
    dump_normal(hd_data, h, f);

  if(
    h->attached_to &&
    (hd_tmp = hd_get_device_by_idx(hd_data, h->attached_to))
  ) {
    s = hd_class_name(hd_data, 3, hd_tmp->base_class, hd_tmp->sub_class, hd_tmp->prog_if);
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

    if(ci->speed) {
      dump_line("Drive Speed: %u\n", ci->speed);
    }

    if(ci->iso9660.ok) {
      if(ci->iso9660.volume) dump_line("Volume ID: \"%s\"\n", ci->iso9660.volume);
      if(ci->iso9660.application) dump_line("Application: \"%s\"\n", ci->iso9660.application);
      if(ci->iso9660.publisher) dump_line("Publisher: \"%s\"\n", ci->iso9660.publisher);
      if(ci->iso9660.preparer) dump_line("Preparer: \"%s\"\n", ci->iso9660.preparer);
      if(ci->iso9660.creation_date) dump_line("Creation date: \"%s\"\n", ci->iso9660.creation_date);
    }
    else {
      if(ci->cdrom) {
        dump_line_str("Drive status: non-ISO9660 cdrom\n");
      }
      else {
        dump_line_str("Drive status: no cdrom found\n");
      }
    }
    if(ci->el_torito.ok) {
      dump_line(
        "El Torito info: platform %u, %sbootable\n",
        ci->el_torito.platform,
        ci->el_torito.bootable ? "" : "not "
      );
      if(ci->el_torito.id_string) dump_line("  Id String: \"%s\"\n",  ci->el_torito.id_string);
      if(ci->el_torito.label) dump_line("  Volume Label: \"%s\"\n",  ci->el_torito.label);
      {
        static char *media[] = {
          "none", "1.2MB Floppy", "1.44MB Floppy", "2.88MB Floppy", "Hard Disk"
        };
        dump_line(
          "  Media: %s starting at sector 0x%04x\n",
          media[ci->el_torito.media_type < sizeof media / sizeof *media ? ci->el_torito.media_type : 0],
          ci->el_torito.start
        );
      }
      if(ci->el_torito.geo.size) dump_line(
        "  Geometry (CHS): %u/%u/%u (%u blocks)\n",
        ci->el_torito.geo.c, ci->el_torito.geo.h, ci->el_torito.geo.s, ci->el_torito.geo.size
      );
      dump_line("  Load: %u bytes", ci->el_torito.load_count * 0x200);
      if(ci->el_torito.load_address) {
        dump_line0(" at 0x%04x\n", ci->el_torito.load_address);
      }
      else {
        dump_line0("\n");
      }
    }
  }

  if(
    h->base_class == bc_storage_device &&
    h->sub_class == sc_sdev_floppy &&
    h->detail &&
    h->detail->type == hd_detail_floppy
  ) {
    floppy_info_t *fi = h->detail->floppy.data;

    if(fi) {
      dump_line_str("Drive status: floppy found\n");
    }
    else {
      dump_line_str("Drive status: no floppy found\n");
    }
  }

  ind -= 2;

  if(h->next) dump_line_str("\n");
}


/*
 * print 'normal' hardware entries
 */
void dump_normal(hd_data_t *hd_data, hd_t *h, FILE *f)
{
  int i, j;
  char *s, *a0;
  uint64_t u64;
  hd_res_t *res;
  char buf[256];
  driver_info_t *di, *di0;
  str_list_t *sl;
  isdn_parm_t *ip;

  if(h->vend || h->dev || h->dev_name || h->vend_name) {
    if(h->vend || h->vend_name || h->dev)
      dump_line("Vendor: %s\n", make_vend_name_str(hd_data, h, buf, sizeof buf));
    dump_line("Model: %s\n", make_device_name_str(hd_data, h, buf, sizeof buf));
  }

  if(h->sub_vend || h->sub_dev || h->sub_dev_name || h->sub_vend_name) {
    if(h->sub_vend || h->sub_vend_name || h->sub_dev)
      dump_line("SubVendor: %s\n", make_sub_vend_name_str(hd_data, h, buf, sizeof buf));
    dump_line("SubDevice: %s\n", make_sub_device_name_str(hd_data, h, buf, sizeof buf));
  }

  if(h->rev_name)
    dump_line("Revision: \"%s\"\n", h->rev_name);
  else if(h->rev)
    dump_line("Revision: 0x%02x\n", h->rev);

  if(h->serial)
    dump_line("Serial ID: \"%s\"\n", h->serial);

  if(h->compat_vend || h->compat_dev) {
    a0 = hd_device_name(hd_data, h->compat_vend, h->compat_dev);
    dump_line(
      "Comaptible to: %s %04x \"%s\"\n",
      vend_id2str(h->compat_vend), ID_VALUE(h->compat_dev), a0 ? a0 : "?"
    );
  }

  if(
    h->bus == bus_usb &&
    h->detail &&
    h->detail->type == hd_detail_usb
  ) {
    usb_t *usb = h->detail->usb.data;

    if(usb && usb->driver)
      dump_line("USB Device status: driver active (\"%s\")\n", usb->driver);
    else
      dump_line_str("USB Device status: no driver loaded\n");
  }

  if(h->broken) {
    dump_line_str("Warning: might be broken\n");
  }

  if(h->unix_dev_name) {
    dump_line("Device File: %s\n", h->unix_dev_name);
  }

  if(h->rom_id) {
#ifdef __i386__
    dump_line("BIOS id: %s\n", h->rom_id);
#endif
#if defined(__PPC__) || defined(__sparc__)
    dump_line("PROM id: %s\n", h->rom_id);
#endif
#if defined(__s390__)
    dump_line("Chp id: %s\n", h->rom_id);
#endif
  }

  if(h->tag.ser_skip) {
    dump_line_str("Tags: ser_skip\n");
  }

  for(res = h->res; res; res = res->next) {
    switch(res->any.type) {
      case res_phys_mem:
        if((u64 = (res->phys_mem.range >> 10) & ~(-1 << 10))) {
          dump_line("Memory Size: %"PRId64"M + %"PRId64"k\n", res->phys_mem.range >> 20, u64);
        }
        else {
          dump_line("Memory Size: %"PRId64"M\n", res->phys_mem.range >> 20);
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
            "Memory Range: 0x%08"PRIx64"-0x%08"PRIx64" (%s)\n",
            res->mem.base, res->mem.base + res->mem.range - 1, s
          );
        }
        else {
          dump_line("Memory Range: 0x%08"PRIx64"-??? (%s)\n", res->mem.base, s);
        }
        break;

      case res_io:
        *(s = buf) = 0;
        strcat(buf, res->io.access == acc_rw ? "rw" : res->io.access == acc_ro ? "ro" : "wo");
        if(!res->io.enabled) strcat(buf, ",disabled");
        if(*s == ',') s++;
        if(res->io.range == 0) {
          dump_line("I/O Ports: 0x%02"PRIx64"-??? (%s)\n", res->io.base, s);
        }
        else if(res->io.range == 1) {
          dump_line("I/O Port: 0x%02"PRIx64" (%s)\n", res->io.base, s);
        }
        else {
          dump_line("I/O Ports: 0x%02"PRIx64"-0x%02"PRIx64" (%s)\n", res->io.base, res->io.base + res->io.range -1, s);
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
          "Resolution: %ux%u@%uHz%s\n",
          res->monitor.width, res->monitor.height, res->monitor.vfreq,
          res->monitor.interlaced ? " (interlaced)" : ""
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
        if(res->baud.speed == 0 || res->baud.speed % 100) {
          dump_line("Speed: %u bps\n", res->baud.speed);
        }
        else if(res->baud.speed % 100000) {
          dump_line("Speed: %s kbps\n", float2str(res->baud.speed, 3));
        }
        else {
          dump_line("Speed: %s Mbps\n", float2str(res->baud.speed, 6));
        }
        if(res->baud.bits || res->baud.stopbits || res->baud.parity || res->baud.handshake) {
          int i = 0;

          dump_line_str("Config: ");
          if(res->baud.bits) {
            dump_line0("%u bits", res->baud.bits);
            i++;
          }
          if(res->baud.parity) {
            dump_line0("%sparity %c", i++ ? ", " : "", res->baud.parity);
          }
          if(res->baud.stopbits) {
            dump_line0("%s%u stopbits", i++ ? ", " : "", res->baud.stopbits);
          }
          if(res->baud.handshake) {
            dump_line0("%shandshake %c", i++ ? ", " : "", res->baud.handshake);
          }
          dump_line0("\n");
        }
        break;

      case res_init_strings:
	dump_line("Init1: %s\n", res->init_strings.init1);
	dump_line("Init2: %s\n", res->init_strings.init2);
        break;

      case res_pppd_option:
	dump_line("PPPD Option: %s\n", res->pppd_option.option);
        break;

      default:
        dump_line("Unkown resource type %d\n", res->any.type);
    }
  }

  di0 = hd_driver_info(hd_data, h);

  for(di = di0, i = 0; di; di = di->next, i++) {
    dump_line("Driver Info #%d:\n", i);
    ind += 2;
    switch(di->any.type) {
      case di_any:
        dump_line_str("Driver Info:");
        for(sl = di->any.hddb0, j = 0; sl; sl = sl->next, j++) {
          dump_line0("%c%s", j ? ',' : ' ', sl->str);
        }
        dump_line0("\n");
        break;

      case di_display:
        if(di->display.width)
          dump_line("Max. Resolution: %ux%u\n", di->display.width, di->display.height);
        if(di->display.min_vsync)
           dump_line("Vert. Sync Range: %u-%u Hz\n", di->display.min_vsync, di->display.max_vsync);
        if(di->display.min_hsync)
           dump_line("Hor. Sync Range: %u-%u kHz\n", di->display.min_hsync, di->display.max_hsync);
        if(di->display.bandwidth)
           dump_line("Bandwidth: %u MHz\n", di->display.bandwidth);
        break;

      case di_module:
        dump_line(
          "Driver Status: %s is %sactive\n",
          di->module.name, di->module.active ? "" : "not "
        );

        dump_line(
          "Driver Activation Cmd: \"%s %s%s%s\"\n",
          di->module.modprobe ? "modprobe" : "insmod",
          di->module.name,
          di->module.mod_args ? " " : "",
          di->module.mod_args ? di->module.mod_args : ""
        );

        if(di->module.conf) {
          char *s = di->module.conf;

          dump_line_str("Driver \"modules.conf\" Entry: \"");
          for(; *s; s++) {
            if(isprint(*s)) {
              dump_line0("%c", *s);
            }
            else {
              switch(*s) {
                case '\n': dump_line0("\\n"); break;
                case '\r': dump_line0("\\r"); break;
                case '\t': dump_line0("\t"); break;	/* *no* typo! */
                default: dump_line0("\\%03o", *s); 
              }
            }
          }
          dump_line0("\"\n");
        }
      break;

      case di_mouse:
        if(di->mouse.xf86) dump_line("XFree86 Protocol: %s\n", di->mouse.xf86);
        if(di->mouse.gpm) dump_line("GPM Protocol: %s\n", di->mouse.gpm);
        break;

      case di_x11:
        if(di->x11.server) {
          dump_line(
            "XFree86 v%s Server%s: %s\n",
            di->x11.xf86_ver, strcmp(di->x11.xf86_ver, "4") ? "" : " Module", di->x11.server
          );
        }
        if(di->x11.x3d) dump_line_str("3D Support: yes\n");
        if(di->x11.script) dump_line("3D Script: %s\n", di->x11.script);
        if(di->x11.colors.all) {
          dump_line_str("Color Depths: ");
          j = 0;
          if(di->x11.colors.c8) { dump_line0("8"); j++; }
          if(di->x11.colors.c15) { if(j) dump_line0(", "); dump_line0("15"); j++; }
          if(di->x11.colors.c16) { if(j) dump_line0(", "); dump_line0("16"); j++; }
          if(di->x11.colors.c24) { if(j) dump_line0(", "); dump_line0("24"); j++; }
          if(di->x11.colors.c32) { if(j) dump_line0(", "); dump_line0("32"); j++; }
          dump_line0("\n");
        }
        if(di->x11.dacspeed) dump_line("Max. DAC Clock: %u MHz\n", di->x11.dacspeed);
        if(di->x11.extensions) {
          dump_line("Extensions: %s", di->x11.extensions->str);
          for(sl = di->x11.extensions->next; sl; sl = sl->next) {
            dump_line0(", %s", sl->str);
          }
          dump_line0("\n");
        }
        if(di->x11.options) {
          dump_line("Options: %s", di->x11.options->str);
          for(sl = di->x11.options->next; sl; sl = sl->next) {
            dump_line0(", %s", sl->str);
          }
          dump_line0("\n");
        }
        if(di->x11.raw) {
          dump_line("XF86Config Entry: %s", di->x11.raw->str);
          for(sl = di->x11.raw->next; sl; sl = sl->next) {
            dump_line0("\\n%s", sl->str);
          }
          dump_line0("\n");
        }
        if(di->x11.packages) {
          dump_line("Packages: %s", di->x11.packages->str);
          for(sl = di->x11.packages->next; sl; sl = sl->next) {
            dump_line0(", %s", sl->str);
          }
          dump_line0("\n");
        }
        break;

      case di_isdn:
        dump_line(
          "I4L Type: %d/%d [%s]\n",
          di->isdn.i4l_type, di->isdn.i4l_subtype, di->isdn.i4l_name
        );
        if((ip = di->isdn.params)) {
          int k, l;

          dump_line_str("Parameter:\n");
          for(k = 0; ip; ip = ip->next, k++) {
            dump_line(
              "  %d%s: (0x%x/%02x): %s = 0x%"PRIx64,
              k, ip->conflict ? "(conflict)" : ip->valid ? "" : "(invalid)",
              ip->type, ip->flags >> 8, ip->name, ip->value
            );
            if(ip->alt_values) {
              for(l = 0; l < ip->alt_values; l++) {
                dump_line0(
                  "%s%s0x%x", l ? "," : " [",
                  ip->alt_value[l] == ip->def_value ? "*" : "",
                  ip->alt_value[l]
                );
              }
              dump_line0("]");
            }
            dump_line0("\n");
          }
        }
        break;

      case di_kbd:
        if(di->kbd.XkbRules) dump_line("XkbRules: %s\n", di->kbd.XkbRules);
        if(di->kbd.XkbModel) dump_line("XkbModel: %s\n", di->kbd.XkbModel);
        if(di->kbd.XkbLayout) dump_line("XkbLayout: %s\n", di->kbd.XkbLayout);
        if(di->kbd.keymap) dump_line("keymap: %s\n", di->kbd.keymap);
        break;

      default:
        dump_line_str("Driver Status: unknown driver info format\n");
    }

    if((hd_data->debug & HD_DEB_DRIVER_INFO)) {
      for(sl = di->any.hddb0, j = 0; sl; sl = sl->next, j++) {
        if(j) {
          dump_line0("|%s", sl->str);
        }
        else {
          dump_line("Driver DB0: %d, %s", di->any.type, sl->str);
        }
      }
      if(di->any.hddb0) dump_line0("\n");

      for(sl = di->any.hddb1, j = 0; sl; sl = sl->next, j++) {
        if(!j) dump_line_str("Driver DB1: \"");
        dump_line0("%s\\n", sl->str);
      }
      if(di->any.hddb1) dump_line0("\"\n");
    }

    ind -= 2;
  }

  di0 = hd_free_driver_info(di0);

}

/*
 * print CPU entries
 */
void dump_cpu(hd_data_t *hd_data, hd_t *hd, FILE *f)
{
  cpu_info_t *ct;
  str_list_t *sl;

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
      case arch_ia64:
	dump_line0 ("IA-64\n");
	break;
      case arch_s390:
	dump_line0 ("S390\n");
	break;
      default:
	dump_line0 ("**UNKNWON**\n");
	break;
  }

  if(ct->vend_name) dump_line("Vendor: \"%s\"\n", ct->vend_name);
 
  if(ct->model_name)
    dump_line(
      "Model: %u.%u.%u \"%s\"\n",
      ct->family, ct->model, ct->stepping, ct->model_name
    );

  if(ct->platform) dump_line("Platform: \"%s\"\n", ct->platform);

  if(ct->features) {
    dump_line("Features: %s", ct->features->str);
    for(sl = ct->features->next; sl; sl = sl->next) {
      dump_line0(",%s", sl->str);
    }
    dump_line0("\n");
  }

  if(ct->clock) dump_line("Clock: %u MHz\n", ct->clock);

  if(ct->cache) dump_line("Cache: %u kb\n", ct->cache);
}


/*
 * print BIOS entries
 */
void dump_bios(hd_data_t *hd_data, hd_t *hd, FILE *f)
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

  if(bt->low_mem_size) dump_line("Base Memory: %uk\n", bt->low_mem_size >> 10);

  if(bt->is_pnp_bios) {
    char *s = isa_id2str(bt->pnp_id);
    dump_line("PnP BIOS: %s\n", s);
    free_mem(s);
  }

  if(bt->lba_support) {
    dump_line_str("BIOS: extended read supported\n");
  }

  if(bt->smp.ok) {
    dump_line("MP spec rev 1.%u info:\n", bt->smp.rev);
    dump_line("  OEM id: \"%s\"\n",  bt->smp.oem_id);
    dump_line("  Product id: \"%s\"\n",  bt->smp.prod_id);
    dump_line("  %u CPUs (%u disabled)\n",  bt->smp.cpus, bt->smp.cpus - bt->smp.cpus_en);
  }

}


/*
 * print PROM entries
 */
void dump_prom(hd_data_t *hd_data, hd_t *hd, FILE *f)
{
  prom_info_t *pt;
  char *s;

  if(!hd->detail || hd->detail->type != hd_detail_prom) return;
  if(!(pt = hd->detail->prom.data)) return;

  if(pt->has_color) {
    s = hd_device_name(hd_data, MAKE_ID(TAG_SPECIAL, 0x0300), MAKE_ID(TAG_SPECIAL, pt->color));
    if(s)
      dump_line("Color: %s (0x%02x)\n", s, pt->color);
    else
      dump_line("Color: 0x%02x\n", pt->color);
  }
}


/*
 * print System entries
 */
void dump_sys(hd_data_t *hd_data, hd_t *hd, FILE *f)
{
  sys_info_t *st;

  if(!hd->detail || hd->detail->type != hd_detail_sys) return;
  if(!(st = hd->detail->sys.data)) return;

  if(st->system_type) {
    dump_line("SystemType: \"%s\"\n", st->system_type);
  }
  if(st->generation) {
    dump_line("Generation: \"%s\"\n", st->generation);
  }
  if(st->vendor) {
    dump_line("Vendor: \"%s\"\n", st->vendor);
  }
  if(st->model) {
    dump_line("Model: \"%s\"\n", st->model);
  }
  if(st->serial) {
    dump_line("Serial ID: \"%s\"\n", st->serial);
  }
  if(st->lang) {
    dump_line("Language: \"%s\"\n", st->lang);
  }
}


char *make_device_name_str(hd_data_t *hd_data, hd_t *h, char *buf, int buf_size)
{
  char *s;

  if(h->dev_name) {
    snprintf(buf, buf_size - 1, "\"%s\"", h->dev_name);
  }
  else {
    s = hd_device_name(hd_data, h->vend, h->dev);
    snprintf(buf, buf_size - 1, "%04x \"%s\"", ID_VALUE(h->dev), s ? s : "?");
  }

  return buf;
}


char *make_sub_device_name_str(hd_data_t *hd_data, hd_t *h, char *buf, int buf_size)
{
  char *s;

  if(h->sub_dev_name) {
    snprintf(buf, buf_size - 1, "\"%s\"", h->sub_dev_name);
  }
  else {
    s = hd_sub_device_name(hd_data, h->vend, h->dev, h->sub_vend, h->sub_dev);
    snprintf(buf, buf_size - 1, "%04x \"%s\"", ID_VALUE(h->sub_dev), s ? s : "?");
  }

  return buf;
}


char *make_vend_name_str(hd_data_t *hd_data, hd_t *h, char *buf, int buf_size)
{
  char *s;

  if(h->vend_name) {
    snprintf(buf, buf_size - 1, "\"%s\"", h->vend_name);
  }
  else {
    s = hd_vendor_name(hd_data, h->vend);
    snprintf(buf, buf_size - 1, "%s \"%s\"", vend_id2str(h->vend), s ? s : "?");
  }

  return buf;
}


char *make_sub_vend_name_str(hd_data_t *hd_data, hd_t *h, char *buf, int buf_size)
{
  char *s;

  if(h->sub_vend_name) {
    snprintf(buf, buf_size - 1, "\"%s\"", h->sub_vend_name);
  }
  else {
    s = hd_vendor_name(hd_data, h->sub_vend);
    snprintf(buf, buf_size - 1, "%s \"%s\"", vend_id2str(h->sub_vend), s ? s : "?");
  }

  return buf;
}


char *vend_id2str(unsigned vend)
{
  static char buf[32];
  char *s;

  *(s = buf) = 0;

  if(ID_TAG(vend) == TAG_EISA) {
    strcpy(s, eisa_vendor_str(vend));
  }
  else {
    if(ID_TAG(vend) == TAG_USB) *s++ = 'u', *s = 0;
    if(ID_TAG(vend) == TAG_SPECIAL) *s++ = 's', *s = 0;
    sprintf(s, "%04x", ID_VALUE(vend));
  }

  return buf;
}

#else	/* ifndef LIBHD_TINY */

void hd_dump_entry(hd_data_t *hd_data, hd_t *h, FILE *f) { }

#endif	/* ifndef LIBHD_TINY */


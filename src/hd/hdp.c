#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

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
static void dump_smbios(hd_data_t *hd_data, FILE *f);
static void dump_prom(hd_data_t *, hd_t *, FILE *);
static void dump_sys(hd_data_t *, hd_t *, FILE *);

static char *dump_hid(hd_data_t *hd_data, hd_id_t *hid, int format, char *buf, int buf_size);
static char *dump_hid2(hd_data_t *hd_data, hd_id_t *hid1, hd_id_t *hid2, char *buf, int buf_size);

/*
 * Dump a hardware entry to FILE *f.
 */
void hd_dump_entry(hd_data_t *hd_data, hd_t *h, FILE *f)
{
  char *s, *a0, *a1, *a2, *s1, *s2;
  char buf1[32], buf2[32];
  hd_t *hd_tmp;
  int i;
  str_list_t *sl;

#ifdef LIBHD_MEMCHECK
  {
    if(libhd_log)
      fprintf(libhd_log, "; %s\t%p\t%p\n", __FUNCTION__, CALLED_FROM(hd_dump_entry, hd_data), hd_data);
  }
#endif

  if(!h) return;

  s = "";
  if(h->is.agp) s = "(AGP)";
  //  pci_flag_pm: dump_line0(", supports PM");
  if(h->is.isapnp) s = "(PnP)";

  a0 = h->bus.name;
  a2 = NULL;
  a1 = h->sub_class.name ?: h->base_class.name;
  if(a1 && h->prog_if.name) {
    str_printf(&a2, 0, "%s (%s)", a1, h->prog_if.name);
  }
  else {
    a2 = new_str(a1 ?: "?");
  }
  dump_line(
    "%02d: %s%s %02x.%x: %02x%02x %s\n",
    h->idx, a0 ? a0 : "?", s, h->slot, h->func,
    h->base_class.id, h->sub_class.id, a2
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

  if(hd_data->flags.dformat == 1) {
    dump_line("ClassName: \"%s\"\n", a2);
    dump_line("Bus: %d\n", h->slot >> 8);
    dump_line("Slot: %d\n", h->slot & 0xff);
    dump_line("Function: %d\n", h->func);
  }

  a2 = free_mem(a2);

  if((hd_data->debug & HD_DEB_CREATION) && h->unique_id) {
    dump_line("Unique ID: %s\n", h->unique_id);
  }

  if((hd_data->debug == -1u) && h->old_unique_id) {
    dump_line("Old Unique ID: %s\n", h->old_unique_id);
  }

  if(h->hw_class && (s = hd_hw_item_name(h->hw_class))) {
    dump_line("Hardware Class: %s\n", s);
  }

  if(h->base_class.id == bc_internal && h->sub_class.id == sc_int_cpu)
    dump_cpu(hd_data, h, f);
  else if(h->base_class.id == bc_internal && h->sub_class.id == sc_int_bios)
    dump_bios(hd_data, h, f);
  else if(h->base_class.id == bc_internal && h->sub_class.id == sc_int_prom)
    dump_prom(hd_data, h, f);
  else if(h->base_class.id == bc_internal && h->sub_class.id == sc_int_sys)
    dump_sys(hd_data, h, f);
  else
    dump_normal(hd_data, h, f);

  s1 = s2 = NULL;
  if(h->is.notready) {
    if(h->base_class.id == bc_storage_device) {
      s1 = "no medium";
    }
    else {
      s1 = "not configured";
    }
  }
  if(h->is.softraiddisk) s2 = "soft raid";
  if(!s1) { s1 = s2; s2 = NULL; }

  if(s1) {
    dump_line("Drive status: %s%s%s\n", s1, s2 ? ", " : "", s2 ?: "");
  }

  if(h->extra_info) {
    dump_line_str("Extra Info: ");
    for(i = 0, sl = h->extra_info; sl; sl = sl->next) {
      dump_line0("%s%s", i ? ", " : "", sl->str);
      i = 1;
    }
    dump_line0("\n");
  }

  if(
    hd_data->debug && (
      h->status.configured ||
      h->status.available ||
      h->status.needed ||
      h->status.invalid ||
      h->is.manual
    )
  ) {
    dump_line_str("Config Status: ");
    i = 0;

    if(h->status.invalid) {
      dump_line0("invalid");
      i++;
    }

    if(h->is.manual) {
      dump_line0("%smanual", i ? ", " : "");
      i++;
    }

    if(h->status.configured && (s = hd_status_value_name(h->status.configured))) {
      dump_line0("%scfg=%s", i ? ", " : "", s);
      i++;
    }

    if(h->status.available && (s = hd_status_value_name(h->status.available))) {
      dump_line0("%savail=%s", i ? ", " : "", s);
      i++;
    }

    if(h->status.needed && (s = hd_status_value_name(h->status.needed))) {
      dump_line0("%sneed=%s", i ? ", " : "", s);
      i++;
    }

    dump_line0("\n");
  }

  if(hd_data->debug == -1u && h->config_string) {
    dump_line("Configured as: \"%s\"\n", h->config_string);
  }

  if(
    h->attached_to &&
    (hd_tmp = hd_get_device_by_idx(hd_data, h->attached_to))
  ) {
    s = hd_tmp->sub_class.name ?: hd_tmp->base_class.name;
    dump_line("Attached to: #%u (%s)\n", h->attached_to, s ?: "?");
  }

  if(
    h->base_class.id == bc_storage_device &&
    h->sub_class.id == sc_sdev_cdrom &&
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
#if 0
    else {
      if(ci->cdrom) {
        dump_line_str("Drive status: non-ISO9660 cdrom\n");
      }
      else {
        dump_line_str("Drive status: no cdrom found\n");
      }
    }
#endif
    if(ci->el_torito.ok) {
      dump_line(
        "El Torito info: platform %u, %sbootable\n",
        ci->el_torito.platform,
        ci->el_torito.bootable ? "" : "not "
      );
      dump_line("  Boot Catalog: at sector 0x%04x\n", ci->el_torito.catalog);
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

#if 0
  if(
    h->base_class.id == bc_storage_device &&
    h->sub_class.id == sc_sdev_floppy &&
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
#endif

  ind -= 2;

  if(h->next) dump_line_str("\n");
}


/*
 * print 'normal' hardware entries
 */
void dump_normal(hd_data_t *hd_data, hd_t *h, FILE *f)
{
  int i, j;
  char *s;
  uint64_t u64;
  hd_res_t *res;
  char buf[256], c0, c1;
  driver_info_t *di;
  str_list_t *sl, *sl1, *sl2;
  isdn_parm_t *ip;

  if(h->model) dump_line("Model: \"%s\"\n", h->model);

  s = NULL;
  switch(h->hotplug) {
    case hp_none:
      break;
    case hp_pcmcia:
      s = "PCMCIA";
      break;
    case hp_cardbus:
      s = "CardBus";
      break;
    case hp_pci:
      s = "PCI";
      break;
    case hp_usb:
      s = "USB";
      break;
    case hp_ieee1394:
      s = "IEEE1394 (FireWire)";
      break;
  }

  if(s) {
    dump_line("Hotplug: %s\n", s);
  }

  if(h->vendor.id || h->vendor.name || h->device.id || h->device.name) {
    if(h->vendor.id || h->vendor.name) {
      dump_line("Vendor: %s\n", dump_hid(hd_data, &h->vendor, 1, buf, sizeof buf));
    }
    dump_line("Device: %s\n", dump_hid(hd_data, &h->device, 0, buf, sizeof buf));
  }

  if(h->sub_vendor.id || h->sub_device.id || h->sub_device.name || h->sub_vendor.name) {
    if(h->sub_vendor.id || h->sub_vendor.name || h->sub_device.id) {
      dump_line("SubVendor: %s\n", dump_hid(hd_data, &h->sub_vendor, 1, buf, sizeof buf));
    }
    dump_line("SubDevice: %s\n", dump_hid(hd_data, &h->sub_device, 0, buf, sizeof buf));
  }

  if(h->revision.name) {
    dump_line("Revision: \"%s\"\n", h->revision.name);
  }
  else if(h->revision.id) {
    dump_line("Revision: 0x%02x\n", h->revision.id);
  }

  if(h->serial) {
    dump_line("Serial ID: \"%s\"\n", h->serial);
  }

  if(h->usb_guid) {
    dump_line("USB GUID: %s\n", h->usb_guid);
  }

  if(h->compat_vendor.id || h->compat_device.id) {
    dump_line(
      "Compatible to: %s\n",
      dump_hid2(hd_data, &h->compat_vendor, &h->compat_device, buf, sizeof buf)
    );
  }

  if(h->driver) {
    dump_line("Driver: \"%s\"\n", h->driver);
  }

  if(
    h->bus.id == bus_usb &&
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
#if defined(__i386__) || defined (__x86_64__)
    dump_line("BIOS id: %s\n", h->rom_id);
#endif
#if defined(__PPC__) || defined(__sparc__)
    dump_line("PROM id: %s\n", h->rom_id);
#endif
#if defined(__s390__) || defined(__s390x__)
    dump_line("Chp id: %s\n", h->rom_id);
#endif
  }

  if(h->tag.ser_skip) {
    dump_line_str("Tags: ser_skip\n");
  }

  if(
    h->is.zip ||
    h->is.cdr || h->is.cdrw || h->is.dvd ||
    h->is.dvdr || h->is.dvdram | h->is.pppoe
  ) {
    dump_line_str("Features:");
    i = 0;
    if(h->is.zip) dump_line0("%s ZIP", i++ ? "," : "");
    if(h->is.cdr) dump_line0("%s CD-R", i++ ? "," : "");
    if(h->is.cdrw) dump_line0("%s CD-RW", i++ ? "," : "");
    if(h->is.dvd) dump_line0("%s DVD", i++ ? "," : "");
    if(h->is.dvdr) dump_line0("%s DVD-R", i++ ? "," : "");
    if(h->is.dvdram) dump_line0("%s DVDRAM", i++ ? "," : "");
    if(h->is.pppoe) dump_line0("%s PPPOE", i++ ? "," : "");
    dump_line0("\n");
  }

  for(res = h->res; res; res = res->next) {
    switch(res->any.type) {
      case res_phys_mem:
        u64 = res->phys_mem.range >> 10;
        c0 = 'M'; c1 = 'k';
        if(u64 >> 20) {
          u64 >>= 10;
          c0 = 'G'; c1 = 'M';
        }
        if((u64 & 0x3ff)) {
          dump_line("Memory Size: %"PRId64" %cB + %"PRId64" %cB\n", u64 >> 10, c0, u64 & 0x3ff, c1);
        }
        else {
          dump_line("Memory Size: %"PRId64" %cB\n", u64 >> 10, c0);
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
	if(res->init_strings.init1) dump_line("Init1: %s\n", res->init_strings.init1);
	if(res->init_strings.init2) dump_line("Init2: %s\n", res->init_strings.init2);
        break;

      case res_pppd_option:
	dump_line("PPPD Option: %s\n", res->pppd_option.option);
        break;

      case res_framebuffer:
        dump_line("Mode 0x%04x: %ux%u (+%u), %u bits\n",
          res->framebuffer.mode,
          res->framebuffer.width,
          res->framebuffer.height,
          res->framebuffer.bytes_p_line,
          res->framebuffer.colorbits
        );
        break;

      default:
        dump_line("Unkown resource type %d\n", res->any.type);
    }
  }

  if((sl = h->requires)) {
    dump_line("Requires: %s", sl->str);
    for(sl = sl->next; sl; sl = sl->next) {
      dump_line0(", %s", sl->str);
    }
    dump_line0("\n");
  }

  for(di = h->driver_info, i = 0; di; di = di->next, i++) {
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
        dump_line_str("Driver Status: ");
        for(sl1 = di->module.names; sl1; sl1 = sl1->next) {
          dump_line0("%s%c", sl1->str, sl1->next ? ',' : ' ');
        }
        dump_line0("%s %sactive\n",
          di->module.names->next ? "are" : "is",
          di->module.active ? "" : "not "
        );

        dump_line_str("Driver Activation Cmd: \"");
        for(sl1 = di->module.names, sl2 = di->module.mod_args; sl1 && sl2; sl1 = sl1->next, sl2 = sl2->next) {
          dump_line0("%s %s%s%s%s",
            di->module.modprobe ? "modprobe" : "insmod",
            sl1->str,
            sl2->str ? " " : "",
            sl2->str ? sl2->str : "",
            (sl1->next && sl2->next) ? "; " : ""
          );
        }

        dump_line0("\"\n");

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
        if(di->mouse.buttons >= 0) dump_line("Buttons: %d\n", di->mouse.buttons);
        if(di->mouse.wheels >= 0) dump_line("Wheels: %d\n", di->mouse.wheels);
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
      case arch_ppc64:
	dump_line0 ("PowerPC (64)\n");
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
      case arch_s390x:
	dump_line0 ("S390x\n");
	break;
      case arch_arm:
	dump_line0 ("ARM\n");
	break;
      case arch_mips:
	dump_line0 ("MIPS\n");
	break;
      case arch_x86_64:
	dump_line0 ("X86-64\n");
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
  if(ct->units) dump_line("Units/Processor: %u\n", ct->units);
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
    dump_line("VESA BIOS Version: %u.%u\n", bt->vbe_ver >> 8, bt->vbe_ver & 0xff);
  }

  if(bt->vbe_video_mem) {
    dump_line("Video Memory: %u kb\n", bt->vbe_video_mem >> 10);
  }

  if(bt->vbe.ok && bt->vbe.current_mode) {
    dump_line("Current VESA Mode: 0x%04x\n", bt->vbe.current_mode);
  }

  if(bt->apm_supported) {
    dump_line("APM Version: %u.%u\n", bt->apm_ver, bt->apm_subver);
    dump_line("APM Status: %s\n", bt->apm_enabled ? "on" : "off");
    dump_line("APM BIOS Flags: 0x%x\n", bt->apm_bios_flags);
  }

  if(bt->led.ok) {
    dump_line_str("BIOS Keyboard LED Status:\n");
    dump_line("  Scroll Lock: %s\n", bt->led.scroll_lock ? "on" : "off");
    dump_line("  Num Lock: %s\n", bt->led.num_lock ? "on" : "off");
    dump_line("  Caps Lock: %s\n", bt->led.caps_lock ? "on" : "off");
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

  if(bt->bios32.ok) {
    dump_line("BIOS32 Service Directory Entry: 0x%05x\n", bt->bios32.entry);
  }

  if(bt->smbios_ver) {
    dump_line("SMBIOS Version: %u.%u\n", bt->smbios_ver >> 8, bt->smbios_ver & 0xff);
  }

  dump_smbios(hd_data, f);
}


/*
 * print SMBIOS entries
 */
void dump_smbios(hd_data_t *hd_data, FILE *f)
{
  hd_smbios_t *sm;
  str_list_t *sl;
  char c, *s, *t;
  unsigned u;
  int i;
  static char *wakeups[] = {
   "Reserved", "Other", "Unknown", "APM Timer",
   "Modem Ring", "LAN Remote", "Power Switch", "PCI PME#",
   "AC Power Restored"
  };
  static char *chassistypes[] = {
    "Unknown", "Other", "Unknown", "Desktop",
    "Low Profile Desktop", "Pizza Box", "Mini Tower", "Tower",
    "Portable", "LapTop", "Notebook", "Hand Held",
    "Docking Station", "All in One", "Sub Notebook", "Space Saving",
    "Lunch Box", "Main Server Chassis", "Expansion Chassis", "SubChassis",
    "Bus Expansion Chassis", "Peripheral Chassis", "RAID Chassis", "Rack Mount Chassis",
    "Sealed-case PC"
  };
  static char *cpustatus[8] = {
    "Unknown Status", "CPU Enabled", "CPU Disabled by User", "CPU Disabled by BIOS",
    "CPU Idle", "Reserved", "Reserved", "Other"
  };
  static char *upgrades[] = {
    NULL, NULL, NULL, "Daughter Board",
    "ZIF Socket", "Replaceable Piggy Back", NULL, "LIF Socket",
    "Slot 1", "Slot 2"
  };
  static char *eccs[5] = {
    "No Error Correction", "Parity", "Single-bit ECC", "Multi-bit ECC", "CRC"
  };
  static char *memtypes[] = {
    "Unknown", "Other", "Unknown", "DRAM",
    "EDRAM", "VRAM", "SRAM", "RAM",
    "ROM", "FLASH", "EEPROM", "FEPROM",
    "EPROM", "CDRAM", "3DRAM", "SDRAM",
    "SGRAM"
  };
  static char *memforms[] = {
    NULL, NULL, NULL, "SIMM",
    "SIP", "Chip", "DIP", "ZIP",
    "Proprietary Card", "DIMM", "TSOP", "Row of Chips",
    "RIMM", "SODIMM"
  };
  static char *mice[] = {
    NULL, "Other", NULL, "Mouse",
    "Track Ball", "Track Point", "Glide Point", "Touch Pad"
  };
  static char *mifaces[] = {
    NULL, NULL, NULL, "Serial",
    "PS/2", "Infrared", "HP-HIL", "Bus Mouse",
    "ADB"
  };
  static char *onboards[] = {
    "Unknown", "Other", "Unknown", "Video",
    "SCSI Controller", "Ethernet", "Token Ring", "Sound"
  };

  if(!hd_data->smbios) return;

  for(sm = hd_data->smbios; sm; sm = sm->next) {
    switch(sm->any.type) {
      case sm_biosinfo:
        fprintf(f, "  BIOS Info:\n");
        if(sm->biosinfo.vendor) fprintf(f, "    Vendor: \"%s\"\n", sm->biosinfo.vendor);
        if(sm->biosinfo.version) fprintf(f, "    Version: \"%s\"\n", sm->biosinfo.version);
        if(sm->biosinfo.date) fprintf(f, "    Date: \"%s\"\n", sm->biosinfo.date);
        fprintf(f, "    Features: %016"PRIx64" %08x\n", sm->biosinfo.features, sm->biosinfo.xfeatures);
        u = sm->biosinfo.features;	/* bits > 31 are not specified anyway */
        if((u & (1 <<  4))) fprintf(f, "      ISA supported\n");
        if((u & (1 <<  5))) fprintf(f, "      MCA supported\n");
        if((u & (1 <<  6))) fprintf(f, "      EISA supported\n");
        if((u & (1 <<  7))) fprintf(f, "      PCI supported\n");
        if((u & (1 <<  8))) fprintf(f, "      PCMCIA supported\n");
        if((u & (1 <<  9))) fprintf(f, "      PnP supported\n");
        if((u & (1 << 10))) fprintf(f, "      APM supported\n");
        if((u & (1 << 11))) fprintf(f, "      BIOS flashable\n");
        if((u & (1 << 12))) fprintf(f, "      BIOS shadowing allowed\n");
        if((u & (1 << 13))) fprintf(f, "      VL-VESA supported\n");
        if((u & (1 << 14))) fprintf(f, "      ESCD supported\n");
        if((u & (1 << 15))) fprintf(f, "      CD boot supported\n");
        if((u & (1 << 16))) fprintf(f, "      Selectable boot supported\n");
        if((u & (1 << 17))) fprintf(f, "      BIOS ROM socketed\n");
        if((u & (1 << 18))) fprintf(f, "      PCMCIA boot supported\n");
        if((u & (1 << 19))) fprintf(f, "      EDD spec supported\n");
        u = sm->biosinfo.xfeatures;
        if((u & (1 <<  0))) fprintf(f, "      ACPI supported\n");
        if((u & (1 <<  1))) fprintf(f, "      USB Legacy supported\n");
        if((u & (1 <<  2))) fprintf(f, "      AGP supported\n");
        if((u & (1 <<  3))) fprintf(f, "      I2O boot supported\n");
        if((u & (1 <<  4))) fprintf(f, "      LS-120 boot supported\n");
        if((u & (1 <<  5))) fprintf(f, "      ATAPI ZIP boot supported\n");
        if((u & (1 <<  6))) fprintf(f, "      IEEE1394 boot supported\n");
        if((u & (1 <<  7))) fprintf(f, "      Smart Battery supported\n");
        if((u & (1 <<  8))) fprintf(f, "      BIOS Boot Spec supported\n");
        break;

      case sm_sysinfo:
        fprintf(f, "  System Info:\n");
        if(sm->sysinfo.manuf) fprintf(f, "    Manufacturer: \"%s\"\n", sm->sysinfo.manuf);
        if(sm->sysinfo.product) fprintf(f, "    Product: \"%s\"\n", sm->sysinfo.product);
        if(sm->sysinfo.version) fprintf(f, "    Version: \"%s\"\n", sm->sysinfo.version);
        if(sm->sysinfo.serial) fprintf(f, "    Serial: \"%s\"\n", sm->sysinfo.serial);
        s = wakeups[sm->sysinfo.wake_up < sizeof wakeups / sizeof *wakeups ? sm->sysinfo.wake_up : 0];
        fprintf(f, "    Wake-up: 0x%02x (%s)\n", sm->sysinfo.wake_up, s);
        break;

      case sm_boardinfo:
        fprintf(f, "  Board Info:\n");
        if(sm->boardinfo.manuf) fprintf(f, "    Manufacturer: \"%s\"\n", sm->boardinfo.manuf);
        if(sm->boardinfo.product) fprintf(f, "    Product: \"%s\"\n", sm->boardinfo.product);
        if(sm->boardinfo.version) fprintf(f, "    Version: \"%s\"\n", sm->boardinfo.version);
        if(sm->boardinfo.serial) fprintf(f, "    Serial: \"%s\"\n", sm->boardinfo.serial);
        break;

      case sm_chassis:
        fprintf(f, "  Chassis Info:\n");
        if(sm->chassis.manuf) fprintf(f, "    Manufacturer: \"%s\"\n", sm->boardinfo.manuf);
        s = chassistypes[sm->chassis.ch_type < sizeof chassistypes / sizeof *chassistypes ? sm->chassis.ch_type : 0];
        fprintf(f, "    Type: 0x%02x (%s)\n", sm->chassis.ch_type, s);
        break;

      case sm_processor:
        fprintf(f, "  Processor Info:\n");
        if(sm->processor.socket) {
          fprintf(f, "    Processor Socket: \"%s\"", sm->processor.socket);
          s = NULL;
          if(sm->processor.upgrade < sizeof upgrades / sizeof *upgrades) s = upgrades[sm->processor.upgrade];
          if(s) fprintf(f, " (%s)", s);
          fprintf(f, "\n");
        }
        if(sm->processor.manuf) fprintf(f, "    Processor Manufacturer: \"%s\"\n", sm->processor.manuf);
        if(sm->processor.version) fprintf(f, "    Processor Version: \"%s\"\n", sm->processor.version);
        if(sm->processor.voltage) {
          fprintf(f, "    Voltage: %u.%u V\n", sm->processor.voltage / 10, sm->processor.voltage % 10);
        }
        if(sm->processor.ext_clock) fprintf(f, "    External Clock: %u\n", sm->processor.ext_clock);
        if(sm->processor.max_speed) fprintf(f, "    Max. Speed: %u\n", sm->processor.max_speed);
        if(sm->processor.current_speed) fprintf(f, "    Current Speed: %u\n", sm->processor.current_speed);
        fprintf(f, "    Status: 0x%02x (Socket %s, %s)\n",
          sm->processor.status,
          (sm->processor.status & 0x40) ? "Populated" : "Empty",
          cpustatus[sm->processor.status & 7]
        );
        break;

      case sm_onboard:
        fprintf(f, "  On Board Devices:\n");
        for(i = 0; (unsigned) i < sizeof sm->onboard.descr / sizeof *sm->onboard.descr; i++) {
          if(sm->onboard.descr[i]) {
            u = sm->onboard.dtype[i] & 0x7f;
            s = onboards[u < sizeof onboards / sizeof *onboards ? u : 0];
            fprintf(f, "    %s: \"%s\"%s\n",
              s,
              sm->onboard.descr[i],
              (sm->onboard.dtype[i] & 0x80) ? "" : " (disabled)"
            );
          }
        }
        break;

      case sm_lang:
        fprintf(f, "  Language Info:\n");
        if((sl = sm->lang.strings)) {
          fprintf(f, "    Languages: ");
          for(; sl; sl = sl->next) {
            fprintf(f, "%s%s", sl->str, sl->next ? ", " : "");
          }
          fprintf(f, "\n");
        }
        if(sm->lang.current) fprintf(f, "    Current: %s\n", sm->lang.current);
        break;

      case sm_memarray:
        fprintf(f, "  Physical Memory Array:\n");
        if(sm->memarray.max_size) {
          u = sm->memarray.max_size;
          c = 'k';
          if(!(u & 0x3ff)) { u >>= 10; c = 'M'; }
          if(!(u & 0x3ff)) { u >>= 10; c = 'G'; }
          fprintf(f, "    Max. Size: %u %cB", u, c);
          if(sm->memarray.ecc >= 3 && sm->memarray.ecc < 8) {
            fprintf(f, " (%s)", eccs[sm->memarray.ecc - 3]);
          }
          fprintf(f, "\n");
        }
        break;

      case sm_memdevice:
        fprintf(f, "  Memory Device:\n");
        if(sm->memdevice.location) fprintf(f, "    Location: \"%s\"\n", sm->memdevice.location);
        if(sm->memdevice.bank) fprintf(f, "    Bank: \"%s\"\n", sm->memdevice.bank);
        if(sm->memdevice.size) {
          u = sm->memdevice.size;
          c = 'k';
          if(!(u & 0x3ff)) { u >>= 10; c = 'M'; }
          if(!(u & 0x3ff)) { u >>= 10; c = 'G'; }
          fprintf(f, "    Size: %u %cB\n", u, c);

          fprintf(f, "    Type: %u bits", sm->memdevice.width);
          if(sm->memdevice.eccbits) fprintf(f, " (+%u ecc bits)", sm->memdevice.eccbits);
          fprintf(f, ",");
          u = sm->memdevice.type2;
          if((u & (1 <<  3))) fprintf(f, " Fast-paged");
          if((u & (1 <<  4))) fprintf(f, " Static column");
          if((u & (1 <<  5))) fprintf(f, " Pseudo static");
          if((u & (1 <<  6))) fprintf(f, " RAMBUS");
          if((u & (1 <<  7))) fprintf(f, " Syncronous");
          if((u & (1 <<  8))) fprintf(f, " CMOS");
          if((u & (1 <<  9))) fprintf(f, " EDO");
          if((u & (1 << 10))) fprintf(f, " Window DRAM");
          if((u & (1 << 11))) fprintf(f, " Cache DRAM");
          if((u & (1 << 12))) fprintf(f, " Non-volatile");
          u = sm->memdevice.type1;
          fprintf(f, " %s", memtypes[u < sizeof memtypes / sizeof *memtypes ? u : 0]);
          u = sm->memdevice.form;
          s = memforms[u < sizeof memforms / sizeof *memforms ? u : 0];
          if(s) fprintf(f, ", %s", s);
          fprintf(f, "\n");
          if(sm->memdevice.speed) fprintf(f, "    Speed: %u MHz", sm->memdevice.speed);
        }
        else {
          fprintf(f, "    Size: No Memory Installed\n");
        }
        break;

      case sm_mouse:
        fprintf(f, "  Pointing Device:\n");
        s = mice[sm->mouse.mtype < sizeof mice / sizeof *mice ? sm->mouse.mtype : 0];
        u = sm->mouse.interface;
        t = mifaces[u < sizeof mifaces / sizeof *mifaces ? u : 0];
        if(u == 0xa0) t = "DB-9 Bus Mouse";
        if(u == 0xa1) t = "micro-DIN Bus Mouse";
        if(u == 0xa2) t = "USB";
        if(s) {
          fprintf(f, "    Mouse: %s%s%s%s, %u buttons\n",
            s,
            t ? " (" : "", t ?: "", t ? ")" : "",
            sm->mouse.buttons
          );
        }
        break;

      case sm_oem:
      case sm_config:
        fprintf(f,
          sm->any.type == sm_oem ? "  OEM Strings:\n" : "  System Config Options (Jumpers & Switches):\n"
        );
        for(sl = sm->any.strings; sl; sl = sl->next) {
          if(sl->str && *sl->str) fprintf(f, "    %s\n", sl->str);
        }

      default:
	break;
    }
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
    // ###########################
    // s = hd_device_name(hd_data, MAKE_ID(TAG_SPECIAL, 0x0300), MAKE_ID(TAG_SPECIAL, pt->color));
    s = NULL;
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


char *dump_hid(hd_data_t *hd_data, hd_id_t *hid, int format, char *buf, int buf_size)
{
  char *s;
  int i;
  unsigned t, id;

  *buf = 0;

  if(hid->id) {
    t = ID_TAG(hid->id);
    id = ID_VALUE(hid->id);

    if(format && t == TAG_EISA) {
      snprintf(buf, buf_size - 1, "%s", eisa_vendor_str(id));
    }
    else {
      snprintf(buf, buf_size - 1, "%s0x%04x", hid_tag_name2(t), id);
    }
  }

  i = strlen(buf);
  s = buf + i;
  buf_size -= i;

  if(!buf_size) return buf;

  if(hid->name) {
    snprintf(s, buf_size - 1, " \"%s\"", hid->name);
  }

  return buf;
}

char *dump_hid2(hd_data_t *hd_data, hd_id_t *hid1, hd_id_t *hid2, char *buf, int buf_size)
{
  char *s;
  int i;
  unsigned t, id1, id2;

  *buf = 0;

  t = 0;
  if(hid1->id) t = ID_TAG(hid1->id);
  if(hid2->id) t = ID_TAG(hid2->id);

  id1 = ID_VALUE(hid1->id);
  id2 = ID_VALUE(hid2->id);

  if(hid1->id || hid2->id) {
    if(t == TAG_EISA) {
      snprintf(buf, buf_size - 1, "%s 0x%04x", eisa_vendor_str(id1), id2);
    }
    else {
      snprintf(buf, buf_size - 1, "%s0x%04x 0x%04x", hid_tag_name2(t), id1, id2);
    }
  }

  i = strlen(buf);
  s = buf + i;
  buf_size -= i;

  if(!buf_size) return buf;

  if(hid2->name) {
    snprintf(s, buf_size - 1, " \"%s\"", hid2->name);
  }

  return buf;
}

#else	/* ifndef LIBHD_TINY */

void hd_dump_entry(hd_data_t *hd_data, hd_t *h, FILE *f) { }

#endif	/* ifndef LIBHD_TINY */


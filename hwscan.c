#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "hd.h"

struct option options[] = {
  { "help", 0, NULL, 'h' },
  { "verbose", 0, NULL, 'v' },
  { "show", 1, NULL, 500 },
  { "list", 0, NULL, 501 },
  { "cfg", 1, NULL, 502 },
  { "avail", 1, NULL, 503 },
  { "need", 1, NULL, 504 },
  { "new", 0, NULL, 505 },
  { "fast", 0, NULL, 506 },
  { "cdrom", 0, NULL, 1000 + hw_cdrom },
  { "floppy", 0, NULL, 1000 + hw_floppy },
  { "disk", 0, NULL, 1000 + hw_disk },
  { "mouse", 0, NULL, 1000 + hw_mouse },
  { "gfxcard", 0, NULL, 1000 + hw_display },
  { "monitor", 0, NULL, 1000 + hw_monitor },
  { "network", 0, NULL, 1000 + hw_network },
  { "sound", 0, NULL, 1000 + hw_sound },
  { "modem", 0, NULL, 1000 + hw_modem },
  { "printer", 0, NULL, 1000 + hw_printer },
  { "storage_ctrl", 0, NULL, 1000 + hw_storage_ctrl },
  { "netcard", 0, NULL, 1000 + hw_network_ctrl },
  { "network_ctrl", 0, NULL, 1000 + hw_network_ctrl },
  { "camera", 0, NULL, 1000 + hw_camera },
  { "isdn", 0, NULL, 1000 + hw_isdn },
  { "tv", 0, NULL, 1000 + hw_tv },
  { "scanner", 0, NULL, 1000 + hw_scanner },
  { "joystick", 0, NULL, 1000 + hw_joystick },
  { "usb", 0, NULL, 1000 + hw_usb },
  { "pci", 0, NULL, 1000 + hw_pci },
  { "isapnp", 0, NULL, 1000 + hw_isapnp },
  { "framebuffer", 0, NULL, 1000 + hw_framebuffer },
  { "keyboard", 0, NULL, 1000 + hw_keyboard },
  { "chipcard", 0, NULL, 1000 + hw_chipcard },
  { "braille", 0, NULL, 1000 + hw_braille },
  { "partition", 0, NULL, 1000 + hw_partition },
  { "usb_ctrl", 0, NULL, 1000 + hw_usb_ctrl },
  { "sys", 0, NULL, 1000 + hw_sys },
  { "cpu", 0, NULL, 1000 + hw_cpu },
  { "bios", 0, NULL, 1000 + hw_bios },
  { "bridge", 0, NULL, 1000 + hw_bridge },
  { "hub", 0, NULL, 1000 + hw_hub },
  { "memory", 0, NULL, 1000 + hw_memory },
  { }
};

int verbose = 0;
hd_hw_item_t scan_item = 0;
int found_items = 0;

int opt_show = 0;
int opt_scan = 0;
int opt_list = 0;
int opt_config_cfg = 0;
int opt_config_avail = 0;
int opt_config_need = 0;
int opt_new = 0;
int opt_fast = 0;

void help(void);
int do_scan(hd_hw_item_t item);
int do_show(char *id);
int do_list(hd_hw_item_t item);
int do_config(int type, char *val, char *id);
int fast_ok(hd_hw_item_t item);

int main(int argc, char **argv)
{
  int rc = 0;

#ifndef LIBHD_TINY

  char *id = NULL;
  char *config_cfg = NULL;
  char *config_avail = NULL;
  char *config_need = NULL;
  int i;
  int ok = 0;
  FILE *f;

  opterr = 0;

  while((i = getopt_long(argc, argv, "hv", options, NULL)) != -1) {
    switch(i) {
      case 'v':
        verbose = 1;
        break;

      case 500:
        opt_show = 1;
        id = optarg;
        break;

      case 501:
        opt_list = 1;
        break;

      case 502:
        opt_config_cfg = 1;
        config_cfg = optarg;
        break;

      case 503:
        opt_config_avail = 1;
        config_avail = optarg;
        break;

      case 504:
        opt_config_need = 1;
        config_need = optarg;
        break;

      case 505:
        opt_new = 1;
        break;

      case 506:
        opt_fast = 1;
        break;

      case 1000 ... 1100:
        opt_scan = 1;
        scan_item = i - 1000;
        break;

      default:
        help();
        return 1;
    }
  }

  if(opt_scan && !opt_list) {
    if(argv[optind] || !scan_item) return help(), 1;
    rc = do_scan(scan_item);
    if(found_items) {
      unlink("/var/lib/hardware/.update"); /* so we do retrigger a rescan */
      if((f = fopen("/var/lib/hardware/.update", "a"))) fclose(f);
    }
    ok = 1;
  }

  if(opt_show) {
    do_show(id);
    ok = 1;
  }

  if(opt_list) {
    do_list(scan_item);
    ok = 1;
  }

  if(opt_config_cfg) {
    if(!argv[optind]) return help(), 1;
    do_config(1, config_cfg, argv[optind]);
    ok = 1;
  }

  if(opt_config_avail) {
    if(!argv[optind]) return help(), 1;
    do_config(2, config_avail, argv[optind]);
    ok = 1;
  }

  if(opt_config_need) {
    if(!argv[optind]) return help(), 1;
    do_config(3, config_need, argv[optind]);
    ok = 1;
  }

  if(!ok) help();

#endif		/* !defined(LIBHD_TINY) */

  return rc;
}

void help()
{
  fprintf(stderr,
    "Usage: hwscan [options]\n"
    "Show information about currently known hardware.\n"
    "  --list            show list of known hardware\n"
    "  --cfg=state id    change 'configured' status; id is one of the\n"
    "                    ids from 'hwscan --list'\n"
    "                    state is one of new, no, yes\n"
    "  --avail=state id  change 'available' status\n"
    "  --need=state id   change 'needed' status\n"
    "  --hw_item         probe for hw_item and update status info\n"
    "  hw_item is one of:\n"
    "    cdrom, floppy, disk, mouse, gfxcard, monitor, network, sound, modem,\n"
    "    printer, storage_ctrl, netcard, camera, isdn, tv, scanner, joystick,\n"
    "    usb, pci, isapnp, framebuffer, keyboard, chipcard, braille, partition,\n"
    "    usb_ctrl, sys, cpu, bios, bridge, hub, memory\n"
  );
}

#ifndef LIBHD_TINY

int do_scan(hd_hw_item_t item)
{
  int run_config = 0;
  hd_status_t status = { };
  hd_data_t *hd_data;
  hd_t *hd, *hd1;
  int err = 0;

  if(opt_fast) opt_fast = fast_ok(item);

  hd_data = calloc(1, sizeof *hd_data);

  hd_data->flags.list_all = 1;
  hd_data->flags.fast = opt_fast;

  hd = hd_list(hd_data, item, 1, NULL);

  if(hd) found_items = 1;

  for(hd1 = hd; hd1; hd1 = hd1->next) {
    err = hd_write_config(hd_data, hd1);
#if 0
    if(verbose) {
      printf(
        "write=%d %s: (cfg=%s, avail=%s, need=%s",
        err,
        hd1->unique_id,
        hd_status_value_name(hd1->status.configured),
        hd_status_value_name(hd1->status.available),
        hd_status_value_name(hd1->status.needed)
      );
      if(hd1->unix_dev_name) {
        printf(", dev=%s", hd1->unix_dev_name);
      }
      printf(
        ") %s\n",
        hd1->model
      );
      
    }
#endif
    if(err) break;
  }

  if(err) {
    fprintf(stderr,
      "Error writing configuration for %s (%s)\n",
      hd1->unique_id,
      hd1->model
    );
    exit(1);
  }

  hd = hd_free_hd_list(hd);

  if(opt_new) {
    status.configured = status_new;
  }
  else {
    status.reconfig = status_yes;
  }

  hd = hd_list_with_status(hd_data, item, status);
  if(hd) run_config = 1;

  if(verbose) {
    for(hd1 = hd; hd1; hd1 = hd1->next) {
      printf(
        "%s: (cfg=%s, avail=%s, need=%s",
        hd1->unique_id,
        hd_status_value_name(hd1->status.configured),
        hd_status_value_name(hd1->status.available),
        hd_status_value_name(hd1->status.needed)
      );
      if(hd1->unix_dev_name) {
        printf(", dev=%s", hd1->unix_dev_name);
      }
      printf(
        ") %s\n",
        hd1->model
      );
    }
  }
  else {
    for(hd1 = hd; hd1; hd1 = hd1->next) printf("%s\n", hd1->unique_id);
  }

  hd = hd_free_hd_list(hd);

  hd_free_hd_data(hd_data);
  free(hd_data);

  return run_config ^ 1;
}


int do_show(char *id)
{
  hd_data_t *hd_data;
  hd_t *hd;

  hd_data = calloc(1, sizeof *hd_data);

  hd = hd_read_config(hd_data, id);

  if(hd) {
    hd_data->debug = -1;
    hd_dump_entry(hd_data, hd, stdout);
    hd = hd_free_hd_list(hd);
  }
  else {
    printf("no such hardware item: %s\n", id);
  }

  hd_free_hd_data(hd_data);
  free(hd_data);

  return 0;
}


int do_list(hd_hw_item_t item)
{
  hd_data_t *hd_data;
  hd_t *hd, *hd_manual;
  char *s;
  char status[64];
  int i;

  hd_data = calloc(1, sizeof *hd_data);

  hd_manual = hd_list(hd_data, hw_manual, 1, NULL);

  if(opt_scan) {
    hd = hd_list(hd_data, item, 0, NULL);
    for(hd = hd; hd; hd = hd->next) printf("%s\n", hd->unique_id);
    hd = hd_free_hd_list(hd);
  }
  else {
    for(hd = hd_manual; hd; hd = hd->next) {
      strcpy(status, "(");

      i = 0;
      if(hd->status.configured && (s = hd_status_value_name(hd->status.configured))) {
        sprintf(status + strlen(status), "%scfg=%s", i ? ", " : "", s);
        i++;
      }

      if(hd->status.available && (s = hd_status_value_name(hd->status.available))) {
        sprintf(status + strlen(status), "%savail=%s", i ? ", " : "", s);
        i++;
      }

      if(hd->status.needed && (s = hd_status_value_name(hd->status.needed))) {
        sprintf(status + strlen(status), "%sneed=%s", i ? ", " : "", s);
        i++;
      }

      strcat(status, ")");

      s = hd_hw_item_name(hd->hw_class);
      if(!s) s = "???";

      printf("%s: %-32s %-16s %s\n", hd->unique_id, status, s, hd->model);
      if(hd->config_string) {
        printf("   configured as: \"%s\"\n", hd->config_string);
      }
    }
  }

  hd_free_hd_list(hd_manual);

  hd_free_hd_data(hd_data);
  free(hd_data);

  return 0;
}


int do_config(int type, char *val, char *id)
{
  hd_data_t *hd_data;
  hd_t *hd;
  hd_status_value_t status = 0;
  int i;
  char *s;

  hd_data = calloc(1, sizeof *hd_data);

  hd = hd_read_config(hd_data, id);

  if(hd) {
    for(i = 1; i < 8; i++) {
      s = hd_status_value_name(i);
      if(s && !strcmp(val, s)) {
        status = i;
        break;
      }
    }
    if(!status) {
      printf("invalid status: %s\n", val);
    }
    else {
      switch(type) {
        case 1:
          hd->status.configured = status;
          break;

        case 2:
          hd->status.available = status;
          break;

        case 3:
          hd->status.needed = status;
          break;
      }
      hd_write_config(hd_data, hd);
    }
    hd = hd_free_hd_list(hd);
  }
  else {
    printf("no such hardware item: %s\n", id);
  }

  hd_free_hd_data(hd_data);
  free(hd_data);

  return 0;
}


/*
 * Check whether a 'fast' scan would suffice to re-check the presence
 * of all known hardware.
 */
int fast_ok(hd_hw_item_t item)
{
  hd_data_t *hd_data;
  hd_t *hd, *hd1;
  int ok = 1;

  if(item != hw_mouse && item != hw_storage_ctrl) {
    return 1;
  }

  hd_data = calloc(1, sizeof *hd_data);

  hd_data->flags.list_all = 1;

  hd = hd_list(hd_data, hw_manual, 1, NULL);

  for(hd1 = hd; hd1; hd1 = hd1->next) {
    /* serial mice */
    if(hd1->hw_class == hw_mouse && hd1->bus == bus_serial) {
      ok = 0;
      break;
    }
    /* parallel zip */
    if(hd1->hw_class == hw_storage_ctrl && hd1->bus == bus_parallel) {
      ok = 0;
      break;
    }
  }

  hd_free_hd_data(hd_data);
  free(hd_data);

  return ok;
}

#endif		/* !defined(LIBHD_TINY) */

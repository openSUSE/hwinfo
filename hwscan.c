#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "hd.h"

struct option options[] = {
  { "help", 0, NULL, 'h' },
  { "verbose", 0, NULL, 'v' },
  { "cdrom", 0, NULL, 1000 + hw_cdrom },
  { "floppy", 0, NULL, 1000 + hw_floppy },
  { "disk", 0, NULL, 1000 + hw_disk },
  { "mouse", 0, NULL, 1000 + hw_mouse },
  { "gfxcard", 0, NULL, 1000 + hw_display },
  { "network", 0, NULL, 1000 + hw_network },
  { "sound", 0, NULL, 1000 + hw_sound },
  { "modem", 0, NULL, 1000 + hw_modem },
  { "printer", 0, NULL, 1000 + hw_printer },
  { "storage_ctrl", 0, NULL, 1000 + hw_storage_ctrl },
  { "network_ctrl", 0, NULL, 1000 + hw_network_ctrl },
  { "camera", 0, NULL, 1000 + hw_camera },
  { "isdn", 0, NULL, 1000 + hw_isdn },
  { "tv", 0, NULL, 1000 + hw_tv },
  { "scanner", 0, NULL, 1000 + hw_scanner }
};

int verbose = 0;
hd_hw_item_t scan_item = 0;

void help(void);
int do_scan(hd_hw_item_t item);

int main(int argc, char **argv)
{
  int i, rc = 0;

  opterr = 0;

  while((i = getopt_long(argc, argv, "hv", options, NULL)) != -1) {
    switch(i) {
      case 'v':
        verbose = 1;
        break;

      case 1000 + hw_disk:
      case 1000 + hw_cdrom:
      case 1000 + hw_mouse:
      case 1000 + hw_display:
        scan_item = i - 1000;
        break;

      default:
        help();
        return 1;
    }
  }

  if(argv[optind] || !scan_item) {
    help();
    return 1;
  }

#ifndef LIBHD_TINY
  do_scan(scan_item);
#endif

  return rc;
}

void help()
{
  fprintf(stderr, "usage: hwscan [options]\n");
}

#ifndef LIBHD_TINY

int do_scan(hd_hw_item_t item)
{
  int run_config = 0;
  hd_status_t status = {};
  hd_data_t *hd_data;
  hd_t *hd, *hd1;
  int err = 0;

  hd_data = calloc(1, sizeof *hd_data);

  hd = hd_list(hd_data, item, 1, NULL);

  for(hd1 = hd; hd1; hd1 = hd1->next) {
    err = hd_write_config(hd_data, hd1);
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

  status.reconfig = status_yes;
  hd = hd_list_with_status(hd_data, item, status);
  if(hd) run_config = 1;

  if(verbose) {
    for(hd1 = hd; hd1; hd1 = hd1->next) {
      printf(
        "%s [cfg=%s, avail=%s, crit=%s",
        hd1->unique_id,
        hd_status_value_name(hd1->status.configured),
        hd_status_value_name(hd1->status.available),
        hd_status_value_name(hd1->status.critical)
      );
      if(hd1->unix_dev_name) {
        printf(", dev=%s", hd1->unix_dev_name);
      }
      printf(
        "]: %s\n",
        hd1->model
      );
    }
    printf("run config: %s\n", run_config ? "yes" : "no");
  }

  hd = hd_free_hd_list(hd);

  hd_free_hd_data(hd_data);
  free(hd_data);

  return run_config;
}

#endif	/* LIBHD_TINY */

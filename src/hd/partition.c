#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/iso_fs.h>

#include "hd.h"
#include "hd_int.h"
#include "partition.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void read_partition(hd_data_t *hd_data);


void hd_scan_partition(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_partition)) return;

  hd_data->module = mod_partition;

  /* some clean-up */
  remove_hd_entries(hd_data);

  hd_data->disks = free_str_list(hd_data->disks);
  hd_data->partitions = free_str_list(hd_data->partitions);

  PROGRESS(1, 0, "partition");

  read_partition(hd_data);

}


void read_partition(hd_data_t *hd_data)
{
  str_list_t *sl, *sl0, *pl0 = NULL;
  char buf[256], *s, *s1;
  int l, type;

  if(!(sl0 = read_file(PROC_PARTITIONS, 2, 0))) return;

  if(hd_data->debug) {
    ADD2LOG("----- "PROC_PARTITIONS" -----\n");
    for(sl = sl0; sl; sl = sl->next) {
      ADD2LOG("  %s", sl->str);
    }
    ADD2LOG("----- "PROC_PARTITIONS" end -----\n");
  }

  for(sl = sl0; sl; sl = sl->next) {
    *buf = 0;
    if(sscanf(sl->str, "%*s %*s %*s %255s", buf) > 0) {
      if(*buf) add_str_list(&pl0, buf);
    }
  }

  free_str_list(sl0);

#if 0
  dasda, dasda1
  hda, hda1
  i2o/hda, i2o/hda1
  i2o/hdab, i2o/hdab1
  sda, sda1
  sdab, sdab1
  ataraid/d0, ataraid/d0p1
  cciss/c0d0, cciss/c0d0p1
  ida/c0d0, ida/c0d0p1
  rd/c0d0, rd/c0d0p1
#endif

  for(sl = pl0; sl; sl = sl->next) {
    s = sl->str;
    l = strlen(s);
    if(!l) continue;

    s1 = s + l - 1;
    while(isdigit(*s1) && s1 > s) s1--;
    if(s1 == s) continue;

    type = 0;
    if(
      strstr(s, "dasd") == s ||
      strstr(s, "hd") == s ||
      strstr(s, "i2o/hd") == s ||
      strstr(s, "sd") == s
    ) type = 1;

    if(!s1[1] || !isdigit(s1[1]) || (type == 0 && *s1 == 'd')) {
      add_str_list(&hd_data->disks, s);
      continue;
    }

    if(*s1 == 'p' || type == 1) {
      add_str_list(&hd_data->partitions, s);
      continue;
    }

  }

  free_str_list(pl0);

  if(hd_data->debug) {
    ADD2LOG("disks:\n");
    for(sl = hd_data->disks; sl; sl = sl->next) ADD2LOG("  %s\n", sl->str);
    ADD2LOG("partitions:\n");
    for(sl = hd_data->partitions; sl; sl = sl->next) ADD2LOG("  %s\n", sl->str);
  }
}


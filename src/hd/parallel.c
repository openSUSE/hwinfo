#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "parallel.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * parallel port device info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static void dump_parallel_data(hd_data_t *hd_data, str_list_t *sl);

void hd_scan_parallel(hd_data_t *hd_data)
{
  hd_t *hd, *hd_i;
  str_list_t *sl, *sl0;
  hd_res_t *res;
  char *pp = NULL, buf[256], unix_dev[] = "/dev/lp0", *s = NULL;
  char *base_class, *device, *vendor, *cmd_set;
  int i, j, port;
  str_list_t *log = NULL;

  if(!hd_probe_feature(hd_data, pr_parallel)) return;

  hd_data->module = mod_parallel;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "pp mod");

  /* parport_probe loaded... ? */
  for(sl = read_kmods(hd_data); sl; sl = sl->next) {
    if(!strcmp(sl->str, "parport_probe")) break;
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class == bc_comm && hd->sub_class == sc_com_par) break;
  }
  /* ... if there seems to be a parallel interface, try to load it */
  if(hd) {
    if(sl) run_cmd(hd_data, "/sbin/rmmod parport_probe");
    run_cmd(hd_data, "/sbin/insmod parport_probe");
  }

  sl = free_str_list(sl);

  /* got through hda...hdp */
  for(i = 0; i < 3; i++, unix_dev[sizeof unix_dev - 2]++) {
    PROGRESS(2, 1 + i, "read info");

    port = 0;
    // ##### read modes as well? (e.g: SPP,ECP,ECPEPP,ECPPS2)
    str_printf(&pp, 0, "%s/%d/hardware", PROC_PARPORT, i);
    sl0 = read_file(pp, 0, 0);
    if(!sl0) continue;		/* file doesn't exist -> no parport entry */
    str_printf(&s, 0, "%s\n", pp);
    add_str_list(&log, s);
    for(sl = sl0; sl; sl = sl->next) {
      str_printf(&s, 0, "  %s", sl->str);
      add_str_list(&log, s);
      if(sscanf(sl->str, "base: %i", &j) == 1) port = j;
    }
    free_str_list(sl0);

    str_printf(&pp, 0, "%s/%d/autoprobe", PROC_PARPORT, i);
    sl0 = read_file(pp, 0, 0);
    str_printf(&s, 0, "%s\n", pp);
    add_str_list(&log, s);
    base_class = device = vendor = cmd_set = NULL;
    for(sl = sl0; sl; sl = sl->next) {
      str_printf(&s, 0, "  %s", sl->str);
      add_str_list(&log, s);
//      fprintf(stderr, "str = \"%s\"\n", sl->str);
      if(sscanf(sl->str, "CLASS: %255[^\n;]", buf) == 1) base_class = new_str(buf);
      if(sscanf(sl->str, "MODEL: %255[^\n;]", buf) == 1) device = new_str(buf);
      if(sscanf(sl->str, "MANUFACTURER: %255[^\n;]", buf) == 1) vendor = new_str(buf);
      if(sscanf(sl->str, "COMMAND SET: %255[^\n;]", buf) == 1) cmd_set = new_str(buf);
    }
    free_str_list(sl0);

    pp = free_mem(pp);
    s = free_mem(s);

//    fprintf(stderr, "port <0x%x\n", port);
//    fprintf(stderr, "class <%s>\n", base_class);
//    fprintf(stderr, "device <%s>\n", device);
//    fprintf(stderr, "vendor <%s>\n", vendor);
//    fprintf(stderr, "cmds <%s>\n", cmd_set);

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class == bc_comm &&
        hd->sub_class == sc_com_par &&
        hd->unix_dev_name &&
        !strcmp(hd->unix_dev_name, unix_dev)
      ) break;
    }

    if(!hd) {
      /* no entry ??? */
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_comm;
      hd->sub_class = sc_com_par;
      hd->unix_dev_name = new_str(unix_dev);
      if(port) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->io.type = res_io;
        res->io.enabled = 1;
        res->io.base = port;
        res->io.access = acc_rw;
      }
    }

    // ##### check if ports match?

    if(
      base_class ||
      (device && strcmp(device, "Unknown device")) ||
      (vendor && strcmp(vendor, "Unknown vendor"))
    ) {
      hd_i = hd;
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->attached_to = hd_i->idx;
      hd->unix_dev_name = new_str(hd_i->unix_dev_name);
      hd->base_class = bc_none;
      if(base_class && !strcmp(base_class, "PRINTER")) hd->base_class = bc_printer;
      hd->bus = bus_parallel;

      if(!hd_find_device_by_name(hd_data, hd->base_class, vendor, device, &hd->vend, &hd->dev)) {
        /* not found in database */
        hd->dev_name = new_str(device);
        hd->vend_name = new_str(vendor);
      }
    }

    free_mem(base_class);
    free_mem(device);
    free_mem(vendor);
    free_mem(cmd_set);
  }

  if((hd_data->debug & HD_DEB_PARALLEL)) dump_parallel_data(hd_data, log);

  free_str_list(log);

}


void dump_parallel_data(hd_data_t *hd_data, str_list_t *sl)
{
  ADD2LOG("----- parallel info -----\n");
  for(; sl; sl = sl->next) {
    ADD2LOG("%s", sl->str);
  }
  ADD2LOG("----- parallel info end -----\n");
}


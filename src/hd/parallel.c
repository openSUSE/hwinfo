#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hd.h"
#include "hd_int.h"
#include "parallel.h"

/**
 * @defgroup PPORTint Parallel port devices
 * @ingroup libhdDEVint
 * @brief Parallel port device information
 *
 * @{
 */

#ifndef LIBHD_TINY

static void do_lp(hd_data_t *hd_data);
static void do_zip(hd_data_t *hd_data);
static void dump_parallel_data(hd_data_t *hd_data, str_list_t *sl);

void hd_scan_parallel(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_parallel)) return;

  hd_data->module = mod_parallel;

  /* some clean-up */
  remove_hd_entries(hd_data);

  if(hd_probe_feature(hd_data, pr_parallel_lp)) do_lp(hd_data);

  if(hd_probe_feature(hd_data, pr_parallel_zip)) do_zip(hd_data);
}

void do_lp(hd_data_t *hd_data)
{
  hd_t *hd, *hd_i;
  str_list_t *sl, *sl0;
  hd_res_t *res;
  char *pp = NULL, buf[256], unix_dev[] = "/dev/lp0", *s = NULL;
  char *base_class, *device, *vendor, *cmd_set;
  int i, j, port;
  str_list_t *log = NULL;

  PROGRESS(1, 0, "pp mod");

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class.id == bc_comm && hd->sub_class.id == sc_com_par) break;
  }

  /* ... if there seems to be a parallel interface, try to load it */
  if(hd || 1) {		/* always load it */
    if(hd_data->kernel_version == KERNEL_22) {
      unload_module(hd_data, "parport_probe");
      probe_module(hd_data, "parport_probe");
    } else {
      unload_module(hd_data, "lp");
      unload_module(hd_data, "parport_pc");
      probe_module(hd_data, "parport_pc");
      probe_module(hd_data, "lp");
    }
  }

  for(i = 0; i < 3; i++, unix_dev[sizeof unix_dev - 2]++) {
    PROGRESS(2, 1 + i, "lp read info");

    port = 0;
    // ##### read modes as well? (e.g: SPP,ECP,ECPEPP,ECPPS2)
    if(hd_data->kernel_version == KERNEL_22)
      str_printf(&pp, 0, PROC_PARPORT_22 "%d/hardware", i);
    else
      str_printf(&pp, 0, PROC_PARPORT_24 "%d/base-addr", i);
    sl0 = read_file(pp, 0, 0);
    if(!sl0) continue;		/* file doesn't exist -> no parport entry */
    str_printf(&s, 0, "%s\n", pp);
    add_str_list(&log, s);
    for(sl = sl0; sl; sl = sl->next) {
      str_printf(&s, 0, "  %s", sl->str);
      add_str_list(&log, s);
      if(hd_data->kernel_version == KERNEL_22) {
        if(sscanf(sl->str, "base: %i", &j) == 1) port = j;
      } else {
        if(sscanf(sl->str, "%i", &j) == 1) port = j;
      }
    }
    free_str_list(sl0);

    if(hd_data->kernel_version == KERNEL_22)
      str_printf(&pp, 0, PROC_PARPORT_22 "%d/autoprobe", i);
    else
      str_printf(&pp, 0, PROC_PARPORT_24 "%d/autoprobe", i);
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

    /* default to printer */
    if(!base_class && vendor && device) base_class = new_str("printer");

    s = free_mem(s);

//    fprintf(stderr, "port <0x%x\n", port);
//    fprintf(stderr, "class <%s>\n", base_class);
//    fprintf(stderr, "device <%s>\n", device);
//    fprintf(stderr, "vendor <%s>\n", vendor);
//    fprintf(stderr, "cmds <%s>\n", cmd_set);

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_comm &&
        hd->sub_class.id == sc_com_par &&
        hd->unix_dev_name &&
        !strcmp(hd->unix_dev_name, unix_dev)
      ) break;
    }

    if(!hd) {
      /* no entry ??? */
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_comm;
      hd->sub_class.id = sc_com_par;
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
      hd->base_class.id = bc_none;
      if(base_class && !strcasecmp(base_class, "printer")) hd->base_class.id = bc_printer;
      hd->bus.id = bus_parallel;

      hd->vendor.name = new_str(vendor);
      hd->device.name = new_str(device);
    }

    free_mem(base_class);
    free_mem(device);
    free_mem(vendor);
    free_mem(cmd_set);
  }

  pp = free_mem(pp);

  if((hd_data->debug & HD_DEB_PARALLEL)) dump_parallel_data(hd_data, log);

  free_str_list(log);

}

void do_zip(hd_data_t *hd_data)
{
  hd_t *hd, *hd_i;
  int i, j, port, is_imm, is_ppa, is_imm0, is_ppa0;
  char *pp = NULL, *s = NULL, *unix_dev = NULL;
  str_list_t *log = NULL, *sl, *sl0;
  int do_imm = hd_probe_feature(hd_data, pr_parallel_imm);

  is_imm = is_imm0 = hd_module_is_active(hd_data, "imm");
  is_ppa = is_ppa0 = hd_module_is_active(hd_data, "ppa");

  if(!(is_imm || is_ppa)) {
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->base_class.id == bc_comm && hd->sub_class.id == sc_com_par) break;
    }
    /* ... if there seems to be a parallel interface, try to load it */
    if(hd) {
      if(do_imm) {
        PROGRESS(5, 0, "imm mod");
        load_module(hd_data, "imm");
      }
      PROGRESS(5, 0, "ppa mod");
      load_module(hd_data, "ppa");
      is_imm = hd_module_is_active(hd_data, "imm");
      is_ppa = hd_module_is_active(hd_data, "ppa");
      if(do_imm && !is_imm) {
        int fd;
        char flush[2] = { 4, 12 };

        fd = open("/dev/lp0", O_NONBLOCK | O_WRONLY);
        if(fd != -1) {
          write(fd, flush, sizeof flush);
          close(fd);
        }
      }
    }
  }

  if(!(is_imm || is_ppa)) return;

  PROGRESS(6, 0, "zip read info");

  for(i = 0; i < 16; i++) {
    str_printf(&pp, 0, PROC_SCSI "/%s/%d", (i % 2) ? "ppa" : "imm", i / 2);
    sl0 = read_file(pp, 0, 0);
    if(!sl0) continue;
    str_printf(&s, 0, "%s\n", pp);
    add_str_list(&log, s);
    port = -1;
    for(sl = sl0; sl; sl = sl->next) {
      str_printf(&s, 0, "  %s", sl->str);
      add_str_list(&log, s);
      if(sscanf(sl->str, "Parport : parport%d", &j) == 1) port = j;
    }
    free_str_list(sl0);
    pp = free_mem(pp);
    s = free_mem(s);

    unix_dev = free_mem(unix_dev);
    if(port >= 0) {
      str_printf(&unix_dev, 0, "/dev/lp%d", port);
    }

    hd = NULL;
    if(unix_dev) {
      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(
          hd->base_class.id == bc_comm &&
          hd->sub_class.id == sc_com_par &&
          hd->unix_dev_name &&
          !strcmp(hd->unix_dev_name, unix_dev)
        ) break;
      }

      if(!hd) {
        /* no entry ??? */
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_comm;
        hd->sub_class.id = sc_com_par;
        hd->unix_dev_name = new_str(unix_dev);
      }
    }

    hd_i = hd;
    hd = add_hd_entry(hd_data, __LINE__, 0);
    if(hd_i) {
      hd->attached_to = hd_i->idx;
      hd->unix_dev_name = new_str(hd_i->unix_dev_name);
    }
    hd->base_class.id = bc_storage;
    hd->sub_class.id = sc_sto_scsi;
    hd->bus.id = bus_parallel;
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x1800);
    hd->device.id = MAKE_ID(TAG_SPECIAL, (i % 2) ? 2 : 1);
  }

  if(!is_imm0) unload_module(hd_data, "imm");
  if(!is_ppa0) unload_module(hd_data, "ppa");

  if((hd_data->debug & HD_DEB_PARALLEL)) dump_parallel_data(hd_data, log);

  free_mem(unix_dev);

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

#endif	/* ifndef LIBHD_TINY */

/** @} */


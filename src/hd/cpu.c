#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "cpu.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * cpu info
 *
 * Note: on other architectures, entries differ (cf. Alpha)!!!
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static void dump_cpu_data(hd_data_t *hd_data);

void hd_scan_cpu(hd_data_t *hd_data)
{
  hd_t *hd;
  unsigned cpus = 0;
  cpu_info_t *ct;
  str_list_t *sl;

#ifdef __i386__
  char model_id[80], vendor_id[80];
  unsigned bogo, mhz, cache, family, model, stepping;
#endif

#ifdef __alpha__
  char model_id[80], system_id[80], serial_number[80];
  unsigned cpu_variation, cpu_revision, u;
  cpu_info_t *ct1;
#endif

#ifdef __PPC__
  char model_id[80], system_id[80], serial_number[80];
  unsigned cpu_variation, cpu_revision;
#endif

  if(!hd_probe_feature(hd_data, pr_cpu)) return;

  hd_data->module = mod_cpu;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->cpu = NULL;

  PROGRESS(1, 0, "cpuinfo");

  hd_data->cpu = read_file(PROC_CPUINFO, 0, 0);
  if((hd_data->debug & HD_DEB_CPU)) dump_cpu_data(hd_data);
  if(!hd_data->cpu) return;

#ifdef __alpha__
  *model_id = *system_id = *serial_number = 0;
  cpu_variation = cpu_revision = 0;
  cpus = 1;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "cpu model : %79[^\n]", model_id) == 1) continue;
    if(sscanf(sl->str, "system type : %79[^\n]", system_id) == 1) continue;
    if(sscanf(sl->str, "cpu variation : %u", &cpu_variation) == 1) continue;
    if(sscanf(sl->str, "cpu revision : %u", &cpu_revision) == 1) continue;
    if(sscanf(sl->str, "system serial number : %79[^\n]", serial_number) == 1) continue;
    if(sscanf(sl->str, "cpus detected : %u", &cpus) == 1) continue;
  }

  if(*model_id || *system_id) {	/* at least one of those */
    ct = new_mem(sizeof *ct);
    ct->architecture = arch_alpha;
    if(model_id) ct->model_name = new_str(model_id);
    if(system_id) ct->vend_name = new_str(system_id);
    if(strncmp(serial_number, "MILO", 4) == 0)
      ct->boot = boot_milo;
    else
      ct->boot = boot_aboot;

    ct->family = cpu_variation;
    ct->model = cpu_revision;
    ct->stepping = 0;
    ct->cache = 0;
    ct->clock = 0;

    for(u = 0; u < cpus; u++) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_internal;
      hd->sub_class = sc_int_cpu;
      hd->slot = u;
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_cpu;
      if(u) {
        hd->detail->cpu.data = ct1 = new_mem(sizeof *ct);
        *ct1 = *ct;
        ct1->model_name = new_str(ct1->model_name);
        ct1->vend_name = new_str(ct1->vend_name);
      }
      else {
        hd->detail->cpu.data = ct;
      }
    }

  }
#endif

#ifdef __sparc__
#endif


/* Intel code  */

#ifdef __i386__
  *model_id = *vendor_id = 0;
  bogo = mhz = cache = family = model= 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "model name : %79[^\n]", model_id) == 1) continue;
    if(sscanf(sl->str, "vendor_id : %79[^\n]", vendor_id) == 1) continue;
    if(sscanf(sl->str, "bogomips : %u", &bogo) == 1) continue;
    if(sscanf(sl->str, "cpu MHz : %u", &mhz) == 1) continue;
    if(sscanf(sl->str, "cache size : %u KB", &cache) == 1) continue;

    if(sscanf(sl->str, "cpu family : %u", &family) == 1) continue;
    if(sscanf(sl->str, "model : %u", &model) == 1) continue;
    if(sscanf(sl->str, "stepping : %u", &stepping) == 1) continue;

    if(strstr(sl->str, "processor") == sl->str || !sl->next) {		/* EOF */
      if(*model_id || *vendor_id) {	/* at least one of those */
        ct = new_mem(sizeof *ct);
	ct->architecture = arch_intel;
        if(model_id) ct->model_name = new_str(model_id);
        if(vendor_id) ct->vend_name = new_str(vendor_id);
        ct->family = family;
        ct->model = model;
        ct->stepping = stepping;
        ct->cache = cache;
	ct->boot = boot_lilo;

        /* round clock to typical values */
        if(mhz >= 38 && mhz <= 42)
          mhz = 40;
        else if(mhz >= 88 && mhz <= 92)
          mhz = 90;
        else {
	  unsigned u, v;

          u = (mhz + 2) % 100;
          v = (mhz + 2) / 100;
          if(u <= 4)
            u = 2;
          else if(u >= 25 && u <= 29)
            u = 25 + 2;
          else if(u >= 33 && u <= 37)
            u = 33 + 2;
          else if(u >= 50 && u <= 54)
            u = 50 + 2;
          else if(u >= 66 && u <= 70)
            u = 66 + 2;
          else if(u >= 75 && u <= 79)
            u = 75 + 2;
          else if(u >= 80 && u <= 84)	/* there are 180MHz PPros */
            u = 80 + 2;
          u -= 2;
          mhz = v * 100 + u;
        }

        ct->clock = mhz;

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_internal;
        hd->sub_class = sc_int_cpu;
        hd->slot = cpus;
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_cpu;
        hd->detail->cpu.data = ct;
        
        *model_id = *vendor_id = 0;
        bogo = mhz = cache = family = model= 0;
        cpus++;
      }
    }
  }
#endif /* __i386__  */

#ifdef __PPC__
  *model_id = *vendor_id = 0;
  bogo = mhz = cache = family = model= 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "cpu : %79[^\n]", model_id) == 1) continue;
//    if(sscanf(sl->str, "vendor_id : %79[^\n]", vendor_id) == 1) continue;
    if(sscanf(sl->str, "bogomips : %u", &bogo) == 1) continue;
    if(sscanf(sl->str, "clock : %u", &mhz) == 1) continue;
    if(sscanf(sl->str, "L2 cache : %u KB", &cache) == 1) continue;

//    if(sscanf(sl->str, "cpu family : %u", &family) == 1) continue;
//    if(sscanf(sl->str, "model : %u", &model) == 1) continue;
//    if(sscanf(sl->str, "stepping : %u", &stepping) == 1) continue;

    if(strstr(sl->str, "processor") == sl->str || !sl->next) {		/* EOF */
      if(*model_id || *vendor_id) {	/* at least one of those */
        ct = new_mem(sizeof *ct);
	ct->architecture = arch_ppc;
        if(model_id) ct->model_name = new_str(model_id);
        if(vendor_id) ct->vend_name = new_str(vendor_id);
        ct->family = family;
        ct->model = model;
        ct->stepping = stepping;
        ct->cache = cache;
	ct->boot = boot_unknown;
        ct->clock = mhz;

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_internal;
        hd->sub_class = sc_int_cpu;
        hd->slot = cpus;
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_cpu;
        hd->detail->cpu.data = ct;
        
        *model_id = *vendor_id = 0;
        bogo = mhz = cache = family = model= 0;
        cpus++;
      }
    }
  }
#endif /* __PPC__  */

}


/*
 * Add some cpu data to the global log.
 */
void dump_cpu_data(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("----- /proc/cpuinfo -----\n");
  for(sl = hd_data->cpu; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/cpuinfo end -----\n");
}


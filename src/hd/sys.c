#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hd.h"
#include "hd_int.h"
#include "sys.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * general system info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void hd_scan_sys(hd_data_t *hd_data)
{
  hd_t *hd;
  sys_info_t *st;
  char buf0[80];
  str_list_t *sl;

  if(!hd_probe_feature(hd_data, pr_sys)) return;

  hd_data->module = mod_sys;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "cpu");

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class = bc_internal;
  hd->sub_class = sc_int_sys;
  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_sys;
  hd->detail->sys.data = st = new_mem(sizeof *st);

  if(!hd_data->cpu) {
    hd_data->cpu = read_file(PROC_CPUINFO, 0, 0);
  }

#ifdef __PPC__
  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "motherboard : %79[^\n]", buf0) == 1) {
      if(strstr(buf0, "MacRISC")) {
        st->system_type = new_str("MacRISC");
      }
    }
    if(sscanf(sl->str, "machine : %79[^\n]", buf0) == 1) {
      if(strstr(buf0, "PReP")) {
        st->system_type = new_str("PReP");
      }
      else if(strstr(buf0, "CHRP")) {
        st->system_type = new_str("CHRP");
      }
    }
  }
#endif	/* __PPC__ */

#ifdef __sparc__
  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "type : %79[^\n]", buf0) == 1) {
      st->system_type = new_str(buf0);
    }
  }
#endif

}

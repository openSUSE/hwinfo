#define _GNU_SOURCE		/* we want memmem() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hd.h"
#include "hd_int.h"
#include "sys.h"
#include "bios.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * general system info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifdef __i386__
int is_txt(char c);
int chk_vaio(hd_data_t *hd_data, sys_info_t *st);
#endif

void hd_scan_sys(hd_data_t *hd_data)
{
  hd_t *hd;
  sys_info_t *st;
#if defined(__PPC__) || defined(__sparc__)
  char buf0[80];
  str_list_t *sl;
#endif

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

#ifdef __i386__
  chk_vaio(hd_data, st);
#endif

}

#ifdef __i386__
int is_txt(char c)
{
  if(c < ' ' || c == 0x7f) return 0;

  return 1;
}

int is_decimal(char c)
{
  if(c < '0' || c > '9') return 0;

  return 1;
}

int txt_len(char *s)
{
  int i;

  for(i = 0; i < 0x100; i++) {
    if(!is_txt(s[i])) break;
  }

  return i;
}

int decimal_len(char *s)
{
  int i;

  for(i = 0; i < 0x100; i++) {
    if(!is_decimal(s[i])) break;
  }

  return i;
}

int chk_vaio(hd_data_t *hd_data, sys_info_t *st)
{
  int i;
  unsigned char *data, *s, *s0, *s1;

  if(!hd_data->bios_rom) return 0;

  data = hd_data->bios_rom + 0xf0000 - BIOS_ROM_START;

  if(!(s = memmem(data, 0x2000, "Sony Corp", sizeof "Sony Corp" - 1))) return 0;

  if((i = txt_len(s))) st->vendor = canon_str(s, i);
  s += i;

  if(!(s = memmem(s, 0x1000, "PCG-", sizeof "PCG-" - 1))) return 0;

  if((i = txt_len(s))) {
    st->system_type = canon_str(s, i);
  }
  s += i;

  for(i = 0; i < 0x1000; i++) {
    if(is_decimal(s[i]) && txt_len(s + i) >= 10 && decimal_len(s + i) >= 5) {
      st->serial = canon_str(s + i, txt_len(s + i));
      break;
    }
  }

  if(st->system_type) {
    s0 = strrchr(st->system_type, '(');
    s1 = strrchr(st->system_type, ')');

    if(s0 && s1 && s1 - s0 >= 3 && s1[1] == 0) {
      st->lang = canon_str(s0 + 1, s1 - s0 - 1);
      for(s = st->lang; *s; s++) {
        if(*s >= 'A' && *s <= 'Z') *s += 'a' - 'A';
      }
      *s0 = 0;	/* cut the system_type entry */
    }
  }

  return st->system_type ? 1 : 0;
}

#endif	/* __i386__ */


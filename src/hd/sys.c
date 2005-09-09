#define _GNU_SOURCE		/* we want memmem() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "hd.h"
#include "hd_int.h"
#include "sys.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * general system info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__i386__)
static void sigsegv_handler(int signum);
static void chk_vmware(hd_data_t *hd_data, sys_info_t *st);
#endif

#if defined(__i386__) || defined(__x86_64__)
static int is_txt(char c);
static int is_decimal(char c);
static int txt_len(char *s);
static int decimal_len(char *s);
static int chk_vaio(hd_data_t *hd_data, sys_info_t *st);
#ifdef UCLIBC
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
#endif
#endif

void hd_scan_sys(hd_data_t *hd_data)
{
  hd_t *hd;
  sys_info_t *st;
#if defined(__PPC__) || defined(__sparc__)
  char buf0[80], *s, *t;
  str_list_t *sl;
#endif

  if(!hd_probe_feature(hd_data, pr_sys)) return;

  hd_data->module = mod_sys;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "cpu");

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class.id = bc_internal;
  hd->sub_class.id = sc_int_sys;
  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_sys;
  hd->detail->sys.data = st = new_mem(sizeof *st);

  if(!hd_data->cpu) {
    hd_data->cpu = read_file(PROC_CPUINFO, 0, 0);
  }

#ifdef __PPC__
  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "motherboard : %79[^\n]", buf0) == 1) {
      if((s = strstr(buf0, "MacRISC"))) {
        for(t = s + sizeof "MacRISC" - 1; isalnum(*t); t++);
        *t = 0;
        st->system_type = new_str(s);
        hd_data->flags.no_parport = 1;
      }
    }
    if(sscanf(sl->str, "machine : %79[^\n]", buf0) == 1) {
      if(strstr(buf0, "PReP")) {
        st->system_type = new_str("PReP");
      }
      else if(strstr(buf0, "CHRP")) {
        st->system_type = new_str(/* is_64 ? "CHRP64" : */ "CHRP");
      }
      else if(strstr(buf0, "iSeries")) {
        st->system_type = new_str("iSeries");
        hd_data->flags.iseries = 1;
      }
      if(strstr(buf0, "PowerBook2,")) {
        st->model = new_str("iBook");
      }
      else if(strstr(buf0, "PowerBook")) {
        st->model = new_str("PowerBook");
      }
    }
    if(sscanf(sl->str, "pmac-generation : %79[^\n]", buf0) == 1) {
      st->generation = new_str(buf0);
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

#if defined(__i386__) || defined(__x86_64__)
  chk_vaio(hd_data, st);
#endif

#if defined(__i386__)
  chk_vmware(hd_data, st);
#endif

  if(st->vendor) hd->vendor.name = new_str(st->vendor);
  if(st->model) hd->device.name = new_str(st->model);
  if(st->serial) hd->serial = new_str(st->serial);
}

#if defined(__i386__)
void sigsegv_handler(int signum) { _exit(77); }

void chk_vmware(hd_data_t *hd_data, sys_info_t *st)
{
  static int is_vmware = -1;
  int child, status;

  /* do the check only once */
  if(is_vmware < 0) {

    child = fork();

    if(child == 0) {
      signal(SIGSEGV, sigsegv_handler);

      asm(
        "push %ebx\n"
        "\tpush %edx\n"
        "\tpush %eax\n"
        "\tpush %ecx\n"
        "\tmov $0x564d5868,%eax\n"
        "\tmov $0xa,%ecx\n"
        "\tmov $0x5658,%edx\n"
        "\tin (%dx),%eax\n"
        "\tpop %ecx\n"
        "\tpop %eax\n"
        "\tpop %edx\n"
        "\tpop %ebx\n"
      );

      _exit(66);
    }
    else {
      if(waitpid(child, &status, 0) == child) {
        status = WEXITSTATUS(status);
        if(status == 66) is_vmware = 1;
        if(status == 77) is_vmware = 0;
      }
    }

    ADD2LOG("  vmware check: %d\n", is_vmware);
  }

  if(is_vmware == 1) {
    st->model = new_str("VMWare");
  }

  hd_data->in_vmware = is_vmware;
}

#endif	/* __i386__ */


#if defined(__i386__) || defined(__x86_64__)
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

  if(!hd_data->bios_rom.data) return 0;

  data = hd_data->bios_rom.data + 0xe8000 - hd_data->bios_rom.start;

  if(!(s = memmem(data, 0x10000, "Sony Corp", sizeof "Sony Corp" - 1))) return 0;

  if((i = txt_len(s))) st->vendor = canon_str(s, i);
  s += i;

  if(!(s = memmem(s, 0x1000, "PCG-", sizeof "PCG-" - 1))) return 0;

  if((i = txt_len(s))) {
    st->model = canon_str(s, i);
  }
  s += i;

  ADD2LOG("  vaio: %s\n", st->model);

  for(i = 0; i < 0x1000; i++) {
    if(is_decimal(s[i]) && txt_len(s + i) >= 10 && decimal_len(s + i) >= 5) {
      st->serial = canon_str(s + i, txt_len(s + i));
      break;
    }
  }

  if(st->model) {
    s0 = strrchr(st->model, '(');
    s1 = strrchr(st->model, ')');

    if(s0 && s1 && s1 - s0 >= 3 && s1[1] == 0) {
      st->lang = canon_str(s0 + 1, s1 - s0 - 1);
      for(s = st->lang; *s; s++) {
        if(*s >= 'A' && *s <= 'Z') *s += 'a' - 'A';
      }
      if(!strcmp(st->lang, "uc")) strcpy(st->lang, "en");
      *s0 = 0;	/* cut the model entry */
    }
  }

  return st->model ? 1 : 0;
}
#endif	/* __i386__ || __x86_64__ */


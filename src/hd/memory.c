#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "memory.h"
#include "klog.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * memory stuff
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

uint64_t kcore_mem(hd_data_t *hd_data);
uint64_t klog_mem(hd_data_t *hd_data, uint64_t *alt);
uint64_t meminfo_mem(hd_data_t *hd_data);

void hd_scan_memory(hd_data_t *hd_data)
{
  hd_t *hd;
  uint64_t kcore, klog, klog_alt, meminfo, msize0, msize1, u;
  hd_res_t *res;
  int i;

  if(!hd_probe_feature(hd_data, pr_memory)) return;

  hd_data->module = mod_memory;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "main memory size");

  kcore = kcore_mem(hd_data);
  klog = klog_mem(hd_data, &klog_alt);
  meminfo = meminfo_mem(hd_data);

  msize0 = klog ? klog : kcore;
  if(!msize0) msize0 = meminfo;

  if(msize0 && kcore > msize0 && ((kcore - msize0) << 2) / msize0 == 0) {
    /* trust kcore value if it's approx. msize0 */
    msize0 = kcore;
  }
  msize1 = msize0;
  if(meminfo > msize1) msize1 = meminfo;
  if(klog_alt > msize0) msize0 = klog_alt;

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class = bc_internal;
  hd->sub_class = sc_int_main_mem;

  res = add_res_entry(&hd->res, new_mem(sizeof *res));
  res->mem.type = res_mem;
  res->mem.range = msize0;
  res->mem.access = acc_rw;
  res->mem.enabled = 1;

  /* round it somewhat */
  for(i = 0, u = msize1; u; i++) {
    u >>= 1;
  }
  if(i > 10) {	/* We *do* have at least 1k memory, do we? */
    msize1 >>= i - 5;
    msize1++;
    msize1 >>= 1;
    msize1 <<= i - 4;
  }

  res = add_res_entry(&hd->res, new_mem(sizeof *res));
  res->phys_mem.type = res_phys_mem;
  res->phys_mem.range = msize1;
}

uint64_t kcore_mem(hd_data_t *hd_data)
{
  uint64_t u = 0;
  size_t ps = getpagesize();
  struct stat sb;

  if(!stat(PROC_KCORE, &sb)) {
    u = sb.st_size;
    if(u > ps) u -= ps;

#if 0
    /* we'll assume no mem modules with less than 256k */
    u += 1 << 17;
    u &= -1 << 18;
#endif
  }

  ADD2LOG("  kcore mem:  0x%"PRIx64"\n", u);

  return u;
}


uint64_t klog_mem(hd_data_t *hd_data, uint64_t *alt)
{
  uint64_t u = 0, u0, u1, u2, u3, mem0 = 0, mem1 = 0;
  str_list_t *sl;
  char *s;
  int i;

  if(!hd_data->klog) read_klog(hd_data);

  for(sl = hd_data->klog; sl; sl = sl->next) {
    if(strstr(sl->str, "<4>Memory: ") == sl->str) {
      if(sscanf(sl->str, "<4>Memory: %"SCNu64"k/%"SCNu64"k", &u0, &u1) == 2) {
        mem0 = u1 << 10;
      }
      if(
        (i = sscanf(sl->str, "<4>Memory: %"SCNu64"k available (%"SCNu64"k kernel code, %"SCNu64"k data, %"SCNu64"k", &u0, &u1, &u2, &u3))  == 4 || i == 1
      ) {
        mem0 = (i == 1 ? u0 : u0 + u1 + u2 + u3) << 10;
      }
      if(
        (s = strstr(sl->str, "[")) &&
        sscanf(s, "[%"SCNx64",%"SCNx64"]", &u0, &u1) == 2 &&
        u1 > u0
      ) {
        mem1 = u1 - u0;
      }
      break;
    }
  }

  u = mem0 ? mem0 : mem1;

#if 0
  /* round it somewhat */
  for(i = 0, u0 = u; u0; i++) {
    u0 >>= 1;
  }
  if(i > 10) {	/* We *do* have at least 1k memory, do we? */
    u >>= i - 6;
    u++;
    u >>= 1;
    u <<= i - 5;
  }
#endif

  ADD2LOG("  klog mem 0: 0x%"PRIx64"\n", mem0);
  ADD2LOG("  klog mem 1: 0x%"PRIx64"\n", mem1);
  ADD2LOG("  klog mem:   0x%"PRIx64"\n", u);

  *alt = mem1;

  return u;
}

uint64_t meminfo_mem(hd_data_t *hd_data)
{
  uint64_t u = 0, u0;
  str_list_t *sl;

  sl = read_file(PROC_MEMINFO, 1, 1);

  if(sl && sscanf(sl->str, "Mem: %"SCNu64"", &u0) == 1) {
    u = u0;
  }

  free_str_list(sl);

  ADD2LOG("  meminfo:    0x%"PRIx64"\n", u);

  return u;
}



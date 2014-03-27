#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>

#include "hd.h"
#include "hd_int.h"
#include "klog.h"

/**
 * @defgroup KLOGint Kernel log information
 * @ingroup libhdINFOint
 * @brief Kernel log information scan functions
 *
 * @{
 */

static int str_ok(str_list_t *sl);
static int str_list_cmp(str_list_t *sl1, str_list_t *sl2);
static void _read_klog(hd_data_t *hd_data);


/*
 * Check if a string starts with '<[0-9]>'.
 */
int str_ok(str_list_t *sl)
{
  return sl->str[0] == '<' && sl->str[2] == '>' && sl->str[1] >= '0' && sl->str[1] <= '9';
}

/*
 * Check if sl1 is idential to sl2; sl1 may be shorter as sl2.
 *
 * Returns 0/1 if they are equal/not equal. If sl1 is NULL, 0 is returned.
 */
int str_list_cmp(str_list_t *sl1, str_list_t *sl2)
{
  for(; sl1; sl1 = sl1->next, sl2 = sl2->next) {
    if(!sl2 || strcmp(sl1->str, sl2->str)) return 1;
  }

  return 0;
}


/*
 * Read kernel log info. Combine with /var/log/boot.msg.
 * Remove time stamps.
 */
void read_klog(hd_data_t *hd_data)
{
  str_list_t *sl, **sl_new;
  char *str, *s;

  _read_klog(hd_data);

  free_str_list(hd_data->klog_raw);
  hd_data->klog_raw = hd_data->klog;
  hd_data->klog = NULL;

  for(sl = hd_data->klog_raw, sl_new = &hd_data->klog; sl; sl = sl->next, sl_new = &(*sl_new)->next) {
    str = add_str_list(sl_new, sl->str)->str;
    if(str[0] == '<' && str[1] && str[2] == '>' && str[3] == '[') {
      s = str + 4;
      while(*s && *s != ']') s++;
      if(*s) s++;
      if(*s) s++;	// skip space
      for(str += 3; (*str++ = *s++););
    }
  }
}


/*
 * Read kernel log info. Combine with /var/log/boot.msg.
 */
void _read_klog(hd_data_t *hd_data)
{
  char buf[0x2000 + 1], *s;
  int i, j, len, n;
  str_list_t *sl, *sl1, *sl2, *sl_last, **ssl, *sl_next;

  /* some clean-up */
  hd_data->klog = free_str_list(hd_data->klog);

  sl1 = read_file(KLOG_BOOT, 0, 0);
  sl2 = NULL;

  /*
   * remove non-canonical lines (not starting with <[0-9]>) at the start and
   * at the end
   */

  /* note: the implementations assumes that at least *one* line is ok */
  for(sl_last = NULL, sl = sl1; sl; sl = (sl_last = sl)->next) {
    if(str_ok(sl)) {
      if(sl_last) {
        sl_last->next = NULL;
        free_str_list(sl1);
        sl1 = sl;
      }
      break;
    }
  }

  for(sl_last = NULL, sl = sl1; sl; sl = (sl_last = sl)->next) {
    if(!str_ok(sl)) {
      if(sl_last) {
        sl_last->next = NULL;
        free_str_list(sl);
      }
      break;
    }
  }

  n = klogctl(3, buf, sizeof buf - 1);
  if(n <= 0) {
    hd_data->klog = sl1;
    return;
  }

  if(n > (int) sizeof buf - 1) n = sizeof buf - 1;
  buf[n] = 0;
  for(i = j = 0; i < n; i++) {
    if(buf[i] == '\n') {
      len = i - j + 1;
      s = new_mem(len + 1);
      memcpy(s, buf + j, len);
      add_str_list(&sl2, s);
      s = free_mem(s);
      j = i + 1;
    }
  }

  /* the 1st line may be incomplete */
  if(sl2 && !str_ok(sl2)) {
    sl_next = sl2->next;
    sl2->next = NULL;
    free_str_list(sl2);
    sl2 = sl_next;
  }

  if(!sl1) {
    hd_data->klog = sl2;
    return;
  }

  if(sl1 && !sl2) {
    hd_data->klog = sl1;
    return;
  }

  /* now, try to join sl1 & sl2 */
  for(sl_last = NULL, sl = sl1; sl; sl = (sl_last = sl)->next) {
    if(!str_list_cmp(sl, sl2)) {
      free_str_list(sl);
      if(sl_last)
        sl_last->next = NULL;
      else
        sl1 = NULL;
      break;
    }
  }

  /* append sl2 to sl1 */
  for(ssl = &sl1; *ssl; ssl = &(*ssl)->next);
  *ssl = sl2;

  hd_data->klog = sl1;
}


/*
 * Add some klog data to the global log.
 */
void dump_klog(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("----- kernel log -----\n");
  for(sl = hd_data->klog_raw; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- kernel log end -----\n");
}

/** @} */


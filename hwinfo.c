#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"

static int get_probe_flags(int, char **, hd_data_t *);
static int get_probe_env(hd_data_t *);
static void progress(char *, char *);

static unsigned deb = 0;
static char *log_file = "";
static char *list = NULL;

static int test = 0;

/*
 * Just scan the hardware and dump all info.
 */
int main(int argc, char **argv)
{
  hd_data_t *hd_data;
  hd_t *hd;
  FILE *f = NULL;
  long l = 0;
  int i;
  unsigned first_probe = 1;

  argc--; argv++;

  hd_data = calloc(1, sizeof *hd_data);
  hd_data->progress = progress;
  hd_data->debug=~(HD_DEB_DRIVER_INFO);

  do {
    if(first_probe)				/* only for the 1st probing */
      hd_set_probe_feature(hd_data, pr_default);
    else {
      hd_clear_probe_feature(hd_data, pr_all);
#if 0
      printf("press return to start re-probe...");
      fflush(stdout);
      getchar();
#endif
    }

    if((i = get_probe_flags(argc, argv, hd_data)) < 0) return 1;
    deb = hd_data->debug;
    argc -= i; argv += i;

    if(get_probe_env(hd_data)) return 2;

    hd_scan(hd_data);
    printf("\r%64s\r", "");

    first_probe = 0;
  } while(argc);

  if(*log_file) f = fopen(log_file, "w+");

  if((hd_data->debug & HD_DEB_SHOW_LOG) && hd_data->log) {
    fprintf(f ? f : stdout,
      "============ start debug info ============\n%s=========== end debug info ============\n",
      hd_data->log
    );
  }

  if(f) l = ftell(f);

  for(hd = hd_data->hd; hd; hd = hd->next) {
    hd_dump_entry(hd_data, hd, f ? f : stdout);
  }

#if 0
  printf("-- list --\n");

  hd = hd_base_class_list(hd_data, bc_display); printf("\n");
  for(; hd; hd = hd->next) hd_dump_entry(hd_data, hd, stdout);
#endif

#if 0	// ##### remove
  printf("-- all --\n");

  hd = hd_net_list(hd_data, 2); printf("\n");
  for(; hd; hd = hd->next) hd_dump_entry(hd_data, hd, stdout);

  printf("-- new --\n");
  getchar();

  hd = hd_net_list(hd_data, 3); printf("\n");
  for(; hd; hd = hd->next) hd_dump_entry(hd_data, hd, stdout);

  printf("-- new again --\n");

  hd = hd_net_list(hd_data, 2); printf("\n");
  for(; hd; hd = hd->next) hd_dump_entry(hd_data, hd, stdout);

  printf("-- nothing --\n");

  hd = hd_net_list(hd_data, 2); printf("\n");
  for(; hd; hd = hd->next) hd_dump_entry(hd_data, hd, stdout);
#endif

  if(list) {
    if(!strcmp(list, "disk")) hd = hd_disk_list(hd_data, 1);
    if(!strcmp(list, "cd")) hd = hd_cd_list(hd_data, 1);
    if(!strcmp(list, "net")) hd = hd_net_list(hd_data, 1);
    if(!strcmp(list, "floppy")) hd = hd_floppy_list(hd_data, 1);
    if(!strcmp(list, "mouse")) hd = hd_mouse_list(hd_data, 1);
    if(!strcmp(list, "keyboard")) hd = hd_keyboard_list(hd_data, 1);

    printf("\n");
    printf("-- %s list --\n", list);
    for(; hd; hd = hd->next) hd_dump_entry(hd_data, hd, stdout);
    printf("-- %s list end --\n", list);
  }

  if(f) {
#if 0
    fseek(f, l, SEEK_SET);
    while((i = fgetc(f)) != EOF) putchar(i);
#endif
    fclose(f);
  }

  hd_free_hd_data(hd_data);
  free(hd_data);

  return 0;
}

/*
 * Parse command line options.
 */
int get_probe_flags(int argc, char **argv, hd_data_t *hd_data)
{
  int i, j, k;
  char *s, *t;
  for(i = 0; i < argc; i++) {
    s = argv[i];

    if(!strcmp(s, ".")) {
      return i + 1;
    }

    t = "debug=";
    if(!strncmp(s, t, strlen(t))) {
      hd_data->debug = strtol(s + strlen(t), NULL, 0);
      continue;
    }

    t = "list=";
    if(!strncmp(s, t, strlen(t))) {
      list = s + strlen(t);
      continue;
    }

    t = "log=";
    if(!strncmp(s, t, strlen(t))) {
      log_file = s + strlen(t);
      continue;
    }

    k = 1;
    if(*s == '+')
      s++;
    else if(*s == '-')
      k = 0, s++;

    if((j = hd_probe_feature_by_name(s))) {
      if(k)
        hd_set_probe_feature(hd_data, j);
      else
        hd_clear_probe_feature(hd_data, j);
      continue;
    }

    fprintf(stderr, "oops: don't know what to do with \"%s\"\n", s);
    return -1;
  }

  return argc;
}

int get_probe_env(hd_data_t *hd_data)
{
  char *s, *t, *env = getenv("probe");
  int j, k;

  if(env) s = env = strdup(env);
  if(!env) return 0;

  while((t = strsep(&s, ","))) {
    if(*t == '+') {
      k = 1;
    }
    else if(*t == '-') {
      k = 0;
    }
    else {
      fprintf(stderr, "oops: don't know what to do with \"%s\"\n", t);
      return -1;
    }

    t++;

    if((j = hd_probe_feature_by_name(t))) {
      if(k)
        hd_set_probe_feature(hd_data, j);
      else
        hd_clear_probe_feature(hd_data, j);
    }
    else if(!strcmp(t, "test")) {
      test = k;
    }
    else {
      fprintf(stderr, "oops: don't know what to do with \"%s\"\n", t);
      return -2;
    }
  }

  free(env);

  return 0;
}


/*
 * A simple progress function.
 */
void progress(char *pos, char *msg)
{
  if(!test) printf("\r%64s\r", "");
  printf("> %s: %s", pos, msg);
  if(test) printf("\n");
  fflush(stdout);
}


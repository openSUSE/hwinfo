#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hd.h"

static int get_probe_flags(int, char **, hd_data_t *);
static void progress(char *, char *);

static unsigned deb = 0;
static char *log_file = "";

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

  do {
    if(first_probe)				/* only for the 1st probing */
      hd_set_probe_feature(hd_data, pr_all);
    else
      hd_clear_probe_feature(hd_data, pr_all);

    first_probe = 0;

    if((i = get_probe_flags(argc, argv, hd_data)) < 0) return 1;
    deb = hd_data->debug;
    argc -= i; argv += i;

    hd_scan(hd_data);
    printf("\r%64s\r", "");

  } while(argc);

  if(*log_file) f = fopen(log_file, "w+");

  if((hd_data->debug & HD_DEB_SHOW_LOG) && hd_data->log) {
    fprintf(f ? f : stdout,
      "============ start debug info ============\n%s=========== end debug info ============\n",
      hd_data->log
    );
  }

  if(f) l = ftell(f);

  if((hd = hd_data->hd))
    while(hd_dump_entry(hd_data, hd, f ? f : stdout), (hd = hd->next));

  if(f) {
    fseek(f, l, SEEK_SET);
    while((i = fgetc(f)) != EOF) putchar(i);
    fclose(f);
  }

  hd_free_hd_data(hd_data);

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

    if(!strcmp(s, ",")) {
      return i + 1;
    }

    t = "debug=";
    if(!strncmp(s, t, strlen(t))) {
      hd_data->debug = strtol(s + strlen(t), NULL, 0);
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


/*
 * A simple progress function.
 */
void progress(char *pos, char *msg)
{
  printf("\r%64s\r", "");
  printf("> %s: %s ", pos, msg);
  fflush(stdout);
}


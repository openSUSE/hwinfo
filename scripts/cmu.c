#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>

int do_cat(int, char **);
int do_mount(int, char **);
int do_sleep(int, char **);
int do_umount(int, char **);

int main(int argc, char **argv)
{
  char *name = strrchr(*argv, '/');

  if(name) name++; else name = *argv;

  argv++; argc--;

  if(!strcmp(name, "cat")) return do_cat(argc, argv);

  if(!strcmp(name, "mount")) return do_mount(argc, argv);

  if(!strcmp(name, "sleep")) return do_sleep(argc, argv);

  if(!strcmp(name, "umount")) return do_umount(argc, argv);

  return 77;
}

int do_cat(int argc, char **argv)
{
  FILE *f;
  int i, c;

  if(!argc) {
    while((c = fgetc(stdin)) != EOF) fputc(c, stdout);
    return 0;
  }

  for(i = 0; i < argc; i++) {
    if((f = fopen(argv[i], "r"))) {
      while((c = fgetc(f)) != EOF) fputc(c, stdout);
      fclose(f);
    }
    else {
      perror(argv[i]);
      return 1;
    }
  }


  return 0;
}

int do_mount(int argc, char **argv)
{
  char *type = NULL, *dir, *dev;

  if(argc != 3) return 1;

  if(strstr(*argv, "-t") == *argv) {
    type = *argv + 2;
  }

  dev = strcmp(argv[1], "none") ? argv[1] : NULL;

  dir = argv[2];

  if(!type) return 2;

  if(!mount(dev, dir, type, 0, 0)) return 0;

  perror("mount");

  return 3;
}

int do_umount(int argc, char **argv)
{
  if(argc != 1) return 1;

  if(!umount(*argv)) return 0;

  perror(*argv);

  return 1;
}

int do_sleep(int argc, char **argv)
{
  int i = 0;

  if(argc != 1) return 1;

  i = atoi(*argv);

  if(i) sleep(i);

  return 0;
}


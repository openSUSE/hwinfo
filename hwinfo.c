#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"

static int get_probe_flags(int, char **, hd_data_t *);
static void progress(char *, char *);

// ##### temporary solution, fix it later!
str_list_t *read_file(char *file_name, unsigned start_line, unsigned lines);
str_list_t *search_str_list(str_list_t *sl, char *str);
str_list_t *add_str_list(str_list_t **sl, char *str);
char *new_str(const char *);

static unsigned deb = 0;
static char *log_file = "";
static char *list = NULL;
static int listplus = 0;

static int test = 0;
static int is_short = 0;

static char *showconfig = NULL;
static char *saveconfig = NULL;
static int hw_item[100] = { };
static int hw_items = 0;

int braille_install_info(hd_data_t *hd_data);
int x11_install_info(hd_data_t *hd_data);
int oem_install_info(hd_data_t *hd_data);
int dump_packages(hd_data_t *hd_data);

void do_hw(hd_data_t *hd_data, FILE *f, hd_hw_item_t hw_item);
void do_short(hd_data_t *hd_data, hd_t *hd, FILE *f);
void do_test(hd_data_t *hd_data);
void help(void);

struct option options[] = {
  { "special", 1, NULL, 1 },
  { "help", 1, NULL, 'h' },
  { "debug", 1, NULL, 'd' },
  { "log", 1, NULL, 'l' },
  { "packages", 0, NULL, 'p' },
  { "test", 0, NULL, 300 },
  { "format", 1, NULL, 301 },
  { "show-config", 1, NULL, 302 },
  { "save-config", 1, NULL, 303 },
  { "short", 0, NULL, 304 },
  { "fast", 0, NULL, 305 },
  { "cdrom", 0, NULL, 1000 + hw_cdrom },
  { "floppy", 0, NULL, 1000 + hw_floppy },
  { "disk", 0, NULL, 1000 + hw_disk },
  { "network", 0, NULL, 1000 + hw_network },
  { "display", 0, NULL, 1000 + hw_display },
  { "gfxcard", 0, NULL, 1000 + hw_display },
  { "framebuffer", 0, NULL, 1000 + hw_framebuffer },
  { "monitor", 0, NULL, 1000 + hw_monitor },
  { "camera", 0, NULL, 1000 + hw_camera },
  { "mouse", 0, NULL, 1000 + hw_mouse },
  { "joystick", 0, NULL, 1000 + hw_joystick },
  { "keyboard", 0, NULL, 1000 + hw_keyboard },
  { "chipcard", 0, NULL, 1000 + hw_chipcard },
  { "sound", 0, NULL, 1000 + hw_sound },
  { "isdn", 0, NULL, 1000 + hw_isdn },
  { "modem", 0, NULL, 1000 + hw_modem },
  { "storage_ctrl", 0, NULL, 1000 + hw_storage_ctrl },
  { "netcard", 0, NULL, 1000 + hw_network_ctrl },
  { "netcards", 0, NULL, 1000 + hw_network_ctrl },	// outdated, just kept for comaptibility
  { "network_ctrl", 0, NULL, 1000 + hw_network_ctrl },
  { "printer", 0, NULL, 1000 + hw_printer },
  { "tv", 0, NULL, 1000 + hw_tv },
  { "scanner", 0, NULL, 1000 + hw_scanner },
  { "braille", 0, NULL, 1000 + hw_braille },
  { "sys", 0, NULL, 1000 + hw_sys },
  { "bios", 0, NULL, 1000 + hw_bios },
  { "cpu", 0, NULL, 1000 + hw_cpu },
  { "partition", 0, NULL, 1000 + hw_partition },
  { "usb_ctrl", 0, NULL, 1000 + hw_usb_ctrl },
  { "usb", 0, NULL, 1000 + hw_usb },
  { "pci", 0, NULL, 1000 + hw_pci },
  { "isapnp", 0, NULL, 1000 + hw_isapnp },
  { "bridge", 0, NULL, 1000 + hw_bridge },
  { "hub", 0, NULL, 1000 + hw_hub },
  { "manual", 0, NULL, 1000 + hw_manual },
  { "all", 0, NULL, 2000 },
  { "reallyall", 0, NULL, 2001 },
  { "smp", 0, NULL, 3000 },
//  { "pci", 0, NULL, 3002 },
//  { "isapnp", 0, NULL, 3003 },
  { "ide", 0, NULL, 3004 },
  { "scsi", 0, NULL, 3005 },
  { }
};


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

  hd_data = calloc(1, sizeof *hd_data);
  hd_data->progress = isatty(1) ? progress : NULL;
  hd_data->debug=~(HD_DEB_DRIVER_INFO | HD_DEB_HDDB);

  for(i = 0; i < argc; i++) {
    if(strstr(argv[i], "--") == argv[i]) break;
  }

  if(i != argc) {
    /* new style interface */

    opterr = 0;

    while((i = getopt_long(argc, argv, "hd:l:p", options, NULL)) != -1) {
      switch(i) {
        case 1:
          if(!strcmp(optarg, "braille")) {
            braille_install_info(hd_data);
          }
          else if(!strcmp(optarg, "x11")) {
            x11_install_info(hd_data);
          }
          else if(!strcmp(optarg, "oem")) {
            oem_install_info(hd_data);
          }
          else {
            help();
            return 1;
          }
          break;

        case 'd':
          hd_data->debug = strtol(optarg, NULL, 0);
          break;

        case 'l':
          log_file = optarg;
          break;

        case 'p':
          dump_packages(hd_data);
	  break;

        case 300:
          do_test(hd_data);
          break;

        case 301:
          hd_data->flags.dformat = strtol(optarg, NULL, 0);
          break;

        case 302:
          showconfig = optarg;
          break;

        case 303:
          saveconfig = optarg;
          break;

        case 304:
          is_short = 1;
          break;

        case 305:
          hd_data->flags.fast = 1;
          break;

        case 1000 ... 1100:
          if(hw_items < sizeof hw_item / sizeof *hw_item)
            hw_item[hw_items++] = i - 1000;
          break;

        case 2000:
        case 2001:
        case 3000:
        case 3002:
        case 3003:
        case 3004:
        case 3005:
          if(hw_items < sizeof hw_item / sizeof *hw_item)
            hw_item[hw_items++] = i;
          break;

        default:
          help();
          return 0;
      }
    }

    if(!hw_items && is_short) hw_item[hw_items++] = 2000;	/* all */

    if(hw_items >= 0 || showconfig || saveconfig) {
      if(*log_file) f = fopen(log_file, "w+");

      for(i = 0; i < hw_items; i++) {
        if(i) fputc('\n', f ? f : stdout);
        do_hw(hd_data, f, hw_item[i]);
      }

#ifndef LIBHD_TINY
      if(showconfig) {
        hd = hd_read_config(hd_data, showconfig);
        if(hd) {
          hd_dump_entry(hd_data, hd, f ? f : stdout);
          hd = hd_free_hd_list(hd);
        }
        else {
          fprintf(f ? f : stdout, "No config data: %s\n", showconfig);
        }
      }

      if(saveconfig) {
        for(hd = hd_data->hd; hd; hd = hd->next) {
          if(!strcmp(hd->unique_id, saveconfig)) {
            i = hd_write_config(hd_data, hd);
            fprintf(f ? f : stdout, "%s: %s\n",
              saveconfig,
              i ? "Error writing config data" : "config saved"
            );
            break;
          }
        }
        if(!hd) {
          fprintf(f ? f : stdout, "No such hardware: %s\n", saveconfig);
        }
      }
#endif

      if(f) fclose(f);
    }

    hd_free_hd_data(hd_data);
    free(hd_data);

    return 0;
  }

  /* old style interface */

  argc--; argv++;

  if(argc == 1 && !strcmp(*argv, "-h")) {
    help();
    return 0;
  }

  do {
    if(first_probe)				/* only for the 1st probing */
      hd_set_probe_feature(hd_data, pr_default);
    else {
      hd_clear_probe_feature(hd_data, pr_all);
    }

    if((i = get_probe_flags(argc, argv, hd_data)) < 0) return 1;
    deb = hd_data->debug;
    argc -= i; argv += i;

    hd_scan(hd_data);
    if(hd_data->progress) printf("\r%64s\r", "");

    first_probe = 0;
  } while(argc);

  if(*log_file) f = fopen(log_file, "w+");

  if((hd_data->debug & HD_DEB_SHOW_LOG) && hd_data->log) {
    if(*log_file) {
      fprintf(f ? f : stdout,
        "============ start hardware log ============\n"
      );
    }
    fprintf(f ? f : stdout,
      "============ start debug info ============\n%s=========== end debug info ============\n",
      hd_data->log
    );
  }

  if(f) l = ftell(f);

  for(hd = hd_data->hd; hd; hd = hd->next) {
    hd_dump_entry(hd_data, hd, f ? f : stdout);
  }

  if(*log_file) {
    fprintf(f ? f : stdout,
      "============ end hardware log ============\n"
    );
  }

  i = -1;
  if(list) {
    if(!strcmp(list, "cdrom")) i = hw_cdrom;
    if(!strcmp(list, "disk")) i = hw_disk;
    if(!strcmp(list, "floppy")) i = hw_floppy;
    if(!strcmp(list, "network")) i = hw_network;
    if(!strcmp(list, "display")) i = hw_display;
    if(!strcmp(list, "monitor")) i = hw_monitor;
    if(!strcmp(list, "mouse")) i = hw_mouse;
    if(!strcmp(list, "keyboard")) i = hw_keyboard;
    if(!strcmp(list, "sound")) i = hw_sound;
    if(!strcmp(list, "isdn")) i = hw_isdn;
    if(!strcmp(list, "modem")) i = hw_modem;
    if(!strcmp(list, "storage_ctrl")) i = hw_storage_ctrl;
    if(!strcmp(list, "network_ctrl")) i = hw_network_ctrl;
    if(!strcmp(list, "netcards")) i = hw_network_ctrl;
    if(!strcmp(list, "printer")) i = hw_printer;
    if(!strcmp(list, "tv")) i = hw_tv;
    if(!strcmp(list, "scanner")) i = hw_scanner;
    if(!strcmp(list, "braille")) i = hw_braille;
    if(!strcmp(list, "sys")) i = hw_sys;
    if(!strcmp(list, "cpu")) i = hw_cpu;

    if(i >= 0) {
      hd = hd_list(hd_data, i, listplus, NULL);
      printf("\n");
      printf("-- %s list --\n", list);
      for(; hd; hd = hd->next) hd_dump_entry(hd_data, hd, stdout);
      printf("-- %s list end --\n", list);
      hd = hd_free_hd_list(hd);
    }
  }

  if(f) fclose(f);

  hd_free_hd_data(hd_data);
  free(hd_data);

  return 0;
}

void do_hw(hd_data_t *hd_data, FILE *f, hd_hw_item_t hw_item)
{
  hd_t *hd, *hd0;
  int smp = -1;
  int i, j;

  hd0 = NULL;

  switch(hw_item) {
    case 3000:
      smp = hd_smp_support(hd_data);
      break;

    case 2000:
    case 2001:
    case 3002:
    case 3003:
    case 3004:
    case 3005:
      i = j = -1;
      switch((int) hw_item) {
        case 2000: i = pr_default; break;
        case 2001: i = pr_all; break;
        case 3002: i = pr_pci; j = bus_pci; break;
        case 3003: i = pr_isapnp; j = bus_isa; break;
        case 3004: i = pr_ide; j = bus_ide; break;
        case 3005: i = pr_scsi; j = bus_scsi; break;
      }
      if(i != -1) {
        hd_clear_probe_feature(hd_data, pr_all);
        hd_set_probe_feature(hd_data, i);
        if(i == pr_isapnp) {
          hd_set_probe_feature(hd_data, pr_isapnp_mod);
          hd_set_probe_feature(hd_data, pr_misc);
        }
        hd_scan(hd_data);
        if(j != -1) {
          hd0 = hd_bus_list(hd_data, j);
        } else {
          hd0 = hd_data->hd;
        }
      }
      break;

    default:
      hd0 = hd_list(hd_data, hw_item, 1, NULL);
  }

  if(hd_data->progress) {
    printf("\r%64s\r", "");
    fflush(stdout);
  }

  if(f) {
    if((hd_data->debug & HD_DEB_SHOW_LOG) && hd_data->log) {
      fprintf(f,
        "============ start hardware log ============\n"
      );
      fprintf(f,
        "============ start debug info ============\n%s=========== end debug info ============\n",
        hd_data->log
      );
    }

    for(hd = hd_data->hd; hd; hd = hd->next) {
      hd_dump_entry(hd_data, hd, f);
    }

    fprintf(f,
      "============ end hardware log ============\n"
    );
  }

  if(hw_item == 3000) {
    fprintf(f ? f : stdout,
      "SMP support: %s",
      smp < 0 ? "unknown" : smp > 0 ? "yes" : "no"
    );
    if(smp > 0) fprintf(f ? f : stdout, " (%u cpus)", smp);
    fprintf(f ? f : stdout, "\n");
  }
  else {
    if(is_short) {
      /* always to stdout */
      do_short(hd_data, hd0, stdout);
      if(f) do_short(hd_data, hd0, f);
    }
    else {
      for(hd = hd0; hd; hd = hd->next) {
        if(hw_item != 3003 || hd->is.isapnp) {
          hd_dump_entry(hd_data, hd, f ? f : stdout);
        }
      }
    }
  }

  if(hw_item == hw_display && hd0) {
    fprintf(f ? f : stdout, "\nPrimary display adapter: #%u\n", hd_display_adapter(hd_data));
  }

  if(hd0 != hd_data->hd) hd_free_hd_list(hd0);
}


void do_short(hd_data_t *hd_data, hd_t *hd, FILE *f)
{
#ifndef LIBHD_TINY
  hd_hw_item_t item;
  hd_t *hd1;
  int i;
  char *s;

  for(item = 1; item < hw_all; item++) {
    i = 0;
    s = hd_hw_item_name(item);
    if(!s) continue;

    if(item == hw_sys) continue;

    for(hd1 = hd; hd1; hd1 = hd1->next) {
      if(hd1->hw_class == item) {
        if(!i++) fprintf(f, "%s:\n", s);
        fprintf(f, "  %-20s %s\n",
          hd1->unix_dev_name ? hd1->unix_dev_name : "",
          hd1->model ? hd1->model : "???"
        );
      }
    }
  }
#endif
}


#if 0
void dump_hddb_data(hd_data_t *hd_data, hddb_data_t *x, char *name);
void *free_mem(void *);
#endif

void do_test(hd_data_t *hd_data)
{
#if 0
  hd_t *hd;
  hd_t *hd0 = NULL;

  for(hd = hd_list(hd_data, hw_cdrom, 1, hd0); hd; hd = hd->next) {
    fprintf(stderr, "cdrom: %s, %s\n", hd->unix_dev_name, hd->model);
  }

  for(hd = hd_list(hd_data, hw_cdrom, 1, hd0); hd; hd = hd->next) {
    fprintf(stderr, "cdrom: %s, %s\n", hd->unix_dev_name, hd->model);
  }
#endif

#if 1
  hd_t *hd;

  hd = hd_list(hd_data, hw_disk, 1, NULL);
  hd_free_hd_list(hd);
  hd_free_hd_data(hd_data);

  hd = hd_list(hd_data, hw_cdrom, 1, NULL);
  hd_free_hd_list(hd);
  hd_free_hd_data(hd_data);

  hd = hd_list(hd_data, hw_storage_ctrl, 1, NULL);
  hd_free_hd_list(hd);
  hd_free_hd_data(hd_data);

  hd = hd_list(hd_data, hw_display, 1, NULL);
  hd_free_hd_list(hd);
  hd_free_hd_data(hd_data);

#if 0
  for(hd = hd_data->hd; hd; hd = hd->next) {
    hd_dump_entry(hd_data, hd, stdout);
  }

  printf("%s\n", hd_data->log);
#endif

#endif

#if 0
  hd_t *hd, *hd0;

  hd0 = hd_list(hd_data, hw_sound, 1, NULL);
  hd0 = hd_list(hd_data, hw_sound, 1, NULL);

  for(hd = hd0; hd; hd = hd->next) {
    hd_dump_entry(hd_data, hd, stdout);
  }

#if 0
  hd_data->log = free_mem(hd_data->log);
  dump_hddb_data(hd_data, hd_data->hddb_dev, "hddb_dev, final");  
  if(hd_data->log) printf("%s", hd_data->log);
#endif

#endif
}


void help()
{
  fprintf(stderr,
    "usage: hwinfo [--short] [--log log_file] [--debug debug_level] [--<hardware_item1> --<hardware_item2> ...]\n"
    "  <hardware_item> is one of:\n"
    "    cdrom, floppy, disk, network, display, monitor, mouse, keyboard,\n"
    "    sound, isdn, modem, storage_ctrl, netcards, printer, tv,\n"
    "    scanner, braille, sys, cpu, smp, usb, pci, isapnp, ide, scsi,\n"
    "    all, reallyall\n\n"
    "  Note: debug info is shown only in the log file. (If you specify a\n"
    "  log file the debug level is implicitly set to a reasonable value.)\n"
  );
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

    t = "list+=";
    if(!strncmp(s, t, strlen(t))) {
      list = s + strlen(t);
      listplus = 1;
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
  if(!test) printf("\r%64s\r", "");
  printf("> %s: %s", pos, msg);
  if(test) printf("\n");
  fflush(stdout);
}


#define INSTALL_INF	"/etc/install.inf"

int braille_install_info(hd_data_t *hd_data)
{
  hd_t *hd;
  int ok = 0;
  char *braille = NULL;
  char *braille_dev = NULL;
  str_list_t *sl0, *sl;
  FILE *f;

  printf("Looking for a braille display...\n");

  hd = hd_list(hd_data, hw_braille, 1, NULL);

  if(hd_data->progress) {
    printf("\r%64s\r", "");
    fflush(stdout);
  }

  for(; hd; hd = hd->next) {
    if(
      hd->base_class == bc_braille &&		/* is a braille display */
      hd->unix_dev_name &&			/* and has a device name */
      (braille = hd_device_name(hd_data, hd->vend, hd->dev))
    ) {
      braille_dev = hd->unix_dev_name;
      ok = 1;
      break;
    }
  }

  if(!ok) return 1;

  printf("found a %s at %s\n", braille, braille_dev);

  sl0 = read_file(INSTALL_INF, 0, 0);
  f = fopen(INSTALL_INF, "w");
  if(!f) {
    perror(INSTALL_INF);
    return 1;
  }
  
  for(sl = sl0; sl; sl = sl->next) {
    if(
      strstr(sl->str, "Braille:") != sl->str &&
      strstr(sl->str, "Brailledevice:") != sl->str
    ) {
      fprintf(f, "%s", sl->str);
    }
  }

  fprintf(f, "Braille: %s\n", braille);
  fprintf(f, "Brailledevice: %s\n", braille_dev);
  
  fclose(f);

  return 0;
}


/*
 * get VGA parameter from /proc/cmdline
 */
int get_fb_mode()
{
#ifndef __PPC__
  FILE *f;
  char buf[256], *s, *t;
  int i, fb_mode = 0;

  if((f = fopen("/proc/cmdline", "r"))) {
    if(fgets(buf, sizeof buf, f)) {
      t = buf;
      while((s = strsep(&t, " "))) {
        if(sscanf(s, "vga=%i", &i) == 1) fb_mode = i;
        if(strstr(s, "vga=normal") == s) fb_mode = 0;
      }
    }
    fclose(f);
  }

  return fb_mode > 0x10 ? fb_mode : 0;
#else /* __PPC__ */
  /* this is the only valid test for active framebuffer ... */
  FILE *f = NULL;
  int fb_mode = 0;
  if((f = fopen("/dev/fb", "r"))) {
    fb_mode++;
    fclose(f);
  }

  return fb_mode;
#endif
}


/*
 * read "x11i=" entry from /proc/cmdline
 */
char *get_x11i()
{
  FILE *f;
  char buf[256], *s, *t;
  static char x11i[64] = { };

  if(!*x11i) return x11i;

  if((f = fopen("/proc/cmdline", "r"))) {
    if(fgets(buf, sizeof buf, f)) {
      t = buf;
      while((s = strsep(&t, " "))) {
        if(sscanf(s, "x11i=%60s", x11i) == 1) break;
      }
    }
    fclose(f);
  }

  return x11i;
}


/*
 * Assumes xf86_ver to be either "3" or "4" (or empty).
 */
char *get_xserver(hd_data_t *hd_data, char **version, char **busid, driver_info_t **x11_driver)
{
  static char display[16];
  static char xf86_ver[2];
  static char id[32];
  char c, *x11i = get_x11i();
  static driver_info_t *di0 = NULL;
  driver_info_t *di;
  hd_t *hd;

  di0 = hd_free_driver_info(di0);
  *x11_driver = NULL;

  *display = *xf86_ver = *id = c = 0;
  *version = xf86_ver;
  *busid = id;

  if(x11i) {
    if(*x11i == '3' || *x11i == '4') {
      c = *x11i;
    }
    else {
      if(*x11i >= 'A' && *x11i <= 'Z') {
        c = '3';
      }
      if(*x11i >= 'a' && *x11i <= 'z') {
        c = '4';
      }
      if(c) {
        strncpy(display, x11i, sizeof display - 1);
        display[sizeof display - 1] = 0;
      }
    }
  }

  if(c) { xf86_ver[0] = c; xf86_ver[1] = 0; }

  hd = hd_get_device_by_idx(hd_data, hd_display_adapter(hd_data));
  if(hd && hd->bus == bus_pci)
    sprintf(id, "%d:%d:%d", hd->slot >> 8, hd->slot & 0xff, hd->func);

  if(*display) return display;

  di0 = hd_driver_info(hd_data, hd);
  for(di = di0; di; di = di->next) {
    if(di->any.type == di_x11 && di->x11.server && di->x11.xf86_ver && !di->x11.x3d) {
      if(c == 0 || c == di->x11.xf86_ver[0]) {
        xf86_ver[0] = di->x11.xf86_ver[0];
        xf86_ver[1] = 0;
        strncpy(display, di->x11.server, sizeof display - 1);
        display[sizeof display - 1] = 0;
        *x11_driver = di;
        break;
      }
    }
  }


  if(*display) return display;

  if(c == 0) c = '4';	/* default to XF 4, if nothing else is known  */

  xf86_ver[0] = c;
  xf86_ver[1] = 0;
  strcpy(display, c == '3' ? "FBDev" : "fbdev");

  return display;
}

int x11_install_info(hd_data_t *hd_data)
{
  hd_t *hd;
  driver_info_t *di;
  char *x11i;
  int fb_mode, kbd_ok = 0;
  unsigned yast2_color = 0;
  char *xkbrules = NULL, *xkbmodel = NULL, *xkblayout = NULL;
  char *xserver, *version, *busid;
  driver_info_t *x11_driver;
  str_list_t *sl0, *sl;
  FILE *f;

  /* get color info */
  hd_set_probe_feature(hd_data, pr_cpu);
  hd_set_probe_feature(hd_data, pr_prom);
  hd_scan(hd_data);

  x11i = get_x11i();
  fb_mode = get_fb_mode();

  hd_list(hd_data, hw_display, 1, NULL);
  xserver = get_xserver(hd_data, &version, &busid, &x11_driver);

  for(hd = hd_list(hd_data, hw_keyboard, 1, NULL); hd; hd = hd->next) {
    kbd_ok = 1;
    di = hd_driver_info(hd_data, hd);
    if(di && di->any.type == di_kbd) {
      xkbrules = di->kbd.XkbRules;
      xkbmodel = di->kbd.XkbModel;
      xkblayout = di->kbd.XkbLayout;
      break;
    }
    /* don't free di */
  }
  
  switch(hd_mac_color(hd_data)) {
    case 0x01:
      yast2_color = 0x5a4add;
      break;
    case 0x04:
      yast2_color = 0x32cd32;
      break;
    case 0x05:
      yast2_color = 0xff7f50;
      break;
    case 0x07:
      yast2_color = 0x000000;
      break;
    case 0xff:
      yast2_color = 0x7f7f7f;
      break;
  }

  if(hd_data->progress) {
    printf("\r%64s\r", "");
    fflush(stdout);
  }

  sl0 = read_file(INSTALL_INF, 0, 0);
  f = fopen(INSTALL_INF, "w");
  if(!f) {
    perror(INSTALL_INF);
    return 1;
  }
  
  for(sl = sl0; sl; sl = sl->next) {
    if(
      strstr(sl->str, "Framebuffer:") != sl->str &&
      strstr(sl->str, "XServer:") != sl->str &&
      strstr(sl->str, "XVersion:") != sl->str &&
      strstr(sl->str, "XBusID:") != sl->str &&
      strstr(sl->str, "X11i:") != sl->str &&
      strstr(sl->str, "Keyboard:") != sl->str &&
      strstr(sl->str, "XkbRules:") != sl->str &&
      strstr(sl->str, "XkbModel:") != sl->str &&
      strstr(sl->str, "XkbLayout:") != sl->str &&
      strstr(sl->str, "XF86Ext:") != sl->str &&
      strstr(sl->str, "XF86Raw:") != sl->str
    ) {
      fprintf(f, "%s", sl->str);
    }
  }

  fprintf(f, "Keyboard: %d\n", kbd_ok);
  if(fb_mode) fprintf(f, "Framebuffer: 0x%04x\n", fb_mode);
  if(x11i) fprintf(f, "X11i: %s\n", x11i);
  if(xserver && *xserver) {
    fprintf(f, "XServer: %s\n", xserver);
    if(*version) fprintf(f, "XVersion: %s\n", version);
    if(*busid) fprintf(f, "XBusID: %s\n", busid);
  }
  if(xkbrules && *xkbrules) fprintf(f, "XkbRules: %s\n", xkbrules);
  if(xkbmodel && *xkbmodel) fprintf(f, "XkbModel: %s\n", xkbmodel);
  if(xkblayout && *xkblayout) fprintf(f, "XkbLayout: %s\n", xkblayout);

  if(x11_driver) {
    for(sl = x11_driver->x11.extensions; sl; sl = sl->next) {
      if(*sl->str) fprintf(f, "XF86Ext:   Load\t\t\"%s\"\n", sl->str);
    }
    for(sl = x11_driver->x11.options; sl; sl = sl->next) {
      if(*sl->str) fprintf(f, "XF86Raw:   Option\t\"%s\"\n", sl->str);
    }
    for(sl = x11_driver->x11.raw; sl; sl = sl->next) {
      if(*sl->str) fprintf(f, "XF86Raw:   %s\n", sl->str);
    }
  }

  fclose(f);

  return 0;
}

char *xserver3map[] =
{
#ifdef __i386__
  "VGA16", "xvga16",
  "RUSH", "xrush",
#endif
#if defined(__i386__) || defined(__alpha__) || defined(__ia64__)
  "SVGA", "xsvga",
  "3DLABS", "xglint",
#endif
#if defined(__i386__) || defined(__alpha__)
  "MACH64", "xmach64",
  "P9000", "xp9k",
  "S3", "xs3",
#endif
#ifdef __alpha__
  "TGA", "xtga",
#endif
#ifdef __sparc__
  "SUNMONO", "xsunmono",
  "SUN", "xsun",
  "SUN24", "xsun24",
#endif
#if 0
  "VMWARE", "xvmware",
#endif
  0, 0
};


int dump_packages(hd_data_t *hd_data)
{
  str_list_t *sl;
  int i;

  hd_data->progress = NULL;
  hd_scan(hd_data);

  sl = get_hddb_packages(hd_data);

  for(i = 0; xserver3map[i]; i += 2) {
    if (!search_str_list(sl, xserver3map[i + 1]))
      add_str_list(&sl, new_str(xserver3map[i + 1]));
  }

  for(; sl; sl = sl->next) {
    printf("%s\n", sl->str);
  }

  return 0;
}

struct x11pack {
  struct x11pack *next;
  char *pack;
};

int oem_install_info(hd_data_t *hd_data)
{
  hd_t *hd;
  str_list_t *str;
  str_list_t *x11packs = 0;
  str_list_t *sl0, *sl;
  FILE *f;
  int pcmcia, i;

  driver_info_x11_t *di, *drvinfo;

  hd_set_probe_feature(hd_data, pr_pci);
  hd_scan(hd_data);
  pcmcia = hd_has_pcmcia(hd_data);

  for(hd = hd_list(hd_data, hw_display, 1, NULL); hd; hd = hd->next) {
    drvinfo = (driver_info_x11_t *)hd_driver_info(hd_data, hd);
    for (di = drvinfo; di; di = (driver_info_x11_t *)di->next) {
      if (di->type != di_x11)
	continue;
      if (di->xf86_ver[0] == '3') {
        char *server = di->server;
        if (server) {
	  for (i = 0; xserver3map[i]; i += 2)
	    if (!strcmp(xserver3map[i], server))
	      break;
	  if (xserver3map[i])
	    if (!search_str_list(x11packs, xserver3map[i + 1]))
	      add_str_list(&x11packs, new_str(xserver3map[i + 1]));
	}
      }
      for (str = di->packages; str; str = str->next)
	if (str->str && *str->str && !search_str_list(x11packs, str->str))
	  add_str_list(&x11packs, new_str(str->str));
    }
    hd_free_driver_info((driver_info_t *)drvinfo);
  }

  if(hd_data->progress) {
    printf("\r%64s\r", "");
    fflush(stdout);
  }

  sl0 = read_file(INSTALL_INF, 0, 0);
  f = fopen(INSTALL_INF, "w");
  if(!f) {
    perror(INSTALL_INF);
    return 1;
  }
  for(sl = sl0; sl; sl = sl->next) {
    if(
      strstr(sl->str, "X11Packages:") != sl->str &&
      strstr(sl->str, "Pcmcia:") != sl->str
    ) {
      fprintf(f, "%s", sl->str);
    }
  }
  if (x11packs) {
    fprintf(f, "X11Packages: ");
    for (sl = x11packs; sl; sl = sl->next) {
      if (sl != x11packs)
        fputc(',', f);
      fprintf(f, "%s", sl->str);
    }
    fputc('\n', f);
  }
  if (pcmcia)
    fprintf(f, "Pcmcia: %d\n", pcmcia);
  fclose(f);
  return 0;
}

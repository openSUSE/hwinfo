#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"

static int get_probe_flags(int, char **, hd_data_t *);
static void progress2(char *, char *);

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
static hd_hw_item_t hw_item[100] = { };
static int hw_items = 0;

int braille_install_info(hd_data_t *hd_data);
int x11_install_info(hd_data_t *hd_data);
int oem_install_info(hd_data_t *hd_data);
void dump_packages(hd_data_t *hd_data);

void do_hw(hd_data_t *hd_data, FILE *f, hd_hw_item_t hw_item);
void do_hw_multi(hd_data_t *hd_data, FILE *f, hd_hw_item_t *hw_items);
void do_short(hd_data_t *hd_data, hd_t *hd, FILE *f);
void do_test(hd_data_t *hd_data);
void help(void);
void dump_db_raw(hd_data_t *hd_data);
void dump_db(hd_data_t *hd_data);
void do_chroot(hd_data_t *hd_data, char *dir);
void ask_db(hd_data_t *hd_data, char *query);
void get_mapping(hd_data_t *hd_data);


struct {
  unsigned db_idx;
  unsigned separate:1;
  char *root;
} opt;

struct option options[] = {
  { "special", 1, NULL, 1 },
  { "help", 0, NULL, 'h' },
  { "debug", 1, NULL, 'd' },
  { "version", 0, NULL, 400 },
  { "log", 1, NULL, 'l' },
  { "packages", 0, NULL, 'p' },
  { "test", 0, NULL, 300 },
  { "format", 1, NULL, 301 },
  { "show-config", 1, NULL, 302 },
  { "save-config", 1, NULL, 303 },
  { "short", 0, NULL, 304 },
  { "fast", 0, NULL, 305 },
  { "dump-db", 1, NULL, 306 },
  { "dump-db-raw", 1, NULL, 307 },
  { "separate", 0, NULL, 308 },
  { "root", 1, NULL, 309 },
  { "db", 1, NULL, 310 },
  { "only", 1, NULL, 311 },
  { "listmd", 0, NULL, 312 },
  { "map", 0, NULL, 313 },
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
  { "storage-ctrl", 0, NULL, 1000 + hw_storage_ctrl },
  { "storage_ctrl", 0, NULL, 1000 + hw_storage_ctrl },
  { "netcard", 0, NULL, 1000 + hw_network_ctrl },
  { "netcards", 0, NULL, 1000 + hw_network_ctrl },	// outdated, just kept for comaptibility
  { "network-ctrl", 0, NULL, 1000 + hw_network_ctrl },
  { "network_ctrl", 0, NULL, 1000 + hw_network_ctrl },
  { "printer", 0, NULL, 1000 + hw_printer },
  { "tv", 0, NULL, 1000 + hw_tv },
  { "dvb", 0, NULL, 1000 + hw_dvb },
  { "scanner", 0, NULL, 1000 + hw_scanner },
  { "braille", 0, NULL, 1000 + hw_braille },
  { "sys", 0, NULL, 1000 + hw_sys },
  { "bios", 0, NULL, 1000 + hw_bios },
  { "cpu", 0, NULL, 1000 + hw_cpu },
  { "partition", 0, NULL, 1000 + hw_partition },
  { "usb-ctrl", 0, NULL, 1000 + hw_usb_ctrl },
  { "usb_ctrl", 0, NULL, 1000 + hw_usb_ctrl },
  { "usb", 0, NULL, 1000 + hw_usb },
  { "pci", 0, NULL, 1000 + hw_pci },
  { "isapnp", 0, NULL, 1000 + hw_isapnp },
  { "scsi", 0, NULL, 1000 + hw_scsi },
  { "ide", 0, NULL, 1000 + hw_ide },
  { "bridge", 0, NULL, 1000 + hw_bridge },
  { "hub", 0, NULL, 1000 + hw_hub },
  { "memory", 0, NULL, 1000 + hw_memory },
  { "manual", 0, NULL, 1000 + hw_manual },
  { "pcmcia", 0, NULL, 1000 + hw_pcmcia },
  { "pcmcia_ctrl", 0, NULL, 1000 + hw_pcmcia_ctrl },
  { "ieee1394", 0, NULL, 1000 + hw_ieee1394 },
  { "ieee1394_ctrl", 0, NULL, 1000 + hw_ieee1394_ctrl },
  { "firewire", 0, NULL, 1000 + hw_ieee1394 },
  { "firewire_ctrl", 0, NULL, 1000 + hw_ieee1394_ctrl },
  { "hotplug", 0, NULL, 1000 + hw_hotplug },
  { "hotplug_ctrl", 0, NULL, 1000 + hw_hotplug_ctrl },
  { "zip", 0, NULL, 1000 + hw_zip },
  { "pppoe", 0, NULL, 1000 + hw_pppoe },
  { "dsl", 0, NULL, 1000 + hw_dsl },
  { "wlan", 0, NULL, 1000 + hw_wlan },
  { "redasd", 0, NULL, 1000 + hw_redasd },
  { "block", 0, NULL, 1000 + hw_block },
  { "tape", 0, NULL, 1000 + hw_tape },
  { "vbe", 0, NULL, 1000 + hw_vbe },
  { "bluetooth", 0, NULL, 1000 + hw_bluetooth },
  { "all", 0, NULL, 2000 },
  { "reallyall", 0, NULL, 2001 },
  { "smp", 0, NULL, 2002 },
  { "arch", 0, NULL, 2003 },
  { "uml", 0, NULL, 2004 },
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
  int i;
  unsigned first_probe = 1;

  hd_data = calloc(1, sizeof *hd_data);
  hd_data->progress = isatty(1) ? progress2 : NULL;
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

        case 306:
          opt.db_idx = strtoul(optarg, NULL, 0);
          dump_db(hd_data);
          break;

        case 307:
          opt.db_idx = strtoul(optarg, NULL, 0);
          dump_db_raw(hd_data);
          break;

        case 308:
          /* basically for debugging */
          opt.separate = 1;
          break;

        case 309:
          opt.root = optarg;
          break;

        case 310:
          ask_db(hd_data, optarg);
          break;

        case 311:
          if(*optarg) add_str_list(&hd_data->only, optarg);
          break;

        case 312:
          hd_data->flags.list_md = 1;
          break;

        case 313:
          get_mapping(hd_data);
          break;

        case 400:
          printf("%s\n", hd_version());
	  break;

        case 1000 ... 1100:
          if(hw_items < (int) (sizeof hw_item / sizeof *hw_item) - 1)
            hw_item[hw_items++] = i - 1000;
          break;

        case 2000:
        case 2001:
        case 2002:
        case 2003:
        case 2004:
          if(hw_items < (int) (sizeof hw_item / sizeof *hw_item) - 1)
            hw_item[hw_items++] = i;
          break;

        default:
          help();
          return 0;
      }
    }

    if(!hw_items && is_short) hw_item[hw_items++] = 2000;	/* all */

    if(hw_items >= 0 || showconfig || saveconfig) {
      if(*log_file) {
        if(!strcmp(log_file, "-")) {
          f = fdopen(1, "w");
        }
        else {
          f = fopen(log_file, "w+");
        }
      }

      if(opt.root) do_chroot(hd_data, opt.root);

      if(opt.separate || hw_items <= 1) {
        for(i = 0; i < hw_items; i++) {
          if(i) fputc('\n', f ? f : stdout);
          do_hw(hd_data, f, hw_item[i]);
        }
      }
      else {
        hw_item[hw_items] = 0;
        do_hw_multi(hd_data, f, hw_item);
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

    if(opt.root && first_probe) do_chroot(hd_data, opt.root);

    hd_scan(hd_data);
    if(hd_data->progress) printf("\r%64s\r", "");

    first_probe = 0;
  } while(argc);

  if(*log_file) {
    if(!strcmp(log_file, "-")) {
      f = fdopen(1, "w");
    }
    else {
      f = fopen(log_file, "w+");
    }
  }

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
    if(!strcmp(list, "dsl")) i = hw_dsl;
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
  int smp = -1, uml = 0, i;
  char *s, *t;
  enum boot_arch b_arch;
  enum cpu_arch c_arch;

  hd0 = NULL;

  switch(hw_item) {
    case 2002:
      smp = hd_smp_support(hd_data);
      break;

    case 2000:
    case 2001:
    case 2003:
      i = -1;
      switch((int) hw_item) {
        case 2000: i = pr_default; break;
        case 2001: i = pr_all; break;
        case 2003: i = pr_cpu; break;
      }
      if(i != -1) {
        hd_clear_probe_feature(hd_data, pr_all);
        hd_set_probe_feature(hd_data, i);
        hd_scan(hd_data);
        hd0 = hd_data->hd;
      }
      break;

    case 2004:
      uml = hd_is_uml(hd_data);
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

  if(hw_item == 2002) {
    fprintf(f ? f : stdout,
      "SMP support: %s",
      smp < 0 ? "unknown" : smp > 0 ? "yes" : "no"
    );
    if(smp > 0) fprintf(f ? f : stdout, " (%u cpus)", smp);
    fprintf(f ? f : stdout, "\n");
  }
  else if(hw_item == 2003) {
    c_arch = hd_cpu_arch(hd_data);
    b_arch = hd_boot_arch(hd_data);

    s = t = "Unknown";
    switch(c_arch) {
      case arch_unknown:
        break;
      case arch_intel:
        s = "X86 (32)";
        break;
      case arch_alpha:
        s = "Alpha";
        break;
      case arch_sparc:
        s = "Sparc (32)";
        break;
      case arch_sparc64:
        s = "UltraSparc (64)";
        break;
      case arch_ppc:
        s = "PowerPC";
        break;
      case arch_ppc64:
        s = "PowerPC (64)";
        break;
      case arch_68k:
        s = "68k";
        break;
      case arch_ia64:
        s = "IA-64";
        break;
      case arch_s390:
        s = "S390";
        break;
      case arch_s390x:
        s = "S390x";
        break;
      case arch_arm:
        s = "ARM";
        break;
      case arch_mips:
        s = "MIPS";
        break;
      case arch_x86_64:
        s = "X86_64";
        break;
    }

    switch(b_arch) {
      case boot_unknown:
        break;
      case boot_lilo:
        t = "lilo";
        break;
      case boot_milo:
        t = "milo";
        break;
      case boot_aboot:
        t = "aboot";
        break;
      case boot_silo:
        t = "silo";
        break;
      case boot_ppc:
        t = "ppc";
        break;
      case boot_elilo:
        t = "elilo";
        break;
      case boot_s390:
        t = "s390";
        break;
      case boot_mips:
        t = "mips";
        break;
      case boot_grub:
        t = "grub";
        break;
    }

    fprintf(f ? f : stdout, "Arch: %s/%s\n", s, t);
  }
  else if(hw_item == 2004) {
    fprintf(f ? f : stdout, "UML: %s\n", uml ? "yes" : "no");
  }
  else {
    if(is_short) {
      /* always to stdout */
      do_short(hd_data, hd0, stdout);
      if(f) do_short(hd_data, hd0, f);
    }
    else {
      for(hd = hd0; hd; hd = hd->next) {
        hd_dump_entry(hd_data, hd, f ? f : stdout);
      }
    }
  }

  if(hw_item == hw_display && hd0) {
    fprintf(f ? f : stdout, "\nPrimary display adapter: #%u\n", hd_display_adapter(hd_data));
  }

  if(hd0 != hd_data->hd) hd_free_hd_list(hd0);
}


void do_hw_multi(hd_data_t *hd_data, FILE *f, hd_hw_item_t *hw_items)
{
  hd_t *hd, *hd0;

  hd0 = hd_list2(hd_data, hw_items, 1);

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

  if(is_short) {
    /* always to stdout */
    do_short(hd_data, hd0, stdout);
    if(f) do_short(hd_data, hd0, f);
  }
  else {
    for(hd = hd0; hd; hd = hd->next) {
      hd_dump_entry(hd_data, hd, f ? f : stdout);
    }
  }

  hd_free_hd_list(hd0);
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
typedef struct {
  char *vendor, *model, *driver;
} scanner_t;

static scanner_t scanner_data[] = {
  { "Abaton", "SCAN 300/GS", "abaton" },
  { "Abaton", "SCAN 300/S", "abaton" },
  { "Acer", "300f", "SnapScan" },
  { "Acer", "310s", "SnapScan" },
  { "Acer", "610plus", "SnapScan" },
  { "Acer", "610s", "SnapScan" },
  { "Acer", "Prisa 1240", "SnapScan" },
  { "Acer", "Prisa 3300", "SnapScan" },
  { "Acer", "Prisa 4300", "SnapScan" },
  { "Acer", "Prisa 5300", "SnapScan" },
  { "Acer", "Prisa 620s", "SnapScan" },
  { "Acer", "Prisa 620u", "SnapScan" },
  { "Acer", "Prisa 620ut", "SnapScan" },
  { "Acer", "Prisa 640bu", "SnapScan" },
  { "Acer", "Prisa 640u", "SnapScan" },
  { "Agfa", "Arcus II", "microtek" },
  { "Agfa", "DuoScan", "microtek" },
  { "Agfa", "FOCUS COLOR", "agfafocus" },
  { "Agfa", "FOCUS GS SCANNER", "agfafocus" },
  { "Agfa", "FOCUS II", "agfafocus" },
  { "Agfa", "FOCUS LINEART SCANNER", "agfafocus" },
  { "Agfa", "SnapScan 1212u", "SnapScan" },
  { "Agfa", "SnapScan 1236s", "SnapScan" },
  { "Agfa", "SnapScan 1236u", "SnapScan" },
  { "Agfa", "SnapScan 300", "SnapScan" },
  { "Agfa", "SnapScan 310", "SnapScan" },
  { "Agfa", "SnapScan 600", "SnapScan" },
  { "Agfa", "SnapScan e20", "SnapScan" },
  { "Agfa", "SnapScan e25", "SnapScan" },
  { "Agfa", "SnapScan e40", "SnapScan" },
  { "Agfa", "SnapScan e50", "SnapScan" },
  { "Agfa", "SnapScan e60", "SnapScan" },
  { "Agfa", "StudioScan", "microtek" },
  { "Agfa", "StudioScan II", "microtek" },
  { "Agfa", "StudioScan IIsi", "microtek" },
  { "Apple", "APPLE SCANNER", "apple" },
  { "Apple", "COLORONESCANNER", "apple" },
  { "Apple", "ONESCANNER", "apple" },
  { "Artec", "A6000C", "artec" },
  { "Artec", "A6000C PLUS", "artec" },
  { "Artec", "AM12S", "artec" },
  { "Artec", "AT12", "artec" },
  { "Artec", "AT3", "artec" },
  { "Artec", "AT6", "artec" },
  { "Artec", "ColorOneScanner", "artec" },
  { "Avision", "AV 620 CS", "avision" },
  { "Avision", "AV 6240", "avision" },
  { "Avision", "AV 630 CS", "avision" },
  { "B&H SCSI", "COPISCAN II 2135", "bh" },
  { "B&H SCSI", "COPISCAN II 2137", "bh" },
  { "B&H SCSI", "COPISCAN II 2137A", "bh" },
  { "B&H SCSI", "COPISCAN II 2138A", "bh" },
  { "B&H SCSI", "COPISCAN II 3238", "bh" },
  { "B&H SCSI", "COPISCAN II 3338", "bh" },
  { "B&H SCSI", "COPISCAN II 6338", "bh" },
  { "BlackWidow", "BW4800SP", "artec" },
  { "Canon", "CANOSCAN 2700F", "canon" },
  { "Canon", "CANOSCAN 300", "canon" },
  { "Canon", "CANOSCAN 600", "canon" },
  { "Devcom", "9636PRO", "pie" },
  { "Devcom", "9636S", "pie" },
  { "EDGE", "KTX-9600US", "umax" },
  { "Epson", "ES-8500", "epson" },
  { "Epson", "EXPRESSION 1600", "epson" },
  { "Epson", "EXPRESSION 1680", "epson" },
  { "Epson", "EXPRESSION 636", "epson" },
  { "Epson", "EXPRESSION 800", "epson" },
  { "Epson", "FILMSCAN 200", "epson" },
  { "Epson", "GT-5500", "epson" },
  { "Epson", "GT-7000", "epson" },
  { "Epson", "GT-8000", "epson" },
  { "Epson", "PERFECTION 1200PHOTO", "epson" },
  { "Epson", "PERFECTION 1200S", "epson" },
  { "Epson", "PERFECTION 1200U", "epson" },
  { "Epson", "PERFECTION 1240", "epson" },
  { "Epson", "PERFECTION 1640", "epson" },
  { "Epson", "PERFECTION 1650", "epson" },
  { "Epson", "PERFECTION 610", "epson" },
  { "Epson", "PERFECTION 636S", "epson" },
  { "Epson", "PERFECTION 636U", "epson" },
  { "Epson", "PERFECTION 640", "epson" },
  { "Epson", "PERFECTION1200", "epson" },
  { "Epson", "Perfection 600", "umax" },
  { "Escom", "Image Scanner 256", "umax" },
  { "Escort", "Galleria 600", "umax" },
  { "Fujitsu", "M3091DCD", "m3091" },
  { "Fujitsu", "M3096G", "m3096g" },
  { "Fujitsu", "SP15C", "sp15c" },
  { "Genius", "ColorPage-HR5 Pro", "umax" },
  { "Guillemot", "Maxi Scan A4 Deluxe", "SnapScan" },
  { "HP", "HP OFFICEJET K SERIES", "hp" },
  { "HP", "HP OFFICEJET V SERIES", "hp" },
  { "HP", "HP PHOTOSMART PHOTOSCANNER", "hp" },
  { "HP", "HP PSC 700 SERIES", "hp" },
  { "HP", "HP PSC 900 SERIES", "hp" },
  { "HP", "HP SCANJET 3C", "hp" },
  { "HP", "HP SCANJET 3P", "hp" },
  { "HP", "HP SCANJET 4100C", "hp" },
  { "HP", "HP SCANJET 4C", "hp" },
  { "HP", "HP SCANJET 4P", "hp" },
  { "HP", "HP SCANJET 5200C", "hp" },
  { "HP", "HP SCANJET 6100C", "hp" },
  { "HP", "HP SCANJET 6200C", "hp" },
  { "HP", "HP SCANJET 6250C", "hp" },
  { "HP", "HP SCANJET 6300C", "hp" },
  { "HP", "HP SCANJET 6350C", "hp" },
  { "HP", "HP SCANJET 6390C", "hp" },
  { "HP", "HP SCANJET IIC", "hp" },
  { "HP", "HP SCANJET IICX", "hp" },
  { "HP", "HP SCANJET IIP", "hp" },
  { "HP", "HP ScanJet 5p", "hp" },
  { "HP", "HP4200", "hp4200" },
  { "Highscreen", "Scanboostar Premium", "umax" },
  { "Linotype Hell", "Jade", "umax" },
  { "Linotype Hell", "Jade2", "umax" },
  { "Linotype Hell", "Linoscan 1400", "umax" },
  { "Linotype Hell", "Opal", "umax" },
  { "Linotype Hell", "Opal Ultra", "umax" },
  { "Linotype Hell", "Saphir", "umax" },
  { "Linotype Hell", "Saphir HiRes", "umax" },
  { "Linotype Hell", "Saphir Ultra", "umax" },
  { "Linotype Hell", "Saphir Ultra II", "umax" },
  { "Linotype Hell", "Saphir2", "umax" },
  { "Microtek", "Phantom 636", "microtek2" },
  { "Microtek", "ScanMaker 330", "microtek2" },
  { "Microtek", "ScanMaker 3600", "sm3600" },
  { "Microtek", "ScanMaker 630", "microtek2" },
  { "Microtek", "ScanMaker 636", "microtek2" },
  { "Microtek", "ScanMaker 9600XL", "microtek2" },
  { "Microtek", "ScanMaker E3plus", "microtek2" },
  { "Microtek", "ScanMaker V300", "microtek2" },
  { "Microtek", "ScanMaker V310", "microtek2" },
  { "Microtek", "ScanMaker V600", "microtek2" },
  { "Microtek", "ScanMaker V6USL", "microtek2" },
  { "Microtek", "ScanMaker X6", "microtek2" },
  { "Microtek", "ScanMaker X6EL", "microtek2" },
  { "Microtek", "ScanMaker X6USB", "microtek2" },
  { "Microtek", "Scanmaker 35", "microtek" },
  { "Microtek", "Scanmaker 35t+", "microtek" },
  { "Microtek", "Scanmaker 45t", "microtek" },
  { "Microtek", "Scanmaker 600G", "microtek" },
  { "Microtek", "Scanmaker 600G S", "microtek" },
  { "Microtek", "Scanmaker 600GS", "microtek" },
  { "Microtek", "Scanmaker 600S", "microtek" },
  { "Microtek", "Scanmaker 600Z", "microtek" },
  { "Microtek", "Scanmaker 600Z S", "microtek" },
  { "Microtek", "Scanmaker 600ZS", "microtek" },
  { "Microtek", "Scanmaker E2", "microtek" },
  { "Microtek", "Scanmaker E3", "microtek" },
  { "Microtek", "Scanmaker E6", "microtek" },
  { "Microtek", "Scanmaker II", "microtek" },
  { "Microtek", "Scanmaker IIG", "microtek" },
  { "Microtek", "Scanmaker IIHR", "microtek" },
  { "Microtek", "Scanmaker III", "microtek" },
  { "Microtek", "Scanmaker IISP", "microtek" },
  { "Microtek", "SlimScan C6", "microtek2" },
  { "Mustek", "1200 CU", "mustek_usb" },
  { "Mustek", "1200 CU Plus", "mustek_usb" },
  { "Mustek", "1200 UB", "mustek_usb" },
  { "Mustek", "600 CU", "mustek_usb" },
  { "Mustek", "Paragon 1200 A3 Pro", "mustek" },
  { "Mustek", "Paragon 1200 III SP", "mustek" },
  { "Mustek", "Paragon 1200 LS", "mustek" },
  { "Mustek", "Paragon 1200 SP Pro", "mustek" },
  { "Mustek", "Paragon 600 II CD", "mustek" },
  { "Mustek", "Paragon 800 II SP", "mustek" },
  { "Mustek", "Paragon MFC-600S", "mustek" },
  { "Mustek", "Paragon MFC-800S", "mustek" },
  { "Mustek", "Paragon MFS-12000CX", "mustek" },
  { "Mustek", "Paragon MFS-12000SP", "mustek" },
  { "Mustek", "Paragon MFS-1200SP", "mustek" },
  { "Mustek", "Paragon MFS-6000CX", "mustek" },
  { "Mustek", "Paragon MFS-6000SP", "mustek" },
  { "Mustek", "Paragon MFS-8000SP", "mustek" },
  { "Mustek", "ScanExpress 12000SP", "mustek" },
  { "Mustek", "ScanExpress 12000SP Plus", "mustek" },
  { "Mustek", "ScanExpress 6000SP", "mustek" },
  { "Mustek", "ScanExpress A3 SP", "mustek" },
  { "Mustek", "ScanMagic 600 II SP", "mustek" },
  { "Mustek", "ScanMagic 9636S", "mustek" },
  { "Mustek", "ScanMagic 9636S Plus", "mustek" },
  { "NEC", "PC-IN500/4C", "nec" },
  { "Nikon", "AX-210", "umax" },
  { "Nikon", "LS-1000", "coolscan" },
  { "Nikon", "LS-20", "coolscan" },
  { "Nikon", "LS-2000", "coolscan" },
  { "Nikon", "LS-30", "coolscan" },
  { "Pie", "9630S", "pie" },
  { "Pie", "ScanAce 1230S", "pie" },
  { "Pie", "ScanAce 1236S", "pie" },
  { "Pie", "ScanAce 630S", "pie" },
  { "Pie", "ScanAce 636S", "plustek" },
  { "Pie", "ScanAce II", "pie" },
  { "Pie", "ScanAce II Plus", "pie" },
  { "Pie", "ScanAce III", "pie" },
  { "Pie", "ScanAce III Plus", "pie" },
  { "Pie", "ScanAce Plus", "pie" },
  { "Pie", "ScanAce ScanMedia", "pie" },
  { "Pie", "ScanAce ScanMedia II", "pie" },
  { "Pie", "ScanAce V", "pie" },
  { "Plustek", "OpticPro 19200S", "artec" },
  { "Polaroid", "DMC", "dmc" },
  { "Ricoh", "Ricoh IS50", "ricoh" },
  { "Ricoh", "Ricoh IS60", "ricoh" },
  { "Scanport", "SQ4836", "microtek2" },
  { "Sharp", "9036 Flatbed scanner", "sharp" },
  { "Sharp", "JX-250", "sharp" },
  { "Sharp", "JX-320", "sharp" },
  { "Sharp", "JX-330", "sharp" },
  { "Sharp", "JX-350", "sharp" },
  { "Sharp", "JX-610", "sharp" },
  { "Siemens", "9036 Flatbed scanner", "s9036" },
  { "Siemens", "FOCUS COLOR PLUS", "agfafocus" },
  { "Siemens", "ST400", "st400" },
  { "Siemens", "ST800", "st400" },
  { "Tamarack", "Artiscan 12000C", "tamarack" },
  { "Tamarack", "Artiscan 6000C", "tamarack" },
  { "Tamarack", "Artiscan 8000C", "tamarack" },
  { "Trust", "Compact Scan USB 19200", "mustek_usb" },
  { "Trust", "Imagery 1200 SP", "mustek" },
  { "Trust", "Imagery 4800 SP", "mustek" },
  { "Trust", "SCSI Connect 19200", "mustek" },
  { "Trust", "SCSI excellence series 19200", "mustek" },
  { "UMAX", "Astra 1200S", "umax" },
  { "UMAX", "Astra 1220S", "umax" },
  { "UMAX", "Astra 2100S", "umax" },
  { "UMAX", "Astra 2200", "umax" },
  { "UMAX", "Astra 2200 S", "umax" },
  { "UMAX", "Astra 2200 U", "umax" },
  { "UMAX", "Astra 2400S", "umax" },
  { "UMAX", "Astra 600S", "umax" },
  { "UMAX", "Astra 610S", "umax" },
  { "UMAX", "Gemini D-16", "umax" },
  { "UMAX", "Mirage D-16L", "umax" },
  { "UMAX", "Mirage II", "umax" },
  { "UMAX", "Mirage IIse", "umax" },
  { "UMAX", "PL-II", "umax" },
  { "UMAX", "PSD", "umax" },
  { "UMAX", "PowerLook", "umax" },
  { "UMAX", "PowerLook 2000", "umax" },
  { "UMAX", "PowerLook 3000", "umax" },
  { "UMAX", "PowerLook III", "umax" },
  { "UMAX", "Supervista S-12", "umax" },
  { "UMAX", "UC 1200S", "umax" },
  { "UMAX", "UC 1200SE", "umax" },
  { "UMAX", "UC 1260", "umax" },
  { "UMAX", "UC 630", "umax" },
  { "UMAX", "UC 840", "umax" },
  { "UMAX", "UG 630", "umax" },
  { "UMAX", "UG 80", "umax" },
  { "UMAX", "UMAX S-12", "umax" },
  { "UMAX", "UMAX S-12G", "umax" },
  { "UMAX", "UMAX S-6E", "umax" },
  { "UMAX", "UMAX S-6EG", "umax" },
  { "UMAX", "UMAX VT600", "umax" },
  { "UMAX", "Vista S6", "umax" },
  { "UMAX", "Vista S6E", "umax" },
  { "UMAX", "Vista-S8", "umax" },
  { "UMAX", "Vista-T630", "umax" },
  { "Ultima", "A6000C", "artec" },
  { "Ultima", "A6000C PLUS", "artec" },
  { "Ultima", "AM12S", "artec" },
  { "Ultima", "AT12", "artec" },
  { "Ultima", "AT3", "artec" },
  { "Ultima", "AT6", "artec" },
  { "Ultima", "ColorOneScanner", "artec" },
  { "Vobis", "HighScan", "microtek2" },
  { "Vobis", "Scanboostar Premium", "umax" },
  { "Vuego", "Close SnapScan 310 compatible.", "SnapScan" }
};

static char *scanner_info(hd_t *hd)
{
  int i;

  if(!hd->vendor.name || !hd->device.name) return NULL;

  for(i = 0; (unsigned) i < sizeof scanner_data / sizeof *scanner_data; i++) {
    if(
      !strcasecmp(scanner_data[i].vendor, hd->vendor.name) &&
      !strcasecmp(scanner_data[i].model, hd->device.name)
    ) {
      return scanner_data[i].driver;
    }
  }

  return NULL;
}

#endif

void do_test(hd_data_t *hd_data)
{
#if 0
  hd_t *hd, *hd0;
  hd_res_t *res;
  driver_info_t *di;
  FILE *f;
  int i, wheels, buttons;
  unsigned u;
  uint64_t ul;
  char *s, *s1;
  hd_hw_item_t item, items[] = {
    hw_display, hw_monitor, hw_tv, hw_sound, hw_mouse, hw_disk, hw_cdrom,
    hw_floppy, hw_modem, hw_isdn, hw_scanner, hw_camera
  };

  hd_set_probe_feature(hd_data, pr_default);
  hd_scan(hd_data);

  f = fopen("/tmp/hw_overview.log", "w");

  for(i = 0; (unsigned) i < sizeof items / sizeof *items; i++) {
    item = items[i];
    hd0 = hd_list(hd_data, item, 0, NULL);

    if(!hd0) continue;
  
    switch(item) {
      case hw_disk:
        fprintf(f, "Disk\n");
        for(hd = hd0; hd; hd = hd->next) {
          u = 0;
          for(res = hd->res; res; res = res->next) {
            if(res->any.type == res_size && res->size.unit == size_unit_sectors) {
              ul = (uint64_t) res->size.val1 * (res->size.val2 ?: 0x200);
              u = ((ul >> 29) + 1) >> 1;
            }
          }
          s = hd->bus.name;
          fprintf(f, "  %s", hd->model);
          if(u) {
            fprintf(f, " (");
            if(s) fprintf(f, "%s, ", s);
            fprintf(f, "%u GB)", u);
          }
          fprintf(f, "\n");
        }
        fprintf(f, "\n");
        break;

      case hw_cdrom:
        fprintf(f, "CD-ROM\n");
        for(hd = hd0; hd; hd = hd->next) {
          s = hd->bus.name;
          fprintf(f, "  %s (", hd->model);
          if(s) fprintf(f, "%s, ", s);
          fprintf(f, "%s)", hd->prog_if.name ?: "CD-ROM");
          fprintf(f, "\n");
        }
        fprintf(f, "\n");
        break;

      case hw_monitor:
        fprintf(f, "Monitor\n");
        for(hd = hd0; hd; hd = hd->next) {
          s = hd->model;
          if(!strcmp(hd->unique_id, "rdCR.EY_qmtb9YY0")) s = "not detected";
          fprintf(f, "  %s\n", s);
        }
        fprintf(f, "\n");
        break;

      case hw_display:
        fprintf(f, "GFX Card\n");
        for(hd = hd0; hd; hd = hd->next) {
          u = 0;
          s1 = NULL;
          for(di = hd->driver_info; di; di = di->next) {
            if(di->any.type == di_x11) {
              if(!s1) s1 = di->x11.server;
              if(di->x11.x3d && !u) {
                s1 = di->x11.server;
                u = 1;
              }
            }
          }
          if(!s1) {
            s1 = "not supported";
            u = 0;
          }
          fprintf(f, "  %s (%s", hd->model, s1);
          if(u) fprintf(f, ", 3D support");
          fprintf(f, ")");
          fprintf(f, "\n");
        }
        fprintf(f, "\n");
        break;

      case hw_mouse:
        fprintf(f, "Mouse\n");
        for(hd = hd0; hd; hd = hd->next) {
          buttons = wheels = -1;	// make gcc happy
          s = NULL;
          for(di = hd->driver_info; di; di = di->next) {
            if(di->any.type == di_mouse) {
              buttons = di->mouse.buttons;
              wheels = di->mouse.wheels;
              s = di->mouse.xf86;
              break;
            }
          }
          if(!s) {
            s = "not supported";
            buttons = wheels = -1;
          }
          fprintf(f, "  %s (%s", hd->model, s);
          if(buttons >= 0) fprintf(f, ", %d buttons", buttons);
          if(wheels >= 0) fprintf(f, ", %d wheels", wheels);
          fprintf(f, ")");
          fprintf(f, "\n");
        }
        fprintf(f, "\n");
        break;

      case hw_tv:
        fprintf(f, "TV Card\n");
        for(hd = hd0; hd; hd = hd->next) {
          s = NULL;
          for(di = hd->driver_info; di; di = di->next) {
            if(
              (di->any.type == di_any || di->any.type == di_module) &&
              di->any.hddb0 &&
              di->any.hddb0->str
            ) {
              s = di->any.hddb0->str;
              break;
            }
          }
          if(!s) {
            s = "not supported";
          }
          fprintf(f, "  %s (%s)\n", hd->model, s);
        }
        fprintf(f, "\n");
        break;

      case hw_sound:
        fprintf(f, "Sound Card\n");
        for(hd = hd0; hd; hd = hd->next) {
          s = NULL;
          for(di = hd->driver_info; di; di = di->next) {
            if(
              (di->any.type == di_any || di->any.type == di_module) &&
              di->any.hddb0 &&
              di->any.hddb0->str
            ) {
              s = di->any.hddb0->str;
              break;
            }
          }
          if(!s) {
            s = "not supported";
          }
          fprintf(f, "  %s (%s)\n", hd->model, s);
        }
        fprintf(f, "\n");
        break;

      case hw_camera:
        fprintf(f, "Digital Camera/WebCam\n");
        for(hd = hd0; hd; hd = hd->next) {
          fprintf(f, "  %s\n", hd->model);
        }
        fprintf(f, "\n");
        break;

      case hw_floppy:
        fprintf(f, "Floppy/Zip Drive\n");
        for(hd = hd0; hd; hd = hd->next) {
          fprintf(f, "  %s\n", hd->model);
        }
        fprintf(f, "\n");
        break;

      case hw_modem:
        fprintf(f, "Modem\n");
        for(hd = hd0; hd; hd = hd->next) {
          fprintf(f, "  %s\n", hd->model);
        }
        fprintf(f, "\n");
        break;

      case hw_isdn:
        fprintf(f, "ISDN\n");
        for(hd = hd0; hd; hd = hd->next) {
          fprintf(f, "  %s (%ssupported)\n", hd->model, hd->driver_info ? "" : "not ");
        }
        fprintf(f, "\n");
        break;

      case hw_scanner:
        fprintf(f, "Scanner\n");
        for(hd = hd0; hd; hd = hd->next) {
          s = scanner_info(hd);
          if(!s) s = "not supported";
          fprintf(f, "  %s (%s)\n", hd->model, s);
        }
        fprintf(f, "\n");
        break;

      default:
        break;
    }

    hd_free_hd_list(hd0);

  }

  fclose(f);

  f = fopen("/tmp/hw_detail.log", "w");

  if(hd_data->log) {
    fprintf(f,
      "============ start detailed hardware log ============\n"
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
    "============ end detailed hardware log ============\n"
  );

  fclose(f);

  fprintf(stderr, "\n");

#endif

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

#if 0
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
    "Usage: hwinfo [options]\n"
    "Probe for hardware.\n"
    "  --short        just a short listing\n"
    "  --log logfile  write info to logfile\n"
    "  --debug level  set debuglevel\n"
    "  --version      show libhd version\n"
    "  --dump-db n    dump hardware data base, 0: external, 1: internal\n"
    "  --hw_item      probe for hw_item\n"
    "  hw_item is one of:\n"
    "    all, bios, block, bluetooth, braille, bridge, camera, cdrom, chipcard, cpu,\n"
    "    disk, dsl, dvb, floppy, framebuffer, gfxcard, hub, ide, isapnp, isdn,\n"
    "    joystick, keyboard, memory, modem, monitor, mouse, netcard, network,\n"
    "    partition, pci, pcmcia, pcmcia-ctrl, pppoe, printer, scanner, scsi, smp,\n"
    "    sound, storage-ctrl, sys, tape, tv, usb, usb-ctrl, vbe, wlan, zip\n\n"
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

    t = "only=";
    if(!strncmp(s, t, strlen(t))) {
      add_str_list(&hd_data->only, s + strlen(t));
      continue;
    }

    t = "root=";
    if(!strncmp(s, t, strlen(t))) {
      opt.root = s + strlen(t);
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
void progress2(char *pos, char *msg)
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

  hd = hd_list(hd_data, hw_braille, 1, NULL);

  if(hd_data->progress) {
    printf("\r%64s\r", "");
    fflush(stdout);
  }

  for(; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_braille &&	/* is a braille display */
      hd->unix_dev_name &&			/* and has a device name */
      (braille = hd->device.name)
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

  if(*x11i) return x11i;

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
  driver_info_t *di;
  hd_t *hd;

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

  if(hd && hd->bus.id == bus_pci)
    sprintf(id, "%d:%d:%d", hd->slot >> 8, hd->slot & 0xff, hd->func);

  if(!hd || *display) return display;

  for(di = hd->driver_info; di; di = di->next) {
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

  for(hd = hd_list(hd_data, hw_keyboard, 1, NULL); hd; hd = hd->next) {
    kbd_ok = 1;
    di = hd->driver_info;
    if(di && di->any.type == di_kbd) {
      xkbrules = di->kbd.XkbRules;
      xkbmodel = di->kbd.XkbModel;
      xkblayout = di->kbd.XkbLayout;
      break;
    }
    /* don't free di */
  }

  xserver = get_xserver(hd_data, &version, &busid, &x11_driver);

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


void dump_packages(hd_data_t *hd_data)
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
    for(str = hd->requires; str; str = str->next) {
      if(!search_str_list(x11packs, str->str)) {
        add_str_list(&x11packs, str->str);
      }
    }
    drvinfo = (driver_info_x11_t *) hd->driver_info;
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
	      add_str_list(&x11packs, xserver3map[i + 1]);
	}
      }
    }
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


void dump_db_raw(hd_data_t *hd_data)
{
  hd_data->progress = NULL;
  hd_clear_probe_feature(hd_data, pr_all);
  hd_scan(hd_data);

  if(opt.db_idx >= sizeof hd_data->hddb2 / sizeof *hd_data->hddb2) return;

  hddb_dump_raw(hd_data->hddb2[opt.db_idx], stdout);
}


void dump_db(hd_data_t *hd_data)
{
  hd_data->progress = NULL;
  hd_clear_probe_feature(hd_data, pr_all);
  hd_scan(hd_data);

  if(opt.db_idx >= sizeof hd_data->hddb2 / sizeof *hd_data->hddb2) return;

  hddb_dump(hd_data->hddb2[opt.db_idx], stdout);
}


void do_chroot(hd_data_t *hd_data, char *dir)
{
  int i;

  i = chroot(dir);
  ADD2LOG("chroot %s: %s\n", dir, i ? strerror(errno) : "ok");

  if(!i) chdir("/");
}


void ask_db(hd_data_t *hd_data, char *query)
{
  hd_t *hd;
  driver_info_t *di;
  str_list_t *sl, *query_sl;
  unsigned tag = 0, u, cnt;
  char buf[256];

  setenv("hwprobe", "-all", 1);
  hd_scan(hd_data);

  hd = add_hd_entry(hd_data, __LINE__, 0);

  query_sl = hd_split(' ', query);

  for(sl = query_sl; sl; sl = sl->next) {
    if(!strcmp(sl->str, "pci")) { tag = TAG_PCI; continue; }
    if(!strcmp(sl->str, "usb")) { tag = TAG_USB; continue; }
    if(!strcmp(sl->str, "pnp")) { tag = TAG_EISA; continue; }
    if(!strcmp(sl->str, "isapnp")) { tag = TAG_EISA; continue; }
    if(!strcmp(sl->str, "special")) { tag = TAG_SPECIAL; continue; }
    if(!strcmp(sl->str, "pcmcia")) { tag = TAG_PCMCIA; continue; }

    if(sscanf(sl->str, "vendor=%i%n", &u, &cnt) >= 1 && !sl->str[cnt]) {
      hd->vendor.id = MAKE_ID(tag, u);
      continue;
    }

    if(sscanf(sl->str, "vendor=%3s%n", buf, &cnt) >= 1 && !sl->str[cnt]) {
      u = name2eisa_id(buf);
      if(u) hd->vendor.id = u;
      tag = TAG_EISA;
      continue;
    }

    if(sscanf(sl->str, "device=%i%n", &u, &cnt) >= 1 && !sl->str[cnt]) {
      hd->device.id = MAKE_ID(tag, u);
      continue;
    }

    if(sscanf(sl->str, "subvendor=%i%n", &u, &cnt) >= 1 && !sl->str[cnt]) {
      hd->sub_vendor.id = MAKE_ID(tag, u);
      continue;
    }

    if(sscanf(sl->str, "subvendor=%3s%n", buf, &cnt) >= 1 && !sl->str[cnt]) {
      u = name2eisa_id(buf);
      if(u) hd->sub_vendor.id = u;
      tag = TAG_EISA;
      continue;
    }

    if(sscanf(sl->str, "subdevice=%i%n", &u, &cnt) >= 1 && !sl->str[cnt]) {
      hd->sub_device.id = MAKE_ID(tag, u);
      continue;
    }

    if(sscanf(sl->str, "revision=%i%n", &u, &cnt) >= 1 && !sl->str[cnt]) {
      hd->revision.id = u;
      continue;
    }

    if(sscanf(sl->str, "serial=%255s%n", buf, &cnt) >= 1 && !sl->str[cnt]) {
      hd->serial = new_str(buf);
      continue;
    }

  }

  free_str_list(query_sl);

  hddb_add_info(hd_data, hd);

  for(di = hd->driver_info; di; di = di->next) {
    if(di->any.type == di_module && di->module.modprobe) {
      for(sl = di->module.names; sl; sl = sl->next) {
        printf("%s%c", sl->str, sl->next ? ' ' : '\n');
      }
    }
  }
}


int is_same_block_dev(hd_t *hd1, hd_t *hd2)
{
  if(!hd1 || !hd2 || hd1 == hd2) return 0;

  if(
    hd1->base_class.id != hd2->base_class.id ||
    hd1->sub_class.id != hd2->sub_class.id
  ) return 0;

  if(
    !hd1->model ||
    !hd2->model ||
    strcmp(hd1->model, hd2->model)
  ) return 0;

  if(hd1->revision.name || hd2->revision.name) {
    if(
      !hd1->revision.name ||
      !hd2->revision.name ||
      strcmp(hd1->revision.name, hd2->revision.name)
    ) return 0;
  }

  if(hd1->serial || hd2->serial) {
    if(
      !hd1->serial ||
      !hd2->serial ||
      strcmp(hd1->serial, hd2->serial)
    ) return 0;
  }

  return 1;
}


hd_t *get_same_block_dev(hd_t *hd_list, hd_t *hd, hd_status_value_t status)
{
  for(; hd_list; hd_list = hd_list->next) {
    if(hd_list->status.available != status) continue;
    if(is_same_block_dev(hd_list, hd)) return hd_list;
  }

  return NULL;
}


void get_mapping(hd_data_t *hd_data)
{
  hd_t *hd_manual, *hd, *hd2;
  struct {
    hd_t *hd;
    unsigned unknown:1;
  } map[256] = { };
  unsigned maps = 0, u;
  int broken, first;
  hd_hw_item_t hw_items[] = { hw_disk, hw_cdrom, 0 };

  hd_data->progress = NULL;

  hd_data->flags.list_all = 1;

  hd_manual = hd_list2(hd_data, hw_items, 1);
  for(hd = hd_manual; hd && maps < sizeof map / sizeof *map; hd = hd->next) {
    if(!hd->unix_dev_name) continue;

    if(hd->status.available == status_yes) {
      /* check if we already have an active device with the same name */
      for(broken = u = 0; u < maps; u++) {
        if(!strcmp(map[u].hd->unix_dev_name, hd->unix_dev_name)) {
          map[u].unknown = 1;
          broken = 1;
        }
      }
      if(broken) continue;

      /* ensure we really can tell different devices apart */
      if(get_same_block_dev(hd_manual, hd, status_yes)) {
        map[maps].hd = hd;
        map[maps].unknown = 1;
      }
      else {
        map[maps].hd = hd;
      }
      maps++;
    }
  }

  /* ok, we have a list of all new devs */

  for(u = 0; u < maps; u++) {
    if(map[u].unknown) {
      printf("%s\n", map[u].hd->unix_dev_name);
    }
    else {
      first = 1;
      for(hd2 = hd_manual; (hd2 = get_same_block_dev(hd2, map[u].hd, status_no)); hd2 = hd2->next) {
        if(hd2->unix_dev_name && strcmp(map[u].hd->unix_dev_name, hd2->unix_dev_name)) {
          printf("%s\t%s", first ? map[u].hd->unix_dev_name : "", hd2->unix_dev_name);
          first = 0;
        }
      }
      if(!first) printf("\n");
    }

  }

}


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

typedef struct {
  hd_hw_item_t type;
  char *dev;
  char *dev_old;
  char *serial;
  char *model;
  uint64_t size;
  char *id;
  char *p_id;
  unsigned model_ok:1;
  unsigned serial_ok:1;
  unsigned size_ok:1;
  unsigned assigned:1;
} map_t;

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

void do_hw(hd_data_t *hd_data, FILE *f, int hw_item);
void do_hw_multi(hd_data_t *hd_data, FILE *f, hd_hw_item_t *hw_items);
void do_short(hd_data_t *hd_data, hd_t *hd, FILE *f);
void do_test(hd_data_t *hd_data);
void help(void);
void dump_db_raw(hd_data_t *hd_data);
void dump_db(hd_data_t *hd_data);
void do_chroot(hd_data_t *hd_data, char *dir);
void ask_db(hd_data_t *hd_data, char *query);
// void get_mapping(hd_data_t *hd_data);
int get_mapping2(void);
void write_udi(hd_data_t *hd_data, char *udi);

void do_saveconfig(hd_data_t *hd_data, hd_t *hd, FILE *f);

int map_cmp(const void *p0, const void *p1);
unsigned map_fill(map_t *map, hd_data_t *hd_data, hd_t *hd_manual);
void map_dump(map_t *map, unsigned map_len);

struct {
  unsigned db_idx;
  unsigned separate:1;
  unsigned verbose:1;
  char *root;
} opt;

struct option options[] = {
  { "special", 1, NULL, 1 },
  { "help", 0, NULL, 'h' },
  { "debug", 1, NULL, 'd' },
  { "version", 0, NULL, 400 },
  { "log", 1, NULL, 'l' },
  { "packages", 0, NULL, 'p' },
  { "verbose", 0, NULL, 'v' },
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
  { "kernel-version", 1, NULL, 314 },
  { "write-udi", 1, NULL, 315 },
  { "hddb-dir", 1, NULL, 316 },
  { "nowpa", 0, NULL, 317 },
  { "map2", 0, NULL, 318 },
  { "hddb-dir-new", 1, NULL, 319 },
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
  { "pcmcia-ctrl", 0, NULL, 1000 + hw_pcmcia_ctrl },
  { "pcmcia_ctrl", 0, NULL, 1000 + hw_pcmcia_ctrl },
  { "ieee1394", 0, NULL, 1000 + hw_ieee1394 },
  { "ieee1394-ctrl", 0, NULL, 1000 + hw_ieee1394_ctrl },
  { "ieee1394_ctrl", 0, NULL, 1000 + hw_ieee1394_ctrl },
  { "firewire", 0, NULL, 1000 + hw_ieee1394 },
  { "firewire-ctrl", 0, NULL, 1000 + hw_ieee1394_ctrl },
  { "firewire_ctrl", 0, NULL, 1000 + hw_ieee1394_ctrl },
  { "hotplug", 0, NULL, 1000 + hw_hotplug },
  { "hotplug-ctrl", 0, NULL, 1000 + hw_hotplug_ctrl },
  { "hotplug_ctrl", 0, NULL, 1000 + hw_hotplug_ctrl },
  { "mmc-ctrl", 0, NULL, 1000 + hw_mmc_ctrl },
  { "mmc_ctrl", 0, NULL, 1000 + hw_mmc_ctrl },
  { "zip", 0, NULL, 1000 + hw_zip },
  { "pppoe", 0, NULL, 1000 + hw_pppoe },
  { "dsl", 0, NULL, 1000 + hw_dsl },
  { "wlan", 0, NULL, 1000 + hw_wlan },
  { "redasd", 0, NULL, 1000 + hw_redasd },
  { "block", 0, NULL, 1000 + hw_block },
  { "tape", 0, NULL, 1000 + hw_tape },
  { "vbe", 0, NULL, 1000 + hw_vbe },
  { "bluetooth", 0, NULL, 1000 + hw_bluetooth },
  { "fingerprint", 0, NULL, 1000 + hw_fingerprint },
  { "all", 0, NULL, 2000 },
  { "reallyall", 0, NULL, 2001 },
  { "smp", 0, NULL, 2002 },
  { "arch", 0, NULL, 2003 },
  { "uml", 0, NULL, 2004 },
  { "xen", 0, NULL, 2005 },
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

    while((i = getopt_long(argc, argv, "hd:l:pv", options, NULL)) != -1) {
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

        case 'v':
          opt.verbose = 1;
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
          return get_mapping2();
          break;

        case 314:
          if(*optarg) setenv("LIBHD_KERNELVERSION", optarg, 1);
          break;

        case 315:
          write_udi(hd_data, optarg);
          break;

        case 316:
          if(*optarg) setenv("LIBHD_HDDB_DIR", optarg, 1);
          break;

        case 317:
          break;

        case 318:
          return get_mapping2();
          break;

        case 319:
          if(*optarg) setenv("LIBHD_HDDB_DIR_NEW", optarg, 1);
          break;

        case 400:
          printf("%s\n", hd_version());
	  break;

        case 1000 ... 1100:
          if(hw_items < sizeof hw_item / sizeof *hw_item - 1) {
            hw_item[hw_items++] = i - 1000;
          }
          break;

        case 2000 ... 2005:
          if(hw_items < sizeof hw_item / sizeof *hw_item - 1) {
            hw_item[hw_items++] = i;
          }
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
        if(hd_data->debug == -1) {
          fprintf(f ? f : stdout,
            "============ start debug info ============\n%s=========== end debug info ============\n",
            hd_data->log
          );
        }
        if(hd) {
          hd_dump_entry(hd_data, hd, f ? f : stdout);
          hd = hd_free_hd_list(hd);
        }
        else {
          fprintf(f ? f : stdout, "No config data: %s\n", showconfig);
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


/*
 * hw_item might be either a 'real' hd_hw_item_t or some number >= 2000 used
 * for some special probe runs.
 */
void do_hw(hd_data_t *hd_data, FILE *f, int hw_item)
{
  hd_t *hd, *hd0;
  int smp = -1, uml = 0, xen = 0, i;
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
      switch(hw_item) {
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

    case 2005:
      xen = hd_is_xen(hd_data);
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

    i = hd_data->debug;
    hd_data->debug = -1;
    for(hd = hd_data->hd; hd; hd = hd->next) {
      hd_dump_entry(hd_data, hd, f);
    }
    hd_data->debug = i;

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
      case arch_aarch64:
        s = "AArch64";
        break;
      case arch_riscv:
        s = "RISC-V";
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
      case boot_uboot:
        t = "uboot";
        break;
    }

    fprintf(f ? f : stdout, "Arch: %s/%s\n", s, t);
  }
  else if(hw_item == 2004) {
    fprintf(f ? f : stdout, "UML: %s\n", uml ? "yes" : "no");
  }
  else if(hw_item == 2005) {
    fprintf(f ? f : stdout, "Xen: %s\n", xen ? "yes" : "no");
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
      do_saveconfig(hd_data, hd0, f ? f : stdout);
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
  int i;

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

    i = hd_data->debug;
    hd_data->debug = -1;
    for(hd = hd_data->hd; hd; hd = hd->next) {
      hd_dump_entry(hd_data, hd, f);
    }
    hd_data->debug = i;

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
    do_saveconfig(hd_data, hd0, f ? f : stdout);
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


void do_test(hd_data_t *hd_data)
{
  hd_t *hd;
  char buf[10];
  hd_hw_item_t hw_items[] = {
    hw_sound, 0
  };

  for(hd = hd_list2(hd_data, hw_items, 1); hd; hd = hd->next) {
    do {
      *buf = 0;
      fgets(buf, sizeof buf, stdin);
      hd_add_driver_data(hd_data, hd);
      hd_dump_entry(hd_data, hd, stderr);
    } while(*buf == '\n');
  }
}


void help()
{
  fprintf(stderr,
    "Usage: hwinfo [OPTIONS]\n"
    "Probe for hardware.\n"
    "Options:\n"
    "    --<HARDWARE_ITEM>\n"
    "        This option can be given more than once. Probe for a particular\n"
    "        HARDWARE_ITEM. Available hardware items are:\n"
    "        all, arch, bios, block, bluetooth, braille, bridge, camera,\n"
    "        cdrom, chipcard, cpu, disk, dsl, dvb, fingerprint, floppy,\n"
    "        framebuffer, gfxcard, hub, ide, isapnp, isdn, joystick, keyboard,\n"
    "        memory, mmc-ctrl, modem, monitor, mouse, netcard, network, partition,\n"
    "        pci, pcmcia, pcmcia-ctrl, pppoe, printer, redasd,\n"
    "        reallyall, scanner, scsi, smp, sound, storage-ctrl, sys, tape,\n"
    "        tv, uml, usb, usb-ctrl, vbe, wlan, xen, zip\n"
    "    --short\n"
    "        Show only a summary. Use this option in addition to a hardware\n"
    "        probing option.\n"
    "    --listmd\n"
    "        Normally hwinfo does not report RAID devices. Add this option to\n"
    "        see them.\n"
    "    --only DEVNAME\n"
    "        This option can be given more than once. If you add this option\n"
    "        only entries in the device list matching DEVNAME will be shown.\n"
    "        Note that you also have to specify --<HARDWARE_ITEM> to trigger\n"
    "        any device probing.\n"
    "    --save-config SPEC\n"
    "        Store config  for a particular device below /var/lib/hardware.\n"
    "        SPEC can be a device name, an UDI, or 'all'. This option must be\n"
    "        given in addition to a hardware probing option.\n"
    "    --show-config UDI\n"
    "        Show saved config data for a particular device.\n"
    "    --map\n"
    "        If disk names have  changed (e.g. after a kernel update) this\n"
    "        prints a list of disk name mappings. Note  that  you must have\n"
    "        used --save-config at some point before for this can work.\n"
    "    --debug N\n"
    "        Set debug level to N. The debug info is shown only in the log\n"
    "        file. If you specify a log file, the debug level is implicitly\n"
    "        set to a reasonable value (N is a bitmask of individual flags).\n"
    "    --verbose\n"
    "        Increase verbosity. Only together with --map.\n"
    "    --log FILE\n"
    "        Write log info to FILE.\n"
    "        Don't forget to also specify --<HARDWARE_ITEM> to trigger any\n"
    "        device probing.\n"
    "    --dump-db N\n"
    "        Dump hardware data base. N is either 0 for the external data\n"
    "        base in /var/lib/hardware, or 1 for the internal data base.\n"
    "    --version\n"
    "        Print libhd version.\n"
    "    --help\n"
    "        Print usage.\n"
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

  sl = hddb_get_packages(hd_data);

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
    if(!strcmp(sl->str, "sdio")) { tag = TAG_SDIO; continue; }

    if(sscanf(sl->str, "class=%i%n", &u, &cnt) >= 1 && !sl->str[cnt]) {
      hd->base_class.id = u >> 16;
      hd->sub_class.id = (u >> 8) & 0xff;
      hd->prog_if.id = u & 0xff;
      continue;
    }

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


#if 0

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

#endif


void write_udi(hd_data_t *hd_data, char *udi)
{
  hal_prop_t prop = {};
  int i;

  prop.type = p_string;
  prop.key = "foo.bar";
  prop.val.str = "test XXX";

  i = hd_write_properties(udi, &prop);

  fprintf(stderr, "write = %d\n", i);
}



void do_saveconfig(hd_data_t *hd_data, hd_t *hd, FILE *f)
{
#ifndef LIBHD_TINY
  int i;

  if(!saveconfig) return;

  fprintf(f, "\nSave Configuration:\n");
  for(; hd; hd = hd->next) {
    if(
      !strcmp(saveconfig, "all") ||
      (hd->udi && !strcmp(hd->udi, saveconfig)) ||
      (hd->unique_id && !strcmp(hd->unique_id, saveconfig)) ||
      (hd->unix_dev_name && !strcmp(hd->unix_dev_name, saveconfig)) ||
      (hd->unix_dev_name2 && !strcmp(hd->unix_dev_name2, saveconfig)) ||
      search_str_list(hd->unix_dev_names, saveconfig)
    ) {
      i = hd_write_config(hd_data, hd);
      fprintf(f, "  %s: %s\n",
        hd->udi ?: hd->unique_id ?: saveconfig,
        i ? "failed" : "ok"
      );
    }
  }
#endif
}


int map_cmp(const void *p0, const void *p1)
{
  const map_t *m0, *m1;

  m0 = p0;
  m1 = p1;

  if(!m0->dev && !m1->dev) return 0;
  if(!m0->dev && m1->dev) return 1;
  if(m0->dev && !m1->dev) return -1;

  return strcmp(m0->dev, m1->dev);
}


unsigned map_fill(map_t *map, hd_data_t *hd_data, hd_t *hd_manual)
{
  hd_t *hd, *hd_ctrl;
  hd_hw_item_t type;
  hd_res_t *res;
  unsigned map_len = 0;
  int i, j;

  if(!map) return 0;

  for(hd = hd_manual; hd; hd = hd->next) {
    type = hw_none;
    if(hd_is_hw_class(hd, hw_cdrom)) type = hw_cdrom;
    if(hd_is_hw_class(hd, hw_disk)) type = hw_disk;

    if(type == hw_none || !hd->unix_dev_name) continue;

    if(hd->status.available_orig == status_no) continue;

    hd_ctrl = hd_get_device_by_idx(hd_data, hd->attached_to);
    map[map_len].type = type;
    map[map_len].dev = hd->unix_dev_name;
    map[map_len].id = hd->unique_id;
    if(hd_ctrl) map[map_len].p_id = hd_ctrl->unique_id;
    if(hd->serial && *hd->serial) map[map_len].serial = hd->serial;
    if(hd->model) map[map_len].model = hd->model;

    for(res = hd->res; res; res = res->next) {
      if(
        res->any.type == res_size &&
        res->size.unit == size_unit_sectors
      ) {
        map[map_len].size = res->size.val1;
        break;
      }
    }

    map_len++;
  }

  if(map_len) qsort(map, map_len, sizeof *map, map_cmp);

  /* check whether model, serial and size are unique */

  for(i = 0; i < map_len; i++) {
    if(map[i].model) {
      map[i].model_ok = 1;
      for(j = 0; j < map_len; j++) {
        if(j != i && map[j].model && !strcmp(map[i].model, map[j].model)) {
          map[i].model_ok = 0;
          break;
        }
      }
    }

    if(map[i].serial) {
      map[i].serial_ok = 1;
      for(j = 0; j < map_len; j++) {
        if(j != i && map[j].serial && !strcmp(map[i].serial, map[j].serial)) {
          map[i].serial_ok = 0;
          break;
        }
      }
    }

    if(map[i].size) {
      map[i].size_ok = 1;
      for(j = 0; j < map_len; j++) {
        if(j != i && map[i].size == map[j].size) {
          map[i].size_ok = 0;
          break;
        }
      }
    }
  }

  return map_len;
}


void map_dump(map_t *map, unsigned map_len)
{
  int i;

  for(i = 0; i < map_len; i++) {
    fprintf(stderr,
      "%s: %s = %s\n\t%smodel \"%s\", %sserial \"%s\"\n\t%ssize %"PRIu64" sectors\n\t%s @ %s\n\n",
      map[i].type == hw_disk ? " disk" : "cdrom",
      map[i].dev, map[i].dev_old,
      map[i].model_ok ? "*" : "",
      map[i].model,
      map[i].serial_ok ? "*" : "",
      map[i].serial,
      map[i].size_ok ? "*" : "",
      map[i].size,
      map[i].id, map[i].p_id
    );
  }
}


int get_mapping2()
{
  hd_data_t *hd_data, *hd_data_new;
  hd_t *hd_manual, *hd;
  hd_hw_item_t hw_items[] = { hw_disk, hw_storage_ctrl, 0 };
  map_t *map, *map_old;
  unsigned cnt, map_len, map_old_len;
  int err = 0, i, j;
  char *s;

  hd_data = calloc(1, sizeof *hd_data);
  hd_data->flags.list_all = 1;

  hd_data_new = calloc(1, sizeof *hd_data_new);
  hd_data_new->flags.list_all = 1;
  hd_data_new->debug = -1;

  /* first, old data */

  hd_list(hd_data, hw_manual, 1, NULL);
  hd_manual = hd_list2(hd_data, hw_items, 0);

  for(cnt = 0, hd = hd_manual; hd; hd = hd->next) cnt++;
  map_old = cnt ? calloc(cnt, sizeof *map_old) : NULL;
  map_old_len = map_fill(map_old, hd_data, hd_manual);

  /* now, new data */

  s = getenv("LIBHD_HDDB_DIR_NEW");
  if(s) {
    setenv("LIBHD_HDDB_DIR", s, 1);

    hd_list(hd_data_new, hw_manual, 1, NULL);
    hd_manual = hd_list2(hd_data_new, hw_items, 0);
  }
  else {
    hd_data_new->flags.list_all = 0;
    hd_manual = hd_list2(hd_data_new, hw_items, 1);
  }

  for(cnt = 0, hd = hd_manual; hd; hd = hd->next) cnt++;
  map = cnt ? calloc(cnt, sizeof *map) : NULL;
  map_len = map_fill(map, hd_data_new, hd_manual);

  if(map_len) {

    /* try based on serial... */
    for(i = 0; i < map_len; i++) {
      if(map[i].assigned || !map[i].serial_ok) continue;
      for(j = 0; j < map_old_len; j++) {
        if(map_old[j].assigned || !map_old[j].serial_ok) continue;
        if(!strcmp(map[i].serial, map_old[j].serial)) {
          map[i].dev_old = map_old[j].dev;
          map[i].assigned = map_old[j].assigned = 1;
        }
      }
    }

    /* ... then based on model... */
    for(i = 0; i < map_len; i++) {
      if(map[i].assigned || !map[i].model_ok) continue;
      for(j = 0; j < map_old_len; j++) {
        if(map_old[j].assigned || !map_old[j].model_ok) continue;
        if(!strcmp(map[i].model, map_old[j].model)) {
          map[i].dev_old = map_old[j].dev;
          map[i].assigned = map_old[j].assigned = 1;
        }
      }
    }

    /* ... and finally based on disk size */
    for(i = 0; i < map_len; i++) {
      if(map[i].assigned || !map[i].size_ok) continue;
      for(j = 0; j < map_old_len; j++) {
        if(map_old[j].assigned || !map_old[j].size_ok) continue;
        if(map[i].size == map_old[j].size) {
          map[i].dev_old = map_old[j].dev;
          map[i].assigned = map_old[j].assigned = 1;
        }
      }
    }

    if(opt.verbose) {
      map_dump(map_old, map_old_len);
      fprintf(stderr, "- - - - - - - - - - - - - - - - - - - -\n");
      map_dump(map, map_len);
    }

    for(i = 0; i < map_len; i++) {
      if(map[i].dev_old && strcmp(map[i].dev, map[i].dev_old)) {
        printf("%s\t%s\n", map[i].dev, map[i].dev_old);
      }
    }

  }

#if 0

  // based on controller

  for(hd = hd_manual; hd; hd = hd->next) {
    type = hw_none;
    if(hd_is_hw_class(hd, hw_cdrom)) type = hw_cdrom;
    if(hd_is_hw_class(hd, hw_disk)) type = hw_disk;

    if(type == hw_none || !hd->unix_dev_name) continue;

    hd_ctrl = hd_get_device_by_idx(hd_data_new, hd->attached_to);

    if(hd_ctrl) {
      for(i = 0; i < map_len; i++) {
        if(
          map[i].type == type &&
          !map[i].dev_new &&
          map[i].p_id &&
          !strcmp(map[i].p_id, hd_ctrl->unique_id)
        ) {
          map[i].dev_new = hd->unix_dev_name;
          break;
        }
      }
      if(i == map_len) unassigned++;
    }
  }
#endif

  free(map);
  free(map_old);

  hd_free_hd_data(hd_data_new);
  free(hd_data_new);

  hd_free_hd_data(hd_data);
  free(hd_data);

  return err;
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"

static int get_probe_flags(int, char **, hd_data_t *);
static void progress(char *, char *);

// ##### temporary solution, fix it later!
str_list_t *read_file(char *file_name, unsigned start_line, unsigned lines);

static unsigned deb = 0;
static char *log_file = "";
static char *list = NULL;
static int listplus = 0;

static int test = 0;

static int braille_install_info(hd_data_t *hd_data);
static int x11_install_info(hd_data_t *hd_data);

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
  hd_data->debug=~(HD_DEB_DRIVER_INFO | HD_DEB_HDDB);

  if(argc == 1 && !strcmp(*argv, "--special=braille")) {
    return braille_install_info(hd_data);
  }

  if(argc == 1 && !strcmp(*argv, "--special=x11")) {
    return x11_install_info(hd_data);
  }

  if(argc == 1 && !strcmp(*argv, "--test")) {
    hd_t *hd;

    hd_data = calloc(1, sizeof *hd_data);
    hd_data->debug = -1;
    hd_set_probe_feature(hd_data, pr_pci);
    hd_set_probe_feature(hd_data, pr_isapnp);
    hd_set_probe_feature(hd_data, pr_misc);
    hd_scan(hd_data);

    hd_cpu_arch(hd_data);
    hd_free_hd_data(hd_data);

    fprintf(stderr, "1\n");

    hd_data->debug = -1;
    hd_cpu_arch(hd_data);
    hd_free_hd_data(hd_data);
        
    fprintf(stderr, "2\n");

    hd_data->debug = -1;
    hd = hd_list(hd_data, hw_network_ctrl, 1, NULL);
    hd_free_hd_list(hd);
    hd_free_hd_data(hd_data);
    hd_free_hd_data(hd_data);

    fprintf(stderr, "3\n");

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
    printf("\r%64s\r", "");

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
  FILE *f;
  char buf[256], *s, *t;
  int i, fb_mode = 0;

  if((f = fopen("/proc/cmdline", "r"))) {
    if(fread(buf, 1, sizeof buf, f)) {
      t = buf;
      while((s = strsep(&t, " "))) {
        if(sscanf(s, "vga=%i", &i) == 1) fb_mode = i;
      }
    }
    fclose(f);
  }

  return fb_mode > 0x10 ? fb_mode : 0;
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
    if(fread(buf, 1, sizeof buf, f)) {
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

#ifdef __PPC__
  /* temporary hack due to XF4 problems */
  if(!c) c = '3';
#endif

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

  if(c == 0) c = '3';	/* default to XF 3, if nothing else is known  */

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/pci.h>

#include "hd.h"
#include "hd_int.h"
#include "hdx.h"
#include "util.h"

#define MOD_INFO_SEP		'|'
#define MOD_INFO_SEP_STR	"|"


static hd_res_t *get_res(hd_t *h, enum resource_types t, unsigned index);
static char *module_cmd(hd_t *, char *);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * various functions not *directly* related to hardware probing
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


/*
 * Scan the hardware list for CD-ROMs with a given volume_id. Start
 * searching at the start'th entry.
 *
 * Returns a pointer to a hardware entry (hw_t *) or NULL on failure.
 */

// ####### replace or fix this!!!

#if 0
hw_t *find_cdrom_volume(const char *volume_id, int *start)
{
  int i;
  cdrom_info_t *ci;
  hw_t *h;

  for(i = *start; i < hw_len; i++) {
    h = hw + i;
    if(
      h->base_class == bc_storage_device &&
      h->sub_class == sc_sdev_cdrom /* &&
      (h->ext_flags & (1 << cdrom_flag_media_probed)) */
    ) {
      /* ok, found a CDROM device... */
      ci = h->ext;
      /* ... now compare the volume id */
      if(
        ci &&
        (!volume_id || !strncmp(ci->volume, volume_id, strlen(volume_id)))
      ) {
        *start = i + 1;
        return h;
      }
    }
  }

  return NULL;	/* CD not found :-( */
}
#endif


/*
 * Reads the driver info.
 *
 * If the driver is a module, checks if the module is already loaded.
 *
 * If the command line returned is the empty string (""), we could not
 * figure out what to do.
 */
driver_info_t *get_driver_info(hd_t *h)
{
  char *s = NULL, *s1, *t, *s0 = NULL;
  char cmd[256], *cmd_ptr;
  driver_info_t *mi = new_mem(sizeof *mi);
  char *fields[32];
  int i;
  unsigned u1, u2;

  if(h->sub_vend || h->sub_dev) {
    s = sub_device_drv_name(h->vend, h->dev, h->sub_vend, h->sub_dev);
  }

  if(!s && (h->vend || h->dev)) {
    s = device_drv_name(h->vend, h->dev);
  }

  if(!s) return free_mem(mi);

  s0 = new_str(s);
  s = s0;

  /* ok, there is a module entry */
  *(cmd_ptr = cmd) = 0;

  t = "";
  if(*s && s[1] == MOD_INFO_SEP) {
    switch(*s) {
      case 'i':
        t = "insmod";
        mi->type = di_module;
        break;		/* insmod */

      case 'M':		/* conf.modules entry */
        mi->module.autoload = 1;
      case 'm':
        s1 = s + 2;
        if(strsep(&s1, MOD_INFO_SEP_STR) && s1 && *s1) {
          i = 0; t = s1;
          while(t[i]) {
            if(t[i] == '\\') {
              switch(t[i + 1]) {
                case 'n':
                  *t = '\n'; i++;
                  break;

                case 't':
                  *t = '\t'; i++;
                  break;

                case '\\':
                  *t = '\\'; i++;
                  break;

                default:
                  *t = t[i];
              }
            }
            else {
              *t = t[i];
            }
            t++;
          }
          *t = 0;
          mi->module.conf = new_str(s1);
        }
        t = "modprobe";
        mi->type = di_module;
        break;

      case 'p':		/* for mouse driver info */
        mi->type = di_mouse;
        break;

      case 'x':		/* for X servers */
        mi->type = di_x11;
        break;

      case 'd':		/* for displays */
        mi->type = di_display;
        break;

      default:
        s0 = free_mem(s0);
        return free_mem(mi);
    }
    s += s[1] == MOD_INFO_SEP ? 2 : 1;
  }
  else {
    s0 = free_mem(s0);
    return free_mem(mi);
  }

  memset(fields, 0, sizeof fields);
  // split the fields
  for(i = 1, *fields = s1 = s; i < sizeof fields / sizeof *fields - 1; i++) {
    if(strsep(&s1, MOD_INFO_SEP_STR) && s1 && *s1) {
      fields[i] = s1;
    }
    else {
      break;
    }
  }

  if(mi->type == di_module) {
    // ##### s1 may be NULL !!!!   #####
    snprintf(cmd, sizeof cmd, "%s %s", t, s1 = module_cmd(h, s));
    if(s1) mi->module.load_cmd = new_str(cmd);
    s1 = s; strsep(&s1, " \t");
    mi->module.name = new_str(s);
    mi->module.is_active = module_is_active(mi->module.name);
  }

  if(mi->type == di_mouse) {
    if(fields[0] && *fields[0]) mi->mouse.xf86 = new_str(fields[0]);
    if(fields[1] && *fields[1]) mi->mouse.gpm = new_str(fields[1]);
  }

  if(mi->type == di_x11) {
    if(fields[0] && *fields[0]) mi->x11.server = new_str(fields[0]);
    if(fields[1] && *fields[1]) mi->x11.x3d = new_str(fields[1]);

    if(fields[2] && *fields[2]) {
      mi->x11.colors.all = strtol(fields[2], NULL, 16);
      if(mi->x11.colors.all & (1 << 0)) mi->x11.colors.c8 = 1;
      if(mi->x11.colors.all & (1 << 1)) mi->x11.colors.c15 = 1;
      if(mi->x11.colors.all & (1 << 2)) mi->x11.colors.c16 = 1;
      if(mi->x11.colors.all & (1 << 3)) mi->x11.colors.c24 = 1;
      if(mi->x11.colors.all & (1 << 4)) mi->x11.colors.c32 = 1;
    }
    if(fields[3] && *fields[3]) mi->x11.dacspeed = strtol(fields[3], NULL, 10);
  }

  if(mi->type == di_display) {
    if(fields[0] && *fields[0] && sscanf(fields[0], "%ux%u", &u1, &u2) == 2) {
      mi->display.width = u1;
      mi->display.height = u2;
    }

    if(fields[1] && *fields[1]  && sscanf(fields[1], "%u-%u", &u1, &u2) == 2) {
      mi->display.min_vsync = u1;
      mi->display.max_vsync = u2;
    }

    if(fields[2] && *fields[2]  && sscanf(fields[2], "%u-%u", &u1, &u2) == 2) {
      mi->display.min_hsync = u1;
      mi->display.max_hsync = u2;
    }

    if(fields[3] && *fields[3]  && sscanf(fields[3], "%u", &u1) == 1) {
      mi->display.bandwidth = u1;
    }
  }

  s0 = free_mem(s0);
  return mi;
}


int module_is_active(char *mod)
{
  FILE *f;
  char buf[256], *s;

  if(!(f = fopen(PROC_MODULES, "r"))) return 0;

  while(fgets(buf, sizeof buf, f)) {
    s = buf;
    strsep(&s, " \t");
    if(!strcmp(mod, buf)) {
      fclose(f);
      return 1;
    }
  }

  fclose(f);

  return 0;
}

hd_res_t *get_res(hd_t *hd, enum resource_types t, unsigned index)
{
  hd_res_t *res;

  for(res = hd->res; res; res = res->next) {
    if(res->any.type == t) {
      if(!index) return res;
      index--;
    }
  }

  return NULL;
}


char *module_cmd(hd_t *h, char *cmd)
{
  static char buf[256];
  char *s = buf;
  int idx, ofs;
  hd_res_t *res;

  // skip inactive PnP cards
  // ##### Really necessary here?
  if(
    h->detail &&
    h->detail->type == hd_detail_isapnp &&
    !(h->detail->isapnp.data->flags & (1 << isapnp_flag_act))
  ) return NULL;

  *buf = 0;
  while(*cmd) {
    if(sscanf(cmd, "<io%u>%n", &idx, &ofs) >= 1) {
      if((res = get_res(h, res_io, idx))) {
        s += sprintf(s, "0x%02"HD_LL"x", res->io.base);
        cmd += ofs;
      }
      else {
        return NULL;
      }
    }
    else if(sscanf(cmd, "<irq%u>%n", &idx, &ofs) >= 1) {
      if((res = get_res(h, res_irq, idx))) {
        s += sprintf(s, "%u", res->irq.base);
        cmd += ofs;
      }
      else {
        return NULL;
      }
    }
    else {
      *s++ = *cmd++;
    }

    if(s - buf > sizeof buf - 20) return NULL;
  }

  *s = 0;
  return buf;
}


#if 0
/*
 * cf. /usr/src/linux/drivers/block/ide-pci.c
 */
int needs_eide_kernel()
{
  int i, j;
  hw_t *h;
  static unsigned ids[][2] = {
    { PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C561 },
    { PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_1 },
    { PCI_VENDOR_ID_PROMISE, PCI_DEVICE_ID_PROMISE_20246 },
    { PCI_VENDOR_ID_PROMISE, 0x4d38 },		// PCI_DEVICE_ID_PROMISE_20262
    { PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_5513 },
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C621 },
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C558 },
    { PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C825 },
    { PCI_VENDOR_ID_TEKRAM, PCI_DEVICE_ID_TEKRAM_DC290 },
    { PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_87410 },
    { PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_87415 }
  };

  for(i = 0; i < hw_len; i++) {
    h = hw + i;
    if(h->bus == bus_pci) {
      for(j = 0; j < sizeof ids / sizeof *ids; j++) {
        if(h->vend == ids[j][0] && h->dev == ids[j][1]) return 1;
      }
    }
  }

  return 0;
}

/*
 * cf. pcmcia-cs-3.1.1:cardmgr/probe.c
 */
int has_pcmcia_support()
{
  int i, j;
  hw_t *h;
  static unsigned ids[][2] = {
    { 0x1013, 0x1100 },
    { 0x1013, 0x1110 },
    { 0x10b3, 0xb106 },
    { 0x1180, 0x0465 },
    { 0x1180, 0x0466 },
    { 0x1180, 0x0475 },
    { 0x1180, 0x0476 },
    { 0x1180, 0x0478 },
    { 0x104c, 0xac12 },
    { 0x104c, 0xac13 },
    { 0x104c, 0xac15 },
    { 0x104c, 0xac16 },
    { 0x104c, 0xac17 },
    { 0x104c, 0xac19 },
    { 0x104c, 0xac1a },
    { 0x104c, 0xac1d },
    { 0x104c, 0xac1f },
    { 0x104c, 0xac1b },
    { 0x104c, 0xac1c },
    { 0x104c, 0xac1e },
    { 0x104c, 0xac51 },
    { 0x1217, 0x6729 },
    { 0x1217, 0x673a },
    { 0x1217, 0x6832 },
    { 0x1217, 0x6836 },
    { 0x1179, 0x0603 },
    { 0x1179, 0x060a },
    { 0x1179, 0x060f },
    { 0x119b, 0x1221 },
    { 0x8086, 0x1221 }
  };

  for(i = 0; i < hw_len; i++) {
    h = hw + i;
    if(h->bus == bus_pci) {
      for(j = 0; j < sizeof ids / sizeof *ids; j++) {
        if(h->vend == ids[j][0] && h->dev == ids[j][1]) return 1;
      }
    }
  }

  return 0;
}
#endif



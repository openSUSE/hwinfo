#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "prom.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * prom info
 *
 * Note: make sure that hd_scan_pci() has been run!
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__PPC__)

static devtree_t *add_devtree_entry(devtree_t **devtree, devtree_t *new);
static devtree_t *new_devtree_entry(devtree_t *parent);
static void read_str(char *path, char *name, char **str);
static void read_mem(char *path, char *name, unsigned char **mem, unsigned len);
static void read_int(char *path, char *name, int *val);
static void read_devtree(hd_data_t *hd_data);
static void add_pci_prom_devices(hd_data_t *hd_data, hd_t *hd_parent, devtree_t *parent);
static void add_legacy_prom_devices(hd_data_t *hd_data, devtree_t *dt);
static void add_devices(hd_data_t *hd_data);
static void dump_devtree_data(hd_data_t *hd_data);


int detect_smp(hd_data_t *hd_data)
{
  unsigned cpus;
  devtree_t *devtree;

  if(!(devtree = hd_data->devtree)) return -1;	/* hd_scan_prom() not called */

  for(cpus = 0; devtree; devtree = devtree->next) {
    if(devtree->device_type && !strcmp(devtree->device_type, "cpu")) cpus++;
  }

  return cpus > 1 ? cpus : 0;
}

void hd_scan_prom(hd_data_t *hd_data)
{
  hd_t *hd;
  unsigned char buf[16];
  FILE *f;
  prom_info_t *pt;

  if(!hd_probe_feature(hd_data, pr_prom)) return;

  hd_data->module = mod_prom;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->devtree = free_devtree(hd_data);

  PROGRESS(1, 0, "devtree");

  read_devtree(hd_data);
  if(hd_data->debug) dump_devtree_data(hd_data);
  add_devices(hd_data);

  PROGRESS(2, 0, "color");

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class = bc_internal;
  hd->sub_class = sc_int_prom;
  hd->detail = new_mem(sizeof *hd->detail);
  hd->detail->type = hd_detail_prom;
  hd->detail->prom.data = pt = new_mem(sizeof *pt);

  if((f = fopen(PROC_PROM "/color-code", "r"))) {
    if(fread(buf, 1, 2, f) == 2) {
      pt->has_color = 1;
      pt->color = buf[1];
      hd_data->color_code = pt->color | 0x10000;
      ADD2LOG("color-code: 0x%04x\n", (buf[0] << 8) + buf[1]);
    }

    fclose(f);
  }

}

/* store a device tree entry */
devtree_t *add_devtree_entry(devtree_t **devtree, devtree_t *new)
{
  while(*devtree) devtree = &(*devtree)->next;
  return *devtree = new;
}

/* create a new device tree entry */
devtree_t *new_devtree_entry(devtree_t *parent)
{
  static unsigned idx = 0;
  devtree_t *devtree = new_mem(sizeof *devtree);

  if(!parent) idx = 0;
  devtree->idx = ++idx;
  devtree->parent = parent;

  devtree->interrupt = devtree->class_code =
  devtree->device_id = devtree->vendor_id =
  devtree->subdevice_id = devtree->subvendor_id =
  devtree->revision_id = -1;

  return devtree;
}

void read_str(char *path, char *name, char **str)
{
  char *s = NULL;
  str_list_t *sl;

  str_printf(&s, 0, "%s/%s", path, name);
  if((sl = read_file(s, 0, 1))) {
    *str = sl->str;
    sl->str = NULL;
    sl = free_str_list(sl);
  }
  free_mem(s);
}

void read_mem(char *path, char *name, unsigned char **mem, unsigned len)
{
  FILE *f;
  char *s = NULL;
  unsigned char *m = new_mem(len);

  str_printf(&s, 0, "%s/%s", path, name);
  if((f = fopen(s, "r"))) {
    if(fread(m, len, 1, f) == 1) {
      *mem = m;
      m = NULL;
    }
    fclose(f);
  }
  free_mem(s);
  free_mem(m);
}

void read_int(char *path, char *name, int *val)
{
  int *i = NULL;

  read_mem(path, name, (unsigned char **) &i, sizeof *i);
  if(i) *val = *i;
  free_mem(i);
}

void read_devtree_entry(hd_data_t *hd_data, devtree_t *parent, char *dirname)
{
  DIR *dir;
  struct dirent *de;
  struct stat sbuf;
  char *path, *s;
  devtree_t *devtree, *dt2;

  devtree = add_devtree_entry(&hd_data->devtree, new_devtree_entry(parent));

  devtree->filename = new_str(dirname);

  str_printf(&devtree->path, 0, "%s%s%s",
    parent ? parent->path : "", parent && *parent->path ? "/" : "", dirname
  );

  path = 0;
  str_printf(&path, 0, PROC_PROM "/%s", devtree->path);

  if((dir = opendir(path))) {
    while((de = readdir(dir))) {
      if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
      s = NULL;
      str_printf(&s, 0, "%s/%s", path, de->d_name);
      if(!lstat(s, &sbuf)) {
        if(S_ISDIR(sbuf.st_mode)) {
          /* prom entries don't always have unique names, unfortunately... */
          for(dt2 = hd_data->devtree; dt2; dt2 = dt2->next) {
            if(
              dt2->parent == devtree &&
              !strcmp(dt2->filename, de->d_name)
            ) break;
          }
          if(!dt2) read_devtree_entry(hd_data, devtree, de->d_name);
        }
      }
      s = free_mem(s);
    }
    closedir(dir);
  }

  read_str(path, "name", &devtree->name);
  read_str(path, "model", &devtree->model);
  read_str(path, "device_type", &devtree->device_type);
  read_str(path, "compatible", &devtree->compatible);

  read_int(path, "interrupts", &devtree->interrupt);
  read_int(path, "AAPL,interrupts", &devtree->interrupt);
  read_int(path, "class-code", &devtree->class_code);
  read_int(path, "vendor-id", &devtree->vendor_id);
  read_int(path, "device-id", &devtree->device_id);
  read_int(path, "subsystem-vendor-id", &devtree->subvendor_id);
  read_int(path, "subsystem-id", &devtree->subdevice_id);
  read_int(path, "revision-id", &devtree->revision_id);

  read_mem(path, "EDID", &devtree->edid, 0x80);

  if(
    devtree->class_code != -1 && devtree->vendor_id != -1 &&
    devtree->device_id != -1
  ) {
    devtree->pci = 1;
  }

  path = free_mem(path);
}

void read_devtree(hd_data_t *hd_data)
{
  read_devtree_entry(hd_data, NULL, "");

}

void add_pci_prom_devices(hd_data_t *hd_data, hd_t *hd_parent, devtree_t *parent)
{
  hd_t *hd;
  hd_res_t *res;
  devtree_t *dt, *dt2;
  int irq;
  unsigned sound_ok = 0, net_ok = 0, scsi_ok = 0;
  unsigned id;
  char *s;

  for(dt = hd_data->devtree; dt; dt = dt->next) {
    if(
      dt->parent == parent ||
      (
        /* special magic to reach some sound chips */
        dt->parent &&
        dt->parent->parent == parent &&
        !dt->parent->pci
      )
    ) {

      if(
        dt->device_type &&
        (!strcmp(dt->device_type, "block") || !strcmp(dt->device_type, "swim3"))
      ) {
        /* block devices */
        // ###### fix: add floppy disk???

        s = dt->compatible ? dt->compatible : dt->name;
        id = 0;

        if(s) {
          if(strstr(s, "swim3")) {
            id = MAKE_ID(TAG_SPECIAL, 0x0040);
          }
        }

        if(id) {
          hd = add_hd_entry(hd_data, __LINE__, 0);
          hd->bus = bus_none;
          hd->base_class = bc_storage;
          hd->sub_class = sc_sto_floppy;

          hd->vend = MAKE_ID(TAG_SPECIAL, 0x0401);
          hd->dev = id;
          hd->attached_to = hd_parent->idx;
          hd->rom_id = new_str(dt->path);
          if(dt->interrupt) {
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->irq.type = res_irq;
            res->irq.enabled = 1;
            res->irq.base = dt->interrupt;
          }
        }
      }

      if(
        !scsi_ok &&
        dt->device_type &&
        !strcmp(dt->device_type, "scsi")
      ) {
        /* scsi */
        scsi_ok = 1;	/* max. 1 controller */

        s = dt->compatible ? dt->compatible : dt->name;
        id = 0;

        if(s) {
          if(strstr(s, "mesh")) {	/* mesh || chrp,mesh0 */
            id = MAKE_ID(TAG_SPECIAL, 0x0030);
          }
          else if(!strcmp(s, "53c94")) {
            id = MAKE_ID(TAG_SPECIAL, 0x0031);
          }
        }

        if(id) {
          hd = add_hd_entry(hd_data, __LINE__, 0);
          hd->bus = bus_none;
          hd->base_class = bc_storage;
          hd->sub_class = sc_sto_scsi;

          hd->vend = MAKE_ID(TAG_SPECIAL, 0x0401);
          hd->dev = id;
          hd->attached_to = hd_parent->idx;
          hd->rom_id = new_str(dt->path);
          if(dt->interrupt) {
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->irq.type = res_irq;
            res->irq.enabled = 1;
            res->irq.base = dt->interrupt;
          }
        }
      }

      if(
        !net_ok &&
        dt->device_type &&
        !strcmp(dt->device_type, "network")
      ) {
        /* network */
        net_ok = 1;	/* max. 1 controller */

        s = dt->compatible ? dt->compatible : dt->name;
        id = 0;

        if(s) {
          if(!strcmp(s, "mace")) {
            id = MAKE_ID(TAG_SPECIAL, 0x0020);
          }
          else if(!strcmp(s, "bmac")) {
            id = MAKE_ID(TAG_SPECIAL, 0x0021);
          }
          else if(!strcmp(s, "bmac+")) {
            id = MAKE_ID(TAG_SPECIAL, 0x0022);
          }
        }

        if(id) {
          hd = add_hd_entry(hd_data, __LINE__, 0);
          hd->bus = bus_none;
          hd->base_class = bc_network;
          hd->sub_class = 0;	/* ethernet */

          hd->vend = MAKE_ID(TAG_SPECIAL, 0x0401);
          hd->dev = id;
          hd->attached_to = hd_parent->idx;
          hd->rom_id = new_str(dt->path);
          if(dt->interrupt) {
            res = add_res_entry(&hd->res, new_mem(sizeof *res));
            res->irq.type = res_irq;
            res->irq.enabled = 1;
            res->irq.base = dt->interrupt;
          }
        }
      }

      if(
        !sound_ok &&
        dt->device_type &&
        strstr(dt->device_type, "sound") == dt->device_type
      ) {
        /* sound */
        sound_ok = 1;	/* max 1 controller */

        for(dt2 = dt; dt2; dt2 = dt2->next) {
          if(
            (
              dt2 == dt ||
              (dt2->parent && dt2->parent == dt)
            ) &&
            (
              !strcmp(dt2->device_type, "sound") ||
              !strcmp(dt2->device_type, "soundchip")
            )
          ) break;
        }
        if(!dt2) dt2 = dt;

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->bus = bus_none;
        hd->base_class = bc_multimedia;
        hd->sub_class = sc_multi_audio;
        hd->attached_to = hd_parent->idx;
        hd->rom_id = new_str(dt2->path);
        irq = dt2->interrupt;
        if(irq <= 1 && dt2->parent && !dt2->parent->pci) {
          irq = dt2->parent->interrupt;
        }
        if(irq > 1) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->irq.type = res_irq;
          res->irq.enabled = 1;
          res->irq.base = irq;
        }

        hd->vend = MAKE_ID(TAG_SPECIAL, 0x401);		/* Apple */
        hd->dev = MAKE_ID(TAG_SPECIAL, 0x0010);

        if(dt2->compatible) {
          if(!strcmp(dt2->compatible, "screamer")) {
            hd->dev = MAKE_ID(TAG_SPECIAL, 0x0011);
          }
          else if(!strcmp(dt2->compatible, "burgundy")) {
            hd->dev = MAKE_ID(TAG_SPECIAL, 0x0012);
          }
          else if(!strcmp(dt2->compatible, "daca")) {
            hd->dev = MAKE_ID(TAG_SPECIAL, 0x0013);
          }
          else if(!strcmp(dt2->compatible, "CRUS,CS4236B")) {
            hd->vend = MAKE_ID(TAG_SPECIAL, 0x402);	/* IBM */
            hd->dev = MAKE_ID(TAG_SPECIAL, 0x0014);
          }
        }
      }
    }
  }
}

void add_legacy_prom_devices(hd_data_t *hd_data, devtree_t *dt)
{
  hd_t *hd;
  hd_res_t *res;
  unsigned id;

  if(dt->pci) return;

  if(
    dt->device_type &&
    !strcmp(dt->device_type, "display")
  ) {
    /* display devices */

    id = 0;

    if(dt->name) {
      if(!strcmp(dt->name, "valkyrie")) {
        id = MAKE_ID(TAG_SPECIAL, 0x3000);
      }
      else if(!strcmp(dt->name, "platinum")) {
        id = MAKE_ID(TAG_SPECIAL, 0x3001);
      }
    }

    if(id) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus = bus_none;
      hd->base_class = bc_display;
      hd->sub_class = sc_dis_other;

      hd->vend = MAKE_ID(TAG_SPECIAL, 0x0401);
      hd->dev = id;
      hd->rom_id = new_str(dt->path);
      if(dt->interrupt) {
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->irq.type = res_irq;
        res->irq.enabled = 1;
        res->irq.base = dt->interrupt;
      }
    }
  }
}

void add_devices(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res;
  devtree_t *dt;
  unsigned pci_slot = 0, u;

  /* remove old assignments */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(ID_TAG(hd->dev) == TAG_PCI && ID_TAG(hd->vend) == TAG_PCI) {
      hd->rom_id = free_mem(hd->rom_id);
      hd->detail = free_hd_detail(hd->detail);
    }
  }

  for(dt = hd_data->devtree; dt; dt = dt->next) {
    if(dt->pci) {
      for(hd = hd_data->hd; hd; hd = hd->next) {
        if(
          /* do *not* compare class ids */
          /* It would be better to check the slot numbers instead but
           * as they are not stored within /proc/device-tree in a consistent
           * way, we can't do that.
           */
          !hd->rom_id &&
          ID_TAG(hd->dev) == TAG_PCI &&
          ID_TAG(hd->vend) == TAG_PCI &&
          ID_VALUE(hd->dev) == dt->device_id &&
          ID_VALUE(hd->vend) == dt->vendor_id &&
          (dt->subvendor_id == -1 || ID_VALUE(hd->sub_vend) == dt->subvendor_id) &&
          (dt->subdevice_id == -1 || ID_VALUE(hd->sub_dev) == dt->subdevice_id) &&
          hd->rev == dt->revision_id
        ) break;
      }

#if 0
/*
 * no longer needed, the kernel does it
 */
      if(!hd) {
        /* no appropriate pci entry; create one */

        hd = add_hd_entry(hd_data, __LINE__, 0);

        hd->bus = bus_pci;
        hd->slot = pci_slot++ + 0xff00;
        hd->base_class = (dt->class_code >> 16) & 0xff;
        hd->sub_class =  (dt->class_code >> 8) & 0xff;
        hd->prog_if = dt->class_code & 0xff;

        /* fix up old VGA's entries */
        if(hd->base_class == bc_none && hd->sub_class == 0x01) {
          hd->base_class = bc_display;
          hd->sub_class = sc_dis_vga;
        }

        hd->dev = MAKE_ID(TAG_PCI, dt->device_id);
        hd->vend = MAKE_ID(TAG_PCI, dt->vendor_id);
        if(dt->subdevice_id != -1) {
          hd->sub_dev = MAKE_ID(TAG_PCI, dt->subdevice_id);
        }
        if(dt->subvendor_id != -1) {
          hd->sub_vend = MAKE_ID(TAG_PCI, dt->subvendor_id);
        }
        hd->rev = dt->revision_id;

        if((hd->base_class == 0 || hd->base_class == 0xff) && hd->sub_class == 0) {
          if((u = device_class(hd_data, hd->vend, hd->dev))) {
            hd->base_class = u >> 8;
            hd->sub_class = u & 0xff;
          }
        } 

        if(dt->interrupt > 1) {
          res = add_res_entry(&hd->res, new_mem(sizeof *res));
          res->irq.type = res_irq;
          res->irq.enabled = 1;
          res->irq.base = dt->interrupt;
        }
      }
#endif

      if(hd) {
        hd->rom_id = new_str(dt->path);
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_devtree;
        hd->detail->devtree.data = dt;
        add_pci_prom_devices(hd_data, hd, dt);
      }
    }
    else {
      add_legacy_prom_devices(hd_data, dt);
    }
  }
}

void dump_devtree_data(hd_data_t *hd_data)
{
  unsigned u;
  devtree_t *devtree;

  devtree = hd_data->devtree;
  if(!devtree) return;

  ADD2LOG("----- /proc device tree -----\n");

  for(; devtree; devtree = devtree->next) {
    u = devtree->parent ? devtree->parent->idx : 0;
    ADD2LOG("  %02u @%02u  %s", devtree->idx, u, devtree->path);
    if(devtree->pci) ADD2LOG("  [pci]");
    ADD2LOG("\n");

    ADD2LOG(
      "    name \"%s\", model \"%s\", dtype \"%s\", compat \"%s\"\n",
      devtree->name ? devtree->name : "",
      devtree->model ? devtree->model : "",
      devtree->device_type ? devtree->device_type : "",
      devtree->compatible ? devtree->compatible : ""
    );

    if(
      devtree->class_code != -1 || devtree->vendor_id != -1 ||
      devtree->device_id != -1 || devtree->revision_id != -1 ||
      devtree->subdevice_id != -1 || devtree->subvendor_id != -1 ||
      devtree->interrupt != -1
    ) {
      ADD2LOG("  ");
      if(devtree->class_code != -1) ADD2LOG("  class 0x%06x", devtree->class_code);
      if(devtree->vendor_id != -1) ADD2LOG("  vend 0x%04x", devtree->vendor_id);
      if(devtree->device_id != -1) ADD2LOG("  dev 0x%04x", devtree->device_id);
      if(devtree->subvendor_id != -1) ADD2LOG("  svend 0x%04x", devtree->subvendor_id);
      if(devtree->subdevice_id != -1) ADD2LOG("  sdev 0x%04x", devtree->subdevice_id);
      if(devtree->revision_id != -1) ADD2LOG("  rev 0x%02x", devtree->revision_id);
      if(devtree->interrupt != -1) ADD2LOG("  irq %d", devtree->interrupt);
      ADD2LOG("\n");
    }

    if(devtree->edid) {
      ADD2LOG("    EDID record:\n");
      for(u = 0; u < 0x80; u += 0x10) {
        ADD2LOG("    %02x  ", u);
        hexdump(&hd_data->log, 1, 0x10, devtree->edid + u);
        ADD2LOG("\n");
      }
    }

    if(devtree->next) ADD2LOG("\n");
  }

  ADD2LOG("----- /proc device tree end -----\n");
}


#endif /* defined(__PPC__) */


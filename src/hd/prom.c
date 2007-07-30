#define _GNU_SOURCE
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

/**
 * @defgroup PROMint PROM information (PowerPC)
 * @ingroup libhdINFOint
 * @brief PowerPC PROM information
 *
 * Note: make sure that hd_scan_sysfs_pci() has been run!
 *
 * @{
 */

#if defined(__PPC__)

static devtree_t *add_devtree_entry(devtree_t **devtree, devtree_t *new);
static devtree_t *new_devtree_entry(devtree_t *parent);
static void read_str(char *path, char *name, char **str);
static void read_mem(char *path, char *name, unsigned char **mem, unsigned len);
static void read_int(char *path, char *name, int *val);
static void read_devtree(hd_data_t *hd_data);
static void add_legacy_prom_devices(hd_data_t *hd_data, devtree_t *dt);
static void add_prom_ehea(hd_data_t *hd_data, devtree_t *dt);
static void add_devices(hd_data_t *hd_data);
static void dump_devtree_data(hd_data_t *hd_data);

static unsigned veth_cnt, vscsi_cnt;
static unsigned snd_aoa_layout_id;

int detect_smp_prom(hd_data_t *hd_data)
{
  unsigned cpus;
  devtree_t *devtree;

  if(!(devtree = hd_data->devtree)) return -1;	/* hd_scan_prom() not called */

  for(cpus = 0; devtree; devtree = devtree->next) {
    if(devtree->device_type && !strcmp(devtree->device_type, "cpu")) cpus++;
  }

  return cpus > 1 ? cpus : 0;
}

static void prom_add_pmac_devices(hd_data_t *hd_data)
{
	hd_t *hd;
	hd = add_hd_entry(hd_data, __LINE__, 0);
	hd->bus.id = bus_none;
	hd->base_class.id = bc_multimedia;
	hd->sub_class.id = sc_multi_audio;
	hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x401);
	if (snd_aoa_layout_id)
		hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0015);
	else
		hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0010);

}
void hd_scan_prom(hd_data_t *hd_data)
{
  hd_t *hd;
  unsigned char buf[42];
  FILE *f;
  prom_info_t *pt;

  if(!hd_probe_feature(hd_data, pr_prom)) return;

  hd_data->module = mod_prom;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->devtree = free_devtree(hd_data);

  veth_cnt = vscsi_cnt = 0;

  PROGRESS(1, 0, "devtree");

  read_devtree(hd_data);
  if(hd_data->debug) dump_devtree_data(hd_data);
  add_devices(hd_data);

  PROGRESS(2, 0, "color");

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class.id = bc_internal;
  hd->sub_class.id = sc_int_prom;
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
  if((f = fopen(PROC_PROM "/compatible", "r"))) {
    if(fread(buf, 1, sizeof(buf), f) > 2) {
      if(memmem(buf, sizeof(buf),"MacRISC", 7))
	      prom_add_pmac_devices(hd_data);
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
  unsigned char *p = NULL;

  read_mem(path, name, &p, sizeof (int));
  if(p) memcpy(val, p, sizeof (int));
  free_mem(p);
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
      if(!strcmp(de->d_name, "layout-id"))
        snd_aoa_layout_id = 1;
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
  if(!devtree->edid) read_mem(path, "DFP,EDID", &devtree->edid, 0x80);
  if(!devtree->edid) read_mem(path, "LCD,EDID", &devtree->edid, 0x80);

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

void add_legacy_prom_devices(hd_data_t *hd_data, devtree_t *dt)
{
  if(dt->pci) return;

  add_prom_ehea(hd_data, dt);
}

void add_prom_ehea(hd_data_t *hd_data, devtree_t *dt)
{
  hd_t *hd;
  hd_res_t *res;
  char *path = NULL;
  unsigned char *hw_addr_bin = NULL;
  char *hw_addr = NULL;
  int slot;

  if(
    dt->device_type &&
    dt->compatible &&
    !strcmp(dt->device_type, "network") &&
    !strcmp(dt->compatible, "IBM,lhea-ethernet")
  ) {
    str_printf(&path, 0, PROC_PROM "/%s", dt->path);

    ADD2LOG("  ehea: %s\n", path);

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->bus.id = bus_none;
    hd->base_class.id = bc_network;
    hd->sub_class.id = 0;	/* ethernet */

    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);
    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1003);
    hd->rom_id = new_str(dt->path);

    read_int(path, "ibm,hea-port-no", &slot);
    hd->slot = slot;

    read_str(path, "ibm,fw-adapter-name", &hd->device.name);
    if(!hd->device.name) {
      hd->device.name = new_str("IBM Host Ethernet Adapter");
    }

    // "mac-address" or "local-mac-address" ?
    read_mem(path, "local-mac-address", &hw_addr_bin, 6);

    if(hw_addr_bin) {
      str_printf(
        &hw_addr, 0, "%02x:%02x:%02x:%02x:%02x:%02x",
        hw_addr_bin[0], hw_addr_bin[1],
        hw_addr_bin[2], hw_addr_bin[3],
        hw_addr_bin[4], hw_addr_bin[5]
      );
      res = new_mem(sizeof *res);
      res->hwaddr.type = res_hwaddr;
      res->hwaddr.addr = new_str(hw_addr);
      add_res_entry(&hd->res, res);
    }
  }

  free_mem(hw_addr_bin);
  free_mem(hw_addr);
  free_mem(path);
}

void add_devices(hd_data_t *hd_data)
{
  devtree_t *dt;

  for(dt = hd_data->devtree; dt; dt = dt->next) {
    if(!dt->pci) add_legacy_prom_devices(hd_data, dt);
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

/** @} */


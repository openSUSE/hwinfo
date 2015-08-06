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
static void dump_devtree_data(hd_data_t *hd_data);

static unsigned veth_cnt, vscsi_cnt;
static unsigned snd_aoa_layout_id;
static enum pmac_model model;
static devtree_t *devtree_edid;

static const struct pmac_mb_def pmac_mb[] = {
#ifndef __powerpc64__
	{ iMac_1,	"iMac,1" },	/* iMac (first generation) */
	{ AAPL_3400,	"AAPL,3400/2400" },	/* PowerBook 3400 */
	{ AAPL_3500,	"AAPL,3500" },	/* PowerBook 3500 */
	{ AAPL_7200,	"AAPL,7200" },	/* PowerMac 7200 */
	{ AAPL_7300,	"AAPL,7300" },	/* PowerMac 7200/7300 */
	{ AAPL_7500,	"AAPL,7500" },	/* PowerMac 7500 */
	{ AAPL_8500,	"AAPL,8500" },	/* PowerMac 8500/8600 */
	{ AAPL_9500,	"AAPL,9500" },	/* PowerMac 9500/9600 */
	{ AAPL_Gossamer,	"AAPL,Gossamer" },	/* PowerMac G3 (Gossamer) */
	{ AAPL_PowerBook1998,	"AAPL,PowerBook1998" },	/* PowerBook Wallstreet */
	{ AAPL_PowerMac_G3,	"AAPL,PowerMac G3" },	/* PowerMac G3 (Silk) */
	{ AAPL_ShinerESB,	"AAPL,ShinerESB" },	/* Apple Network Server */
	{ AAPL_e407,	"AAPL,e407" },	/* Alchemy */
	{ AAPL_e411,	"AAPL,e411" },	/* Gazelle */
	{ PowerBook1_1,	"PowerBook1,1" },	/* PowerBook 101 (Lombard) */
	{ PowerBook2_1,	"PowerBook2,1" },	/* iBook (first generation) */
	{ PowerBook2_2,	"PowerBook2,2" },	/* iBook FireWire */
	{ PowerBook3_1,	"PowerBook3,1" },	/* PowerBook Pismo */
	{ PowerBook3_2,	"PowerBook3,2" },	/* PowerBook Titanium */
	{ PowerBook3_3,	"PowerBook3,3" },	/* PowerBook Titanium II */
	{ PowerBook3_4,	"PowerBook3,4" },	/* PowerBook Titanium III */
	{ PowerBook3_5,	"PowerBook3,5" },	/* PowerBook Titanium IV */
	{ PowerBook4_1,	"PowerBook4,1" },	/* iBook 2 */
	{ PowerBook4_2,	"PowerBook4,2" },	/* iBook 2 */
	{ PowerBook4_3,	"PowerBook4,3" },	/* iBook 2 rev. 2 */
	{ PowerBook5_1,	"PowerBook5,1" },	/* PowerBook G4 17" */
	{ PowerBook5_2,	"PowerBook5,2" },	/* PowerBook G4 15" */
	{ PowerBook5_3,	"PowerBook5,3" },	/* PowerBook G4 17" */
	{ PowerBook5_4,	"PowerBook5,4" },	/* PowerBook G4 15" */
	{ PowerBook5_5,	"PowerBook5,5" },	/* PowerBook G4 17" */
	{ PowerBook5_6,	"PowerBook5,6" },	/* PowerBook G4 15" */
	{ PowerBook5_7,	"PowerBook5,7" },	/* PowerBook G4 17" */
	{ PowerBook5_8,	"PowerBook5,8" },	/* PowerBook G4 15" */
	{ PowerBook5_9,	"PowerBook5,9" },	/* PowerBook G4 17" */
	{ PowerBook6_1,	"PowerBook6,1" },	/* PowerBook G4 12" */
	{ PowerBook6_2,	"PowerBook6,2" },	/* PowerBook G4 */
	{ PowerBook6_3,	"PowerBook6,3" },	/* iBook G4 */
	{ PowerBook6_4,	"PowerBook6,4" },	/* PowerBook G4 12" */
	{ PowerBook6_5,	"PowerBook6,5" },	/* iBook G4 */
	{ PowerBook6_7,	"PowerBook6,7" },	/* iBook G4 */
	{ PowerBook6_8,	"PowerBook6,8" },	/* PowerBook G4 12" */
	{ PowerMac1_1,	"PowerMac1,1" },	/* Blue&White G3 */
	{ PowerMac1_2,	"PowerMac1,2" },	/* PowerMac G4 PCI Graphics */
	{ PowerMac2_1,	"PowerMac2,1" },	/* iMac FireWire */
	{ PowerMac2_2,	"PowerMac2,2" },	/* iMac FireWire */
	{ PowerMac3_1,	"PowerMac3,1" },	/* PowerMac G4 AGP Graphics */
	{ PowerMac3_2,	"PowerMac3,2" },	/* PowerMac G4 AGP Graphics */
	{ PowerMac3_3,	"PowerMac3,3" },	/* PowerMac G4 AGP Graphics */
	{ PowerMac3_4,	"PowerMac3,4" },	/* PowerMac G4 Silver */
	{ PowerMac3_5,	"PowerMac3,5" },	/* PowerMac G4 Silver */
	{ PowerMac3_6,	"PowerMac3,6" },	/* PowerMac G4 Windtunnel */
	{ PowerMac4_1,	"PowerMac4,1" },	/* iMac "Flower Power" */
	{ PowerMac4_2,	"PowerMac4,2" },	/* Flat panel iMac */
	{ PowerMac4_4,	"PowerMac4,4" },	/* eMac */
	{ PowerMac5_1,	"PowerMac5,1" },	/* PowerMac G4 Cube */
	{ PowerMac6_1,	"PowerMac6,1" },	/* Flat panel iMac */
	{ PowerMac6_3,	"PowerMac6,3" },	/* Flat panel iMac */
	{ PowerMac6_4,	"PowerMac6,4" },	/* eMac */
	{ RackMac1_1,	"RackMac1,1" },	/* XServe */
	{ RackMac1_2,	"RackMac1,2" },	/* XServe rev. 2 */
#endif /* __powerpc64__ */
	{ PowerMac7_2,	"PowerMac7,2" },	/* PowerMac G5 */
	{ PowerMac7_3,	"PowerMac7,3" },	/* PowerMac G5 */
	{ PowerMac8_1,	"PowerMac8,1" },	/* iMac G5 */
	{ PowerMac9_1,	"PowerMac9,1" },	/* PowerMac G5 */
	{ PowerMac10_1,	"PowerMac10,1" },	/* Mac mini */
	{ PowerMac11_2,	"PowerMac11,2" },	/* PowerMac G5 Dual Core */
	{ PowerMac12_1,	"PowerMac12,1" },	/* iMac G5 (iSight) */
	{ RackMac3_1,	"RackMac3,1" },	/* XServe G5 */
	{ }
};

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

#define HSYNCP (( 1 << 1 ))
#define HSYNCM (( 1 << 2 ))
#define VSYNCP (( 1 << 3 ))
#define VSYNCM (( 1 << 4 ))

struct modelist {
	unsigned short clock;
	unsigned short width, height;
	unsigned short hblank, hsync_ofs, hsync;
	unsigned short vblank, vsync_ofs, vsync;
	unsigned short flags;
	unsigned char e0x23, e0x24;
	unsigned char min_hsync, max_hsync;
	unsigned char min_vsync, max_vsync;
};

static const struct modelist mode_800x600_60 = {
	.clock = 39955 / 10,
	.width = 800,
	.height = 600,
	.hblank = 256,
	.hsync_ofs = 56,
	.hsync = 128,
	.vblank = 23,
	.vsync_ofs = 1,
	.vsync = 4,
	.e0x23 = (1 << 0),
};
static const struct modelist mode_1024x768_60 = {
	.clock = 65000 / 10,
	.width = 1024,
	.height = 768,
	.hblank = 320,
	.hsync_ofs = 40,
	.hsync = 136,
	.vblank = 29,
	.vsync_ofs = 3,
	.vsync = 6,
	.e0x24 = (1 << 3),
};
/* Modeline "1024x768-74" 80.71  1024 1080 1192 1360  768 769 772 802 +hsync +vsync */
static const struct modelist mode_1024x768_75 = {
	.clock = 80711 / 10,
	.width = 1024,
	.height = 768,
	.hblank = 336,
	.hsync_ofs = 56,
	.hsync = 112,
	.vblank = 34,
	.vsync_ofs = 1,
	.vsync = 3,
	.flags = VSYNCP | HSYNCP,
	.e0x24 = (1 << 1),
	.min_vsync = 74,
	.max_vsync = 116,
	.min_hsync = 59,
	.max_hsync = 60,
};
/* Modeline "800x600-94" 63.89  800 848 936 1072  600 601 604 634 +vsync +hsync */
static const struct modelist mode_800x600_94 = {
	.clock = 63890 / 10,
	.width = 800,
	.height = 600,
	.hblank = 272,
	.hsync_ofs = 48,
	.hsync = 88,
	.vblank = 34,
	.vsync_ofs = 1,
	.vsync = 3,
	.flags = VSYNCP | HSYNCP,
};
/* Modeline "640x480-116" 50.56  640 680 744 848  480 481 484 514 +vsync +hsync */
static const struct modelist mode_640x480_116 = {
	.clock = 50560 / 10,
	.width = 640,
	.height = 480,
	.hblank = 208,
	.hsync_ofs = 20,
	.hsync = 64,
	.vblank = 34,
	.vsync_ofs = 1,
	.vsync = 3,
	.flags = VSYNCP | HSYNCP,
};

static void prom_add_first_detailed_timing(unsigned char *e, unsigned short width_mm, unsigned short height_mm, const struct modelist *m)
{
	unsigned char i;
	i = 0x36;
	e[i + 0] = e[i + 1] = e[i + 2] = 0;
	e[i + 3] = 0xfd;
	e[i + 5] = m->min_vsync;
	e[i + 6] = m->max_vsync;
	e[i + 7] = m->min_hsync;
	e[i + 8] = m->max_hsync;
}
static void prom_add_detailed_timing(int num, unsigned char *e, unsigned short width_mm, unsigned short height_mm, const struct modelist *m)
{
	unsigned char i;
	i = 0x36 + (num * 0x12);
	e[i + 0] = (m->clock) & 0x00ff;
	e[i + 1] = ((m->clock) & 0xff00) >> 8;
	e[i + 2] = m->width & 0x00ff;
	e[i + 4] |= ((m->width & 0x0f00) >> 8) << 4;
	e[i + 5] = m->height & 0x00ff;
	e[i + 7] |= ((m->height & 0x0f00) >> 8) << 4;
	e[i + 12] |= width_mm & 0x00ff;
	e[i + 14] |= ((width_mm & 0x0f00) >> 8) << 4;
	e[i + 13] |= height_mm & 0x00ff;
	e[i + 14] |= (height_mm & 0x0f00) >> 8;
	e[i + 3] = m->hblank & 0x00ff;
	e[i + 4] |= (m->hblank & 0x0f01) >> 8;
	e[i + 8] = m->hsync_ofs & 0x00ff;
	e[i + 11] |= ((m->hsync_ofs & 0x0300) >> 8) << 2;
	e[i + 9] = m->hsync & 0x00ff;
	e[i + 11] |= (m->hsync & 0x0300) >> 8;
	e[i + 6] = m->vblank & 0x00ff;
	e[i + 7] |= (m->vblank & 0x0f00) >> 8;
	e[i + 10] |= (m->vsync_ofs & 0x000f) << 4;
	e[i + 11] |= ((m->vsync_ofs & 0x0030) >> 4) << 2;
	e[i + 10] |= m->vsync & 0x000f;
	e[i + 11] |= (m->vsync & 0x0030) >> 4;
	if (m->flags) {
		e[i + 17] |= 3 << 3;
		if (m->flags & HSYNCP)
			e[i + 17] |= 0x4;
		if (m->flags & HSYNCM)
			e[i + 17] &= ~0x4;
		if (m->flags & VSYNCP)
			e[i + 17] |= 0x2;
		if (m->flags & VSYNCM)
			e[i + 17] &= ~0x2;
	}
}

static void prom_add_edid_data(int model)
{
	unsigned char e[0x80];
	unsigned short width_mm, height_mm, crt_imac;
	const struct modelist *m;
	memset(e, 0, sizeof(e));
	width_mm = height_mm = crt_imac = 0;
	m = NULL;
	switch (model) {
	case PowerBook2_1:	/* mach64 */
	case PowerBook2_2:	/* r128 */
		width_mm = 247;
		height_mm = 186;
		m = &mode_800x600_60;
		break;
	case AAPL_PowerBook1998:	/* mach64 */
	case PowerBook1_1:	/* r128 */
	case PowerBook3_1:	/* r128 */
		width_mm = 286;
		height_mm = 214;
		m = &mode_1024x768_60;
		break;
	case iMac_1:		/* mach64 */
	case PowerMac2_1:	/* r128 */
	case PowerMac2_2:	/* r128 */
	case PowerMac4_1:	/* r128 */
		crt_imac = 1;
		width_mm = 286;
		height_mm = 212;
		m = &mode_1024x768_75;
		break;
	case AAPL_3400:	/* 800x600 chipsfb */
	case AAPL_3500:	/* 800x600 chipsfb */
	default:
		return;
	};
	e[0x12] = 0x01;
	e[0x13] = 0x03;
	e[0x08] = 0x06;		/* high vendor Apple */
	e[0x09] = 0x10;		/* low */
	e[0x0a] = (unsigned char)model;	/* low */
	e[0x0b] = 0x42;		/* high device */
	if (m->e0x23)
		e[0x23] = m->e0x23;
	if (m->e0x24)
		e[0x24] = m->e0x24;
	prom_add_first_detailed_timing(e, width_mm, height_mm, m);
	prom_add_detailed_timing(1, e, width_mm, height_mm, m);
	if (crt_imac && 0) {
		prom_add_detailed_timing(2, e, width_mm, height_mm, &mode_800x600_94);
		prom_add_detailed_timing(3, e, width_mm, height_mm, &mode_640x480_116);
	}
	if (!devtree_edid->edid)
		devtree_edid->edid = malloc(sizeof(e));
	memcpy(devtree_edid->edid, e, sizeof(e));
}

static void prom_add_pmac_devices(hd_data_t *hd_data, const unsigned char *buf)
{
	const struct pmac_mb_def *m = pmac_mb;

	while (m->string) {
		if (strcmp(buf, m->string) == 0) {
			model = m->model;
			break;
		}
		m++;
	}
	if (model)
		prom_add_edid_data(model);
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
  unsigned char buf[256];
  FILE *f;
  prom_info_t *pt;

  if(!hd_probe_feature(hd_data, pr_prom)) return;

  hd_data->module = mod_prom;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->devtree = free_devtree(hd_data);
  devtree_edid = NULL;

  veth_cnt = vscsi_cnt = 0;

  PROGRESS(1, 0, "devtree");

  read_devtree(hd_data);
  if((f = fopen(PROC_PROM "/compatible", "r"))) {
    if(fread(buf, 1, sizeof buf - 1, f) > 2) {
      buf[sizeof buf - 1] = 0;
      if(memmem(buf, sizeof buf - 1, "MacRISC", 7)) prom_add_pmac_devices(hd_data, buf);
    }
    fclose(f);
  }
  if(hd_data->debug) dump_devtree_data(hd_data);

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
  read_str(path, "ccin", &devtree->ccin);
  read_str(path, "fru-number", &devtree->fru_number);
  read_str(path, "ibm,loc-code", &devtree->loc_code);
  read_str(path, "serial-number", &devtree->serial_number);
  read_str(path, "part-number", &devtree->part_number);
  read_str(path, "description", &devtree->description);

  read_int(path, "interrupts", &devtree->interrupt);
  read_int(path, "AAPL,interrupts", &devtree->interrupt);
  read_int(path, "class-code", &devtree->class_code);
  read_int(path, "vendor-id", &devtree->vendor_id);
  read_int(path, "device-id", &devtree->device_id);
  read_int(path, "subsystem-vendor-id", &devtree->subvendor_id);
  read_int(path, "subsystem-id", &devtree->subdevice_id);
  read_int(path, "revision-id", &devtree->revision_id);

  if(devtree_edid == NULL && devtree->device_type && strcmp(devtree->device_type, "display") == 0)
    devtree_edid = devtree;
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

    if (strstr(devtree->path, "vpd") == devtree->path)
      ADD2LOG(
        "    ccin \"%s\", fru-number \"%s\", location-code \"%s\", serial-number \"%s\", part-number \"%s,\"\n"
        "    description \"%s\"\n",
        devtree->ccin ? devtree->ccin : "",
        devtree->fru_number ? devtree->fru_number : "",
        devtree->loc_code ? devtree->loc_code : "",
        devtree->serial_number ? devtree->serial_number : "",
        devtree->part_number ? devtree->part_number : "",
        devtree->description ? devtree->description : ""
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
        hd_log_hex(hd_data, 1, 0x10, devtree->edid + u);
        ADD2LOG("\n");
      }
    }

    if(devtree->next) ADD2LOG("\n");
  }

  ADD2LOG("----- /proc device tree end -----\n");
}

#endif /* defined(__PPC__) */

/** @} */


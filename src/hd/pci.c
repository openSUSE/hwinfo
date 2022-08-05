#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/pci.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "pci.h"

/**
 * @defgroup PCIint PCI
 * @ingroup libhdBUSint
 * @brief PCI bus scan functions
 *
 * @{
 */

/*
 * linux/ioport.h
 */
#define IORESOURCE_BITS		0x000000ff
#define IORESOURCE_IO		0x00000100
#define IORESOURCE_MEM		0x00000200
#define IORESOURCE_IRQ		0x00000400
#define IORESOURCE_DMA		0x00000800
#define IORESOURCE_PREFETCH	0x00001000
#define IORESOURCE_READONLY	0x00002000
#define IORESOURCE_CACHEABLE	0x00004000
#define IORESOURCE_DISABLED	0x10000000


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * pci stuff
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void add_pci_data(hd_data_t *hd_data);
// static void add_driver_info(hd_data_t *hd_data);
static pci_t *add_pci_entry(hd_data_t *hd_data, pci_t *new_pci);
static unsigned char pci_cfg_byte(pci_t *pci, int fd, unsigned idx);
static void dump_pci_data(hd_data_t *hd_data);
static void hd_read_macio(hd_data_t *hd_data);
static void hd_read_vio(hd_data_t *hd_data);
static void hd_read_xen(hd_data_t *hd_data);
static void hd_read_ps3_system_bus(hd_data_t *hd_data);
static void hd_read_vm(hd_data_t *hd_data);
static void add_mv643xx_eth(hd_data_t *hd_data, char *entry, char *platform_type);
static void hd_read_platform(hd_data_t *hd_data);
static void hd_read_of_platform(hd_data_t *hd_data);
static void add_xen_network(hd_data_t *hd_data);
static void add_xen_storage(hd_data_t *hd_data);
static void hd_read_virtio(hd_data_t *hd_data);
static void hd_read_uisvirtpci(hd_data_t *hd_data);
static void hd_read_ibmebus(hd_data_t *hd_data);
static void add_edid_from_file(const char *file, pci_t *pci, int index, hd_data_t *hd_data);
static void hd_read_mmc(hd_data_t *hd_data);
static void hd_read_sdio(hd_data_t *hd_data);
static void hd_read_nd(hd_data_t *hd_data);
static void hd_read_visorbus(hd_data_t *hd_data);
static void hd_read_mdio(hd_data_t *hd_data);

void hd_scan_sysfs_pci(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_pci)) return;

  hd_data->module = mod_pci;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->pci = NULL;

  PROGRESS(1, 0, "sysfs drivers");

  hd_sysfs_driver_list(hd_data);

  PROGRESS(2, 0, "get sysfs pci data");

  hd_pci_read_data(hd_data);
  if(hd_data->debug) dump_pci_data(hd_data);

  add_pci_data(hd_data);

  PROGRESS(3, 0, "macio");
  hd_read_macio(hd_data);

  PROGRESS(4, 0, "vio");
  hd_read_vio(hd_data);

  PROGRESS(5, 0, "xen");
  hd_read_xen(hd_data);

  PROGRESS(6, 0, "ps3");
  hd_read_ps3_system_bus(hd_data);
  
  PROGRESS(7, 0, "platform");
  hd_read_platform(hd_data);

  PROGRESS(8, 0, "of_platform");
  hd_read_of_platform(hd_data);

  PROGRESS(9, 0, "vm");
  hd_read_vm(hd_data);

  PROGRESS(10, 0, "virtio");
  hd_read_virtio(hd_data);

  PROGRESS(11, 0, "ibmebus");
  hd_read_ibmebus(hd_data);

  PROGRESS(12, 0, "uisvirtpci");
  hd_read_uisvirtpci(hd_data);

  PROGRESS(13, 0, "mmc");
  hd_read_mmc(hd_data);

  PROGRESS(14, 0, "sdio");
  hd_read_sdio(hd_data);

  PROGRESS(15, 0, "nd");
  hd_read_nd(hd_data);

  PROGRESS(16, 0, "visorbus");
  hd_read_visorbus(hd_data);

  PROGRESS(17, 0, "mdio");
  hd_read_mdio(hd_data);
}


/*
 * Get the (raw) PCI data, taken from /sys/bus/pci.
 *
 * Note: non-root users can only read the first 64 bytes (of 256)
 * of the device headers.
 */
void hd_pci_read_data(hd_data_t *hd_data)
{
  uint64_t ul0, ul1, ul2;
  unsigned u, u0, u1, u2, u3;
  unsigned char nxt;
  str_list_t *sl;
  char *s;
  pci_t *pci;
  int fd;
  str_list_t *sf_bus, *sf_bus_e, *sf_drm_dirs, *sf_drm_dir, *sf_drm_subdirs,
    *sf_drm_subdir;
  char *sf_dev, *sf_drm = NULL, *sf_drm_subpath = NULL, *sf_drm_edid = NULL;

  sf_bus = read_dir("/sys/bus/pci/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: pci\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/pci/devices", sf_bus_e->str));

    ADD2LOG(
      "  pci device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    if(sscanf(sf_bus_e->str, "%x:%x:%x.%x", &u0, &u1, &u2, &u3) != 4) continue;

    pci = add_pci_entry(hd_data, new_mem(sizeof *pci));

    pci->sysfs_id = new_str(sf_dev);
    pci->sysfs_bus_id = new_str(sf_bus_e->str);

    pci->bus = (u0 << 8) + u1;
    pci->slot = u2;
    pci->func = u3;

    if((s = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      pci->modalias = canon_str(s, strlen(s));
      ADD2LOG("    modalias = \"%s\"\n", pci->modalias);
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "class"), &ul0, 0)) {
      ADD2LOG("    class = 0x%x\n", (unsigned) ul0);
      pci->prog_if = ul0 & 0xff;
      pci->sub_class = (ul0 >> 8) & 0xff;
      pci->base_class = (ul0 >> 16) & 0xff;
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "vendor"), &ul0, 0)) {
      ADD2LOG("    vendor = 0x%x\n", (unsigned) ul0);
      pci->vend = ul0 & 0xffff;
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "device"), &ul0, 0)) {
      ADD2LOG("    device = 0x%x\n", (unsigned) ul0);
      pci->dev = ul0 & 0xffff;
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "subsystem_vendor"), &ul0, 0)) {
      ADD2LOG("    subvendor = 0x%x\n", (unsigned) ul0);
      pci->sub_vend = ul0 & 0xffff;
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "subsystem_device"), &ul0, 0)) {
      ADD2LOG("    subdevice = 0x%x\n", (unsigned) ul0);
      pci->sub_dev = ul0 & 0xffff;
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "irq"), &ul0, 0)) {
      ADD2LOG("    irq = %d\n", (unsigned) ul0);
      pci->irq = ul0;
    }

    if((s = get_sysfs_attr_by_path(sf_dev, "label"))) {
      pci->label = canon_str(s, strlen(s));
      ADD2LOG("    label = \"%s\"\n", pci->label);
    }

    sl = hd_attr_list(get_sysfs_attr_by_path(sf_dev, "resource"));
    for(u = 0; sl; sl = sl->next, u++) {
      if(
        sscanf(sl->str, "0x%"SCNx64" 0x%"SCNx64" 0x%"SCNx64, &ul0, &ul1, &ul2) == 3 &&
        ul1 &&
        u < sizeof pci->base_addr / sizeof *pci->base_addr
      ) {
        ADD2LOG("    res[%u] = 0x%"PRIx64" 0x%"PRIx64" 0x%"PRIx64"\n", u, ul0, ul1, ul2);
        pci->base_addr[u] = ul0;
        pci->base_len[u] = ul1 + 1 - ul0;
        pci->addr_flags[u] = ul2;
      }
    }

    s = NULL;
    str_printf(&s, 0, "%s/config", sf_dev);
    if((fd = open(s, O_RDONLY)) != -1) {
      pci->data_len = pci->data_ext_len = read(fd, pci->data, 0x40);
      ADD2LOG("    config[%u]\n", pci->data_len);

      if(pci->data_len >= 0x40) {
        pci->hdr_type = pci->data[PCI_HEADER_TYPE] & 0x7f;
        pci->cmd = pci->data[PCI_COMMAND] + (pci->data[PCI_COMMAND + 1] << 8);

        if(pci->hdr_type == 1 || pci->hdr_type == 2) {	/* PCI or CB bridge */
          pci->secondary_bus = pci->data[PCI_SECONDARY_BUS];
          /* PCI_SECONDARY_BUS == PCI_CB_CARD_BUS */
        }

        for(u = 0; u < sizeof pci->base_addr / sizeof *pci->base_addr; u++) {
          if((pci->addr_flags[u] & IORESOURCE_IO)) {
            if(!(pci->cmd & PCI_COMMAND_IO)) pci->addr_flags[u] |= IORESOURCE_DISABLED;
          }

          if((pci->addr_flags[u] & IORESOURCE_MEM)) {
            if(!(pci->cmd & PCI_COMMAND_MEMORY)) pci->addr_flags[u] |= IORESOURCE_DISABLED;
          }
        }

        /* let's go through the capability list */
        if(
          pci->hdr_type == PCI_HEADER_TYPE_NORMAL &&
          (nxt = pci->data[PCI_CAPABILITY_LIST])
        ) {
          /*
           * Cut out after 16 capabilities to avoid infinite recursion due
           * to (potentially) malformed data. 16 is more or less
           * arbitrary, though (the capabilities are bits in a byte, so 8 entries
           * should suffice).
           */
          for(u = 0; u < 16 && nxt && nxt <= 0xfe; u++) {
            switch(pci_cfg_byte(pci, fd, nxt)) {
              case PCI_CAP_ID_PM:
                pci->flags |= (1 << pci_flag_pm);
                break;

              case PCI_CAP_ID_AGP:
                pci->flags |= (1 << pci_flag_agp);
                break;
            }
            nxt = pci_cfg_byte(pci, fd, nxt + 1);
          }
        }
      }

      close(fd);
    }

    /* FIXME: stil valid? */
    for(u = 0; u < sizeof pci->edid_len / sizeof *pci->edid_len; u++) {
      str_printf(&s, 0, "%s/edid%u", sf_dev, u + 1);
      add_edid_from_file(s, pci, u, hd_data);
    }
    s = free_mem(s);

    /* try searching the monitor data in <PCI_dev>/drm/x/x/edid files if no data found*/
    if (pci->edid_len[0] == 0) {
      str_printf(&sf_drm, 0, "%s/drm", sf_dev);
      u = 0;

      /* get <PCI_dev>/drm/x listing */
      sf_drm_dirs = read_dir(sf_drm, 'd');
      for(sf_drm_dir = sf_drm_dirs; sf_drm_dir; sf_drm_dir = sf_drm_dir->next) {
        str_printf(&sf_drm_subpath, 0, "%s/drm/%s", sf_dev, sf_drm_dir->str);

        /* get <PCI_dev>/drm/x/x listing */
        sf_drm_subdirs = read_dir(sf_drm_subpath, 'd');
        for(sf_drm_subdir = sf_drm_subdirs; sf_drm_subdir; sf_drm_subdir = sf_drm_subdir->next) {
          /* try loading <PCI_dev>/drm/x/x/edid file */
          str_printf(&sf_drm_edid, 0, "%s/%s/edid", sf_drm_subpath, sf_drm_subdir->str);
          add_edid_from_file(sf_drm_edid, pci, u, hd_data);

          if (pci->edid_len[u] > 0) {
            u = u + 1;
          }
        }

        free_str_list(sf_drm_subdirs);
      }

      sf_drm_subpath = free_mem(sf_drm_subpath);
      sf_drm_edid = free_mem(sf_drm_edid);
      sf_drm = free_mem(sf_drm);
      free_str_list(sf_drm_dirs);
    }

    pci->rev = pci->data[PCI_REVISION_ID];

    if((pci->addr_flags[6] & IORESOURCE_MEM)) {
      if(!(pci->data[PCI_ROM_ADDRESS] & PCI_ROM_ADDRESS_ENABLE)) {
        pci->addr_flags[6] |= IORESOURCE_DISABLED;
      }
    }

    pci->flags |= (1 << pci_flag_ok);

    free_mem(sf_dev);
  }

  free_str_list(sf_bus);
}

void add_edid_from_file(const char *file, pci_t *pci, int index, hd_data_t *hd_data) {
  int fd, i;

  if((fd = open(file, O_RDONLY)) != -1) {
    if (index < sizeof pci->edid_len / sizeof *pci->edid_len) {
      pci->edid_len[index] = read(fd, pci->edid_data[index], sizeof pci->edid_data[index]);
      ADD2LOG("    found edid file at %s (size: %d)\n", file, pci->edid_len[index]);

      if(pci->edid_len[index] > 0) {
        for(i = 0; i < sizeof pci->edid_data[index]; i += 0x10) {
          ADD2LOG("      ");
          hd_log_hex(hd_data, 1, 0x10, pci->edid_data[index] + i);
          ADD2LOG("\n");
        }
      }
    }
    else {
      ADD2LOG("    monitor list full, ignoring monitor data %s\n", file);
    }
    close(fd);
  }
  else {
    pci->edid_len[index] = 0;
  }
}

void add_pci_data(hd_data_t *hd_data)
{
  hd_t *hd, *hd2;
  pci_t *pci, *pnext;
  unsigned u;
  char *s, *t;
  str_list_t *net_ifs, *net_ifs2;

  PROGRESS(4, 0, "build list");

  for(pci = hd_data->pci; pci; pci = pnext) {
    pnext = pci->next;
    hd = add_hd_entry(hd_data, __LINE__, 0);

    hd->sysfs_id = new_str(hd_sysfs_id(pci->sysfs_id));
    s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
    if(s) add_str_list(&hd->drivers, s);

    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_pci;
    hd->detail->pci.data = pci;

    pci->next = NULL;

    hd_pci_complete_data(hd);

    if((u = device_class(hd_data, hd->vendor.id, hd->device.id))) {
      hd->base_class.id = u >> 8;
      hd->sub_class.id = u & 0xff;
    }

    /*
     * If there are several network interfaces attached to a single
     * function, just replicate entries.
     */

    /* old kernels: subdir 'net/<interface>' */
    s = NULL;
    str_printf(&s, 0, "/sys%s/net", hd->sysfs_id);
    net_ifs = read_dir(s, 'D');
    s = free_mem(s);

    if(!net_ifs) {
      /* old kernels: links 'net:<interface>' */
      str_list_t *tmp, *sl;

      str_printf(&s, 0, "/sys%s", hd->sysfs_id);
      tmp = read_dir(s, 'l');
      s = free_mem(s);

      for(sl = tmp; sl; sl = sl->next) {
        if(!strncmp(sl->str, "net:", sizeof "net:" - 1)) {
          add_str_list(&net_ifs, sl->str + sizeof "net:" - 1);
        }
      }

      free_str_list(tmp);
    }

    if(net_ifs) {
      hd->unix_dev_name = new_str(net_ifs->str);
      net_ifs2 = net_ifs->next;

      for(; net_ifs2; net_ifs2 = net_ifs2->next) {
        hd2 = add_hd_entry(hd_data, __LINE__, 0);
        hd2->sysfs_id = new_str(hd->sysfs_id);
        hd2->sysfs_bus_id = new_str(hd->sysfs_bus_id);
        if(hd->drivers) {
          add_str_list(&hd2->drivers, hd->drivers->str);
        }
        hd2->unix_dev_name = new_str(net_ifs2->str);
        hd2->detail = hd->detail;
        hd_pci_complete_data(hd2);
        hd2->detail = NULL;

        if((u = device_class(hd_data, hd2->vendor.id, hd2->device.id))) {
          hd2->base_class.id = u >> 8;
          hd2->sub_class.id = u & 0xff;
        }
      }
    }

    free_str_list(net_ifs);
  }

  hd_data->pci = NULL;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->bus.id == bus_pci && hd->sysfs_id) {
      s = new_str(hd->sysfs_id);

      if((t = strrchr(s, '/'))) {
        *t = 0;
        if((hd2 = hd_find_sysfs_id(hd_data, s))) {
          hd->attached_to = hd2->idx;
        }
      }
      free_mem(s);
    }
  }

//  add_driver_info(hd_data);
}


void hd_pci_complete_data(hd_t *hd)
{
  pci_t *pci;
  hd_res_t *res;
  unsigned u;

  if(
    !hd->detail ||
    hd->detail->type != hd_detail_pci ||
    !(pci = hd->detail->pci.data)
  ) return;

  hd->bus.id = bus_pci;

  if(pci->sysfs_bus_id && *pci->sysfs_bus_id) {
    hd->sysfs_bus_id = pci->sysfs_bus_id;
    pci->sysfs_bus_id = NULL;
  }

  if(pci->modalias && *pci->modalias) {
    hd->modalias = pci->modalias;
    pci->modalias = NULL;
  }

  if(pci->label && *pci->label) {
    hd->label = pci->label;
    pci->label = NULL;
  }

  hd->slot = pci->slot + (pci->bus << 8);
  hd->func = pci->func;
  hd->base_class.id = pci->base_class;
  hd->sub_class.id = pci->sub_class;
  hd->prog_if.id = pci->prog_if;

  /* fix up old VGA's entries */
  if(hd->base_class.id == bc_none && hd->sub_class.id == 0x01) {
    hd->base_class.id = bc_display;
    hd->sub_class.id = sc_dis_vga;
  }

  if(pci->dev || pci->vend) {
    hd->device.id = MAKE_ID(TAG_PCI, pci->dev);
    hd->vendor.id = MAKE_ID(TAG_PCI, pci->vend);
  }
  if(pci->sub_dev || pci->sub_vend) {
    hd->sub_device.id = MAKE_ID(TAG_PCI, pci->sub_dev);
    hd->sub_vendor.id = MAKE_ID(TAG_PCI, pci->sub_vend);
  }
  hd->revision.id = pci->rev;

  for(u = 0; u < sizeof pci->base_addr / sizeof *pci->base_addr; u++) {
    if((pci->addr_flags[u] & IORESOURCE_IO)) {
      res = new_mem(sizeof *res);
      res->io.type = res_io;
      res->io.enabled = pci->addr_flags[u] & IORESOURCE_DISABLED ? 0 : 1;
      res->io.base = pci->base_addr[u];
      res->io.range = pci->base_len[u];
      res->io.access = pci->addr_flags[u] & IORESOURCE_READONLY ? acc_ro : acc_rw;
      add_res_entry(&hd->res, res);
    }

    if((pci->addr_flags[u] & IORESOURCE_MEM)) {
      res = new_mem(sizeof *res);
      res->mem.type = res_mem;
      res->mem.enabled = pci->addr_flags[u] & IORESOURCE_DISABLED ? 0 : 1;
      res->mem.base = pci->base_addr[u];
      res->mem.range = pci->base_len[u];
      res->mem.access = pci->addr_flags[u] & IORESOURCE_READONLY ? acc_ro : acc_rw;
      res->mem.prefetch = pci->addr_flags[u] & IORESOURCE_PREFETCH ? flag_yes : flag_no;
      add_res_entry(&hd->res, res);
    }
  }

  if(pci->irq) {
    res = new_mem(sizeof *res);
    res->irq.type = res_irq;
    res->irq.enabled = 1;
    res->irq.base = pci->irq;
    add_res_entry(&hd->res, res);
  }

  if(pci->flags & (1 << pci_flag_agp)) hd->is.agp = 1;
}


#if 0
/*
 * Add driver info in some special cases.
 */
void add_driver_info(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->bus.id != bus_pci) continue;

    if(
      (
        hd->base_class.id == bc_serial &&
        hd->sub_class.id == sc_ser_fire
      ) ||
      (
        hd->base_class.id == bc_serial &&
        hd->sub_class.id == sc_ser_usb
      )
    ) {
      for(res = hd->res; res; res = res->next) {
        if(res->any.type == res_irq) break;
      }
      if(!res) hd->is.notready = 1;
      continue;
    }
  }
}
#endif


#if 1
/*
 * Store a raw PCI entry; just for convenience.
 */
pci_t *add_pci_entry(hd_data_t *hd_data, pci_t *new_pci)
{
  pci_t **pci = &hd_data->pci;

  while(*pci) pci = &(*pci)->next;

  return *pci = new_pci;
}

#else

/*
 * Store a raw PCI entry; just for convenience.
 *
 * Reverse order.
 */
pci_t *add_pci_entry(hd_data_t *hd_data, pci_t *new_pci)
{
  new_pci->next = hd_data->pci;

  return hd_data->pci = new_pci;
}
#endif


/*
 * get a byte from pci config space
 */
unsigned char pci_cfg_byte(pci_t *pci, int fd, unsigned idx)
{
  unsigned char uc;

  if(idx >= sizeof pci->data) return 0;
  if(idx < pci->data_len) return pci->data[idx];
  if(idx < pci->data_ext_len && pci->data[idx]) return pci->data[idx];
  if(lseek(fd, idx, SEEK_SET) != (off_t) idx) return 0;
  if(read(fd, &uc, 1) != 1) return 0;
  pci->data[idx] = uc;

  if(idx >= pci->data_ext_len) pci->data_ext_len = idx + 1;

  return uc;
}
/*
 * Add a dump of all raw PCI data to the global log.
 */
void dump_pci_data(hd_data_t *hd_data)
{
  pci_t *pci;
  char *s = NULL;
  char buf[32];
  int i, j;

  ADD2LOG("---------- PCI raw data ----------\n");

  for(pci = hd_data->pci; pci; pci = pci->next) {

    if(!(pci->flags & (1 << pci_flag_ok))) str_printf(&s, -1, "oops");
    if(pci->flags & (1 << pci_flag_pm)) str_printf(&s, -1, ",pm");
    if(pci->flags & (1 << pci_flag_agp)) str_printf(&s, -1, ",agp");
    if(!s) str_printf(&s, 0, "%s", "");

    *buf = 0;
    if(pci->secondary_bus) {
      sprintf(buf, "->%02x", pci->secondary_bus);
    }

    ADD2LOG(
      "bus %02x%s, slot %02x, func %x, vend:dev:s_vend:s_dev:rev %04x:%04x:%04x:%04x:%02x\n",
      pci->bus, buf, pci->slot, pci->func, pci->vend, pci->dev, pci->sub_vend, pci->sub_dev, pci->rev
    );
    ADD2LOG(
      "class %02x, sub_class %02x prog_if %02x, hdr %x, flags <%s>, irq %u\n",
      pci->base_class, pci->sub_class, pci->prog_if, pci->hdr_type, *s == ',' ? s + 1 : s, pci->irq 
    );

    s = free_mem(s);

    for(i = 0; i < 6; i++) {
      if(pci->base_addr[i] || pci->base_len[i])
        ADD2LOG("  addr%d %08"PRIx64", size %08"PRIx64"\n", i, pci->base_addr[i], pci->base_len[i]);
    }
    if(pci->rom_base_addr)
      ADD2LOG("  rom   %08"PRIx64"\n", pci->rom_base_addr);

    if(pci->log) ADD2LOG("%s", pci->log);

    for(i = 0; (unsigned) i < pci->data_ext_len; i += 0x10) {
      ADD2LOG("  %02x: ", i);
      j = pci->data_ext_len - i;
      hd_log_hex(hd_data, 1, j > 0x10 ? 0x10 : j, pci->data + i);
      ADD2LOG("\n");
    }

    if(pci->next) ADD2LOG("\n");
  }

  ADD2LOG("---------- PCI raw data end ----------\n");
}


/*
 * Get mac-io data from sysfs.
 */
void hd_read_macio(hd_data_t *hd_data)
{
  char *s, *t;
  char *macio_name, *macio_type, *macio_compat, *macio_modalias;
  hd_t *hd, *hd2;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev;

  sf_bus = read_dir("/sys/bus/macio/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: macio\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/macio/devices", sf_bus_e->str));

    ADD2LOG(
      "  macio device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    macio_name = macio_type = macio_compat = macio_modalias = NULL;

    if((s = get_sysfs_attr_by_path(sf_dev, "name"))) {
      macio_name = canon_str(s, strlen(s));
      ADD2LOG("    name = \"%s\"\n", macio_name);
    }

    if((s = get_sysfs_attr_by_path(sf_dev, "type"))) {
      macio_type = canon_str(s, strlen(s));
      ADD2LOG("    type = \"%s\"\n", macio_type);
    }

    if((s = get_sysfs_attr_by_path(sf_dev, "compatible"))) {
      macio_compat = canon_str(s, strlen(s));
      ADD2LOG("    compatible = \"%s\"\n", macio_compat);
    }

    if((s = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      macio_modalias = canon_str(s, strlen(s));
      ADD2LOG("    modalias = \"%s\"\n", macio_modalias);
    }

    if(
      macio_type && (
        !strcmp(macio_type, "network") ||
        !strcmp(macio_type, "scsi")
      )
    ) {
      hd = add_hd_entry(hd_data, __LINE__, 0);

      if(!strcmp(macio_type, "network")) {
        hd->base_class.id = bc_network;
        hd->sub_class.id = 0;	/* ethernet */

        if(macio_compat && !strcmp(macio_compat, "wireless")) {
          hd->sub_class.id = 0x82;
          hd->is.wlan = 1;
        }
      }
      else { /* scsi */
        hd->base_class.id = bc_storage;
        hd->sub_class.id = sc_sto_scsi;
      }

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
      if(s) add_str_list(&hd->drivers, s);

      hd->modalias = macio_modalias;
      macio_modalias = NULL;

      s = new_str(hd->sysfs_id);

      if((t = strrchr(s, '/'))) {
        *t = 0;
        if((t = strrchr(s, '/'))) {
          *t = 0;
          if((hd2 = hd_find_sysfs_id(hd_data, s))) {
            hd->attached_to = hd2->idx;

            hd->vendor.id = hd2->vendor.id;
            hd->device.id = hd2->device.id;

          }
        }
      }
      free_mem(s);
    }

    free_mem(sf_dev);
  }

  free_str_list(sf_bus);
}


/*
 * Get vio data from sysfs.
 */
void hd_read_vio(hd_data_t *hd_data)
{
  char *s, *vio_devspec, *vio_name, *vio_modalias;
  int eth_cnt = 0, scsi_cnt = 0, dasd_cnt = 0, cd_cnt = 0;
  hd_t *hd;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev;

  sf_bus = read_dir("/sys/bus/vio/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: vio\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/vio/devices", sf_bus_e->str));

    ADD2LOG(
      "  vio device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    vio_devspec = vio_name = vio_modalias = NULL;

    if((s = get_sysfs_attr_by_path(sf_dev, "devspec"))) {
      vio_devspec = canon_str(s, strlen(s));
      ADD2LOG("    name = \"%s\"\n", vio_devspec);
    }

    if((s = get_sysfs_attr_by_path(sf_dev, "name"))) {
      vio_name = canon_str(s, strlen(s));
      ADD2LOG("    type = \"%s\"\n", vio_name);
    }

    if((s = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      vio_modalias = canon_str(s, strlen(s));
      ADD2LOG("    modalias = \"%s\"\n", vio_modalias);
    }

    if(
      vio_name && (
        !strcmp(vio_name, "l-lan") || /* pseries && iseries */
        !strcmp(vio_name, "vnic") || /* ibmvnic */
        !strcmp(vio_name, "viodasd") || /* iseries */
        !strcmp(vio_name, "viocd") || /* iseries */
        !strcmp(vio_name, "vfc-client") || /* ibmvfc */
        !strcmp(vio_name, "v-scsi" /* pseries */)
      )
    ) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus.id = bus_vio;
      if(vio_modalias) hd->modalias = new_str(vio_modalias);

      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6001);

      if(!strcmp(vio_name, "l-lan") || !strcmp(vio_name, "vnic")) {
        hd->base_class.id = bc_network;
        hd->sub_class.id = 0;	/* ethernet */
        hd->slot = eth_cnt++;
        str_printf(&hd->device.name, 0, "Virtual Ethernet card %d", hd->slot);
      } else if(!strcmp(vio_name, "viodasd")) {
        hd->base_class.id = bc_storage;
        hd->sub_class.id = sc_sto_other;
        hd->slot = dasd_cnt++;
        str_printf(&hd->device.name, 0, "Virtual DASD %d", hd->slot);
      } else if(!strcmp(vio_name, "viocd")) {
        hd->base_class.id = bc_storage;
        hd->sub_class.id = sc_sto_other;
        hd->slot = cd_cnt++;
        str_printf(&hd->device.name, 0, "Virtual CD %d", hd->slot);
      } else if(!strcmp(vio_name, "vfc-client")) {
        hd->base_class.id = bc_storage;
        hd->sub_class.id = sc_sto_scsi;
        hd->slot = scsi_cnt++;
        str_printf(&hd->device.name, 0, "Virtual FC %d", hd->slot);
      }
      else { /* scsi */
        hd->base_class.id = bc_storage;
        hd->sub_class.id = sc_sto_scsi;
        hd->slot = scsi_cnt++;
        str_printf(&hd->device.name, 0, "Virtual SCSI %d", hd->slot);
      }

      hd->rom_id = new_str(vio_devspec ? vio_devspec + 1 : 0);	/* skip leading '/' */

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
      if(s) add_str_list(&hd->drivers, s);
    }

    free_mem(sf_dev);
  }

  free_str_list(sf_bus);
}


/*
 * Marvell Gigabit Ethernet in Pegasos2
 */
void add_mv643xx_eth(hd_data_t *hd_data, char *entry, char *platform_type)
{
  hd_t *hd;
  char *sf_dev = NULL;

  str_printf(&sf_dev, 0, "%s/%s", "/sys/devices/platform", entry);
  ADD2LOG("  platform device: adding %s\n", entry);

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class.id = bc_network;
  hd->sub_class.id = 0;

  hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
  hd->sysfs_bus_id = new_str(entry);
  hd->modalias = new_str(platform_type);

  hd->vendor.id = MAKE_ID(TAG_PCI, 0x11ab);
  hd->device.id = MAKE_ID(TAG_PCI, 0x6460);

  free_mem(sf_dev);
}


/*
 * Get platform data from sysfs.
 */
void hd_read_platform(hd_data_t *hd_data)
{
  char *s, *platform_type, *device_type, *driver;
  str_list_t *sf_bus, *sf_bus_e, *sf_bus_canonical, *sf_eth_dev = NULL;
  char *sf_dev;
  int mv643xx_eth_seen = 0;
  int is_net, is_storage, is_usb, is_xhci, is_ehci;
  hd_t *hd;
  char *sysfs_device_dir = "/sys/bus/platform/devices";

  sf_bus = read_dir(sysfs_device_dir, 'l');
  sf_bus_canonical = read_dir_canonical(sysfs_device_dir, 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: platform\n");
    return;
  }

  /* list of network interfaces */
  str_list_t *net_list = read_dir_canonical("/sys/class/net", 'l');

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link(sysfs_device_dir, sf_bus_e->str));

    ADD2LOG(
      "  platform device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    if((s = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      platform_type = canon_str(s, strlen(s));
      device_type = new_str("");
      if((s = get_sysfs_attr_by_path(sf_dev, "uevent"))) {
        char *t, *t2;
        if((t = strstr(s, "OF_NAME="))) {
          t += sizeof "OF_NAME=" - 1;
          if((t2 = strchr(t, '\n'))) *t2 = 0;
          free_mem(device_type);
          device_type = new_str(t);
        }
      }
      // 'driver' is a static reference, don't free
      driver = hd_sysfs_find_driver(hd_data, hd_sysfs_id(sf_dev), 1);
      if(!driver) driver = "";
      ADD2LOG("    type = \"%s\", modalias = \"%s\", driver = \"%s\"\n", device_type, platform_type, driver);
      /*
       * it's a network device if
       *   - there's a link to a network interface in a subdir *AND*
       *   - there's no other device that is actually a subdevice of this one
       */
      is_net = 0;
      sf_eth_dev = subcomponent_list(net_list, sf_dev, 0);
      is_net = !!sf_eth_dev && !has_subcomponent(sf_bus_canonical, sf_dev);
      is_storage =
        !strcmp(device_type, "sata") ||
        !strcmp(platform_type, "acpi:HISI0161:") ||
        !strcmp(platform_type, "acpi:HISI0162:");
      is_usb = (
        !strcmp(device_type, "usb") ||
        !strcmp(device_type, "dwusb") ||
        strstr(platform_type, ":xhci-hcd") ||
        !strcmp(driver, "ohci-platform") ||
        !strcmp(driver, "ehci-platform") ||
        !strcmp(driver, "xhci-plat-hcd")
      );
      is_xhci = (
        strstr(platform_type, "xhci-") ||
        strstr(driver, "xhci-")
      );
      is_ehci = !!strstr(driver, "ehci-");
      if(is_net) {
        for(str_list_t *sl = sf_eth_dev; sl; sl = sl->next) {
          ADD2LOG("    is net: interface = %s\n", sl->str);
        }
      }
      if(is_storage) ADD2LOG("    is storage\n");
      if(is_usb) ADD2LOG("    is usb\n");
      if(is_xhci) ADD2LOG("    is xhci\n");
      if(is_ehci) ADD2LOG("    is ehci\n");
      if(
        /* there is 'mv643xx_eth.0', 'mv643xx_eth.1' and 'mv643xx_eth_shared.' */
        is_net &&
        strstr(platform_type, "mv643xx_eth") &&
        !mv643xx_eth_seen++
      ) {
        add_mv643xx_eth(hd_data, sf_bus_e->str, platform_type);
      }
      else if(is_net) {
        /* note there might be more than one interface per device - hence this is a list */
        for(str_list_t *sl = sf_eth_dev; sl; sl = sl->next) {
          hd = add_hd_entry(hd_data, __LINE__, 0);
          hd->base_class.id = bc_network;
          hd->sub_class.id = 0;
          str_printf(&hd->device.name, 0, "ARM Ethernet controller");
          hd->modalias = new_str(platform_type);
          /*
           * the interface link ends with 'net' + interface, e.g. .../net/ethX
           * -> strip these two parts to form the sysfs id
           */
          char *tmp = new_str(hd_sysfs_id(sl->str));
          char *slash = strrchr(tmp, '/');
          if(slash) *slash = 0, slash = strrchr(tmp, '/');
          if(slash) *slash = 0;
          hd->sysfs_id = new_str(tmp);
          free_mem(tmp);
          /*
           * the bus id is the last part of the sysfs id - if that fails for
           * some reason fall back to device link name
           */
          tmp = strrchr(hd->sysfs_id, '/');
          if(tmp) {
            hd->sysfs_bus_id = new_str(tmp + 1);
          }
          else {
            hd->sysfs_bus_id = new_str(sf_bus_e->str);
          }
          s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
          if(s) add_str_list(&hd->drivers, s);
        }
      }
      else if(is_storage) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_storage;
        if(!strcmp(device_type, "sata")) {
          str_printf(&hd->device.name, 0, "ARM SATA controller");
          hd->sub_class.id = sc_sto_ide;
        }
        else if(!strcmp(platform_type, "acpi:HISI0162:") || !strcmp(platform_type, "acpi:HISI0161:")) {
          str_printf(&hd->device.name, 0, "HISILICON SAS controller");
          hd->sub_class.id = sc_sto_scsi;
        }
        else {
          str_printf(&hd->device.name, 0, "Storage controller");
          hd->sub_class.id = sc_sto_other;
        }
        hd->modalias = new_str(platform_type);
        hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
        hd->sysfs_bus_id = new_str(sf_bus_e->str);
        s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
        if(s) add_str_list(&hd->drivers, s);
      }
      else if(is_usb) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_serial;
        hd->sub_class.id = sc_ser_usb;

        if(is_xhci) {
          hd->prog_if.id = pif_usb_xhci;
          str_printf(&hd->device.name, 0, "ARM USB XHCI controller");
        }
        else {
          if(is_ehci) hd->prog_if.id = pif_usb_ehci;
          str_printf(&hd->device.name, 0, "ARM USB controller");
        }
        hd->modalias = new_str(platform_type);
        hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
        hd->sysfs_bus_id = new_str(sf_bus_e->str);
        s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
        if(s) add_str_list(&hd->drivers, s);
      }
      free_str_list(sf_eth_dev);
      free_mem(device_type);
      free_mem(platform_type);
    }

    free_mem(sf_dev);
  }

  free_str_list(net_list);

  free_str_list(sf_bus);
}


/*
 * Get platform data from sysfs.
 */
void hd_read_of_platform(hd_data_t *hd_data)
{
  char *s, *modalias;
  str_list_t *sf_bus, *sf_bus_e;
  hd_t *hd;
  char *sf_dev;

  sf_bus = read_dir("/sys/bus/of_platform/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: of_platform\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/of_platform/devices", sf_bus_e->str));
    ADD2LOG(
      "  of_platform device: name = %s\n    path = %s\n",
      sf_bus_e->str, hd_sysfs_id(sf_dev)
    );
    if((modalias = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      int len = strlen(modalias);
      if (len > 0 && modalias[len - 1] == '\n')
	      modalias[len - 1] = '\0';
      ADD2LOG("    modalias = \"%s\"\n", modalias);
      if (0) ;
      else if (strstr(modalias, "Cmpc5200-fec")) {
		/* EFIKA52K network */
          hd = add_hd_entry(hd_data, __LINE__, 0);

          hd->vendor.id = MAKE_ID(TAG_PCI, 0x1957); /* Freescale */
          hd->base_class.id = bc_network;
          hd->sub_class.id = 0;	/* ethernet */
          str_printf(&hd->device.name, 0, "mpc5200 Ethernet %d", hd->slot);

          hd->modalias = new_str(modalias);

          hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
          hd->sysfs_bus_id = new_str(sf_bus_e->str);
          s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
          if(s) add_str_list(&hd->drivers, s);
      } else if (strstr(modalias, "Cmpc5200-ata")) {
		/* EFIKA52K SATA */
          hd = add_hd_entry(hd_data, __LINE__, 0);

          hd->vendor.id = MAKE_ID(TAG_PCI, 0x1957); /* Freescale */
          hd->base_class.id = bc_storage;
          hd->sub_class.id = sc_sto_ide;	/* 2.5" disk */
          str_printf(&hd->device.name, 0, "mpc5200 SATA %d", hd->slot);

          hd->modalias = new_str(modalias);

          hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
          hd->sysfs_bus_id = new_str(sf_bus_e->str);
          s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
          if(s) add_str_list(&hd->drivers, s);
      } else if (strstr(modalias, "Cmpc5200-psc-ac97")) {
		/* EFIKA52K sound */
          hd = add_hd_entry(hd_data, __LINE__, 0);

          hd->vendor.id = MAKE_ID(TAG_PCI, 0x1957); /* Freescale */
          hd->base_class.id = bc_multimedia;
          hd->sub_class.id = sc_multi_audio;
          str_printf(&hd->device.name, 0, "mpc5200 AC97 %d", hd->slot);

          hd->modalias = new_str(modalias);

          hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
          hd->sysfs_bus_id = new_str(sf_bus_e->str);
          s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
          if(s) add_str_list(&hd->drivers, s);
      } else if (strstr(modalias, "Cmpc5200-usb")) {
		/* EFIKA52K USB */
          hd = add_hd_entry(hd_data, __LINE__, 0);

          hd->vendor.id = MAKE_ID(TAG_PCI, 0x1957); /* Freescale */
          hd->base_class.id = bc_serial;
          hd->sub_class.id = sc_ser_usb;
          hd->prog_if.id = pif_usb_ohci;

          str_printf(&hd->device.name, 0, "mpc5200 USB %d", hd->slot);

          hd->modalias = new_str(modalias);

          hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
          hd->sysfs_bus_id = new_str(sf_bus_e->str);
          s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
          if(s) add_str_list(&hd->drivers, s);
      }
    }
    free_mem(sf_dev);
  }
  free_str_list(sf_bus);
}


/*
 * Get ps3 data from sysfs.
 */
void hd_read_ps3_system_bus(hd_data_t *hd_data)
{
  char *s, *ps3_name;
  int scsi_cnt = 0, eth_cnt = 0, wlan_cnt = 0;
  hd_t *hd;
  str_list_t *sf_bus, *sf_bus_e, *sf_eth_dev, *sf_eth_dev_e;
  char *sf_dev, *sf_eth_net, *sf_eth_wireless;

  sf_bus = read_dir("/sys/bus/ps3_system_bus/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: ps3_system_bus\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/ps3_system_bus/devices", sf_bus_e->str));

    ADD2LOG(
      "  ps3 device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    ps3_name = NULL;

    if((s = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      ps3_name = canon_str(s, strlen(s));
      ADD2LOG("    modalias = \"%s\"\n", ps3_name);
    }

    /* network devices */
    if(ps3_name && !strcmp(ps3_name, "ps3:3")) {
      /* read list of available devices */
      sf_eth_net = new_str(hd_read_sysfs_link(sf_dev, "net"));
      sf_eth_dev = read_dir(sf_eth_net, 'd');

      /* add entries for available devices */
      for(sf_eth_dev_e = sf_eth_dev; sf_eth_dev_e; sf_eth_dev_e = sf_eth_dev_e->next) {
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->bus.id = bus_ps3_system_bus;
        hd->sysfs_bus_id = new_str(sf_bus_e->str);
        hd->slot = eth_cnt + wlan_cnt;
        hd->vendor.id = MAKE_ID(TAG_PCI, 0x104d);		/* Sony */
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1003);		/* PS3_DEV_TYPE_SB_GELIC */
        hd->base_class.id = bc_network;
        hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
        hd->modalias = new_str(ps3_name);
        hd->unix_dev_name = new_str(sf_eth_dev_e->str);		/* this is needed to correctly link to interfaces later */

        /* ethernet and wireless differ only by directory "wireless" so check for it */
        sf_eth_wireless = hd_read_sysfs_link(hd_read_sysfs_link(sf_eth_net, sf_eth_dev_e->str), "wireless");
        if(sf_eth_wireless) {
          hd->sub_class.id = 0x82;	/* wireless */
          str_printf(&hd->device.name, 0, "PS3 Wireless card %d", wlan_cnt++);
        }
        else {
          hd->sub_class.id = 0;		/* ethernet */
          str_printf(&hd->device.name, 0, "PS3 Ethernet card %d", eth_cnt++);
        }
        s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
        if(s) add_str_list(&hd->drivers, s);
      }

      sf_eth_net = free_mem(sf_eth_net);
      sf_eth_dev = free_str_list(sf_eth_dev);
    }

    if ( ps3_name && !strcmp(ps3_name, "ps3:7")) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus.id = bus_ps3_system_bus;

      hd->vendor.id = MAKE_ID(TAG_PCI, 0x104d); /* Sony */

      hd->base_class.id = bc_storage;
      hd->sub_class.id = sc_sto_other;	/* cdrom */
      hd->slot = scsi_cnt++;
      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1001); /* PS3_DEV_TYPE_STOR_ROM */
      str_printf(&hd->device.name, 0, "PS3 CDROM");

      hd->modalias = new_str(ps3_name);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
      if(s) add_str_list(&hd->drivers, s);

    }
    if ( ps3_name && !strcmp(ps3_name, "ps3:6")) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus.id = bus_ps3_system_bus;

      hd->vendor.id = MAKE_ID(TAG_PCI, 0x104d); /* Sony */

      hd->base_class.id = bc_storage;
      hd->sub_class.id = sc_sto_other;	
      hd->slot = scsi_cnt++;
      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1002); /* PS3_DEV_TYPE_STOR_DISK */
      str_printf(&hd->device.name, 0, "PS3 Disk");

      hd->modalias = new_str(ps3_name);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
      if(s) add_str_list(&hd->drivers, s);

    }
    if ( ps3_name && !strcmp(ps3_name, "ps3:9")) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus.id = bus_ps3_system_bus;

      hd->vendor.id = MAKE_ID(TAG_PCI, 0x104d); /* Sony */

      hd->base_class.id = bc_multimedia;
      hd->sub_class.id = sc_multi_audio;
      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1004);
      str_printf(&hd->device.name, 0, "PS3 Soundcard");

      hd->modalias = new_str(ps3_name);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
      if(s) add_str_list(&hd->drivers, s);

    }
    ps3_name = free_mem(ps3_name);

    free_mem(sf_dev);
  }

  free_str_list(sf_bus);
}

void hd_read_ibmebus(hd_data_t *hd_data)
{
  char *sf_dev, *s, *modalias;
  str_list_t *sf_bus, *sf_bus_e;
  hd_t *hd, *hd_ehea_base = NULL;
  int ehea_active = 0;

  sf_bus = read_dir("/sys/bus/ibmebus/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: ibmebus\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/ibmebus/devices", sf_bus_e->str));

    ADD2LOG(
      "  ibmebus device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    if((modalias = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      modalias = canon_str(modalias, strlen(modalias));

      ADD2LOG("    modalias = \"%s\"\n", modalias);

      if(0);
      else if(strstr(modalias, "Nlhea") && strstr(modalias, "CIBM,lhea")) {
        /* ==> 23c00100.lhea/modalias <==
	 * of:NlheaT<NULL>CIBM,lhea
	 * ehea
	 */
        hd = hd_ehea_base = add_hd_entry(hd_data, __LINE__, 0);

        hd->bus.id = bus_ibmebus;
        hd->vendor.id = MAKE_ID(TAG_PCI, 0x1014); /* IBM */
        hd->base_class.id = bc_network;
        hd->sub_class.id = 0;	/* ethernet */
        str_printf(&hd->device.name, 0, "IBM Host Ethernet Adapter");

        hd->modalias = new_str(modalias);

        hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
        hd->sysfs_bus_id = new_str(sf_bus_e->str);
        s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
        if(s) add_str_list(&hd->drivers, s);
      }
      else if(strstr(modalias, "Nethernet") && strstr(modalias, "CIBM,lhea-ethernet")) {
        /* ==> port1/modalias <==
	 * of:NethernetTnetworkCIBM,lhea-ethernet
	 * eth1
	 */
        hd = add_hd_entry(hd_data, __LINE__, 0);
        ehea_active = 1;
        hd->bus.id = bus_ibmebus;
        hd->modalias = new_str(modalias);
        hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
        hd->sysfs_bus_id = new_str(sf_bus_e->str);
        hd->vendor.id = MAKE_ID(TAG_PCI, 0x1014); /* IBM */
        hd->base_class.id = bc_network;
        hd->sub_class.id = 0;	/* ethernet */
        // to get proper module info
        hd->compat_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0403);
        hd->compat_device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
	s = strpbrk(hd->sysfs_bus_id, "0123456789");
	if(s) {
          hd->slot = strtol(s, NULL, 10);
          str_printf(&hd->device.name, 0, "IBM Host Ethernet Adapter Port %d", hd->slot);
	}

        s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
        add_str_list(&hd->drivers, s ?: "ehea");
      }

      modalias = free_mem(modalias);
    }
    free_mem(sf_dev);
  }

  if(hd_ehea_base && ehea_active) {
    // remove it if we have the real devices
    hd_ehea_base->base_class.id = bc_none;
  }
}

/*
 * Get xen (network & storage) data from sysfs.
 */
void hd_read_xen(hd_data_t *hd_data)
{
  char *s, *xen_type, *xen_node;
  int eth_cnt = 0, blk_cnt = 0;
  hd_t *hd;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *drv, *module;
  unsigned u;

  sf_bus = read_dir("/sys/bus/xen/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: xen\n");

    if(hd_is_xen(hd_data)) {
      add_xen_network(hd_data);
      add_xen_storage(hd_data);
    }

    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/xen/devices", sf_bus_e->str));

    ADD2LOG(
      "  xen device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    xen_type = xen_node = NULL;

    if((s = get_sysfs_attr_by_path(sf_dev, "devtype"))) {
      xen_type = canon_str(s, strlen(s));
      ADD2LOG("    type = \"%s\"\n", xen_type);
    }

    if((s = get_sysfs_attr_by_path(sf_dev, "nodename"))) {
      xen_node = canon_str(s, strlen(s));
      ADD2LOG("    node = \"%s\"\n", xen_node);
    }

    drv = new_str(hd_read_sysfs_link(sf_dev, "driver"));

    s = new_str(hd_read_sysfs_link(drv, "module"));
    module = new_str(s ? strrchr(s, '/') + 1 : NULL);
    free_mem(s);

    ADD2LOG("    module = \"%s\"\n", module);

    if(
      xen_type &&
      (
        !strcmp(xen_type, "vif") ||
        !strcmp(xen_type, "vbd")
      )
    ) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus.id = bus_none;

      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6011);	/* xen */

      if(!strcmp(xen_type, "vif")) {	/* network */
        hd->base_class.id = bc_network;
        hd->sub_class.id = 0;	/* ethernet */
        hd->slot = eth_cnt++;
        u = 3;
        if(module) {
          if(!strcmp(module, "xennet")) u = 1;
          if(!strcmp(module, "xen_vnif")) u = 2;
        }
        hd->device.id = MAKE_ID(TAG_SPECIAL, u);
        str_printf(&hd->device.name, 0, "Virtual Ethernet Card %d", hd->slot);
      }
      else {	/* storage */
        hd->base_class.id = bc_storage;
        hd->sub_class.id = sc_sto_other;
        hd->slot = blk_cnt++;
        u = 3;
        if(module) {
          if(!strcmp(module, "xenblk")) u = 1;
          if(!strcmp(module, "xen_vbd")) u = 2;
        }
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1000 + u);
        str_printf(&hd->device.name, 0, "Virtual Storage %d", hd->slot);
      }

      hd->rom_id = new_str(xen_node);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
      if(s) add_str_list(&hd->drivers, s);
    }

    free_mem(sf_dev);
    free_mem(drv);
    free_mem(module);
  }

  free_str_list(sf_bus);

  /* maybe only one of xen_vnif, xen_vbd was loaded */
  if(!eth_cnt && !hd_module_is_active(hd_data, "xen_vnif")) add_xen_network(hd_data);
  if(!blk_cnt && !hd_module_is_active(hd_data, "xen_vbd")) add_xen_storage(hd_data);
}


/*
 * fake xen network device
 */
void add_xen_network(hd_data_t *hd_data)
{
  hd_t *hd;

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class.id = bc_network;
  hd->sub_class.id = 0;	/* ethernet */
  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6011);	/* xen */
  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0002);	/* xen-vnif */
  hd->device.name = new_str("Virtual Ethernet Card");
}


/*
 * fake xen storage controller
 */
void add_xen_storage(hd_data_t *hd_data)
{
  hd_t *hd;

  hd = add_hd_entry(hd_data, __LINE__, 0);
  hd->base_class.id = bc_storage;
  hd->sub_class.id = sc_sto_other;
  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6011);	/* xen */
  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1002);	/* xen-vbd */
  hd->device.name = new_str("Virtual Storage");
}


/*
 * Get microsoft vm (network) data from sysfs.
 */
void hd_read_vm(hd_data_t *hd_data)
{
  int eth_cnt = 0, blk_cnt = 0;
  hd_t *hd;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *drv, *drv_name;

  sf_bus = read_dir("/sys/bus/vmbus/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: vm\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/vmbus/devices", sf_bus_e->str));

    ADD2LOG(
      "  vm device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    drv_name = NULL;
    drv = new_str(hd_read_sysfs_link(sf_dev, "driver"));
    if(drv) {
      drv_name = strrchr(drv, '/');
      if(drv_name) drv_name++;
    }

    ADD2LOG("    driver = \"%s\"\n", drv_name ?: "");

    if(
      drv_name &&
      !strcmp(drv_name, "hv_netvsc")
    ) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus.id = bus_none;

      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6013);	/* virtual */

      hd->base_class.id = bc_network;
      hd->sub_class.id = 0;	/* ethernet */
      hd->slot = eth_cnt++;
      hd->device.id = MAKE_ID(TAG_SPECIAL, 1);
      str_printf(&hd->device.name, 0, "Virtual Ethernet Card %d", hd->slot);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      if(drv_name) add_str_list(&hd->drivers, drv_name);
    }
    else if(
      drv_name &&
      !strcmp(drv_name, "hv_storvsc")
    ) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->bus.id = bus_none;

      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6013);	/* virtual */

      hd->base_class.id = bc_storage;
      hd->sub_class.id = sc_sto_other;
      hd->slot = blk_cnt++;
      hd->device.id = MAKE_ID(TAG_SPECIAL, 2);
      str_printf(&hd->device.name, 0, "Storage %d", hd->slot);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      if(drv_name) add_str_list(&hd->drivers, drv_name);
    }

    free_mem(sf_dev);
    free_mem(drv);
  }

  free_str_list(sf_bus);
}


/*
 * virtio
 */
void hd_read_virtio(hd_data_t *hd_data)
{
  int net_cnt = 0, blk_cnt = 0;
  unsigned dev;
  uint64_t ul0; 
  hd_t *hd, *hd2;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *drv, *drv_name, *modalias, *s, *t;

  sf_bus = read_dir("/sys/bus/virtio/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: virtio\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/virtio/devices", sf_bus_e->str));

    ADD2LOG(
      "  virtio device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    drv_name = NULL;
    drv = new_str(hd_read_sysfs_link(sf_dev, "driver"));
    if(drv) {
      drv_name = strrchr(drv, '/');
      if(drv_name) drv_name++;
    }

    ADD2LOG("    driver = \"%s\"\n", drv_name);

    if((modalias = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      modalias = canon_str(modalias, strlen(modalias));
      ADD2LOG("    modalias = \"%s\"\n", modalias);
    }

    if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "device"), &ul0, 0)) {
      dev = ul0;
      ADD2LOG("    device = %u\n", dev);
    }
    else {
      dev = 0;
    }

    if(dev) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6014);	/* virtio */
      hd->device.id = MAKE_ID(TAG_SPECIAL, dev);

      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      if(drv_name) add_str_list(&hd->drivers, drv_name);
      if(modalias) { hd->modalias = modalias; modalias = NULL; }

      switch(dev) {
        case 1:	/* network */
          hd->bus.id = bus_virtio;
          hd->base_class.id = bc_network;
          hd->sub_class.id = 0;	/* ethernet */
          hd->slot = net_cnt++;
          str_printf(&hd->device.name, 0, "Ethernet Card %d", hd->slot);
          break;

        case 2:	/* storage */
          hd->bus.id = bus_virtio;
          hd->base_class.id = bc_storage;
          hd->sub_class.id = sc_sto_other;
          hd->slot = blk_cnt++;
          str_printf(&hd->device.name, 0, "Storage %d", hd->slot);
          break;
      }

      /*
       * virtio devs are kind of 'subdevices' to real pci devices; but
       * the supposedly 'real' devices mess up our device list :-(
       *
       * here we track down the 'real' devices and disable them in any
       * future device listing by classifying them right now as 'unknown'
       *
       * this works because devices will never be re-classified
       */
      s = new_str(hd->sysfs_id);	// get a writable copy

      if((t = strrchr(s, '/'))) {
        *t = 0;				// cut out last path element
        if((hd2 = hd_find_sysfs_id(hd_data, s))) {
          hd->attached_to = hd2->idx;
          // hasn't been classified yet and has the same base class
          if(
            !hd2->hw_class &&
            hd->base_class.id == hd2->base_class.id
          ) {
            hd2->hw_class = hw_unknown;
          }
        }
      }

      free_mem(s);
    }

    free_mem(modalias);

    free_mem(sf_dev);
    free_mem(drv);
  }

  free_str_list(sf_bus);
}


/*
 * uisvirtpci
 */
void hd_read_uisvirtpci(hd_data_t *hd_data)
{
  uint64_t ul0;
  hd_t *hd;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *drv, *drv_name;

  sf_bus = read_dir("/sys/bus/uisvirtpci/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: uisvirtpci\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/uisvirtpci/devices", sf_bus_e->str));

    ADD2LOG(
      "  uisvirtpci device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->vendor.id = MAKE_ID(TAG_PCI, 0xA0F1);	/* Unisys */

    drv_name = NULL;
    drv = new_str(hd_read_sysfs_link(sf_dev, "driver"));
    if(drv) {
        drv_name = strrchr(drv, '/');
        if(drv_name) {
            drv_name++;
            ADD2LOG("    driver = \"%s\"\n", drv_name);

            if(!strcmp(drv_name,"uisvirtnic")) {
              hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0002);
              hd->base_class.id = bc_network;
            }
            else if(!strcmp(drv_name,"uisvirthba")) {
              hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0003);
              hd->base_class.id = bc_storage;
            }
            else {
              hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0000);
              hd->base_class.id = bc_other;
            }
        }
    }
    else {
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
        hd->base_class.id = bc_bridge;
    }

   if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "device"), &ul0, 0)) {
       hd->device.id = MAKE_ID(TAG_SPECIAL, ul0 );
       ADD2LOG("    device = %lu\n", ul0);
    }

    hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
    hd->sysfs_bus_id = new_str(sf_bus_e->str);
    if(drv_name) add_str_list(&hd->drivers, drv_name);

    free_mem(sf_dev);
    free_mem(drv);
  }

  free_str_list(sf_bus);
}


/*
 * mmc
 */
void hd_read_mmc(hd_data_t *hd_data)
{
  int cnt;
  hd_t *hd;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *drv, *drv_name, *modalias, *mmc_type;

  sf_bus = read_dir("/sys/bus/mmc/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: mmc\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/mmc/devices", sf_bus_e->str));

    ADD2LOG(
      "  mmc device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    drv_name = NULL;
    drv = new_str(hd_read_sysfs_link(sf_dev, "driver"));
    if(drv) {
      drv_name = strrchr(drv, '/');
      if(drv_name) drv_name++;
    }

    ADD2LOG("    driver = \"%s\"\n", drv_name);

    if((modalias = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      modalias = canon_str(modalias, strlen(modalias));
      ADD2LOG("    modalias = \"%s\"\n", modalias);
    }

    if(!sf_bus_e->str || sscanf(sf_bus_e->str, "mmc%u", &cnt) != 1) {
      cnt = -1;
    }
    ADD2LOG("    index = %d\n", cnt);

    if((mmc_type = get_sysfs_attr_by_path(sf_dev, "type"))) {
      mmc_type = canon_str(mmc_type, strlen(mmc_type));
      ADD2LOG("    type = \"%s\"\n", mmc_type);
    }

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x6015);	/* mmc, see src/ids/src/special */
    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0000);
    if(mmc_type && !strcmp(mmc_type, "SD")) hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
    if(mmc_type && !strcmp(mmc_type, "SDIO")) hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0002);

    hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
    hd->sysfs_bus_id = new_str(sf_bus_e->str);
    if(drv_name) add_str_list(&hd->drivers, drv_name);
    if(modalias) { hd->modalias = modalias; modalias = NULL; }

    hd->bus.id = bus_mmc;
    hd->base_class.id = bc_mmc_ctrl;
    if(mmc_type && cnt >= 0) {
      hd->slot = cnt;
      str_printf(&hd->device.name, 0, "%s Controller %d", mmc_type, hd->slot);
    }
    else {
      str_printf(&hd->device.name, 0, "MMC Controller");
    }

    free_mem(mmc_type);
    free_mem(modalias);

    free_mem(sf_dev);
    free_mem(drv);
  }

  free_str_list(sf_bus);
}


/*
 * sdio
 */
void hd_read_sdio(hd_data_t *hd_data)
{
  uint64_t ul0;
  hd_t *hd, *hd2;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *drv, *drv_name, *modalias, *s, *t;

  sf_bus = read_dir("/sys/bus/sdio/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: sdio\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/sdio/devices", sf_bus_e->str));

    ADD2LOG(
      "  sdio device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    drv_name = NULL;
    drv = new_str(hd_read_sysfs_link(sf_dev, "driver"));
    if(drv) {
      drv_name = strrchr(drv, '/');
      if(drv_name) drv_name++;
    }

    ADD2LOG("    driver = \"%s\"\n", drv_name);

    if((modalias = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      modalias = canon_str(modalias, strlen(modalias));
      ADD2LOG("    modalias = \"%s\"\n", modalias);
    }

    hd = add_hd_entry(hd_data, __LINE__, 0);

   if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "vendor"), &ul0, 0)) {
       hd->vendor.id = MAKE_ID(TAG_SDIO, ul0);
       ADD2LOG("    vendor = %lu\n", ul0);
    }

   if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "device"), &ul0, 0)) {
       hd->device.id = MAKE_ID(TAG_SDIO, ul0);
       ADD2LOG("    device = %lu\n", ul0);
    }

    hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
    hd->sysfs_bus_id = new_str(sf_bus_e->str);
    if(drv_name) add_str_list(&hd->drivers, drv_name);
    if(modalias) { hd->modalias = modalias; modalias = NULL; }

    hd->bus.id = bus_sdio;

    s = new_str(hd->sysfs_id);	// get a writable copy

    if((t = strrchr(s, '/'))) {
      *t = 0;				// cut out last path element
      if((hd2 = hd_find_sysfs_id(hd_data, s))) {
        hd->attached_to = hd2->idx;
      }
    }

    free_mem(s);

    if(hd_read_sysfs_link(sf_dev, "net")) {
      hd->base_class.id = bc_network;
      hd->sub_class.id = sc_nif_other;
    }

    free_mem(modalias);

    free_mem(sf_dev);
    free_mem(drv);
  }

  free_str_list(sf_bus);
}


/*
 * nd (nvdimm)
 */
void hd_read_nd(hd_data_t *hd_data)
{
  int i, blk_cnt = 0;
  hd_t *hd, *hd2;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *drv, *drv_name, *modalias, *s, *t;

  sf_bus = read_dir("/sys/bus/nd/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: nd\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/nd/devices", sf_bus_e->str));

    ADD2LOG(
      "  nd device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    drv_name = NULL;
    drv = new_str(hd_read_sysfs_link(sf_dev, "driver"));
    if(drv) {
      drv_name = strrchr(drv, '/');
      if(drv_name) drv_name++;
    }

    ADD2LOG("    driver = \"%s\"\n", drv_name);

    if((modalias = get_sysfs_attr_by_path(sf_dev, "modalias"))) {
      modalias = canon_str(modalias, strlen(modalias));
      ADD2LOG("    modalias = \"%s\"\n", modalias);
    }

    if(hd_read_sysfs_link(sf_dev, "block")) {
      ADD2LOG("    block device\n");

      char *sysfs_id = new_str(hd_sysfs_id(sf_dev));
      // go 3 levels up (bus, region, namespace)
      for(i = 0; i < 3; i++) {
        if((t = strrchr(sysfs_id, '/'))) *t = 0;
      }

      // if we haven't created it yet...
      if(!hd_find_sysfs_id(hd_data, sysfs_id)) {
        hd = add_hd_entry(hd_data, __LINE__, 0);

        hd->base_class.id = bc_storage;
        hd->sub_class.id = sc_sto_other;

        hd->sysfs_id = sysfs_id;
        sysfs_id = NULL;
        hd->sysfs_bus_id = new_str(sf_bus_e->str);
        if(drv_name) add_str_list(&hd->drivers, drv_name);
        if(modalias) { hd->modalias = modalias; modalias = NULL; }

        hd->bus.id = bus_nd;
        hd->slot = blk_cnt++;
        str_printf(&hd->device.name, 0, "NVDIMM Storage %d", hd->slot);

        // check for a parent device and connect to it
        s = new_str(hd->sysfs_id);		// get a writable copy
        if((t = strrchr(s, '/'))) {
          *t = 0;				// cut out last path element
          if((hd2 = hd_find_sysfs_id(hd_data, s))) {
            hd->attached_to = hd2->idx;
          }
        }
        free_mem(s);
      }

      free_mem(sysfs_id);
    }

    free_mem(modalias);
    free_mem(sf_dev);
    free_mem(drv);
  }

  free_str_list(sf_bus);
}


/*
 * visorbus
 */
void hd_read_visorbus(hd_data_t *hd_data)
{
  uint64_t ul0;
  hd_t *hd;
  str_list_t *sf_bus, *sf_bus_e;
  char *sf_dev, *drv, *drv_name;

  sf_bus = read_dir("/sys/bus/visorbus/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: visorbus\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/visorbus/devices", sf_bus_e->str));

    ADD2LOG(
      "  visorbus device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->vendor.id = MAKE_ID(TAG_PCI, 0xA0F1);	/* Unisys */

    drv_name = NULL;
    drv = new_str(hd_read_sysfs_link(sf_dev, "driver"));
    if(drv) {
        drv_name = strrchr(drv, '/');
        if(drv_name) {
            drv_name++;
            ADD2LOG("    driver = \"%s\"\n", drv_name);

            if(!strcmp(drv_name,"visornic")) {
              hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0002);
              hd->base_class.id = bc_network;
            }
            else if(!strcmp(drv_name,"visorhba")) {
              hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0003);
              hd->base_class.id = bc_storage;
            }
            else {
              hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0000);
              hd->base_class.id = bc_other;
            }
        }
    }
    else {
        hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
        hd->base_class.id = bc_bridge;
    }

   if(hd_attr_uint(get_sysfs_attr_by_path(sf_dev, "device"), &ul0, 0)) {
       hd->device.id = MAKE_ID(TAG_SPECIAL, ul0 );
       ADD2LOG("    device = %lu\n", ul0);
    }

    hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
    hd->sysfs_bus_id = new_str(sf_bus_e->str);
    if(drv_name) add_str_list(&hd->drivers, drv_name);

    free_mem(sf_dev);
    free_mem(drv);
  }

  free_str_list(sf_bus);
}


/*
 * Get mdio data from sysfs.
 */
void hd_read_mdio(hd_data_t *hd_data)
{
  str_list_t *sf_bus, *sf_bus_e, *sf_eth_dev = NULL, *net_if;
  char *s, *sf_dev, *sf_eth_net;
  hd_t *hd;

  sf_bus = read_dir("/sys/bus/mdio_bus/devices", 'l');

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: mdio\n");
    return;
  }

  for(sf_bus_e = sf_bus; sf_bus_e; sf_bus_e = sf_bus_e->next) {
    sf_dev = new_str(hd_read_sysfs_link("/sys/bus/mdio_bus/devices", sf_bus_e->str));

    ADD2LOG(
      "  mdio device: name = %s\n    path = %s\n",
      sf_bus_e->str,
      hd_sysfs_id(sf_dev)
    );

    sf_eth_net = new_str(hd_read_sysfs_link(sf_dev, "net"));
    sf_eth_dev = read_dir(sf_eth_net, 'd');

    for(net_if = sf_eth_dev; net_if; net_if = net_if->next) {
      ADD2LOG("    mdio net: sf_eth_net = %s, if = %s\n", sf_eth_net, net_if->str);

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_network;
      hd->sub_class.id = 0;
      str_printf(&hd->device.name, 0, "Ethernet controller");
      hd->sysfs_id = new_str(hd_sysfs_id(sf_dev));
      hd->sysfs_bus_id = new_str(sf_bus_e->str);
      hd->unix_dev_name = new_str(net_if->str);
      s = hd_sysfs_find_driver(hd_data, hd->sysfs_id, 1);
      if(s) add_str_list(&hd->drivers, s);
    }

    free_mem(sf_eth_net);
    free_str_list(sf_eth_dev);

    free_mem(sf_dev);
  }

  free_str_list(sf_bus);
}

/** @} */

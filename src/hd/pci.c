#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/pci.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "pci.h"

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

static struct sysfs_attribute *hd_read_single_sysfs_attribute(char *path, char *name);
static void add_pci_data(hd_data_t *hd_data);
// static void add_driver_info(hd_data_t *hd_data);
static pci_t *add_pci_entry(hd_data_t *hd_data, pci_t *new_pci);
static unsigned char pci_cfg_byte(pci_t *pci, int fd, unsigned idx);
static void dump_pci_data(hd_data_t *hd_data);

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
}


/*
 * sysfs_get_device_attr() reads *all* device attributes, then returns the
 * requested one.
 *
 * This leads to problems where some attribute *must not* be read.
 */
struct sysfs_attribute *hd_read_single_sysfs_attribute(char *path, char *name)
{
  char *attr_path = NULL;
  struct sysfs_attribute *attr;

  str_printf(&attr_path, 0, "%s/%s", path, name);
  attr = sysfs_open_attribute(attr_path);
  free_mem(attr_path);

  sysfs_read_attribute(attr);

  return attr;
}


/*
 * Get the (raw) PCI data, taken from /sys/bus/pci/.
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
  int fd, i;

  struct sysfs_bus *sf_bus;
  struct dlist *sf_dev_list;
  struct sysfs_device *sf_dev;
  struct sysfs_attribute *attr;

  sf_bus = sysfs_open_bus("pci");

  if(!sf_bus) {
    ADD2LOG("sysfs: no such bus: pci\n");
    return;
  }

  sf_dev_list = sysfs_get_bus_devices(sf_bus);
  if(sf_dev_list) dlist_for_each_data(sf_dev_list, sf_dev, struct sysfs_device) {
    ADD2LOG(
      "  pci device: name = %s, bus_id = %s, bus = %s\n    path = %s\n",
      sf_dev->name,
      sf_dev->bus_id,
      sf_dev->bus,
      hd_sysfs_id(sf_dev->path)
    );

    if(sscanf(sf_dev->bus_id, "%x:%x:%x.%x", &u0, &u1, &u2, &u3) != 4) continue;

    pci = add_pci_entry(hd_data, new_mem(sizeof *pci));

    pci->sysfs_id = new_str(sf_dev->path);
    pci->sysfs_bus_id = new_str(sf_dev->bus_id);

    pci->bus = (u0 << 8) + u1;
    pci->slot = u2;
    pci->func = u3;

    if((s = hd_attr_str(attr = hd_read_single_sysfs_attribute(sf_dev->path, "modalias")))) {
      pci->modalias = canon_str(s, strlen(s));
      ADD2LOG("    modalias = \"%s\"\n", pci->modalias);
    }
    sysfs_close_attribute(attr);

    if(hd_attr_uint(attr = hd_read_single_sysfs_attribute(sf_dev->path, "class"), &ul0, 0)) {
      ADD2LOG("    class = 0x%x\n", (unsigned) ul0);
      pci->prog_if = ul0 & 0xff;
      pci->sub_class = (ul0 >> 8) & 0xff;
      pci->base_class = (ul0 >> 16) & 0xff;
    }
    sysfs_close_attribute(attr);

    if(hd_attr_uint(attr = hd_read_single_sysfs_attribute(sf_dev->path, "vendor"), &ul0, 0)) {
      ADD2LOG("    vendor = 0x%x\n", (unsigned) ul0);
      pci->vend = ul0 & 0xffff;
    }
    sysfs_close_attribute(attr);

    if(hd_attr_uint(attr = hd_read_single_sysfs_attribute(sf_dev->path, "device"), &ul0, 0)) {
      ADD2LOG("    device = 0x%x\n", (unsigned) ul0);
      pci->dev = ul0 & 0xffff;
    }
    sysfs_close_attribute(attr);

    if(hd_attr_uint(attr = hd_read_single_sysfs_attribute(sf_dev->path, "subsystem_vendor"), &ul0, 0)) {
      ADD2LOG("    subvendor = 0x%x\n", (unsigned) ul0);
      pci->sub_vend = ul0 & 0xffff;
    }
    sysfs_close_attribute(attr);

    if(hd_attr_uint(attr = hd_read_single_sysfs_attribute(sf_dev->path, "subsystem_device"), &ul0, 0)) {
      ADD2LOG("    subdevice = 0x%x\n", (unsigned) ul0);
      pci->sub_dev = ul0 & 0xffff;
    }
    sysfs_close_attribute(attr);

    if(hd_attr_uint(attr = hd_read_single_sysfs_attribute(sf_dev->path, "irq"), &ul0, 0)) {
      ADD2LOG("    irq = %d\n", (unsigned) ul0);
      pci->irq = ul0;
    }
    sysfs_close_attribute(attr);

    sl = hd_attr_list(attr = hd_read_single_sysfs_attribute(sf_dev->path, "resource"));
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
    sysfs_close_attribute(attr);

    s = NULL;
    str_printf(&s, 0, "%s/config", sf_dev->path);
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

    str_printf(&s, 0, "%s/edid1", sf_dev->path);
    if((fd = open(s, O_RDONLY)) != -1) {
      pci->edid_len = read(fd, pci->edid, sizeof pci->edid);

      ADD2LOG("    edid[%u]\n", pci->edid_len);

      if(pci->edid_len > 0) {
        for(i = 0; i < sizeof pci->edid; i += 0x10) {
          ADD2LOG("      ");
          hexdump(&hd_data->log, 1, 0x10, pci->edid + i);
          ADD2LOG("\n");
        }
      }

      close(fd);
    }

    s = free_mem(s);

    pci->rev = pci->data[PCI_REVISION_ID];

    if((pci->addr_flags[6] & IORESOURCE_MEM)) {
      if(!(pci->data[PCI_ROM_ADDRESS] & PCI_ROM_ADDRESS_ENABLE)) {
        pci->addr_flags[6] |= IORESOURCE_DISABLED;
      }
    }

    pci->flags |= (1 << pci_flag_ok);
  }

  sysfs_close_bus(sf_bus);
}


void add_pci_data(hd_data_t *hd_data)
{
  hd_t *hd, *hd2;
  pci_t *pci, *pnext;
  unsigned u;
  char *s, *t;

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
      hexdump(&hd_data->log, 1, j > 0x10 ? 0x10 : j, pci->data + i);
      ADD2LOG("\n");
    }

    if(pci->next) ADD2LOG("\n");
  }

  ADD2LOG("---------- PCI raw data end ----------\n");
}


/*
 * Parse attribute and return integer value.
 */
int hd_attr_uint(struct sysfs_attribute *attr, uint64_t *u, int base)
{
  char *s;
  uint64_t u2;
  int ok;

  if(!(s = hd_attr_str(attr))) return 0;

  u2 = strtoull(s, &s, base);
  ok = !*s || isspace(*s) ? 1 : 0;

  if(ok && u) *u = u2;

  return ok;
}


/*
 * Return attribute as string list.
 */
str_list_t *hd_attr_list(struct sysfs_attribute *attr)
{
  static str_list_t *sl = NULL;

  free_str_list(sl);

  return sl = hd_split('\n', hd_attr_str(attr));
}


/*
 * Return attribute as string.
 */
char *hd_attr_str(struct sysfs_attribute *attr)
{
  return attr ? attr->value : NULL;
}


/*
 * Remove leading "/sys" from path.
 */
char *hd_sysfs_id(char *path)
{
  if(!path || !*path) return NULL;

  return strchr(path + 1, '/');
}


/*
 * Convert '!' to '/'.
 */
char *hd_sysfs_name2_dev(char *str)
{
  static char *s = NULL;

  if(!str) return NULL;

  free_mem(s);
  s = str = new_str(str);

  while(*str) {
    if(*str == '!') *str = '/';
    str++;
  }

  return s;
}


/*
 * Convert '/' to '!'.
 */
char *hd_sysfs_dev2_name(char *str)
{
  static char *s = NULL;

  if(!str) return NULL;

  free_mem(s);
  s = str = new_str(str);

  while(*str) {
    if(*str == '/') *str = '!';
    str++;
  }

  return s;
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/pci.h>

#include "hd.h"
#include "hd_int.h"
#include "pci.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * pci stuff
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static void get_pci_data(hd_data_t *hd_data);
static void dump_pci_data(hd_data_t *hd_data);
static int f_read(int fd, off_t ofs, void *buf, size_t len);
static int f_write(int fd, off_t ofs, void *buf, size_t len);
static unsigned get_pci_addr_range(hd_data_t *hd_data, pci_t *pci, int fd, unsigned addr, unsigned mask);
static pci_t *add_pci_entry(hd_data_t *hd_data, pci_t *new_pci);

void hd_scan_pci(hd_data_t *hd_data)
{
  hd_t *hd;
  pci_t *p;
  hd_res_t *res;
  int j;
  unsigned long ul;

  if(!hd_probe_feature(hd_data, pr_pci)) return;

  hd_data->module = mod_pci;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->pci = NULL;

  PROGRESS(1, 0, "get pci data");

  get_pci_data(hd_data);
  if((hd_data->debug & HD_DEB_PCI)) dump_pci_data(hd_data);

  PROGRESS(4, 0, "build list");

  for(p = hd_data->pci; p; p = p->next) {
    hd = add_hd_entry(hd_data, __LINE__, 0);

    hd->bus = bus_pci;
    hd->slot = p->slot + (p->bus << 8);
    hd->func = p->func;
    hd->base_class = p->base_class;
    hd->sub_class = p->sub_class;
    hd->prog_if = p->prog_if;

    hd->dev = p->dev;
    hd->vend = p->vend;
    hd->sub_dev = p->sub_dev;
    hd->sub_vend = p->sub_vend;
    hd->rev = p->rev;

    for(j = 0; j < 6; j++) {
      ul = p->base_addr[j];
      if(ul & ~PCI_BASE_ADDRESS_SPACE) {
        if((ul & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO) {
          res = new_mem(sizeof *res);
          res->io.type = res_io;
          res->io.enabled = p->cmd & PCI_COMMAND_IO ? 1 : 0;
          res->io.base =  ul & PCI_BASE_ADDRESS_IO_MASK;
          res->io.range = p->base_len[j];
          res->io.access = acc_rw;
          add_res_entry(&hd->res, res);
        }
        if((ul & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY) {
          res = new_mem(sizeof *res);
          res->mem.type = res_mem;
          res->mem.enabled = p->cmd & PCI_COMMAND_MEMORY ? 1 : 0;
          res->mem.base =  ul & PCI_BASE_ADDRESS_MEM_MASK;
          res->mem.range = p->base_len[j];
          res->mem.access = acc_rw;
          res->mem.prefetch = ul & PCI_BASE_ADDRESS_MEM_PREFETCH ? flag_yes : flag_no;
          add_res_entry(&hd->res, res);
        }
      }
    }
    if((ul = p->rom_base_addr)) {
      res = new_mem(sizeof *res);
      res->mem.type = res_mem;
      res->mem.enabled = ul & PCI_ROM_ADDRESS_ENABLE ? 1 : 0;
      res->mem.base =  ul & PCI_ROM_ADDRESS_MASK;
      res->mem.range = p->rom_base_len;
      res->mem.access = acc_ro;
      add_res_entry(&hd->res, res);
    }

    if(p->irq) {
      res = new_mem(sizeof *res);
      res->irq.type = res_irq;
      res->irq.enabled = 1;
      res->irq.base = p->irq;
      add_res_entry(&hd->res, res);
    }

    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_pci;
    hd->detail->pci.data = p;
  }
}


/*
 * Get the (raw) PCI data.
 * The device list is taken from /proc/bus/pci/devices,
 * individual device info from /proc/bus/pci/.
 *
 * Note: non-root users can only read the first 64 bytes (of 256)
 * of the device headers.
 */
void get_pci_data(hd_data_t *hd_data)
{
  unsigned char *t;
  unsigned long u, ul[10], nxt;
  uint64 u64;
  int fd, i, j, o_fl;
  pci_t *p;
  char *pci_data_file = NULL;
  str_list_t *sl, *sl0;
  int prog_cnt = 0;

  /*
   * Read the devices file and build a list of all PCI devices.
   * The list holds preliminary info that gets extended later.
   */
  if(!(sl0 = read_file(PROC_PCI_DEVICES, 0, 0))) return;
  for(sl = sl0; sl; sl = sl->next) {
    if(
      sscanf(sl->str,
        " %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx",
        ul, ul + 1, ul + 2, ul + 3, ul + 4, ul + 5, ul + 6, ul + 7, ul + 8, ul + 9
      ) == 10
    ) {
      p = add_pci_entry(hd_data, new_mem(sizeof *p));

      p->bus = ul[0] >> 8;
      /* combine them, as we have no extra field for the bus index */
      p->slot = PCI_SLOT(ul[0] & 0xff);
      p->func = PCI_FUNC(ul[0] & 0xff);

      p->dev = ul[1] & 0xffff;
      p->vend = (ul[1] >> 16) & 0xffff;

      p->irq = ul[2];
      for(i = 0; i < 6; i++) p->base_addr[i] = ul[i + 3];
      p->rom_base_addr = ul[9];
    }
  }

  sl0 = free_str_list(sl0);

  /*
   * Now add the full PCI info.
   */
  for(p = hd_data->pci; p; p = p->next) {
    str_printf(&pci_data_file, 0, PROC_PCI_BUS "/%02x/%02x.%x", p->bus, p->slot, p->func);
    if(
      (fd = open(pci_data_file, o_fl = O_RDWR)) >= 0 ||
      (fd = open(pci_data_file, o_fl = O_RDONLY)) >= 0
    ) {
      PROGRESS(2, ++prog_cnt, "raw data");

      p->data_len = read(fd, p->data, sizeof p->data);
      if(p->data_len >= 0x40) {
        p->hdr_type = p->data[PCI_HEADER_TYPE] & 0x7f;
        p->cmd = p->data[PCI_COMMAND] + (p->data[PCI_COMMAND + 1] << 8);
        ul[0] = p->data[PCI_VENDOR_ID] + (p->data[PCI_VENDOR_ID + 1] << 8);
        ul[1] = p->data[PCI_DEVICE_ID] + (p->data[PCI_DEVICE_ID + 1] << 8);
        if(ul[0] == p->vend && ul[1] == p->dev) {
          /* these are header type specific */
          if(p->hdr_type == PCI_HEADER_TYPE_NORMAL) {
            p->sub_dev = p->data[PCI_SUBSYSTEM_ID] + (p->data[PCI_SUBSYSTEM_ID + 1] << 8);
            p->sub_vend = p->data[PCI_SUBSYSTEM_VENDOR_ID] + (p->data[PCI_SUBSYSTEM_VENDOR_ID + 1] << 8);
          }
          else if(p->hdr_type == PCI_HEADER_TYPE_CARDBUS) {
            p->sub_dev = p->data[PCI_CB_SUBSYSTEM_ID] + (p->data[PCI_CB_SUBSYSTEM_ID + 1] << 8);
            p->sub_vend = p->data[PCI_CB_SUBSYSTEM_VENDOR_ID] + (p->data[PCI_CB_SUBSYSTEM_VENDOR_ID + 1] << 8);
          }

          p->rev = p->data[PCI_REVISION_ID];
          p->prog_if = p->data[PCI_CLASS_PROG];
          p->sub_class = p->data[PCI_CLASS_DEVICE];
          p->base_class = p->data[PCI_CLASS_DEVICE + 1];
          p->flags |= (1 << pci_flag_ok);

          /*
           * See if we can get the adress *ranges*. This does actuall imply
           * reprogramming the PCI devices. As this is somewhat dangerous in
           * a running system, this feature (pr_pci_range) is normally turned
           * off. (The check is actually in get_pci_addr_range().)
           */
          if(p->hdr_type == PCI_HEADER_TYPE_NORMAL) {
            PROGRESS(3, prog_cnt, "address ranges");

            for(j = 0; j < 6; j++) {
              t = p->data + PCI_BASE_ADDRESS_0 + 4 * j;
              u = t[0] + (t[1] << 8) + (t[2] << 16) + (t[3] << 24);
              /* just checking; actually it's paranoid... */
              if(u == p->base_addr[j]) {
                if(u && o_fl == O_RDWR)
                  p->base_len[j] = get_pci_addr_range(hd_data, p, fd, PCI_BASE_ADDRESS_0 + 4 * j, 0);
              }
              else {
                p->base_addr[j] = u;
                if(u && o_fl == O_RDWR)
                  p->base_len[j] = get_pci_addr_range(hd_data, p, fd, PCI_BASE_ADDRESS_0 + 4 * j, 0);
              }
              if(
                (u & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY &&
                (u & PCI_BASE_ADDRESS_MEM_TYPE_MASK) == PCI_BASE_ADDRESS_MEM_TYPE_64 &&
                j < 5
              ) {
                u64 = t[4] + (t[5] << 8) + (t[6] << 16) + (t[7] << 24);
                p->base_addr[j] += u64 << 32;
                // get the range!
                // ##### the get_pci_addr_range() stuff is awkward
                j++;
              }
            }
            if(p->rom_base_addr)
              p->rom_base_len = get_pci_addr_range(hd_data, p, fd, PCI_ROM_ADDRESS, (unsigned) PCI_ROM_ADDRESS_MASK);
          }

          /* let's get through the capability list */
          if(p->hdr_type == PCI_HEADER_TYPE_NORMAL && (nxt = p->data[PCI_CAPABILITY_LIST])) {
            /*
             * Cut out after 20 capabilities to avoid infinite recursion due
             * to (potentially) malformed data. 20 *is* completely
             * arbitrary, though.
             */
            for(j = 0; j < 20 && nxt && nxt <= 0xfe; j++) {
              switch(p->data[nxt]) {
                case PCI_CAP_ID_PM:
                  p->flags |= (1 << pci_flag_pm);
                  break;

                case PCI_CAP_ID_AGP:
                  p->flags |= (1 << pci_flag_agp);
                  break;
              }
              nxt = p->data[nxt + 1];
            }
          }
        }
      }
      close(fd);
    }
  }

  free_mem(pci_data_file);
}


/*
 * Read from a file.
 */
int f_read(int fd, off_t ofs, void *buf, size_t len)
{
  if(lseek(fd, ofs, SEEK_SET) == -1) return -1;
  return read(fd, buf, len);
}

/*
 * Write to a file.
 */
int f_write(int fd, off_t ofs, void *buf, size_t len)
{
  if(lseek(fd, ofs, SEEK_SET) == -1) return -1;
  return write(fd, buf, len);
}


/*
 * Determine the address ranges by writing -1 to the base regs and
 * looking on the number of 1-bits.
 * This assumes the range to be a power of 2.
 *
 * This function *is* dangerous to execute - you have been warned... :-)
 *
 * ##### FIX: 32bit vs. 64bit !!!!!!!!!!
 * ##### It breaks if unsigned != 32 bit!
 */
unsigned get_pci_addr_range(hd_data_t *hd_data, pci_t *pci, int fd, unsigned addr, unsigned mask)
{
  unsigned u, u0, u1 = -1, cmd = 0, cmd1;
  int err = 0;

  /* it's easier to do the check *here* */
  if(!hd_probe_feature(hd_data, pr_pci_range)) return 0;

  /* PCI_COMMAND is a 16 bit value */
  if(f_read(fd, PCI_COMMAND, &cmd, 2) != 2) return 0;

  if(f_read(fd, addr, &u0, sizeof u0) != sizeof u0) return 0;

  /* disable memory and i/o adresses */
  cmd1 = cmd & ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
  if(f_write(fd, PCI_COMMAND, &cmd1, 2) != 2) err = 1;

  if(!err) {
    /* remember error conditions, but we must get through... */

    /*
     * write -1 and read the value back
     *
     * ???: What about the address type bits? Should they be preserved?
     */
    if(f_write(fd, addr, &u1, sizeof u1) != sizeof u1) err = 2;
    if(f_read(fd, addr, &u1, sizeof u1) != sizeof u1) err = 3;

    /* restore original value */
    if(f_write(fd, addr, &u0, sizeof u0) != sizeof u0) err = 4;
  }

  /* restore original value */
  if(f_write(fd, PCI_COMMAND, &cmd, 2) != 2) err = 5;

  if(!err) {
    /* a mask of 0 is slightly magic... */
    if(mask) {
      u = u1 & mask;
    }
    else {
#ifdef __i386__
      /* Intel processors have 16 bit i/o space */
      if((u0 & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO) u1 |= 0xffff0000;
#endif
      u = u1 & (
        (u0 & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO ?
          PCI_BASE_ADDRESS_IO_MASK :
          PCI_BASE_ADDRESS_MEM_MASK
      );
    }
  }
  else {
    u = 0;
  }

  if(err)
    str_printf(&pci->log, -1, "  err %d: %x %x\n", err, u0, -u);

  return -u;
}


/*
 * Store a raw PCI entry; just for convenience.
 */
pci_t *add_pci_entry(hd_data_t *hd_data, pci_t *new_pci)
{
  pci_t **pci = &hd_data->pci;

  while(*pci) pci = &(*pci)->next;

  return *pci = new_pci;
}


/*
 * Add a dump of all raw PCI data to the global log.
 */
void dump_pci_data(hd_data_t *hd_data)
{
  pci_t *p;
  char *s = NULL;
  int i, j;

  ADD2LOG("---------- PCI raw data ----------\n");

  for(p = hd_data->pci; p; p = p->next) {

    if(!(p->flags & (1 << pci_flag_ok))) str_printf(&s, -1, "oops");
    if(p->flags & (1 << pci_flag_pm)) str_printf(&s, -1, ",pm");
    if(p->flags & (1 << pci_flag_agp)) str_printf(&s, -1, ",agp");
    if(!s) str_printf(&s, 0, "%s", "");

    ADD2LOG(
      "bus %02x, slot %02x, func %x, vend:dev:s_vend:s_dev:rev %04x:%04x:%04x:%04x:%02x\n",
      p->bus, p->slot, p->func, p->vend, p->dev, p->sub_vend, p->sub_dev, p->rev
    );
    ADD2LOG(
      "class %02x, sub_class %02x prog_if %02x, hdr %x, flags <%s>, irq %u\n",
      p->base_class, p->sub_class, p->prog_if, p->hdr_type, *s == ',' ? s + 1 : s, p->irq 
    );

    s = free_mem(s);

    for(i = 0; i < 6; i++) {
      if(p->base_addr[i] || p->base_len[i])
        ADD2LOG("  addr%d %08x, size %08x\n", i, p->base_addr[i], p->base_len[i]);
    }
    if(p->rom_base_addr)
      ADD2LOG("  rom   %08x\n", p->rom_base_addr);

    if(p->log) ADD2LOG("%s", p->log);

    for(i = 0; i < p->data_len; i += 0x10) {
      ADD2LOG("  %02x: ", i);
      j = p->data_len - i;
      hexdump(&hd_data->log, 1, j > 0x10 ? 0x10 : j, p->data + i);
      ADD2LOG("\n");
    }

    if(p->next) ADD2LOG("\n");
  }

  ADD2LOG("---------- PCI raw data end ----------\n");
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/hdreg.h>

#include "hd.h"
#include "hd_int.h"
#include "misc.h"
#include "klog.h"

static void read_ioports(misc_t *m);
static void read_dmas(misc_t *m);
static void read_irqs(misc_t *m);
static int active_vga_card(hd_t *);

static void dump_misc_proc_data(hd_data_t *hd_data);
static void dump_misc_data(hd_data_t *hd_data);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * misc info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void hd_scan_misc(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res;
  int fd, i;
  char *s = NULL;
  bios_info_t *bt = NULL;
  char par[] = "parport0";
  int fd_ser0, fd_ser1;

  if(!hd_probe_feature(hd_data, pr_misc)) return;

  hd_data->module = mod_misc;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->misc = free_misc(hd_data->misc);

  PROGRESS(9, 0, "kernel log");
  read_klog(hd_data);
  if((hd_data->debug & HD_DEB_MISC)) dump_klog(hd_data);

  PROGRESS(1, 0, "misc data");
  hd_data->misc = new_mem(sizeof *hd_data->misc);

  /* this is enough to load the module */
  fd_ser0 = fd_ser1 = -1;

#if !defined(__sparc__)
  /* On sparc, the close needs too long */
  if(hd_probe_feature(hd_data, pr_misc_serial)) {
    PROGRESS(1, 1, "open serial");
    fd_ser0 = open("/dev/ttyS0", O_RDONLY | O_NONBLOCK);
    fd_ser1 = open("/dev/ttyS1", O_RDONLY | O_NONBLOCK);
    /* keep the devices open until the resources have been read */
  }
#endif

  /* this is enough to load the module */
  if(!hd_data->flags.no_parport && hd_probe_feature(hd_data, pr_misc_par)) {
    PROGRESS(1, 2, "open parallel");
    /* what can the BIOS tell us? */
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_internal &&
        hd->sub_class.id == sc_int_bios &&
        hd->detail &&
        hd->detail->type == hd_detail_bios &&
        hd->detail->bios.data
      ) break;
    }
    if(hd) {
      bt = hd->detail->bios.data;
      if(bt->par_port0) {
        str_printf(&s, 0, "io=0x%x", bt->par_port0);
        if(bt->par_port1) {
          str_printf(&s, -1, ",0x%x", bt->par_port1);
          if(bt->par_port2) str_printf(&s, -1, ",0x%x", bt->par_port2);
        }
	str_printf(&s, -1, " irq=none,none,none");
      }
      unload_module(hd_data, "parport_probe");
      unload_module(hd_data, "lp");
      unload_module(hd_data, "parport_pc");
      unload_module(hd_data, "parport");

      /* now load it with the right io */
      load_module(hd_data, "parport");
      load_module_with_params(hd_data, "parport_pc", s);
      free_mem(s);
    }
    /* now load the rest of the modules */
    fd = open("/dev/lp0", O_RDONLY | O_NONBLOCK);
    if(fd >= 0) close(fd);
  }

  /*
   * floppy driver resources are allocated only temporarily,
   * so we access it just before we read the resources
   */
  if(hd_probe_feature(hd_data, pr_misc_floppy)) {
    /* look for a floppy *device* entry... */
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_storage_device &&
        hd->sub_class.id == sc_sdev_floppy &&
        hd->unix_dev_name &&
        !strncmp(hd->unix_dev_name, "/dev/fd", sizeof "/dev/fd" - 1)
      ) {

        PROGRESS(1, 3, "read floppy");
        i = 5;
        hd->block0 = read_block0(hd_data, hd->unix_dev_name, &i);
        hd->is.notready = hd->block0 ? 0 : 1;
        if(i < 0) {
          hd->tag.remove = 1;
          ADD2LOG("misc.floppy: removing floppy entry %u (timed out)\n", hd->idx);
        }

        if(!hd->is.notready) {
          struct hd_geometry geo;
          int fd;
          unsigned size, blk_size = 0x200;

          fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK);
          if(fd >= 0) {
            if(!ioctl(fd, HDIO_GETGEO, &geo)) {
              ADD2LOG("floppy ioctl(geo) ok\n");
              res = add_res_entry(&hd->res, new_mem(sizeof *res));
              res->disk_geo.type = res_disk_geo;
              res->disk_geo.cyls = geo.cylinders;
              res->disk_geo.heads = geo.heads;
              res->disk_geo.sectors = geo.sectors;
              res->disk_geo.geotype = geo_logical;
              size = geo.cylinders * geo.heads * geo.sectors;
              for(res = hd->res; res; res = res->next) {
                if(res->any.type == res_size && res->size.unit == size_unit_sectors) {
                  res->size.val1 = size; res->size.val2 = blk_size;
                  break;
                }
              }
              if(!res) {
                res = add_res_entry(&hd->res, new_mem(sizeof *res));
                res->size.type = res_size;
                res->size.unit = size_unit_sectors;
                res->size.val1 = size; res->size.val2 = blk_size;
              }
            }
            close(fd);
          }
        }

        break;
      }
    }
    remove_tagged_hd_entries(hd_data);
  }

  PROGRESS(2, 1, "io");
  read_ioports(hd_data->misc);

  PROGRESS(2, 2, "dma");
  read_dmas(hd_data->misc);

  PROGRESS(2, 3, "irq");
  read_irqs(hd_data->misc);

  if((hd_data->debug & HD_DEB_MISC)) dump_misc_proc_data(hd_data);

  if(fd_ser0 >= 0) close(fd_ser0);
  if(fd_ser1 >= 0) close(fd_ser1);

  /* now create some system generic entries */

  /* FPU */
  PROGRESS(3, 0, "FPU");
  res = NULL;
  gather_resources(hd_data->misc, &res, "fpu", 0);
  if(res) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_internal;
    hd->sub_class.id = sc_int_fpu;
    hd->res = res;
  }

  /* DMA */
  PROGRESS(3, 1, "DMA");
  res = NULL;
  gather_resources(hd_data->misc, &res, "dma1", 0);
  gather_resources(hd_data->misc, &res, "dma2", 0);
  gather_resources(hd_data->misc, &res, "dma page reg", 0);
  gather_resources(hd_data->misc, &res, "cascade", W_DMA);
  if(res) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_system;
    hd->sub_class.id = sc_sys_dma;
    hd->res = res;
  }

  /* PIC */
  PROGRESS(3, 2, "PIC");
  res = NULL;
  gather_resources(hd_data->misc, &res, "pic1", 0);
  gather_resources(hd_data->misc, &res, "pic2", 0);
  gather_resources(hd_data->misc, &res, "cascade", W_IRQ);
  if(res) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_system;
    hd->sub_class.id = sc_sys_pic;
    hd->res = res;
  }

  /* timer */
  PROGRESS(3, 3, "timer");
  res = NULL;
  gather_resources(hd_data->misc, &res, "timer", 0);
  if(res) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_system;
    hd->sub_class.id = sc_sys_timer;
    hd->res = res;
  }

  /* real time clock */
  PROGRESS(3, 4, "RTC");
  res = NULL;
  gather_resources(hd_data->misc, &res, "rtc", 0);
  if(res) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_system;
    hd->sub_class.id = sc_sys_rtc;
    hd->res = res;
  }

  /* keyboard */
  res = NULL;
  gather_resources(hd_data->misc, &res, "keyboard", 0);
  if(res) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_input;
    hd->sub_class.id = sc_inp_keyb;
    hd->res = res;
  }

  /* parallel ports */
  for(i = 0; i < 1; i++, par[sizeof par - 2]++) {
    res = NULL;
    gather_resources(hd_data->misc, &res, par, 0);
    if(res) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_comm;
      hd->sub_class.id = sc_com_par;
      str_printf(&hd->unix_dev_name, 0, "/dev/lp%d", i);
      hd->res = res;
    }
  }

  /* floppy controller */
  res = NULL;
  gather_resources(hd_data->misc, &res, "floppy", 0);
  gather_resources(hd_data->misc, &res, "floppy DIR", 0);
  if(res) {
    /* look for an existing entry */
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->base_class.id == bc_storage && hd->sub_class.id == sc_sto_floppy) break;
    }

    /* missing, so create one */
    if(!hd) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_storage;
      hd->sub_class.id = sc_sto_floppy;
    }

    hd->res = res;
  }

  /*
   * look for PS/2 port
   *
   * The catch is, that sometimes /dev/psaux is accessible only for root,
   * so the open() may fail but there are irq events registered.
   *
   */
  fd = open(DEV_PSAUX, O_RDONLY | O_NONBLOCK);
  if(fd >= 0) close(fd);

  res = NULL;
  gather_resources(hd_data->misc, &res, "PS/2 Mouse", 0);

  if(res || fd >= 0) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_ps2;

    if(res) {
      hd->res = res;
    }
  }
}


void hd_scan_misc2(hd_data_t *hd_data)
{
  hd_t *hd, *hd1;
  misc_t *m;
  hd_res_t *res, *res1, *res2;
  int i;

  if(!hd_probe_feature(hd_data, pr_misc)) return;

  hd_data->module = mod_misc;

  PROGRESS(5, 0, "misc data");

  /* create some more system generic entries */

  /* IDE */

// ###### add special ide detail to hd_t!!!
  res = NULL;
  gather_resources(hd_data->misc, &res, "ide0", 0);
  gather_resources(hd_data->misc, &res, "ide1", 0);
  gather_resources(hd_data->misc, &res, "ide2", 0);
  gather_resources(hd_data->misc, &res, "ide3", 0);
  if(res) {
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class.id == bc_storage &&
        hd->sub_class.id == sc_sto_ide &&
        have_common_res(hd->res, res)
      ) break;
    }
    if(!hd) {
      /* eg. non-PCI IDE controller */
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_storage;
      hd->sub_class.id = sc_sto_ide;
      /* use join_res to join the i/o ranges of ide0/1 */
      join_res_io(&hd->res, res);
      join_res_irq(&hd->res, res);
      join_res_dma(&hd->res, res);
      free_res_list(res);
    }
    else {
      /* eg. PCI IDE controller, add resources */
      join_res_io(&hd->res, res);
      join_res_irq(&hd->res, res);
      join_res_dma(&hd->res, res);
      free_res_list(res);
    }
  }

  /* VGA */
  res = NULL;
  gather_resources(hd_data->misc, &res, "vga+", 0);
  gather_resources(hd_data->misc, &res, "vesafb", 0);
  if(res) {
    for(i = 0, hd1 = NULL, hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->base_class.id == bc_display && hd->sub_class.id == sc_dis_vga) {
        i++;
        hd1 = hd;
      }
    }
    if(i == 0) {
      /* non-PCI VGA card ??? - really, we shouldn't care... */
      /* FIX THIS !!! ############### */
#ifdef __alpha__
      free_res_list(res);
#else
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_display;
      hd->sub_class.id = sc_dis_vga;
      hd->res = res;
#endif
    }
    else if(i == 1) {
      /* 1 PCI vga card, add resources */
      join_res_io(&hd1->res, res);
      join_res_irq(&hd1->res, res);
      join_res_dma(&hd1->res, res);
      free_res_list(res);
      hd_data->display = hd1->idx;
    }
    else {
      /* more than 1: look again, now only 'active' cards */
      for(i = 0, hd1 = NULL, hd = hd_data->hd; hd; hd = hd->next) {
        if(
          hd->base_class.id == bc_display &&
          hd->sub_class.id == sc_dis_vga &&
          active_vga_card(hd)
        ) {
          i++;
          hd1 = hd;
        }
      }
      if(i == 1) {
        /* 'the' active PCI vga card, add resources */
        join_res_io(&hd1->res, res);
        join_res_irq(&hd1->res, res);
        join_res_dma(&hd1->res, res);
        hd_data->display = hd1->idx;
      }
      else {
       /* now, what??? */
       ADD2LOG("Oopy, could not figure out *the* active display adapter!\n");
      }
      free_res_list(res);
    }
  }

  /* serial ports */
  res = NULL;
  gather_resources(hd_data->misc, &res, "serial(auto)", 0);
  gather_resources(hd_data->misc, &res, "serial(set)", 0);
  gather_resources(hd_data->misc, &res, "serial", 0);
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(hd->base_class.id == bc_comm && hd->sub_class.id == sc_com_ser) {
      for(res1 = hd->res; res1; res1 = res1->next) {
        for(res2 = res; res2; res2 = res2->next) {
          if(res1->any.type == res2->any.type) {
            switch(res1->any.type) {
              case res_irq:
                if(res1->irq.base == res2->irq.base) {
                  res2->any.type = res_any;
                }
                break;
              case res_io:
                if(
                  res1->io.base == res2->io.base &&
                  (!res1->io.range || res1->io.range == res2->io.range)
                ) {
                  res1->io.range = res2->io.range;
                  res2->any.type = res_any;
                }
                break;
              default:		/* gcc -Wall */
		break;
            }
          }
        }
      }
    }
  }

  /* if any of the serial resources are unaccounted for, make an extra entry */
  for(res2 = res; res2; res2 = res2->next) {
    if(res2->any.type != res_any) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_comm;
      hd->sub_class.id = sc_com_ser;
      hd->prog_if.id = 0x80;
      for(; res2; res2 = res2->next) {
        if(res2->any.type != res_any) {
          res1 = add_res_entry(&hd->res, new_mem(sizeof *res));
          *res1 = *res2;
          res1->next = NULL;
        }
      }
      break;
    }
  }
  free_res_list(res);

  /* go through our list and assign event counts to irq entries */
  m = hd_data->misc;
  for(hd = hd_data->hd; hd; hd = hd->next) {
    for(res = hd->res; res; res = res->next) {
      if(res->irq.type == res_irq) {
        for(i = 0; (unsigned) i < m->irq_len; i++) {
          if(res->irq.base == m->irq[i].irq) {
            res->irq.triggered = m->irq[i].events;
            break;
          }
        }
      }
    }
  }

  /* look for entries with matching start address */
  m = hd_data->misc;
  for(hd = hd_data->hd; hd; hd = hd->next) {
    for(res = hd->res; res; res = res->next) {
      if(res->io.type == res_io) {
        for(i = 0; (unsigned) i < m->io_len; i++) {
          if(res->io.base == m->io[i].addr && res->io.range < m->io[i].size) {
            res->io.range = m->io[i].size;
            break;
          }
        }
      }
    }
  }

  if((hd_data->debug & HD_DEB_MISC)) dump_misc_data(hd_data);
}


/*
 * read /proc/ioports
 */
void read_ioports(misc_t *m)
{
  char buf[100];
  misc_io_t *r;
  uint64_t u, v;
  str_list_t *sl;

  if(!(m->proc_io = read_file(PROC_IOPORTS, 0, 0))) return;

  for(sl = m->proc_io; sl; sl = sl->next) {
    if(sscanf(sl->str, " %"PRIx64" - %"PRIx64" : %99[^\n]", &u, &v, buf) == 3) {
      m->io = add_mem(m->io, sizeof *m->io, m->io_len);
      r = m->io + m->io_len++;
      r->addr = u;
      r->size = v >= u ? v - u + 1 : 0;
      r->dev = new_str(buf);
    }
  }
}

/*
 * read /proc/dma
 */
void read_dmas(misc_t *m)
{
  char buf[100];
  misc_dma_t *d;
  unsigned u;
  str_list_t *sl;

  if(!(m->proc_dma = read_file(PROC_DMA, 0, 0))) return;

  for(sl = m->proc_dma; sl; sl = sl->next) {
    if(sscanf(sl->str, " %u : %99[^\n]", &u, buf) == 2) {
      m->dma = add_mem(m->dma, sizeof *m->dma, m->dma_len);
      d = m->dma + m->dma_len++;
      d->channel = u;
      d->dev = new_str(buf);
    }
  }
}


/*
 * read /proc/interrupts
 *
 * This is somewhat more tricky, as the irq event counts are done separately
 * per cpu *and* there may be irq sharing.
 */
void read_irqs(misc_t *m)
{
  char buf[100], buf2[100], *s;
  misc_irq_t *ir;
  int i, j;
  unsigned u, v, k;
  str_list_t *sl;

  if(!(m->proc_irq = read_file(PROC_INTERRUPTS, 1, 0))) return;

  for(sl = m->proc_irq; sl; sl = sl->next) {
    /* irq */
    i = 0;
    if(sscanf(sl->str, " %u: %n", &u, &i) < 1) continue;
    v = 0;
    j = i;
    /* add up all event counters */
    while(j < (int) strlen(sl->str) && sscanf(sl->str + j, " %u %n", &k, &i) >= 1) {
      if(!i) break;
      v += k;
      j += i;
    }
    /* device driver name string */
#if defined(__PPC__)
    if(
      sscanf(sl->str + j, " %*s Edge %99[^\n]", buf) == 1 ||
      sscanf(sl->str + j, " %*s Level %99[^\n]", buf) == 1 ||
      sscanf(sl->str + j, " %*s %99[^\n]", buf) == 1
    ) {
#else
#if defined(__alpha__) || defined(__sparc__)
    if(sscanf(sl->str + j, " %99[^\n]", buf) == 1) {
#else	/* __i386__ || __x86_64__ || __ia64__ */
    if(sscanf(sl->str + j, " %*s %99[^\n]", buf) == 1) {
#endif
#endif
      m->irq = add_mem(m->irq, sizeof *m->irq, m->irq_len);
      ir = m->irq + m->irq_len++;
      ir->irq = u;
      ir->events = v;

      /* split device driver names (separated by ',') */
      s = buf;
      while(*s && sscanf(s, " %99[^,] %n", buf2, &j) >= 1) {
        ir->dev = add_mem(ir->dev, sizeof *ir->dev, ir->devs);
        ir->dev[ir->devs++] = new_str(buf2);
        s += j;
        if(*s) s++;	/* skip ',' */
      }
    }
  }
}

void gather_resources(misc_t *m, hd_res_t **r, char *name, unsigned which)
{
  int i, j;
  hd_res_t *res;

  if(!m) return;

  if(!which) which = W_IO | W_DMA | W_IRQ;

  if((which & W_IO)) for(i = 0; (unsigned) i < m->io_len; i++) {
    if(!strcmp(name, m->io[i].dev)) {
      res = add_res_entry(r, new_mem(sizeof **r));
      res->io.type = res_io;
      res->io.enabled = 1;
      res->io.base = m->io[i].addr;
      res->io.range = m->io[i].size;
      res->io.access = acc_rw;
      m->io[i].tag++;
    }
  }

  if((which & W_DMA)) for(i = 0; (unsigned) i < m->dma_len; i++) {
    if(!strcmp(name, m->dma[i].dev)) {
      res = add_res_entry(r, new_mem(sizeof **r));
      res->dma.type = res_dma;
      res->dma.enabled = 1;
      res->dma.base = m->dma[i].channel;
      m->dma[i].tag++;
    }
  }

  if((which & W_IRQ)) for(i = 0; (unsigned) i < m->irq_len; i++) {
    for(j = 0; j <  m->irq[i].devs; j++) {
      if(!strcmp(name, m->irq[i].dev[j])) {
        res = add_res_entry(r, new_mem(sizeof **r));
        res->irq.type = res_irq;
        res->irq.enabled = 1;
        res->irq.base = m->irq[i].irq;
        res->irq.triggered = m->irq[i].events;
        m->irq[i].tag++;
      }
    }
  }
}


int active_vga_card(hd_t *hd)
{
  hd_res_t *res;

  if(hd->bus.id != bus_pci) return 1;

  for(res = hd->res; res; res = res->next) {
    if(
      (res->mem.type == res_mem && res->mem.enabled) ||
      (res->io.type == res_io && res->io.enabled)
    ) return 1;
  }

  return 0;
}


/*
 * Add some proc info to the global log.
 */
void dump_misc_proc_data(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("----- /proc/ioports -----\n");
  for(sl = hd_data->misc->proc_io; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/ioports end -----\n");

  ADD2LOG("----- /proc/interrupts -----\n");
  for(sl = hd_data->misc->proc_irq; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/interrupts end -----\n");

  ADD2LOG("----- /proc/dma -----\n");
  for(sl = hd_data->misc->proc_dma; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/dma end -----\n");

}


/*
 * Add the resource usage to the global log.
 */
void dump_misc_data(hd_data_t *hd_data)
{
  misc_t *m = hd_data->misc;
  int i, j;

  ADD2LOG("----- misc resources -----\n");

  for(i = 0; (unsigned) i < m->io_len; i++) {
    ADD2LOG(
      "i/o:%u 0x%04"PRIx64" - 0x%04"PRIx64" (0x%02"PRIx64") \"%s\"\n",
      m->io[i].tag,
      m->io[i].addr, m->io[i].addr + m->io[i].size - 1,
      m->io[i].size, m->io[i].dev
    );
  }

  for(i = 0; (unsigned) i < m->irq_len; i++) {
    ADD2LOG(
      "irq:%u %2u (%9u)",
      m->irq[i].tag, m->irq[i].irq, m->irq[i].events
    );
    for(j = 0; j <  m->irq[i].devs; j++) {
      ADD2LOG(" \"%s\"", m->irq[i].dev[j]);
    }
    ADD2LOG("\n");
  }

  for(i = 0; (unsigned) i < m->dma_len; i++) {
    ADD2LOG(
      "dma:%u %u \"%s\"\n",
      m->dma[i].tag, m->dma[i].channel, m->dma[i].dev
    );
  }

  ADD2LOG("----- misc resources end -----\n");
}

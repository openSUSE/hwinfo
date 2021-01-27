#if defined(__i386__) || defined (__x86_64__)

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <sys/time.h>
#include <x86emu.h>

#include "hd.h"
#include "hd_int.h"

#define STR_SIZE 128

#define VBIOS_MEM	0xa0000
#define VBIOS_MEM_SIZE	0x10000

#define VBIOS_ROM	0xc0000
#define VBIOS_ROM_SIZE	0x10000

#define VBIOS_GAP1	0xd0000
#define VBIOS_GAP1_SIZE	0x20000

#define SBIOS_ROM	0xf0000
#define SBIOS_ROM_SIZE	0x10000

#define VBE_BUF		0x8000

/*
 * I/O loop detection (if vm->no_io is set)
 *
 * IO_LOOP_MAX_SIZE: max number of instructions between reading the same I/O port
 *   again (and no other I/O in between)
 *
 * IO_LOOP_MIN_MATCHES: at least this number of read accesses will return 0 before
 *   starting to emulate a counter
 */
#define IO_LOOP_MAX_SIZE	20
#define IO_LOOP_MIN_MATCHES	50

#define ADD_RES(w, h, f, i) \
  res[res_cnt].width = w, \
  res[res_cnt].height = h, \
  res[res_cnt].vfreq = f, \
  res[res_cnt++].il = i;

#define LPRINTF(a...) hd_log_printf(vm->hd_data, a)

typedef struct vm_s {
  x86emu_t *emu;
  x86emu_memio_handler_t old_memio;

  unsigned ports;
  unsigned force:1;
  unsigned timeout;
  unsigned no_io:1;

  unsigned all_modes:1;
  unsigned mode;
  unsigned mode_set:1;

  unsigned trace_flags;
  unsigned dump_flags;

  int trace_only;
  int dump_only;

  int exec_count;

  struct {
    u64 last_tsc;
    u32 last_addr;
    u32 value;
    unsigned matched;
  } io_loop;

  hd_data_t *hd_data;
} vm_t;


static void flush_log(x86emu_t *emu, char *buf, unsigned size);

static void vm_write_byte(x86emu_t *emu, unsigned addr, unsigned val, unsigned perm);
static void vm_write_word(x86emu_t *emu, unsigned addr, unsigned val, unsigned perm);
// static void vm_write_dword(x86emu_t *emu, unsigned addr, unsigned val, unsigned perm);
static void copy_to_vm(x86emu_t *emu, unsigned dst, unsigned char *src, unsigned size, unsigned perm);
static void copy_from_vm(x86emu_t *emu, void *dst, unsigned src, unsigned len);

static int do_int(x86emu_t *emu, u8 num, unsigned type);
static vm_t *vm_new(void);
static void vm_free(vm_t *vm);
static unsigned vm_run(x86emu_t *emu, double *t);
static int vm_prepare(vm_t *vm);

static unsigned new_memio(x86emu_t *emu, u32 addr, u32 *val, unsigned type);
static double get_time(void);
static void *map_mem(vm_t *vm, unsigned start, unsigned size, int rw);

void print_vbe_info(vm_t *vm, x86emu_t *emu, unsigned mode);

void probe_all(vm_t *vm, vbe_info_t *vbe);
void get_video_mode(vm_t *vm, vbe_info_t *vbe);
void list_modes(vm_t *vm, vbe_info_t *vbe);

void print_edid(int port, unsigned char *edid);
int chk_edid_info(unsigned char *edid);


void get_vbe_info(hd_data_t *hd_data, vbe_info_t *vbe)
{
  int i, err;
  char *t;
  unsigned u, tbits, dbits;
  str_list_t *sl;
  vm_t *vm;

  PROGRESS(4, 1, "vbe info");

  vm = vm_new();
  vm->hd_data = hd_data;
  hd_data->vm = vm;

  for(sl = get_probe_val_list(hd_data, pr_x86emu); sl && (t = sl->str); sl = sl->next) {
    err = 0;
    u = 1;
    tbits = dbits = 0;
    while(*t == '+' || *t == '-') u = *t++ == '+' ? 1 : 0;
         if(!strcmp(t, "trace")) tbits = X86EMU_TRACE_DEFAULT;
    else if(!strcmp(t, "code"))  tbits = X86EMU_TRACE_CODE;
    else if(!strcmp(t, "regs"))  tbits = X86EMU_TRACE_REGS;
    else if(!strcmp(t, "data"))  tbits = X86EMU_TRACE_DATA;
    else if(!strcmp(t, "acc"))   tbits = X86EMU_TRACE_ACC;
    else if(!strcmp(t, "io"))    tbits = X86EMU_TRACE_IO;
    else if(!strcmp(t, "ints"))  tbits = X86EMU_TRACE_INTS;
    else if(!strcmp(t, "time"))  tbits = X86EMU_TRACE_TIME;
    else if(!strcmp(t, "dump"))         dbits = X86EMU_DUMP_DEFAULT;
    else if(!strcmp(t, "dump.regs"))    dbits = X86EMU_DUMP_REGS;
    else if(!strcmp(t, "dump.mem"))     dbits = X86EMU_DUMP_MEM;
    else if(!strcmp(t, "dump.mem.acc")) dbits = X86EMU_DUMP_ACC_MEM;
    else if(!strcmp(t, "dump.mem.inv")) dbits = X86EMU_DUMP_INV_MEM;
    else if(!strcmp(t, "dump.attr"))    dbits = X86EMU_DUMP_ATTR;
    else if(!strcmp(t, "dump.io"))      dbits = X86EMU_DUMP_IO;
    else if(!strcmp(t, "dump.ints"))    dbits = X86EMU_DUMP_INTS;
    else if(!strcmp(t, "dump.time"))    dbits = X86EMU_DUMP_TIME;
    else if(!strcmp(t, "force")) vm->force = u;
    else if(!strncmp(t, "timeout=", sizeof "timeout=" - 1)) {
      i = strtol(t + sizeof "timeout=" - 1, NULL, 0);
      if(i) vm->timeout = i;
    }
    else if(!strncmp(t, "trace.only=", sizeof "trace.only=" - 1)) {
      vm->trace_only = strtol(t + sizeof "trace.only=" - 1, NULL, 0);
    }
    else if(!strncmp(t, "dump.only=", sizeof "dump.only=" - 1)) {
      vm->dump_only = strtol(t + sizeof "dump.only=" - 1, NULL, 0);
    }
    else err = 5;
    if(err) {
      ADD2LOG("x86emu: invalid flag '%s'\n", t);
    }
    else {
      if(tbits) {
        if(u) {
          vm->trace_flags |= tbits;
        }
        else {
          vm->trace_flags &= ~tbits;
        }
      }
      if(dbits) {
        if(u) {
          vm->dump_flags |= dbits;
        }
        else {
          vm->dump_flags &= ~dbits;
        }
      }
    }
  }

  if(!vm_prepare(vm)) {
    ADD2LOG("x86emu: could not init vm\n");
    vm_free(vm);

    return;
  }

  vm->ports = 3;

  i = get_probe_val_int(hd_data, pr_bios_ddc_ports);
  if(i > sizeof vbe->ddc_port / sizeof *vbe->ddc_port) i = sizeof vbe->ddc_port / sizeof *vbe->ddc_port;
  if(i) vm->ports = i;

  if(hd_probe_feature(hd_data, pr_bios_fb)) {
    PROGRESS(4, 2, "mode info");

    // there shouldn't any real io be needed for this
    vm->no_io = 1;
    list_modes(vm, vbe);
  }

  if(hd_probe_feature(hd_data, pr_bios_ddc)) {
    PROGRESS(4, 3, "ddc info");

    ADD2LOG("vbe: probing %d ports\n", vm->ports);

    // for ddc probing we have to allow direct io accesses
    vm->no_io = 0;
    probe_all(vm, vbe);
  }

  if(hd_probe_feature(hd_data, pr_bios_mode)) {
    PROGRESS(4, 4, "gfx mode");

    // there shouldn't any real io be needed for this
    vm->no_io = 1;
    get_video_mode(vm, vbe);
  }

  vm_free(vm);
}


void flush_log(x86emu_t *emu, char *buf, unsigned size)
{
  vm_t *vm = emu->private;
  hd_data_t *hd_data = vm->hd_data;

  if(!buf || !size || !hd_data) return;

  hd_log(hd_data, buf, size);
}


unsigned vm_read_segofs16(x86emu_t *emu, unsigned addr)
{
  return x86emu_read_word(emu, addr) + (x86emu_read_word(emu, addr + 2) << 4);
}


void vm_write_byte(x86emu_t *emu, unsigned addr, unsigned val, unsigned perm)
{
  x86emu_write_byte_noperm(emu, addr, val);
  x86emu_set_perm(emu, addr, addr, perm | X86EMU_PERM_VALID);
}


void vm_write_word(x86emu_t *emu, unsigned addr, unsigned val, unsigned perm)
{
  x86emu_write_byte_noperm(emu, addr, val);
  x86emu_write_byte_noperm(emu, addr + 1, val >> 8);
  x86emu_set_perm(emu, addr, addr + 1, perm | X86EMU_PERM_VALID);
}


#if 0
void vm_write_dword(x86emu_t *emu, unsigned addr, unsigned val, unsigned perm)
{
  x86emu_write_byte_noperm(emu, addr, val);
  x86emu_write_byte_noperm(emu, addr + 1, val >> 8);
  x86emu_write_byte_noperm(emu, addr + 2, val >> 16);
  x86emu_write_byte_noperm(emu, addr + 3, val >> 24);
  x86emu_set_perm(emu, addr, addr + 3, perm | X86EMU_PERM_VALID);
}
#endif

void copy_to_vm(x86emu_t *emu, unsigned dst, unsigned char *src, unsigned size, unsigned perm)
{
  if(!size) return;

  while(size--) vm_write_byte(emu, dst++, *src++, perm);
}


void copy_from_vm(x86emu_t *emu, void *dst, unsigned src, unsigned len)
{
  unsigned char *p = dst;
  unsigned u;

  for(u = 0; u < len; u++) {
    p[u] = x86emu_read_byte_noperm(emu, src + u);
  }
}


int do_int(x86emu_t *emu, u8 num, unsigned type)
{
  if((type & 0xff) == INTR_TYPE_FAULT) {
    x86emu_stop(emu);

    return 0;
  }

  // ignore ints != (0x10 or 0x42 or 0x6d)
  if(num != 0x10 && num != 0x42 && num != 0x6d) return 1;

  return 0;
}


vm_t *vm_new()
{
  vm_t *vm;

  vm = calloc(1, sizeof *vm);

  vm->emu = x86emu_new(0, X86EMU_PERM_RW);
  vm->emu->private = vm;

  x86emu_set_log(vm->emu, 200000000, flush_log);
  x86emu_set_intr_handler(vm->emu, do_int);

  vm->trace_only = -1;
  vm->dump_only = -1;

  return vm;
}


void vm_free(vm_t *vm)
{
  if(!vm) return;

  x86emu_done(vm->emu);

  free(vm);
}


unsigned vm_run(x86emu_t *emu, double *t)
{
  vm_t *vm = emu->private;
  unsigned err;

  x86emu_log(emu, "=== emulation log %d %s===\n", vm->exec_count, vm->no_io ? "(no i/o) " : "");

  *t = get_time();

  if(vm->trace_only == -1 || vm->trace_only == vm->exec_count) {
    emu->log.trace = vm->trace_flags;
  }
  else {
    emu->log.trace = 0;
  }

  x86emu_reset_access_stats(emu);

  iopl(3);
  err = x86emu_run(emu, X86EMU_RUN_LOOP | X86EMU_RUN_NO_CODE | X86EMU_RUN_TIMEOUT);
  iopl(0);

  *t = get_time() - *t;

  if(
    vm->dump_flags &&
    (vm->dump_only == -1 || vm->dump_only == vm->exec_count)
  ) {
    x86emu_log(emu, "\n; - - - final state\n");
    x86emu_dump(emu, vm->dump_flags);
    x86emu_log(emu, "; - - -\n");
  }

  x86emu_log(emu, "=== emulation log %d end ===\n", vm->exec_count);

  x86emu_clear_log(emu, 1);

  vm->exec_count++;

  return err;
}


int vm_prepare(vm_t *vm)
{
  int ok = 0;
  unsigned u;
  unsigned char *p1, *p2;

  LPRINTF("=== bios setup ===\n");

  p1 = map_mem(vm, 0, 0x1000, 0);
  if(!p1) {
    LPRINTF("failed to read /dev/mem\n");
    return ok;
  }

  copy_to_vm(vm->emu, 0x10*4, p1 + 0x10*4, 4, X86EMU_PERM_RW);		// video bios entry
  copy_to_vm(vm->emu, 0x42*4, p1 + 0x42*4, 4, X86EMU_PERM_RW);		// old video bios entry
  copy_to_vm(vm->emu, 0x6d*4, p1 + 0x6d*4, 4, X86EMU_PERM_RW);		// saved video bios entry
  copy_to_vm(vm->emu, 0x400, p1 + 0x400, 0x100, X86EMU_PERM_RW);

  munmap(p1, 0x1000);

  p2 = map_mem(vm, VBIOS_ROM, VBIOS_ROM_SIZE, 0);
  if(!p2 || p2[0] != 0x55 || p2[1] != 0xaa || p2[2] == 0) {
    if(p2) munmap(p2, VBIOS_ROM_SIZE);
    LPRINTF("error: no video bios\n");
    return ok;
  }

  copy_to_vm(vm->emu, VBIOS_ROM, p2, p2[2] * 0x200, X86EMU_PERM_RX);

  munmap(p2, VBIOS_ROM_SIZE);

  LPRINTF("video bios: size 0x%04x\n", x86emu_read_byte(vm->emu, VBIOS_ROM + 2) * 0x200);
  LPRINTF("video bios: entry 0x%04x:0x%04x\n",
    x86emu_read_word(vm->emu, 0x10*4 +  2),
    x86emu_read_word(vm->emu, 0x10*4)
  );

  // initialize fake video memory
  for(u = VBIOS_MEM; u < VBIOS_MEM + VBIOS_MEM_SIZE; u++) {
    vm_write_byte(vm->emu, u, 0, X86EMU_PERM_RW);
  }

  // start address 0:0x7c00
  x86emu_set_seg_register(vm->emu, vm->emu->x86.R_CS_SEL, 0);
  vm->emu->x86.R_EIP = 0x7c00;

  // int 0x10 ; hlt
  vm_write_word(vm->emu, 0x7c00, 0x10cd, X86EMU_PERM_RX);
  vm_write_byte(vm->emu, 0x7c02, 0xf4, X86EMU_PERM_RX);

  // stack & buffer space
  x86emu_set_perm(vm->emu, VBE_BUF, 0xffff, X86EMU_PERM_RW);

  // make memory between mapped VBIOS ROM areas writable
  x86emu_set_perm(vm->emu, VBIOS_GAP1, VBIOS_GAP1 + VBIOS_GAP1_SIZE - 1, X86EMU_PERM_RW);

  vm->emu->timeout = vm->timeout ?: 20;

  vm->old_memio = x86emu_set_memio_handler(vm->emu, new_memio);

  ok = 1;

  return ok;
}


/*
 * I/O emulation used if vm->no_io is set. Otherwise real port accesses are done.
 *
 * The emulated I/O always returns 0. Unless a close loop reading a specific
 * port is detected. In that case it starts to emulate a counter on this
 * specific port, assuming the code waits for something to change.
 *
 * Reading any other port in between resets the logic.
 *
 * The detailed behavior is controlled by IO_LOOP_MAX_SIZE and IO_LOOP_MIN_MATCHES.
 */
unsigned new_memio(x86emu_t *emu, u32 addr, u32 *val, unsigned type)
{
  vm_t *vm = emu->private;

  if(vm->no_io) {
    if((type & ~0xff) == X86EMU_MEMIO_I) {
      // tsc is incremented by 1 on each instruction in x86emu
      u64 tsc = emu->x86.R_TSC;

      if(addr == vm->io_loop.last_addr && tsc - vm->io_loop.last_tsc < IO_LOOP_MAX_SIZE) {
        if(vm->io_loop.matched > IO_LOOP_MIN_MATCHES) {
          vm->io_loop.value++;
        }
        else {
          vm->io_loop.matched++;
        }
      }
      else {
        vm->io_loop.matched = 0;
        vm->io_loop.value = 0;
      }
      vm->io_loop.last_addr = addr;
      vm->io_loop.last_tsc = tsc;

      *val = vm->io_loop.value;

      return 0;
    }

    if((type & ~0xff) == X86EMU_MEMIO_O) {
      return 0;
    }
  }

  return vm->old_memio(emu, addr, val, type);
}


double get_time()
{
  static struct timeval t0 = { };
  struct timeval t1 = { };

  gettimeofday(&t1, NULL);

  if(!timerisset(&t0)) t0 = t1;

  timersub(&t1, &t0, &t1);

  return t1.tv_sec + t1.tv_usec / 1e6;
}


void *map_mem(vm_t *vm, unsigned start, unsigned size, int rw)
{
  int fd;
  void *p;

  if(!size) return NULL;

  fd = open("/dev/mem", rw ? O_RDWR : O_RDONLY);

  if(fd == -1) return NULL;

  p = mmap(NULL, size, rw ? PROT_READ | PROT_WRITE : PROT_READ, MAP_SHARED, fd, start);

  if(p == MAP_FAILED) {
    LPRINTF("error: [0x%x, %u]: mmap failed: %s\n", start, size, strerror(errno));
    close(fd);

    return NULL;
  }

  LPRINTF("[0x%x, %u]: mmap ok\n", start, size);

  close(fd);

  return p;
}


void list_modes(vm_t *vm, vbe_info_t *vbe)
{
  x86emu_t *emu = NULL;
  int err = 0, i;
  double d1, d2;
  unsigned char buf2[0x100], tmp[0x100];
  unsigned u, ml;
  unsigned modelist[0x100];
  unsigned bpp;
  int res_bpp;
  vbe_mode_info_t *mi;
  char s[64];

  LPRINTF("=== running bios\n");

  emu = x86emu_clone(vm->emu);

  emu->x86.R_EAX = 0x4f00;
  emu->x86.R_EBX = 0;
  emu->x86.R_ECX = 0;
  emu->x86.R_EDX = 0;
  emu->x86.R_EDI = VBE_BUF;

  x86emu_write_dword(emu, VBE_BUF, 0x32454256);		// "VBE2"

  err = vm_run(emu, &d1);

  LPRINTF("=== vbe get info: %s (time %.3fs, eax 0x%x, err = 0x%x)\n",
    emu->x86.R_AX == 0x4f ? "ok" : "failed",
    d1,
    emu->x86.R_EAX,
    err
  );

  if(!err && emu->x86.R_AX == 0x4f) {
    LPRINTF("=== vbe info\n");

    vbe->ok = 1;

    vbe->version = x86emu_read_word(emu, VBE_BUF + 0x04);
    vbe->oem_version = x86emu_read_word(emu, VBE_BUF + 0x14);
    vbe->memory = x86emu_read_word(emu, VBE_BUF + 0x12) << 16;

    LPRINTF(
      "version = %u.%u, oem version = %u.%u\n",
      vbe->version >> 8, vbe->version & 0xff, vbe->oem_version >> 8, vbe->oem_version & 0xff
    );

    LPRINTF("memory = %uk\n", vbe->memory >> 10);

    buf2[sizeof buf2 - 1] = 0;

    u = vm_read_segofs16(emu, VBE_BUF + 0x06);
    copy_from_vm(emu, buf2, u, sizeof buf2 - 1);
    vbe->oem_name = canon_str(buf2, strlen(buf2));
    LPRINTF("oem name [0x%05x] = \"%s\"\n", u, vbe->oem_name);

    u = vm_read_segofs16(emu, VBE_BUF + 0x16);
    copy_from_vm(emu, buf2, u, sizeof buf2 - 1);
    vbe->vendor_name = canon_str(buf2, strlen(buf2));
    LPRINTF("vendor name [0x%05x] = \"%s\"\n", u, vbe->vendor_name);

    u = vm_read_segofs16(emu, VBE_BUF + 0x1a);
    copy_from_vm(emu, buf2, u, sizeof buf2 - 1);
    vbe->product_name = canon_str(buf2, strlen(buf2));
    LPRINTF("product name [0x%05x] = \"%s\"\n", u, vbe->product_name);

    u = vm_read_segofs16(emu, VBE_BUF + 0x1e);
    copy_from_vm(emu, buf2, u, sizeof buf2 - 1);
    vbe->product_revision = canon_str(buf2, strlen(buf2));
    LPRINTF("product revision [0x%05x] = \"%s\"\n", u, vbe->product_revision);

    ml = vm_read_segofs16(emu, VBE_BUF + 0x0e);

    for(vbe->modes = 0; vbe->modes < sizeof modelist / sizeof *modelist; ) {
      u = x86emu_read_word(emu, ml + 2 * vbe->modes);
      if(u == 0xffff) break;
      modelist[vbe->modes++] = u;
    }

    LPRINTF("%u video modes:\n", vbe->modes);

    vbe->mode = new_mem(vbe->modes * sizeof *vbe->mode);

    for(i = 0; i < vbe->modes; i++) {

      mi = vbe->mode + i;

      mi->number = modelist[i];
      
      emu = x86emu_done(emu);
      emu = x86emu_clone(vm->emu);

      emu->x86.R_EAX = 0x4f01;
      emu->x86.R_EBX = 0;
      emu->x86.R_ECX = modelist[i];
      emu->x86.R_EDI = VBE_BUF;

      err = vm_run(emu, &d1);
      d2 += d1;

      LPRINTF("=== vbe mode info [0x%04x]: %s (time %.3fs, eax 0x%x, err = 0x%x)\n",
        modelist[i],
        emu->x86.R_AX == 0x4f ? "ok" : "failed",
        d1,
        emu->x86.R_EAX,
        err
      );

      if(err || emu->x86.R_AX != 0x4f) continue;

      copy_from_vm(emu, tmp, VBE_BUF, sizeof tmp);

      mi->attributes = tmp[0x00] + (tmp[0x01] << 8);

      mi->width = tmp[0x12] + (tmp[0x13] << 8);
      mi->height = tmp[0x14] + (tmp[0x15] << 8);
      mi->bytes_p_line = tmp[0x10] + (tmp[0x11] << 8);

      mi->win_A_start = (tmp[0x08] + (tmp[0x09] << 8)) << 4;
      mi->win_B_start = (tmp[0x0a] + (tmp[0x0b] << 8)) << 4;

      mi->win_A_attr = tmp[0x02];
      mi->win_B_attr = tmp[0x03];

      mi->win_gran = (tmp[0x04] + (tmp[0x05] << 8)) << 10;
      mi->win_size = (tmp[0x06] + (tmp[0x07] << 8)) << 10;

      bpp = res_bpp = 0;
      switch(tmp[0x1b]) {
        case 0:
          bpp = -1;
          break;

        case 1:
          bpp = 2;
          break;

        case 2:
          bpp = 1;
          break;

        case 3:
          bpp = 4;
          break;

        case 4:
          bpp = 8;
          break;

        case 6:
          bpp = tmp[0x1f] + tmp[0x21] + tmp[0x23];
          res_bpp = tmp[0x19] - bpp;
          if(res_bpp < 0) res_bpp = 0;
      }

      if(vbe->version >= 0x0200) {
        mi->fb_start = tmp[0x28] + (tmp[0x29] << 8) + (tmp[0x2a] << 16) + (tmp[0x2b] << 24);
      }

      if(vbe->version >= 0x0300) {
        mi->pixel_clock = tmp[0x3e] + (tmp[0x3f] << 8) + (tmp[0x40] << 16) + (tmp[0x41] << 24);
      }

      mi->pixel_size = bpp;

      if(bpp == -1u) {
        LPRINTF("  0x%04x[%02x]: %ux%u, text\n", mi->number, mi->attributes, mi->width, mi->height);
      }
      else {
        if(
          (mi->attributes & 1) &&		/* mode is supported */
          mi->fb_start
        ) {
          if(!vbe->fb_start) vbe->fb_start = mi->fb_start;
        }
        *s = 0;
        if(res_bpp) sprintf(s, "+%d", res_bpp);
        LPRINTF(
          "  0x%04x[%02x]: %ux%u+%u, %u%s bpp",
          mi->number, mi->attributes, mi->width, mi->height, mi->bytes_p_line, mi->pixel_size, s
        );

        if(mi->pixel_clock) LPRINTF(", max. %u MHz", mi->pixel_clock/1000000);

        if(mi->fb_start) LPRINTF(", fb: 0x%08x", mi->fb_start);

        LPRINTF(", %04x.%x", mi->win_A_start, mi->win_A_attr);

        if(mi->win_B_start || mi->win_B_attr) LPRINTF("/%04x.%x", mi->win_B_start, mi->win_B_attr);

        LPRINTF(": %uk", mi->win_size >> 10);

        if(mi->win_gran != mi->win_size) LPRINTF("/%uk", mi->win_gran >> 10);

        LPRINTF("\n");
      }
    }
  }
  else {
    LPRINTF("=== no vbe info\n");
  }

  x86emu_done(emu);
}



void probe_all(vm_t *vm, vbe_info_t *vbe)
{
  x86emu_t *emu = NULL;
  int err = 0, i;
  unsigned port, cnt;
  double d1, d2, timeout;
  unsigned char edid[0x80];

  LPRINTF("=== running bios\n");

  timeout = get_time() + (vm->timeout ?: 20);

  for(port = 0; port < vm->ports; port++) {
    d1 = d2 = 0;

    for(cnt = 0; cnt < 2 && get_time() <= timeout; cnt++) {
      if(!vm->force) {
        emu = x86emu_done(emu);
        emu = x86emu_clone(vm->emu);

        emu->x86.R_EAX = 0x4f15;
        emu->x86.R_EBX = 0;
        emu->x86.R_ECX = port;
        emu->x86.R_EDX = 0;
        emu->x86.R_EDI = 0;
        emu->x86.R_ES = 0;

        err = vm_run(emu, &d1);
        d2 += d1;

        LPRINTF("=== port %u, try %u: %s (time %.3fs, eax 0x%x, err = 0x%x)\n",
          port,
          cnt,
          emu->x86.R_AX == 0x4f ? "ok" : "failed",
          d1,
          emu->x86.R_EAX,
          err
        );

        if(err || emu->x86.R_AX != 0x4f) continue;

        LPRINTF("=== port %u, try %u: bh = %d, bl = 0x%02x\n",
          port,
          cnt,
          emu->x86.R_BH,
          emu->x86.R_BL
        );

        if(!(emu->x86.R_BL & 3)) {
          err = -1;
          continue;
        }
      }

      emu = x86emu_done(emu);
      emu = x86emu_clone(vm->emu);

      emu->x86.R_EAX = 0x4f15;
      emu->x86.R_EBX = 1;
      emu->x86.R_ECX = port;
      emu->x86.R_EDX = 0;
      emu->x86.R_EDI = VBE_BUF;

      err = vm_run(emu, &d1);
      d2 += d1;

      LPRINTF("=== port %u, try %u: %s (time %.3fs, eax 0x%x, err = 0x%x)\n",
        port,
        cnt,
        emu->x86.R_AX == 0x4f ? "ok" : "failed",
        d1,
        emu->x86.R_EAX,
        err
      );

      if(err || emu->x86.R_AX == 0x4f) break;
    }

    if(!emu) {
      LPRINTF("=== timeout\n");
      break;
    }

    LPRINTF("=== port %u: %s (time %.3fs, eax 0x%x, err = 0x%x)\n",
      port,
      emu->x86.R_AX == 0x4f ? "ok" : "failed",
      d2,
      emu->x86.R_EAX,
      err
    );

    copy_from_vm(emu, edid, VBE_BUF, sizeof edid);

    LPRINTF("=== port %u: ddc data ===\n", port);
    for(i = 0; i < 0x80; i++) {
      LPRINTF("%02x", edid[i]);
      LPRINTF((i & 15) == 15 ? "\n" : " ");
    }
    LPRINTF("=== port %u: ddc data end ===\n", port);

    if(!err && emu->x86.R_AX == 0x4f) {
      LPRINTF("=== port %u: monitor info ok\n", port);

      vbe->ok = 1;
      vbe->ddc_ports = port + 1;

      memcpy(vbe->ddc_port[port], edid, sizeof *vbe->ddc_port);
    }
    else {
      if(!err) err = -1;
      LPRINTF("=== port %u: no monitor info\n", port);
    }

    emu = x86emu_done(emu);
  }
}


void get_video_mode(vm_t *vm, vbe_info_t *vbe)
{
  x86emu_t *emu = NULL;
  int err = 0;
  double d;

  LPRINTF("=== running bios\n");

  emu = x86emu_clone(vm->emu);

  emu->x86.R_EAX = 0x4f03;
  emu->x86.R_EBX = 0;

  err = vm_run(emu, &d);

  LPRINTF("=== vbe get current video mode: %s (time %.3fs, eax 0x%x, err = 0x%x)\n",
    emu->x86.R_AX == 0x4f ? "ok" : "failed",
    d,
    emu->x86.R_EAX,
    err
  );

  if(!err && emu->x86.R_AX == 0x4f) {
    vbe->ok = 1;
    vbe->current_mode = emu->x86.R_BX;

    LPRINTF("=== current mode: 0x%04x\n", vbe->current_mode);
  }
  else {
    LPRINTF("=== current mode: no info\n");
  }

  x86emu_done(emu);
}


#endif	/* defined(__i386__) || defined (__x86_64__) */

#include <stdio.h>
#include <stdarg.h>
#ifdef __i386__
#include <sys/vm86.h>
#else
#include "vm86_struct.h"
#endif

#define INT2PTR(a)	((a) + (unsigned char *) 0)

#include "../x86emu/include/x86emu.h"
#include "AsmMacros.h"

int emu_vm86_ret;

static u8 Mem_rb(u32 addr) {
  return *(u8 *)(INT2PTR(addr));
}
static void Mem_wb(u32 addr, u8 val) {
  *(u8 *)INT2PTR(addr) = val;
}
#ifdef __ia64__

static u16 Mem_rw(u32 addr) {
  return *(u8 *)INT2PTR(addr) | *(u8 *)INT2PTR(addr + 1) << 8;
}
static u32 Mem_rl(u32 addr) {
  return *(u8 *)INT2PTR(addr)           | *(u8 *)INT2PTR(addr + 1) << 8 |
         *(u8 *)INT2PTR(addr + 2) << 16 | *(u8 *)INT2PTR(addr + 3) << 24;
}
static void Mem_ww(u32 addr, u16 val) {
  *(u8 *)INT2PTR(addr) = val;
  *(u8 *)INT2PTR(addr + 1) = val >> 8;
}
static void Mem_wl(u32 addr, u32 val) {
  *(u8 *)INT2PTR(addr) = val;
  *(u8 *)INT2PTR(addr + 1) = val >> 8;
  *(u8 *)INT2PTR(addr + 2) = val >> 16;
  *(u8 *)INT2PTR(addr + 3) = val >> 24;
}

#else

static u16 Mem_rw(u32 addr) {
  return *(u16 *)INT2PTR(addr);
}
static u32 Mem_rl(u32 addr) {
  return *(u32 *)INT2PTR(addr);
}
static void Mem_ww(u32 addr, u16 val) {
  *(u16 *)INT2PTR(addr) = val;
}
static void Mem_wl(u32 addr, u32 val) {
  *(u32 *)INT2PTR(addr) = val;
}

#endif

static void do_int(int num) {
  emu_vm86_ret = VM86_INTx | (num << 8);
  M.x86.intr = INTR_HALTED;
}


static u8 deb_inb(X86EMU_pioAddr addr)
{
  u8 u;

  u = inb(addr);
  fprintf(stderr, "%04x:%04x  inb  %04x = %02x\n", M.x86.R_CS, (unsigned) M.x86.R_EIP, addr, u);

  return u;
}

static u16 deb_inw(X86EMU_pioAddr addr)
{
  u16 u;

  u = inw(addr);
  fprintf(stderr, "%04x:%04x  inw  %04x = %04x\n", M.x86.R_CS, (unsigned) M.x86.R_EIP, addr, u);

  return u;
}

static u32 deb_inl(X86EMU_pioAddr addr)
{
  u32 u;

  u = inl(addr);
  fprintf(stderr, "%04x:%04x  inl  %04x = %08x\n", M.x86.R_CS, (unsigned) M.x86.R_EIP, addr, (unsigned) u);

  return u;
}

static void deb_outb(X86EMU_pioAddr addr, u8 val)
{
  fprintf(stderr, "%04x:%04x  outb %04x, %02x\n", M.x86.R_CS, (unsigned) M.x86.R_EIP, addr, val);
  outb(addr, val);
}

static void deb_outw(X86EMU_pioAddr addr, u16 val)
{
  fprintf(stderr, "%04x:%04x  outw %04x, %04x\n", M.x86.R_CS, (unsigned) M.x86.R_EIP, addr, val);
  outw(addr, val);
}

static void deb_outl(X86EMU_pioAddr addr, u32 val)
{
  fprintf(stderr, "%04x:%04x  outl %04x, %08x\n", M.x86.R_CS, (unsigned) M.x86.R_EIP, addr, (unsigned) val);
  outl(addr, val);
}

int
emu_vm86(struct vm86_struct *vm, unsigned debug)
{
  int i;
  unsigned timeout;

  X86EMU_memFuncs memFuncs;
  X86EMU_intrFuncs intFuncs[256];
  X86EMU_pioFuncs pioFuncs;

  memFuncs.rdb = Mem_rb;
  memFuncs.rdw = Mem_rw;
  memFuncs.rdl = Mem_rl;
  memFuncs.wrb = Mem_wb;
  memFuncs.wrw = Mem_ww;
  memFuncs.wrl = Mem_wl;
  X86EMU_setupMemFuncs(&memFuncs);

if(debug) {
  pioFuncs.inb = deb_inb;
  pioFuncs.inw = deb_inw;
  pioFuncs.inl = deb_inl;
  pioFuncs.outb = deb_outb;
  pioFuncs.outw = deb_outw;
  pioFuncs.outl = deb_outl;
} else {
  pioFuncs.inb = (u8(*)(u16))inb;
  pioFuncs.inw = (u16(*)(u16))inw;
  pioFuncs.inl = (u32(*)(u16))inl;
  pioFuncs.outb = (void(*)(u16, u8))outb;
  pioFuncs.outw = (void(*)(u16, u16))outw;
  pioFuncs.outl = (void(*)(u16, u32))outl;
}
  X86EMU_setupPioFuncs(&pioFuncs);

  for (i=0;i<256;i++)
      intFuncs[i] = do_int;
  X86EMU_setupIntrFuncs(intFuncs);

  M.mem_base = 0;
  M.mem_size = 1024*1024 + 1024;

  M.x86.R_EAX = vm->regs.eax;
  M.x86.R_EBX = vm->regs.ebx;
  M.x86.R_ECX = vm->regs.ecx;
  M.x86.R_EDX = vm->regs.edx;

  M.x86.R_ESP = vm->regs.esp;
  M.x86.R_EBP = vm->regs.ebp;
  M.x86.R_ESI = vm->regs.esi;
  M.x86.R_EDI = vm->regs.edi;
  M.x86.R_EIP = vm->regs.eip;
  M.x86.R_EFLG = vm->regs.eflags;

  M.x86.R_CS = vm->regs.cs;
  M.x86.R_DS = vm->regs.ds;
  M.x86.R_SS = vm->regs.ss;
  M.x86.R_ES = vm->regs.es;
  M.x86.R_FS = vm->regs.fs;
  M.x86.R_GS = vm->regs.gs;

  emu_vm86_ret = 0;
  /* set timeout, 20s normal, 60s for debugging */
  timeout = debug ? (1 << 31) + 60 : 20;
  X86EMU_exec(timeout);

  vm->regs.eax = M.x86.R_EAX;
  vm->regs.ebx = M.x86.R_EBX;
  vm->regs.ecx = M.x86.R_ECX;
  vm->regs.edx = M.x86.R_EDX;

  vm->regs.esp = M.x86.R_ESP;
  vm->regs.ebp = M.x86.R_EBP;
  vm->regs.esi = M.x86.R_ESI;
  vm->regs.edi = M.x86.R_EDI;
  vm->regs.eip = M.x86.R_EIP;
  vm->regs.eflags = M.x86.R_EFLG;

  vm->regs.cs = M.x86.R_CS;
  vm->regs.ds = M.x86.R_DS;
  vm->regs.ss = M.x86.R_SS;
  vm->regs.es = M.x86.R_ES;
  vm->regs.fs = M.x86.R_FS;
  vm->regs.gs = M.x86.R_GS;

  if (emu_vm86_ret == 0 && *(unsigned char *)INT2PTR(((u32)M.x86.R_CS << 4) + (M.x86.R_IP - 1)) == 0xf4)
    {
      vm->regs.eip--;
      return VM86_UNKNOWN;
    }
  return emu_vm86_ret ? emu_vm86_ret : -1;
}

void
printk(const char *fmt, ...)
{
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(stderr, fmt, argptr);
    va_end(argptr);
}


#include <stdio.h>
#include <stdarg.h>
#ifdef __i386__
#include <sys/vm86.h>
#else
#include "vm86_struct.h"
#endif

#include "emu_x86emu.h"
#include "AsmMacros.h"

int emu_vm86_ret;

static u8 Mem_rb(u32 addr) {
  return *(u8 *)addr;
}
static u16 Mem_rw(u32 addr) {
  return *(u16 *)addr;
}
static u32 Mem_rl(u32 addr) {
  return *(u32 *)addr;
}
static void Mem_wb(u32 addr, u8 val) {
  *(u8 *)addr = val;
}
static void Mem_ww(u32 addr, u16 val) {
  *(u16 *)addr = val;
}
static void Mem_wl(u32 addr, u32 val) {
  *(u32 *)addr = val;
}
static void do_int(int num) {
  emu_vm86_ret = VM86_INTx | (num << 8);
  M.x86.intr = INTR_HALTED;
}

int
emu_vm86(struct vm86_struct *vm)
{
  int i;

  X86EMU_intrFuncs intFuncs[256];
  X86EMU_pioFuncs pioFuncs = {
      (u8(*)(u16))inb,
      (u16(*)(u16))inw,
      (u32(*)(u16))inl,
      (void(*)(u16, u8))outb,
      (void(*)(u16, u16))outw,
      (void(*)(u16, u32))outl
  };

  X86EMU_memFuncs memFuncs = {
      (u8(*)(u32))Mem_rb,
      (u16(*)(u32))Mem_rw,
      (u32(*)(u32))Mem_rl,
      (void(*)(u32, u8))Mem_wb,
      (void(*)(u32, u16))Mem_ww,
      (void(*)(u32, u32))Mem_wl
  };

  X86EMU_setupMemFuncs(&memFuncs);

  M.mem_base = 0;
  M.mem_size = 1024*1024 + 1024;
  X86EMU_setupPioFuncs(&pioFuncs);

  for (i=0;i<256;i++)
      intFuncs[i] = do_int;
  X86EMU_setupIntrFuncs(intFuncs);

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
  X86EMU_exec();

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

  if (emu_vm86_ret == 0 && *(unsigned char *)(((u32)M.x86.R_CS << 4) + (M.x86.R_IP - 1)) == 0xf4)
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


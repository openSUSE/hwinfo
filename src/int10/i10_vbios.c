/*
 * Copyright 1999 Egbert Eich
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the authors not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE AUTHORS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <setjmp.h>
#if defined(__alpha__) || defined (__ia64__)
#include <sys/io.h>
//#elif defined(HAVE_SYS_PERM)
#else
#include <sys/perm.h>
#endif
#include "v86bios.h"
#include "pci.h"
#include "AsmMacros.h"
#include "vbios.h"

void log_err(char *format, ...) __attribute__ ((format (printf, 1, 2)));

#define SIZE 0x100000
#define VRAM_START 0xA0000
#define VRAM_SIZE 0x1FFFF
#define V_BIOS_SIZE 0x1FFFF
#define BIOS_START 0x7C00            /* default BIOS entry */

static CARD8 code[] = { 0xcd, 0x10, 0xf4 };	/* int 0x10, hlt */
// static CARD8 code13[] = { 0xcd, 0x13, 0xf4 };	/* int 0x13, hlt */

static int map(void);
static void unmap(void);
static int map_vram(void);
static void unmap_vram(void);
static int copy_vbios(void);
// static int copy_sbios(void);
#if MAP_SYS_BIOS
static int copy_sys_bios(void);
#endif
static int copy_bios_ram();
static int setup_system_bios(void);
static void setup_int_vect(void);
static int chksum(CARD8 *start);

void loadCodeToMem(unsigned char *ptr, CARD8 *code);

static int vram_mapped = 0;
static int int10inited = 0;

static sigjmp_buf longjmp_buf;

static void sigsegv_handler(int);

int InitInt10(int pci_cfg_method)
{
  if(geteuid()) return -1;

  if(!map()) return -1;

  if(!setup_system_bios()) {
    unmap();
    return -1;
  }

  setup_io();

  if(iopl(3) < 0) {
    unmap();
    return -1;
  }

  scan_pci(pci_cfg_method);

  for(; CurrentPci; CurrentPci = CurrentPci->next) {
    if(CurrentPci->active) break;
  }

  iopl(0);

  setup_int_vect();

  if(!copy_vbios()) {
    unmap();
    return -1;
  }

#if 0
  if(!copy_sbios()) {
    unmap();
    return -1;
  }
#endif

  if(!map_vram() || !copy_bios_ram()) {
    unmap();
    return -1;
  }

  int10inited = 1;

#if 0
  /* int 13 default location (fdd) */
  ((CARD16*)0)[(0x13<<1)+1] = 0xf000;
  ((CARD16*)0)[0x13<<1] = 0xec59;
#endif

  return 0;
}


void FreeInt10()
{
  if(!int10inited) return;

  unmap_vram();
  unmap();

  int10inited = 0;
}


void sigsegv_handler(int num)
{
  siglongjmp(longjmp_buf, num + 1000);
}


int CallInt10(int *ax, int *bx, int *cx, unsigned char *buf, int len, int cpuemu)
{
  i86biosRegs bRegs;
  void (*old_sigsegv_handler)(int) = SIG_DFL;
  void (*old_sigill_handler)(int) = SIG_DFL;
  void (*old_sigtrap_handler)(int) = SIG_DFL;
  int jmp;

  if(!int10inited) return -1;
  memset(&bRegs, 0, sizeof bRegs);
  bRegs.ax = *ax;
  bRegs.bx = *bx;
  bRegs.cx = *cx;
  bRegs.dx = 0;
  bRegs.es = 0x7e0;
  bRegs.di = 0x0;
  if(buf) memcpy((unsigned char *) 0x7e00, buf, len);

  iopl(3);

  jmp = sigsetjmp(longjmp_buf, 1);

  if(!jmp) {
    old_sigsegv_handler = signal(SIGSEGV, sigsegv_handler);
    old_sigill_handler = signal(SIGILL, sigsegv_handler);
    old_sigtrap_handler = signal(SIGTRAP, sigsegv_handler);

    loadCodeToMem((unsigned char *) BIOS_START, code);
    do_x86(BIOS_START, &bRegs, cpuemu);
  }
  else {
    int10inited = 0;
    log_err("oops: got signal %d in vm86() code\n", jmp - 1000);
  }

  signal(SIGTRAP, old_sigtrap_handler);
  signal(SIGILL, old_sigill_handler);
  signal(SIGSEGV, old_sigsegv_handler);

  iopl(0);

  if(buf) memcpy(buf, (unsigned char *) 0x7e00, len);

  *ax = bRegs.ax;
  *bx = bRegs.bx;
  *cx = bRegs.cx;

  return bRegs.ax;
}


#if 0
int CallInt13(int *ax, int *bx, int *cx, int *dx, unsigned char *buf, int len, int cpuemu)
{
  i86biosRegs bRegs;

  if(!int10inited) return -1;
  memset(&bRegs, 0, sizeof bRegs);
  bRegs.ax = *ax;
  bRegs.bx = *bx;
  bRegs.cx = *cx;
  bRegs.dx = *dx;
  bRegs.es = 0x7e0;
  bRegs.ds = 0x7e0;
  bRegs.di = 0x0;
  bRegs.si = 0x0;
  if(buf) memcpy((unsigned char *) 0x7e00, buf, len);

  iopl(3);

  loadCodeToMem((unsigned char *) BIOS_START, code13);
  do_x86(BIOS_START, &bRegs, cpuemu);

  iopl(0);

  if(buf) memcpy(buf, (unsigned char *) 0x7e00, len);

  *ax = bRegs.ax;
  *bx = bRegs.bx;
  *cx = bRegs.cx;
  *dx = bRegs.dx;

  return bRegs.ax;
}
#endif


int map()
{
  void* mem;

  mem = mmap(0, (size_t) SIZE, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);

  if(mem) {
    perror("anonymous map");
    return 0;
  }

  memset(mem, 0, SIZE);

  loadCodeToMem((unsigned char *) BIOS_START, code);

  return 1;
}


void unmap()
{
  munmap(0, SIZE);
}


static int
map_vram(void)
{
	int mem_fd;

#ifdef __ia64__
	if ((mem_fd = open(MEM_FILE,O_RDWR | O_SYNC))<0) 
#else
	if ((mem_fd = open(MEM_FILE,O_RDWR))<0) 
#endif
	  {
		perror("opening memory");
		return 0;
	}

#ifndef __alpha__
	if (mmap((void *) VRAM_START, (size_t) VRAM_SIZE,
					 PROT_EXEC | PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
					 mem_fd, VRAM_START) == (void *) -1) 
#else
		 if (!_bus_base()) sparse_shift = 7; /* Uh, oh, JENSEN... */
		 if (!_bus_base_sparse()) sparse_shift = 0;
		 if ((vram_map = mmap(0,(size_t) (VRAM_SIZE << sparse_shift),
												 PROT_READ | PROT_WRITE,
												 MAP_SHARED,
												 mem_fd, (VRAM_START << sparse_shift)
												 | _bus_base_sparse())) == (void *) -1)
#endif
	  {
	    perror("mmap error in map_hardware_ram");
			close(mem_fd);
			return (0);
		}
	vram_mapped = 1;
	close(mem_fd);
	return (1);
}


void unmap_vram()
{
  if(!vram_mapped) return;

  munmap((void*) VRAM_START, VRAM_SIZE);

  vram_mapped = 0;
}


/*
 * Read video BIOS from /dev/mem.
 *
 * Return:
 *  0: failed
 *  1: ok
 */
int copy_vbios()
{
  int mem_fd, size, ok = 0;
  unsigned char tmp[3];

  if((mem_fd = open(MEM_FILE, O_RDONLY)) == -1) {
    log_err("vbe: failed to open BIOS memory, errno = %d", errno);

    return 0;
  }

  if(lseek(mem_fd, (off_t) V_BIOS, SEEK_SET) != (off_t) V_BIOS) {
    log_err("vbe: lseek failed, errno = %d\n", errno);
  }
  else {
    if(read(mem_fd, tmp, sizeof tmp) != sizeof tmp) {
      log_err("vbe: failed to read %u bytes at 0x%x, errno = %d\n", (unsigned) sizeof tmp, V_BIOS, errno);
    }
    else {
      if(lseek(mem_fd, V_BIOS, SEEK_SET) != V_BIOS) {
        log_err("vbe: lseek failed, errno = %d\n", errno);
      }
      else {
        if(tmp[0] != 0x55 || tmp[1] != 0xAA ) {
          log_err("vbe: no bios found at: 0x%x\n", V_BIOS);
        }
        else {
          size = tmp[2] * 0x200;

          if(read(mem_fd, (char *) V_BIOS, size) != size) {
            log_err("vbe: failed to read %d bytes at 0x%x, errno = %d\n", size, V_BIOS, errno);
          }
          else {
            if(chksum((CARD8 *) V_BIOS)) ok = 1;
          }
        }
      }
    }
  }

  close(mem_fd);

  return ok;
}


#if 0
#define SYS_BIOS	0xe0000
#define SYS_BIOS_SIZE	0x20000
int copy_sbios()
{
  int fd;

  if((fd = open(MEM_FILE, O_RDONLY)) < 0) {
    perror("opening memory");
    return 0;
  }

  if(lseek(fd,(off_t) SYS_BIOS, SEEK_SET) == (off_t) SYS_BIOS) {
    read(fd, (void *) SYS_BIOS, SYS_BIOS_SIZE);
  }

  close(fd);

  return 1;
}
#endif


#if MAP_SYS_BIOS
static int
copy_sys_bios(void)
{
#define SYS_BIOS 0xF0000
	int mem_fd;

	if ((mem_fd = open(MEM_FILE,O_RDONLY))<0) {
		perror("opening memory");
		return (0);
	}
  
	if (lseek(mem_fd,(off_t) SYS_BIOS,SEEK_SET) != (off_t) SYS_BIOS) 
		goto Error;
	if (read(mem_fd, (char *)SYS_BIOS, (size_t) 0xFFFF) != (size_t) 0xFFFF) 
		goto Error;

	close(mem_fd);
	return (1);

Error:
	perror("sys_bios");
	close(mem_fd);
	return (0);
}
#endif


static int
copy_bios_ram(void)
{
#define BIOS_RAM 0
	int mem_fd;

	if ((mem_fd = open(MEM_FILE,O_RDONLY))<0) {
		perror("opening memory");
		return (0);
	}
  
	if (lseek(mem_fd,(off_t) BIOS_RAM,SEEK_SET) != (off_t) BIOS_RAM) 
		goto Error;
	if (read(mem_fd, (char *)BIOS_RAM, (size_t) 0x1000) != (size_t) 0x1000) 
		goto Error;

	close(mem_fd);
	return (1);

Error:
	perror("bios_ram");
	close(mem_fd);
	return (0);
}


void loadCodeToMem(unsigned char *ptr, CARD8 *code)
{
  while((*ptr++ = *code++) != 0xf4 /* hlt */);

  return;
}
		

/*
 * here we are really paranoid about faking a "real"
 * BIOS. Most of this information was pulled from
 * dosem.
 */
static void
setup_int_vect(void)
{
	const CARD16 cs = 0x0000;
	const CARD16 ip = 0x0;
	int i;
	
	/* let the int vects point to the SYS_BIOS seg */
	for (i=0; i<0x80; i++) {
		((CARD16*)0)[i<<1] = ip;
		((CARD16*)0)[(i<<1)+1] = cs;
	}
	/* video interrupts default location */
	((CARD16*)0)[(0x42<<1)+1] = 0xf000;
	((CARD16*)0)[0x42<<1] = 0xf065;
	((CARD16*)0)[(0x10<<1)+1] = 0xf000;
	((CARD16*)0)[0x10<<1] = 0xf065;
	/* video param table default location (int 1d) */
	((CARD16*)0)[(0x1d<<1)+1] = 0xf000;
	((CARD16*)0)[0x1d<<1] = 0xf0A4;
	/* font tables default location (int 1F) */
	((CARD16*)0)[(0x1f<<1)+1] = 0xf000;
	((CARD16*)0)[0x1f<<1] = 0xfa6e;

	/* int 11 default location */
	((CARD16*)0)[(0x11<1)+1] = 0xf000;
	((CARD16*)0)[0x11<<1] = 0xf84d;
	/* int 12 default location */
	((CARD16*)0)[(0x12<<1)+1] = 0xf000;
	((CARD16*)0)[0x12<<1] = 0xf841;
	/* int 15 default location */
	((CARD16*)0)[(0x15<<1)+1] = 0xf000;
	((CARD16*)0)[0x15<<1] = 0xf859;
	/* int 1A default location */
	((CARD16*)0)[(0x1a<<1)+1] = 0xf000;
	((CARD16*)0)[0x1a<<1] = 0xff6e;
	/* int 05 default location */
	((CARD16*)0)[(0x05<<1)+1] = 0xf000;
	((CARD16*)0)[0x05<<1] = 0xff54;
	/* int 08 default location */
	((CARD16*)0)[(0x8<<1)+1] = 0xf000;
	((CARD16*)0)[0x8<<1] = 0xfea5;
	/* int 13 default location (fdd) */
	((CARD16*)0)[(0x13<<1)+1] = 0xf000;
	((CARD16*)0)[0x13<<1] = 0xec59;
	/* int 0E default location */
	((CARD16*)0)[(0xe<<1)+1] = 0xf000;
	((CARD16*)0)[0xe<<1] = 0xef57;
	/* int 17 default location */
	((CARD16*)0)[(0x17<<1)+1] = 0xf000;
	((CARD16*)0)[0x17<<1] = 0xefd2;
	/* fdd table default location (int 1e) */
	((CARD16*)0)[(0x1e<<1)+1] = 0xf000;
	((CARD16*)0)[0x1e<<1] = 0xefc7;
}

static int
setup_system_bios(void)
{
	char *date = "06/01/99";
	char *eisa_ident = "PCI/ISA";
	
#if MAP_SYS_BIOS
	if (!copy_sys_bios()) return 0;
	return 1;
#endif
//    memset((void *)0xF0000,0xf4,0xfff7);
	
	/*
	 * we trap the "industry standard entry points" to the BIOS
	 * and all other locations by filling them with "hlt"
	 * TODO: implement hlt-handler for these
	 */
	memset((void *)0xF0000,0xf4,0x10000);

	/*
	 * TODO: we should copy the fdd table (0xfec59-0xfec5b)
	 * the video parameter table (0xf0ac-0xf0fb)
	 * and the font tables (0xfa6e-0xfe6d)
	 * from the original bios here
	 */
	
	/* set bios date */
	strcpy((char *)0xFFFF5,date);
	/* set up eisa ident string */
	strcpy((char *)0xFFFD9,eisa_ident);
	/* write system model id for IBM-AT */
	((char *)0)[0xFFFFE] = 0xfc;

	return 1;
}


/*
 * Check BIOS CRC.
 *
 * Return:
 *  0: failed
 *  1: ok
 */
int chksum(CARD8 *start)
{
  CARD16 size;
  CARD8 val = 0;
  int i;

  size = start[2] * 0x200;
  for(i = 0; i < size; i++) val += start[i];
	
  if(!val) return 1;

  log_err("vbe: BIOS chksum wrong\n");

  return 0;
}


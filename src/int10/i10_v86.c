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
#include <errno.h>
#include <asm/unistd.h>
#include <stdio.h>
#include <string.h>
#ifdef __i386__
#include <sys/vm86.h>
#else
#include "vm86_struct.h"
#endif
#include <signal.h>
#include "v86bios.h"
#include "AsmMacros.h"

extern int emu_vm86(struct vm86_struct *vm, unsigned debug);

#define INT2PTR(a)	((a) + (unsigned char *) 0)

void log_err(char *format, ...) __attribute__ ((format (printf, 1, 2)));

struct vm86_struct vm86s;

static int vm86_GP_fault(void);
static int vm86_do_int(int num);
#ifdef __i386__
static int vm86_rep(struct vm86_struct *ptr);
#endif
void log_registers(void);

#define CPU_REG(x) (vm86s.regs.x)
#define CPU_REG_LW(reg)      (*((CARD16 *)&CPU_REG(reg)))
#define CPU_REG_HW(reg)      (*((CARD16 *)&CPU_REG(reg) + 1))
#define CPU_REG_LB(reg)      (*(CARD8 *)&CPU_REG(e##reg))
#define SEG_ADR(type, seg, reg)  type((CPU_REG_LW(seg) << 4) + CPU_REG_LW(e##reg) + (unsigned char *) 0)
#define DF (1 << 10)

struct pio P;

void
setup_io(void) 
{
    P.inb = (CARD8(*)(CARD16))inb;
    P.inw = (CARD16(*)(CARD16))inw;
    P.inl = (CARD32(*)(CARD16))inl;
    P.outb = (void(*)(CARD16,CARD8))outb;
    P.outw = (void(*)(CARD16,CARD16))outw;
    P.outl = (void(*)(CARD16,CARD32))outl;
}

	
static void
setup_vm86(unsigned long bios_start, i86biosRegsPtr regs)
{
	CARD32 eip;
	CARD16 cs;
	
	vm86s.flags = VM86_SCREEN_BITMAP;
	vm86s.flags = 0;
	vm86s.screen_bitmap = 0;
	vm86s.cpu_type = CPU_586;
	memset(&vm86s.int_revectored, 0xff,sizeof(vm86s.int_revectored)) ;
	memset(&vm86s.int21_revectored, 0xff,sizeof(vm86s.int21_revectored)) ;

	eip = bios_start & 0xFFFF;
	cs = (bios_start & 0xFF0000) >> 4;
	
	CPU_REG(eax) = regs->ax;
	CPU_REG(ebx) = regs->bx;
	CPU_REG(ecx) = regs->cx;
	CPU_REG(edx) = regs->dx;
	CPU_REG(esi) = 0;
	CPU_REG(edi) = regs->di;
	CPU_REG(ebp) = 0;
	CPU_REG(eip) = eip;
	CPU_REG(cs) = cs;               
	CPU_REG(esp) = 0x100;
	CPU_REG(ss) = 0x30;               /* This is the standard pc bios stack */
	CPU_REG(es) = regs->es;
	CPU_REG(ds) = 0x40;               /* standard pc ds */
	CPU_REG(fs) = 0;
	CPU_REG(gs) = 0;
	CPU_REG(eflags) |= (VIF_MASK | VIP_MASK);
}

void
collect_bios_regs(i86biosRegsPtr regs)
{
	regs->ax = CPU_REG(eax);
	regs->bx = CPU_REG(ebx);
	regs->cx = CPU_REG(ecx);
	regs->dx = CPU_REG(edx);
	regs->es = CPU_REG(es);
	regs->ds = CPU_REG(ds);
	regs->di = CPU_REG(edi);
	regs->si = CPU_REG(esi);
}

static int do_vm86(int cpuemu)
{
  int retval;
	
#ifdef V86BIOS_DEBUG
	dump_registers();
#endif

#ifdef __i386__
	if(cpuemu) {
	  retval = emu_vm86(&vm86s, cpuemu & 2);
	}
	else {
	  retval = vm86_rep(&vm86s);
	}
#else
	retval = emu_vm86(&vm86s, cpuemu & 2);
#endif

	switch (VM86_TYPE(retval)) {
	case VM86_UNKNOWN:
		if (!vm86_GP_fault())
			return 0;
		break;
	case VM86_STI:
		log_err("vm86_sti :-((\n");
		log_registers();
		return 0;
	case VM86_INTx:
		if (!vm86_do_int(VM86_ARG(retval))) {
			log_err("Unknown vm86_int: %X\n",VM86_ARG(retval));
			log_registers();
			return 0;
		}
		/* I'm not sure yet what to do if we can handle ints */
		break;
	case VM86_SIGNAL:
		log_err("VBE: received a signal!\n");
		log_registers();
		return 0;
	default:
		log_err("unknown type(0x%x)=0x%x\n",
				VM86_ARG(retval),VM86_TYPE(retval));
		log_registers();
		return 0;
	}
	
	return 1;
}

void
do_x86(unsigned long bios_start, i86biosRegsPtr regs, int cpuemu)
{
    setup_vm86(bios_start, regs);
    while(do_vm86(cpuemu)) {};
    collect_bios_regs(regs);
}

/* get the linear address */
#define LIN_PREF_SI  ((pref_seg << 4) + CPU_REG_LW(esi))

#define LWECX       (prefix66 ^ prefix67 ? CPU_REG(ecx) : CPU_REG_LW(ecx))
#define SET_LWECX_ZERO (prefix66 ^ prefix67 ? (CPU_REG(ecx) = 0) : (CPU_REG_LW(ecx) = 0))

static int
vm86_GP_fault(void)
{
	unsigned char *csp, *lina;
	CARD32 org_eip;
	int pref_seg;
	int done,is_rep,prefix66,prefix67;


	csp = lina = SEG_ADR((unsigned char *), cs, ip);
#ifdef V86BIOS_DEBUG
	printf("exception: \n");
	dump_code();
#endif

	is_rep = 0;
	prefix66 = prefix67 = 0;
	pref_seg = -1;

	/* eat up prefixes */
	done = 0;
	do {
		switch (*(csp++)) {
		case 0x66:      /* operand prefix */  prefix66=1; break;
		case 0x67:      /* address prefix */  prefix67=1; break;
		case 0x2e:      /* CS */              pref_seg=CPU_REG(cs); break;
		case 0x3e:      /* DS */              pref_seg=CPU_REG(ds); break;
		case 0x26:      /* ES */              pref_seg=CPU_REG(es); break;
		case 0x36:      /* SS */              pref_seg=CPU_REG(ss); break;
		case 0x65:      /* GS */              pref_seg=CPU_REG(gs); break;
		case 0x64:      /* FS */              pref_seg=CPU_REG(fs); break;
		case 0xf2:      /* repnz */
		case 0xf3:      /* rep */             is_rep=1; break;
		default: done=1;
		}
	} while (!done);
	csp--;   /* oops one too many */
	org_eip = CPU_REG(eip);
	CPU_REG_LW(eip) += (csp - lina);

	switch (*csp) {

	case 0x6c:                    /* insb */
		/* NOTE: ES can't be overwritten; prefixes 66,67 should use esi,edi,ecx
		 * but is anyone using extended regs in real mode? */
		/* WARNING: no test for DI wrapping! */
		CPU_REG_LW(edi) += port_rep_inb(CPU_REG_LW(edx), 
										SEG_ADR((CARD8 *),es,di),
										CPU_REG_LW(eflags)&DF,
										(is_rep? LWECX:1));
		if (is_rep) SET_LWECX_ZERO;
		CPU_REG_LW(eip)++;
		break;

	case 0x6d:                  /* (rep) insw / insd */
		/* NOTE: ES can't be overwritten */
		/* WARNING: no test for _DI wrapping! */
		if (prefix66) {
			CPU_REG_LW(edi) += port_rep_inl(CPU_REG_LW(edx),
											SEG_ADR((CARD32 *),es,di),
											CPU_REG_LW(eflags)&DF,
											(is_rep? LWECX:1));
		}
		else {
			CPU_REG_LW(edi) += port_rep_inw(CPU_REG_LW(edx), 
											SEG_ADR((CARD16 *),es,di),
											CPU_REG_LW(eflags)&DF,
											(is_rep? LWECX:1));
		}
		if (is_rep) SET_LWECX_ZERO;
		CPU_REG_LW(eip)++;
		break;

	case 0x6e:                  /* (rep) outsb */
		if (pref_seg < 0) pref_seg = CPU_REG_LW(ds);
		/* WARNING: no test for _SI wrapping! */
		CPU_REG_LW(esi) += port_rep_outb(CPU_REG_LW(edx),(CARD8*)INT2PTR(LIN_PREF_SI),
										 CPU_REG_LW(eflags)&DF,
										 (is_rep? LWECX:1));
		if (is_rep) SET_LWECX_ZERO;
		CPU_REG_LW(eip)++;
		break;

	case 0x6f:                  /* (rep) outsw / outsd */
		if (pref_seg < 0) pref_seg = CPU_REG_LW(ds);
		/* WARNING: no test for _SI wrapping! */
		if (prefix66) {
			CPU_REG_LW(esi) += port_rep_outl(CPU_REG_LW(edx),
											 (CARD32 *)INT2PTR(LIN_PREF_SI),
											 CPU_REG_LW(eflags)&DF,
											 (is_rep? LWECX:1));
		}
		else {
			CPU_REG_LW(esi) += port_rep_outw(CPU_REG_LW(edx),
											 (CARD16 *)INT2PTR(LIN_PREF_SI),
											 CPU_REG_LW(eflags)&DF,
											 (is_rep? LWECX:1));
		} 
		if (is_rep) SET_LWECX_ZERO;
		CPU_REG_LW(eip)++;
		break;

	case 0xe5:                  /* inw xx, inl xx */
		if (prefix66) CPU_REG(eax) = P.inl((int) csp[1]);
		else CPU_REG_LW(eax) = P.inw((int) csp[1]);  
		CPU_REG_LW(eip) += 2;
		break;
	case 0xe4:                  /* inb xx */
		CPU_REG_LW(eax) &= ~(CARD32)0xff;
		CPU_REG_LB(ax) |= P.inb((int) csp[1]);
		CPU_REG_LW(eip) += 2;
		break;
	case 0xed:                  /* inw dx, inl dx */
		if (prefix66) CPU_REG(eax) = P.inl(CPU_REG_LW(edx)); 
		else CPU_REG_LW(eax) = P.inw(CPU_REG_LW(edx));
		CPU_REG_LW(eip) += 1;
		break;
	case 0xec:                  /* inb dx */
		CPU_REG_LW(eax) &= ~(CARD32)0xff;
		CPU_REG_LB(ax) |= P.inb(CPU_REG_LW(edx));
		CPU_REG_LW(eip) += 1;
		break;

	case 0xe7:                  /* outw xx */
		if (prefix66) P.outl((int)csp[1], CPU_REG(eax));
		else P.outw((int)csp[1], CPU_REG_LW(eax));
		CPU_REG_LW(eip) += 2;
		break;
	case 0xe6:                  /* outb xx */
		P.outb((int) csp[1], CPU_REG_LB(ax));
		CPU_REG_LW(eip) += 2;
		break;
	case 0xef:                  /* outw dx */
		if (prefix66) P.outl(CPU_REG_LW(edx), CPU_REG(eax));
		else P.outw(CPU_REG_LW(edx), CPU_REG_LW(eax));
		CPU_REG_LW(eip) += 1;
		break;
	case 0xee:                  /* outb dx */
		P.outb(CPU_REG_LW(edx), CPU_REG_LB(ax));
		CPU_REG_LW(eip) += 1;
		break;

	case 0xf4:
#ifdef V86BIOS_DEBUG
		printf("hlt at %p\n", lina);
#endif
		return 0;

	case 0x0f: 
		log_err("CPU 0x0f Trap at eip=0x%lx\n",CPU_REG(eip));
		goto op0ferr; 
		break;

	case 0xf0:                  /* lock */
	default:
		log_err("unknown reason for exception\n");
		log_registers();
	op0ferr:
		log_err("cannot continue\n");
		return 0;
	}                           /* end of switch() */
	return 1;
}

static int
vm86_do_int(int num)
{
	int val;
	struct regs86 regs;

	/* try to run bios interrupt */
	
	/* if not installed fall back */
#define COPY(x) regs.x = CPU_REG(x)
#define COPY_R(x) CPU_REG(x) = regs.x
	
	COPY(eax);
	COPY(ebx);
	COPY(ecx);
	COPY(edx);
	COPY(esi);
	COPY(edi);
	COPY(ebp);
	COPY(eip);
	COPY(esp);
	COPY(cs);
	COPY(ss);
	COPY(ds);
	COPY(es);
	COPY(fs);
	COPY(gs);
	COPY(eflags);

	if (!(val = int_handler(num,&regs)))
		if (!(val = run_bios_int(num,&regs)))
			return val;
		
	COPY_R(eax);
	COPY_R(ebx);
	COPY_R(ecx);
	COPY_R(edx);
	COPY_R(esi);
	COPY_R(edi);
	COPY_R(ebp);
	COPY_R(eip);
	COPY_R(esp);
	COPY_R(cs);
	COPY_R(ss);
	COPY_R(ds);
	COPY_R(es);
	COPY_R(fs);
	COPY_R(gs);
	COPY_R(eflags);

	return val;
#undef COPY
#undef COPY_R
}

#ifdef __i386__

static int
vm86_rep(struct vm86_struct *ptr) 
{

	int __res;

	/* stay away from %ebx */
	__asm__ __volatile__("push %%ebx\n\tmov %%ecx,%%ebx\n\tpush %%gs\n\tint $0x80\n\tpop %%gs\n\tpop %%ebx\n"
						 :"=a" (__res):"a" ((int)113),
						 "c" ((struct vm86_struct *)ptr));

			if ((__res) < 0) {
				errno = -__res;
				__res=-1;
			}
			else errno = 0;
			return __res;
}

#endif

#ifdef __i386__

#define pushw(base, ptr, val) \
__asm__ __volatile__( \
		"decw %w0\n\t" \
		"movb %h2,(%1,%0)\n\t" \
		"decw %w0\n\t" \
		"movb %b2,(%1,%0)" \
		: "=r" (ptr) \
		: "r" (base), "q" (val), "0" (ptr))

#else

#define pushw(base, ptr, val) {					\
	ptr = ((ptr) - 1) & 0xffff;				\
	*((unsigned char *)(base) + (ptr)) = (val) >> 8;	\
	ptr = ((ptr) - 1) & 0xffff;				\
	*((unsigned char *)(base) + (ptr)) = (val);		\
	}

#endif

int
run_bios_int(int num, struct regs86 *regs)
{
	CARD16 *ssp;
	CARD32 sp;
	CARD32 eflags;

#ifdef V86BIOS_DEBUG
	static int firsttime = 1;
#endif
	/* check if bios vector is initialized */
	if (((CARD16*)0)[(num<<1)+1] == 0x0000) { /* SYS_BIOS_SEG ?*/
		return 0;
	}
	
#ifdef V86BIOS_DEBUG
	if (firsttime) {
		dprint(0,0x3D0);
		firsttime = 0;
	}
#endif
	
	ssp = (CARD16*)INT2PTR(CPU_REG(ss)<<4);
	sp = (CARD32) CPU_REG_LW(esp);

	eflags = regs->eflags;
	eflags = ((eflags & VIF_MASK) != 0)
		? (eflags | IF_MASK) : (eflags & ~(CARD32) IF_MASK);
	pushw(ssp, sp, eflags);
	pushw(ssp, sp, regs->cs);
	pushw(ssp, sp, (CARD16)regs->eip);
	regs->esp -= 6;
	regs->cs = ((CARD16 *) 0)[(num << 1) + 1];
	regs->eip = (regs->eip & 0xFFFF0000) | ((CARD16 *) 0)[num << 1];
#ifdef V86BIOS_DEBUG
	dump_code();
#endif
	regs->eflags = regs->eflags
					   & ~(VIF_MASK | TF_MASK | IF_MASK | NT_MASK);
	return 1;
}

CARD32
getIntVect(int num)
{
	return ((CARD32*)0)[num];
}

CARD32
getIP(void)
{
    return (CPU_REG(cs) << 4) + CPU_REG(eip);
}

void log_registers()
{
  log_err(
    "  eax %08x, ebx %08x, ecx %08x, edx %08x\n"
    "  esi %08x, edi %08x, ebp %08x, esp %08x\n"
    "  ds %04x, es %04x, fs %04x, gs %04x, ss %04x\n"
    "  cs:eip %04x:%08x\n",
    (unsigned) CPU_REG(eax), (unsigned) CPU_REG(ebx), (unsigned) CPU_REG(ecx), (unsigned) CPU_REG(edx),
    (unsigned) CPU_REG(esi), (unsigned) CPU_REG(edi), (unsigned) CPU_REG(ebp), (unsigned) CPU_REG(esp),
    (unsigned) CPU_REG(ds), (unsigned) CPU_REG(es),
    (unsigned) CPU_REG(fs), (unsigned) CPU_REG(gs), (unsigned) CPU_REG(ss),
    (unsigned) CPU_REG(cs), (unsigned) CPU_REG(eip)
  );
}


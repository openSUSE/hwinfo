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
#if defined(__alpha__) || defined (__ia64__)
#include <sys/io.h>
#endif

#include "v86bios.h"
#include "AsmMacros.h"
#include "pci.h"

static int int1A_handler(struct regs86 *regs);
static int int42_handler(int num, struct regs86 *regs);

int
int_handler(int num, struct regs86 *regs)
{
	switch (num) {
	case 0x10:
	case 0x42:
		return (int42_handler(num,regs));
	case 0x1A:
		return (int1A_handler(regs));
	default:
		return 0;
	}
	return 0;
}

static int
int42_handler(int num,struct regs86 *regs)
{
	unsigned char c;
	CARD32 val;

	/*
	 * video bios has modified these -
	 * leave it to the video bios to do this
	 */

	val = getIntVect(num);
	if (val != 0xF000F065) 
	  return 0;
	
	if ((regs->ebx & 0xff) == 0x32) {
		switch (regs->eax & 0xFFFF) {
		case 0x1200:
			c = inb(0x3cc);
			c |= 0x02;
			outb(0x3c2,c);
			return 1;
		case 0x1201:
			c = inb(0x3cc);
			c &= ~0x02;
			outb(0x3c2,c);
			return 1;
		}
	}
	if (num == 0x42)
		return 1;
	else
		return 0;
}

#define SUCCESSFUL              0x00
#define DEVICE_NOT_FOUND        0x86
#define BAD_REGISTER_NUMBER     0x87

static int
int1A_handler(struct regs86 *regs)
{
	CARD32 Slot;
//	PciStructPtr pPci;

	if (! CurrentPci) return 0; /* oops */

	switch (regs->eax & 0xFFFF) {
	case 0xb101:
		regs->eax  &= 0xFF00;   /* no config space/special cycle support */
		regs->edx = 0x20494350; /* " ICP" */
		regs->ebx  = 0x0210;    /* Version 2.10 */
		regs->ecx  &= 0xFF00;
		regs->ecx |= (pciMaxBus & 0xFF);   /* Max bus number in system */
		regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
		return 1;
	case 0xb102:
		if (((regs->edx & 0xFFFF) == CurrentPci->VendorID) &&
			((regs->ecx & 0xFFFF) == CurrentPci->DeviceID) &&
			(regs->esi == 0)) {
			regs->eax = (regs->eax & 0x00FF) | (SUCCESSFUL << 8);
			regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
			regs->ebx = pciSlotBX(CurrentPci);
		} else  {
			regs->eax = (regs->eax & 0x00FF) | (DEVICE_NOT_FOUND << 8);
			regs->eflags |= ((unsigned long)0x01); /* set carry flag */
		}
		return 1;
	case 0xb103:
		if (((regs->ecx & 0xFF) == CurrentPci->Interface) &&
			(((regs->ecx & 0xFF00) >> 8) == CurrentPci->SubClass) &&
			(((regs->ecx & 0xFFFF0000) >> 16) == CurrentPci->BaseClass) && 
			((regs->esi & 0xff) == 0)) {
			regs->eax = (regs->eax & 0x00FF) | (SUCCESSFUL << 8);
			regs->ebx = pciSlotBX(CurrentPci);
			regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
		} else {
			regs->eax = (regs->eax & 0x00FF) | (DEVICE_NOT_FOUND << 8);
			regs->eflags |= ((unsigned long)0x01); /* set carry flag */
		}
		return 1;
	case 0xb108:
		if ((Slot = findPci(regs->ebx))) {
			regs->ecx &= 0xFFFFFF00;
			regs->ecx |= PciRead8(regs->edi,Slot);
			regs->eax = (regs->eax & 0x00FF) | (SUCCESSFUL << 8);
			regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
		} else {
			regs->eax = (regs->eax & 0x00FF) | (BAD_REGISTER_NUMBER << 8);
			regs->eflags |= ((unsigned long)0x01); /* set carry flag */
		}
		return 1;
	case 0xb109:
		if ((Slot = findPci(regs->ebx))) {
			regs->ecx &= 0xFFFF0000;
			regs->ecx |= PciRead16(regs->edi,Slot);
			regs->eax = (regs->eax & 0x00FF) | (SUCCESSFUL << 8);
			regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
		} else {
			regs->eax = (regs->eax & 0x00FF) | (BAD_REGISTER_NUMBER << 8);
			regs->eflags |= ((unsigned long)0x01); /* set carry flag */
		}
		return 1;
	case 0xb10a:
		if ((Slot = findPci(regs->ebx))) {
			regs->ecx &= 0;
			regs->ecx |= PciRead32(regs->edi,Slot);
			regs->eax = (regs->eax & 0x00FF) | (SUCCESSFUL << 8);
			regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
		} else {
			regs->eax = (regs->eax & 0x00FF) | (BAD_REGISTER_NUMBER << 8);
			regs->eflags |= ((unsigned long)0x01); /* set carry flag */
		}
		return 1;
	case 0xb10b:
		if ((Slot = findPci(regs->ebx))) {
			PciWrite8(regs->edi,(CARD8)regs->ecx,Slot);
			regs->eax = (regs->eax & 0x00FF) | (SUCCESSFUL << 8);
			regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
		} else {
			regs->eax = (regs->eax & 0x00FF) | (BAD_REGISTER_NUMBER << 8);
			regs->eflags |= ((unsigned long)0x01); /* set carry flag */
		}
		return 1;
	case 0xb10c:
		if ((Slot = findPci(regs->ebx))) {
			PciWrite16(regs->edi,(CARD16)regs->ecx,Slot);
			regs->eax = (regs->eax & 0x00FF) | (SUCCESSFUL << 8);
			regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
		} else {
			regs->eax = (regs->eax & 0x00FF) | (BAD_REGISTER_NUMBER << 8);
			regs->eflags |= ((unsigned long)0x01); /* set carry flag */
		}
		return 1;
	case 0xb10d:
		if ((Slot = findPci(regs->ebx))) {
			PciWrite32(regs->edi,(CARD32)regs->ecx,Slot);
			regs->eax = (regs->eax & 0x00FF) | (SUCCESSFUL << 8);
			regs->eflags &= ~((unsigned long)0x01); /* clear carry flag */
		} else {
			regs->eax = (regs->eax & 0x00FF) | (BAD_REGISTER_NUMBER << 8);
			regs->eflags |= ((unsigned long)0x01); /* set carry flag */
		}
		return 1;
	default:
		return 0;
	}
}

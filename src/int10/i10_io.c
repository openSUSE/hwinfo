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

#include <stdio.h>
#if defined(__alpha__) || defined (__ia64__)
#include <sys/io.h>
#endif
#include "AsmMacros.h"
#include "v86bios.h"
#include "pci.h"


int
port_rep_inb(CARD16 port, CARD8 *base, int d_f, CARD32 count)
{
	register int inc = d_f ? -1 : 1;
	CARD8 *dst = base;
	
	while (count--) {
		*dst = inb(port);
		dst += inc;
	}
	return (dst-base);
}

int
port_rep_inw(CARD16 port, CARD16 *base, int d_f, CARD32 count)
{
	register int inc = d_f ? -1 : 1;
	CARD16 *dst = base;
	
	while (count--) {
		*dst = inw(port);
		dst += inc;
	}
	return (dst-base);
}

int
port_rep_inl(CARD16 port, CARD32 *base, int d_f, CARD32 count)
{
	register int inc = d_f ? -1 : 1;
	CARD32 *dst = base;
	
	while (count--) {
		*dst = inl(port);
		dst += inc;
	}
	return (dst-base);
}

int
port_rep_outb(CARD16 port, CARD8 *base, int d_f, CARD32 count)
{
	register int inc = d_f ? -1 : 1;
	CARD8 *dst = base;
	
	while (count--) {
		outb(port,*dst);
		dst += inc;
	}
	return (dst-base);
}

int
port_rep_outw(CARD16 port, CARD16 *base, int d_f, CARD32 count)
{
	register int inc = d_f ? -1 : 1;
	CARD16 *dst = base;
	
	while (count--) {
		outw(port,*dst);
		dst += inc;
	}
	return (dst-base);
}

int
port_rep_outl(CARD16 port, CARD32 *base, int d_f, CARD32 count)
{
	register int inc = d_f ? -1 : 1;
	CARD32 *dst = base;
	
	while (count--) {
		outl(port,*dst);
		dst += inc;
	}
	return (dst-base);
}

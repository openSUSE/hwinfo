#############################################################################
#
#						Realmode X86 Emulator Library
#
#            	Copyright (C) 1996-1999 SciTech Software, Inc.
#
#  ========================================================================
#
#  Permission to use, copy, modify, distribute, and sell this software and
#  its documentation for any purpose is hereby granted without fee,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation, and that the name of the authors not be used
#  in advertising or publicity pertaining to distribution of the software
#  without specific, written prior permission.  The authors makes no
#  representations about the suitability of this software for any purpose.
#  It is provided "as is" without express or implied warranty.
#
#  THE AUTHORS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
#  EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
#  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
#  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.
#
#  ========================================================================
#
# Descripton:   Linux specific makefile for the x86emu library.
#
#############################################################################

TARGETLIB = ../libhd.a
TARGETDEBUGLIB = ../libx86emud.a

OBJS=\
debug.o \
decode.o \
fpu.o \
ops.o \
ops2.o \
prim_ops.o \
sys.o

DEBUGOBJS=debug.d \
          decode.d \
	  fpu.d \
	  ops.d \
	  ops2.d \
	  prim_ops.d \
	  sys.d

.SUFFIXES: .d

all: $(TARGETLIB)

$(TARGETLIB): $(OBJS)
# prefix objects with x86emu_
	@for i in $? ; do cp -p $$i x86emu_$$i ; ar rv $@ x86emu_$$i || exit ; rm -f x86emu_$$i ; done

$(TARGETDEBUGLIB): $(DEBUGOBJS)
	ar rv $(TARGETDEBUGLIB) $(DEBUGOBJS)

INCS   = -I. -Ix86emu -Iinclude
CFLAGS += -D__DRIVER__ -DFORCE_POST -D_CEXPORT= -DNO_LONG_LONG
CDEBUGFLAGS = -DDEBUG

.c.o:
	gcc -c $(CFLAGS) $(INCS) $*.c

.c.d:
	gcc -c -o$*.d $(CFLAGS) $(CDEBUGFLAGS) $(INCS) $*.c

.cpp.o:
	gcc -c $(CFLAGS) $(INCS) $*.cpp

install: all

distclean: clean
	@rm -f *~

clean:
	@rm -f *.o *.d

validate:	validate.o ../libx86emu.a
	gcc -o validate validate.o -lx86emu -L..

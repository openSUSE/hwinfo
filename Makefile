TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hw
CLEANFILES	= hw hw.static

include Makefile.common

OBJS_NO_TINY	= names.o cpu.o memory.o monitor.o bios.o parallel.o modem.o

.PNONY:	static tiny

hw: hw.o $(LIBHD)
	$(CC) hw.o $(LDFLAGS) -o $@

static: hw
	$(CC) -static hw.o $(LDFLAGS) -o hw.static
	strip -R .note -R .comment hw.static

tiny:
	@make EXTRA_FLAGS=-DLIBHD_TINY
	@cp $(LIBHD) $(LIBHD_T)
	@ar d $(LIBHD_T) $(OBJS_NO_TINY)

install:
	install -d -m 755 /usr/sbin /usr/lib /usr/include
	install -m 755 -s hw /usr/sbin/hwinfo
	install -m 644 $(LIBHD) /usr/lib
	install -m 644 src/hd/hd.h /usr/include

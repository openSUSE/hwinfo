TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hwinfo
CLEANFILES	= hwinfo hwinfo.static

include Makefile.common

OBJS_NO_TINY	= names.o parallel.o modem.o

.PNONY:	static tiny

hwinfo: hwinfo.o $(LIBHD)
	$(CC) hwinfo.o $(LDFLAGS) -o $@

static: hwinfo
	$(CC) -static hwinfo.o $(LDFLAGS) -o hwinfo.static
	strip -R .note -R .comment hwinfo.static

tiny:
	@make EXTRA_FLAGS=-DLIBHD_TINY
	@cp $(LIBHD) $(LIBHD_T)
	@ar d $(LIBHD_T) $(OBJS_NO_TINY)

install:
	install -d -m 755 /usr/sbin /usr/lib /usr/include
	install -m 755 -s hwinfo /usr/sbin
	install -m 644 $(LIBHD) /usr/lib
	install -m 644 src/hd/hd.h /usr/include

TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hwinfo
CLEANFILES	= hwinfo hwinfo.static

include Makefile.common

OBJS_NO_TINY	= names.o parallel.o modem.o

.PNONY:	static tiny

hwinfo: hwinfo.o $(LIBHD)
	$(CC) hwinfo.o $(LDFLAGS) -lhd -o $@

static: hwinfo
	$(CC) -static hwinfo.o $(LDFLAGS) -o hwinfo.static
	strip -R .note -R .comment hwinfo.static

tiny:
	@make EXTRA_FLAGS=-DLIBHD_TINY

shared: hwinfo.o
	@make EXTRA_FLAGS=-fpic
	$(CC) -shared -Wl,--whole-archive $(LIBHD) -Wl,--no-whole-archive \
		-Wl,-soname=libhd.so.$(LIBHD_MAJOR_VERSION)\
		-o $(LIBHD_SO) 
	$(CC) hwinfo.o $(LDFLAGS) -Lsrc -lhd -lpthread -o hwinfo

install:
	install -d -m 755 /usr/sbin /usr/lib /usr/include
	install -m 755 -s hwinfo /usr/sbin
	if [ -f $(LIBHD_SO) ] ; then \
		install $(LIBHD_SO) /usr/lib ; \
		ln -snf libhd.so.$(LIBHD_VERSION) /usr/lib/libhd.so ; \
	else \
		install -m 644 $(LIBHD) /usr/lib ; \
	fi
	install -m 644 src/hd/hd.h /usr/include

TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hwinfo hwscan
CLEANFILES	= hwinfo hwinfo.static hwscan hwscan.static
LIBDIR		= /usr/lib

include Makefile.common

SHARED_FLAGS	=
OBJS_NO_TINY	= names.o parallel.o modem.o

.PNONY:	fullstatic static shared tiny

hwscan: hwscan.o $(LIBHD)
	$(CC) hwscan.o $(LDFLAGS) -lhd -o $@

hwinfo: hwinfo.o $(LIBHD)
	$(CC) hwinfo.o $(LDFLAGS) -lhd -o $@

# kept for compatibility
shared:
	@make

tiny:
	@make EXTRA_FLAGS=-DLIBHD_TINY SHARED_FLAGS=

tinydiet:
	@make CC="diet gcc" EXTRA_FLAGS="-fno-pic -DLIBHD_TINY -DDIET" SHARED_FLAGS=

static:
	@make SHARED_FLAGS=

fullstatic: static
	$(CC) -static hwinfo.o $(LDFLAGS) -lhd -o hwinfo.static
	$(CC) -static hwscan.o $(LDFLAGS) -lhd -o hwscan.static
	strip -R .note -R .comment hwinfo.static
	strip -R .note -R .comment hwscan.static

install:
	install -d -m 755 $(DESTDIR)/usr/sbin $(DESTDIR)$(LIBDIR) $(DESTDIR)/usr/include
	install -m 755 -s hwinfo $(DESTDIR)/usr/sbin
	install -m 755 -s hwscan $(DESTDIR)/usr/sbin
	if [ -f $(LIBHD_SO) ] ; then \
		install $(LIBHD_SO) $(DESTDIR)$(LIBDIR) ; \
		ln -snf libhd.so.$(LIBHD_VERSION) $(DESTDIR)$(LIBDIR)/libhd.so.$(LIBHD_MAJOR_VERSION) ; \
		ln -snf libhd.so.$(LIBHD_MAJOR_VERSION) $(DESTDIR)$(LIBDIR)/libhd.so ; \
	else \
		install -m 644 $(LIBHD) $(DESTDIR)$(LIBDIR) ; \
	fi
	install -m 644 src/hd/hd.h $(DESTDIR)/usr/include
	install -m 755 hwbootscan $(DESTDIR)/usr/sbin
	install -m 755 hwbootscan.rc $(DESTDIR)/etc/init.d/hwscan

TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hwinfo hwscan hwscand hwscanqueue
CLEANFILES	= hwinfo hwinfo.static hwscan hwscan.static doc/libhd doc/*~
LIBDIR		= /usr/lib
LIBS		= -lhd
SLIBS		= -lhd -lsysfs

include Makefile.common

SHARED_FLAGS	=
OBJS_NO_TINY	= names.o parallel.o modem.o

.PHONY:	fullstatic static shared tiny doc diet tinydiet uc tinyuc

hwscan: hwscan.o $(LIBHD)
	$(CC) hwscan.o $(LDFLAGS) $(LIBS) -o $@

hwinfo: hwinfo.o $(LIBHD)
	$(CC) hwinfo.o $(LDFLAGS) $(LIBS) -o $@

hwscand: hwscand.o
	$(CC) $< $(LDFLAGS) -o $@

hwscanqueue: hwscanqueue.o
	$(CC) $< $(LDFLAGS) -o $@

# kept for compatibility
shared:
	@make

tiny:
	@make EXTRA_FLAGS=-DLIBHD_TINY SHARED_FLAGS= LIBS="$(SLIBS)"

diet:
	@make CC="diet gcc" EXTRA_FLAGS="-fno-pic -DDIET" SHARED_FLAGS= LIBS="$(SLIBS)"

tinydiet:
	@make CC="diet gcc" EXTRA_FLAGS="-fno-pic -DLIBHD_TINY -DDIET" SHARED_FLAGS= LIBS="$(SLIBS)"

uc:
	@make CC="/opt/i386-linux-uclibc/bin/i386-uclibc-gcc" EXTRA_FLAGS="-fno-pic -DUCLIBC" SHARED_FLAGS= LIBS="$(SLIBS)"

tinyuc:
	@make CC="/opt/i386-linux-uclibc/usr/bin/gcc" EXTRA_FLAGS="-fno-pic -DLIBHD_TINY -DUCLIBC" SHARED_FLAGS= LIBS="$(SLIBS)"

static:
	make SHARED_FLAGS= LIBS="$(SLIBS)"

fullstatic: static
	$(CC) -static hwinfo.o $(LDFLAGS) $(SLIBS) -o hwinfo.static
	$(CC) -static hwscan.o $(LDFLAGS) $(SLIBS) -o hwscan.static
	strip -R .note -R .comment hwinfo.static
	strip -R .note -R .comment hwscan.static

doc:
	@cd doc ; doxygen libhd.doxy

install:
	install -d -m 755 $(DESTDIR)/usr/sbin $(DESTDIR)$(LIBDIR) \
		$(DESTDIR)/usr/include $(DESTDIR)/etc/init.d
	install -m 755 $(TARGETS) $(DESTDIR)/usr/sbin
	install -m 755 -s src/ids/check_hd $(DESTDIR)/usr/sbin
	install -m 755 src/ids/convert_hd $(DESTDIR)/usr/sbin
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
	install -m 755 src/isdn/cdb/mk_isdnhwdb $(DESTDIR)/usr/sbin
	install -d -m 755 $(DESTDIR)/usr/share/hwinfo
	install -m 644 src/isdn/cdb/ISDN.CDB.txt $(DESTDIR)/usr/share/hwinfo
	install -m 644 src/isdn/cdb/ISDN.CDB.hwdb $(DESTDIR)/usr/share/hwinfo


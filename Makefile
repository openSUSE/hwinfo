TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hwinfo hwscan hwscand hwscanqueue
CLEANFILES	= hwinfo hwinfo.static hwscan hwscan.static hwscand hwscanqueue doc/libhd doc/*~
LIBDIR		= /lib
ULIBDIR		= /usr$(LIBDIR)
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
	install -d -m 755 $(DESTDIR)/sbin $(DESTDIR)/usr/sbin $(DESTDIR)$(LIBDIR) $(DESTDIR)$(ULIBDIR)\
		$(DESTDIR)/usr/include $(DESTDIR)/etc/init.d
	install -m 755 hwinfo $(DESTDIR)/usr/sbin
	install -m 755 hwscan hwscand hwscanqueue $(DESTDIR)/sbin
	install -m 755 -s src/ids/check_hd $(DESTDIR)/usr/sbin
	install -m 755 src/ids/convert_hd $(DESTDIR)/usr/sbin
	if [ -f $(LIBHD_SO) ] ; then \
		install $(LIBHD_SO) $(DESTDIR)$(LIBDIR) ; \
		ln -snf libhd.so.$(LIBHD_VERSION) $(DESTDIR)$(LIBDIR)/libhd.so.$(LIBHD_MAJOR_VERSION) ; \
		ln -snf $(LIBDIR)/libhd.so.$(LIBHD_MAJOR_VERSION) $(DESTDIR)$(ULIBDIR)/libhd.so ; \
	else \
		install -m 644 $(LIBHD) $(DESTDIR)$(ULIBDIR) ; \
	fi
	install -m 644 src/hd/hd.h $(DESTDIR)/usr/include
	install -m 755 hwbootscan getsysinfo gen-hwcfg-disk.sh $(DESTDIR)/usr/sbin
	install -m 755 hwbootscan.rc $(DESTDIR)/etc/init.d/hwscan
	install -m 755 src/isdn/cdb/mk_isdnhwdb $(DESTDIR)/usr/sbin
	install -d -m 755 $(DESTDIR)/usr/share/hwinfo
	install -m 644 src/isdn/cdb/ISDN.CDB.txt $(DESTDIR)/usr/share/hwinfo
	install -m 644 src/isdn/cdb/ISDN.CDB.hwdb $(DESTDIR)/usr/share/hwinfo


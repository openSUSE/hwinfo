TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hwinfo hwinfo.pc
CLEANFILES	= hwinfo hwinfo.pc hwinfo.static hwscan hwscan.static hwscand hwscanqueue doc/libhd doc/*~
LIBDIR		= /usr/lib
ULIBDIR		= $(LIBDIR)
LIBS		= -lhd
SLIBS		= -lhd -ldbus-1 -lhal -lx86emu
TLIBS		= -lhd_tiny -ldbus-1 -lhal -lx86emu
SO_LIBS		= -ldbus-1 -lhal -lx86emu
TSO_LIBS	= -ldbus-1 -lhal -lx86emu

# ia64
ifneq "$(findstring $(ARCH), i386 x86_64)" ""
SLIBS		+= -lx86emu
TLIBS		+= -lx86emu
SO_LIBS		+= -lx86emu
TSO_LIBS	+= -lx86emu
endif

export SO_LIBS

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

hwinfo.pc: hwinfo.pc.in
	VERSION=`cat VERSION`; \
	sed -e "s,@VERSION@,$${VERSION},g" -e 's,@LIBDIR@,$(ULIBDIR),g;s,@LIBS@,$(LIBS),g' $< > $@.tmp && mv $@.tmp $@

# kept for compatibility
shared:
	@make

tiny:
	@make EXTRA_FLAGS=-DLIBHD_TINY LIBHD_BASE=libhd_tiny LIBS="$(TLIBS)" SO_LIBS="$(TSO_LIBS)"

tinyinstall:
	@make EXTRA_FLAGS=-DLIBHD_TINY LIBHD_BASE=libhd_tiny LIBS="$(TLIBS)" SO_LIBS="$(TSO_LIBS)" install

tinystatic:
	@make EXTRA_FLAGS=-DLIBHD_TINY LIBHD_BASE=libhd_tiny SHARED_FLAGS= LIBS="$(TLIBS)" SO_LIBS="$(TSO_LIBS)"

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
	strip -R .note -R .comment hwinfo.static

doc:
	@cd doc ; doxygen libhd.doxy

install:
	install -d -m 755 $(DESTDIR)/sbin $(DESTDIR)/usr/sbin $(DESTDIR)$(ULIBDIR) \
		$(DESTDIR)$(ULIBDIR)/pkgconfig $(DESTDIR)/usr/include
	install -m 755 hwinfo $(DESTDIR)/usr/sbin
	install -m 755 src/ids/check_hd $(DESTDIR)/usr/sbin
	install -m 755 src/ids/convert_hd $(DESTDIR)/usr/sbin
	if [ -f $(LIBHD_SO) ] ; then \
		install $(LIBHD_SO) $(DESTDIR)$(ULIBDIR) ; \
		ln -snf $(LIBHD_NAME) $(DESTDIR)$(ULIBDIR)/$(LIBHD_SONAME) ; \
		ln -snf $(LIBHD_SONAME) $(DESTDIR)$(ULIBDIR)/$(LIBHD_BASE).so ; \
	else \
		install -m 644 $(LIBHD) $(DESTDIR)$(ULIBDIR) ; \
	fi
	install -m 644 hwinfo.pc $(DESTDIR)$(ULIBDIR)/pkgconfig
	install -m 644 src/hd/hd.h $(DESTDIR)/usr/include
	install -m 755 getsysinfo $(DESTDIR)/usr/sbin
	install -m 755 src/isdn/cdb/mk_isdnhwdb $(DESTDIR)/usr/sbin
	install -d -m 755 $(DESTDIR)/usr/share/hwinfo
	install -m 644 src/isdn/cdb/ISDN.CDB.txt $(DESTDIR)/usr/share/hwinfo
	install -m 644 src/isdn/cdb/ISDN.CDB.hwdb $(DESTDIR)/usr/share/hwinfo


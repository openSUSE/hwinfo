TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.log
DISTCLEANFILES	= *.h *.xml *.ids check_hd

include $(TOPDIR)/Makefile.common

IDFILES	= src/bus src/class src/extra src/special \
	  src/scanner src/network src/names.usbutils src/names.usb \
	  src/isapnp src/monitor src/camera src/tv src/dvb src/chipcard

ifeq "$(ARCH)" "i386"
IDFILES += src/x11.i386
endif

ifeq "$(ARCH)" "ppc"
IDFILES += src/x11.ppc src/x11.ppc.special
endif

ifeq "$(ARCH)" "alpha"
IDFILES += src/x11.axp
endif

ifeq "$(ARCH)" "sparc"
IDFILES += src/x11.sparc
endif

ifeq "$(ARCH)" "sparc64"
IDFILES += src/x11.sparc
endif

ifeq "$(ARCH)" "ia64"
IDFILES += src/x11.ia64
endif

ifeq "$(ARCH)" "x86_64"
IDFILES += src/x11.x86_64
endif

IDFILES += src/names.pciutils src/storage src/sound src/mouse src/braille

$(LIBHD_D): hd_ids.o
	ar r $(LIBHD) $?

check_hd: check_hd.c
	$(CC) $< -o $@

hd_ids.c: hd_ids.h hd_ids_tiny.h

hd0.ids: $(IDFILES)
	./convert_hd $(IDFILES)
	mv hd.ids hd0.ids

hd_ids.h hd.ids: check_hd hd0.ids
	./check_hd --check --sort --cfile hd_ids.h hd0.ids

hd_ids_tiny.h: check_hd hd.ids
	./check_hd --mini --cfile hd_ids_tiny.h --log=hd_tiny.log --out=hd_tiny.ids hd.ids


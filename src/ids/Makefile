TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.log
DISTCLEANFILES	= *.h *.xml *.ids check_hd

include $(TOPDIR)/Makefile.common

IDFILES	= src/names.SuSE src/bus src/class src/names.special \
	  src/scanner src/network src/names.usbutils src/names.usb \
	  src/names.eisa src/monitor src/names.x11 src/camera src/tv src/dvb

IDFILES += src/names.x11.ia32

ifeq "$(ARCH)" "i386"
IDFILES += src/x11.hwinfo.i386
endif

ifeq "$(ARCH)" "ppc"
IDFILES += src/x11.hwinfo.ppc src/x11.ppc.special
endif

ifeq "$(ARCH)" "alpha"
IDFILES += src/x11.hwinfo.axp
endif

ifeq "$(ARCH)" "sparc"
IDFILES += src/x11.hwinfo.sparc
endif

ifeq "$(ARCH)" "sparc64"
IDFILES += src/x11.hwinfo.sparc
endif

ifeq "$(ARCH)" "ia64"
IDFILES += src/x11.hwinfo.ia64
endif

IDFILES += src/names.pciutils src/storage.scsi src/storage.block \
	   src/sound.alsa src/sound.kernel src/mouse src/braille src/special

$(LIBHD_D): hd_ids.o
	ar r $(LIBHD) $?

check_hd: check_hd.c
	$(CC) $< -o $@

hd_ids.c: hd_ids.h hd_ids_tiny.h

hd0.ids: $(IDFILES)
	./convert_hd --fix-driver $(IDFILES) 2>hd0.log
	mv hd.ids hd0.ids

hd_ids.h hd.ids: check_hd hd0.ids
	./check_hd --check --sort --cfile hd_ids.h hd0.ids

hd_ids_tiny.h: check_hd hd.ids
	./check_hd --mini --cfile hd_ids_tiny.h --log=hd_tiny.log --out=hd_tiny.ids hd.ids


TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.log hd0.ids
DISTCLEANFILES	= *.h *.xml *.ids check_hd

include $(TOPDIR)/Makefile.common

IDFILES	= src/names.SuSE src/names.bus src/names.class src/names.special \
	  src/names.scanner src/names.network src/names.usbutils src/names.usb\
	  src/names.eisa src/names.monitor src/names.x11 src/names.camera

IDFILES += src/drivers.xfree src/names.x11.ia32

IDFILES += src/names.video src/names.pciutils\
	   src/drivers.network src/drivers.scsi src/drivers.block src/drivers.monitor\
	   src/drivers.alsa src/drivers.audio src/drivers.mouse src/drivers.video\
	   src/drivers.braille src/drivers.special

$(LIBHD_D): hd_ids.o
	ar r $(LIBHD) $?

check_hd: check_hd.o
	$(CC) $< $(LDFLAGS) -o $@

hd_ids.c: hd_ids.h

hd0.ids: $(IDFILES)
	./convert_hd --fix-driver $(IDFILES) 2>hd0.log
	mv hd.ids hd0.ids

hd_ids.h: check_hd hd0.ids
	./check_hd --check --sort --cfile hd_ids.h hd0.ids

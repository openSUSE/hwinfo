TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.c *.err hd.names hd.drivers

include $(TOPDIR)/Makefile.common

IDFILES = names.SuSE names.bus names.class names.special names.usb\
	  names.eisa names.monitor names.pciutils\
	  drivers.network drivers.scsi drivers.block drivers.x11 drivers.monitor\
	  drivers.audio drivers.mouse drivers.video\
	  names.SaX2 names.SaX

OBJS	+= names.o names_s.o drivers.o

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

names.c names_s.c drivers.c: $(IDFILES)
	./mk_ids $(IDFILES) 2>mk_ids.err


TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.h *.err hd.names hd.drivers drivers.c

include $(TOPDIR)/Makefile.common

IDFILES	= names.SuSE names.bus names.class names.special \
	  names.scanner names.usb names.eisa names.monitor

ifeq "$(ARCH)" "i386"
IDFILES	+= drivers.x11.ia32 names.x11.ia32
endif

ifeq "$(ARCH)" "ppc"
IDFILES	+= drivers.x11.ppc drivers.x11special.ppc names.x11.ppc
endif

ifeq "$(ARCH)" "alpha"
IDFILES	+= drivers.x11.axp names.x11.axp
endif

ifeq "$(ARCH)" "sparc"
IDFILES	+= drivers.x11.sparc names.x11.sparc
endif

ifeq "$(ARCH)" "sparc64"
IDFILES	+= drivers.x11.sparc names.x11.sparc
endif

ifeq "$(ARCH)" "ia64"
IDFILES	+= drivers.x11.ia64 names.x11.ia64
endif

IDFILES += names.pciutils\
	   drivers.network drivers.scsi drivers.block drivers.monitor\
	   drivers.audio drivers.mouse drivers.video drivers.braille

OBJS	+= names.o drivers.o

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

names.c: names.h names_s.h

names.h names_s.h drivers.h: $(IDFILES)
	./mk_ids $(IDFILES) 2>mk_ids.err


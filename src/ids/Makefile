TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.c *.err hd.names hd.drivers

include $(TOPDIR)/Makefile.common

IDFILES	= names.SuSE names.bus names.class names.special names.usb\
	  names.eisa names.monitor

ifeq "$(ARCH)" "i386"
IDFILES	+= drivers.x11.ia32 names.x11.ia32
endif

ifeq "$(ARCH)" "ppc"
IDFILES	+= drivers.x11.ppc names.x11.ppc
endif

ifeq "$(ARCH)" "axp"
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

#ifeq "$(ARCH)" "s390"
#IDFILES	+= drivers.x11.s390 names.x11.s390
#endif

IDFILES += names.pciutils\
	   drivers.network drivers.scsi drivers.block drivers.monitor\
	   drivers.audio drivers.mouse drivers.video
#	   drivers.x11 names.SaX2 names.SaX

OBJS	+= names.o names_s.o drivers.o

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

names.c names_s.c drivers.c: $(IDFILES)
	./mk_ids $(IDFILES) 2>mk_ids.err


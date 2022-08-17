TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.log src/*~
DISTCLEANFILES	= *.h *.xml *.ids check_hd

include $(TOPDIR)/Makefile.common

# ifeq "$(ARCH)" "ppc"
# IDFILES += src/ppc
# endif

# ifeq "$(ARCH)" "ppc64"
# IDFILES += src/ppc
# endif

IDFILES	+= src/bus src/class src/extra src/special src/scanner src/network \
	  src/usb src/usb2 src/isapnp src/monitor src/camera src/tv2 src/tv src/dvb2 src/dvb \
	  src/chipcard src/modem src/pcmcia src/s390 src/sdio

ifeq "$(ARCH)" "i386"
IDFILES += src/x11.i386 src/modem.i386
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
IDFILES += src/x11.amd64
endif

IDFILES += src/pci src/storage src/sound src/mouse src/braille

$(LIBHD_D): hd_ids.o
	ar r $(LIBHD) $?

CHECK_HD_SRC = check_hd.c \
               thread_pool.c

check_hd: $(CHECK_HD_SRC)
	$(CC) $(CFLAGS) -lpthread $(CHECK_HD_SRC) -o $@

hd_ids.c: hd_ids.h hd_ids_tiny.h

hd_ids.h hd.ids: check_hd $(IDFILES)
	./check_hd --check --sort --cfile hd_ids.h $(IDFILES)

hd_ids_tiny.h: check_hd hd.ids
	./check_hd --mini --cfile hd_ids_tiny.h --log=hd_tiny.log --out=hd_tiny.ids hd.ids


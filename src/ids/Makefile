TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.c *.err hd.names hd.drivers

include $(TOPDIR)/Makefile.common

IDFILES = names.SuSE names.bus names.class names.eisa names.monitor\
	  names.pciutils names.SaX\
	  drivers.SuSE

OBJS	+= names.o names_s.o drivers.o

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

names.c names_s.c drivers.c: $(IDFILES)
	./mk_ids $(IDFILES) 2>mk_ids.err


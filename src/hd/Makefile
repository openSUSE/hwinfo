TOPDIR	= ../..
TARGETS	= $(LIBHD_D)

include $(TOPDIR)/Makefile.common

version.h: $(TOPDIR)/VERSION
	@echo "#define HD_VERSION \"`cat $(TOPDIR)/VERSION`\"" >$@

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

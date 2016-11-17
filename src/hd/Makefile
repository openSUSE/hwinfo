TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= version.h

include $(TOPDIR)/Makefile.common

version.h: $(TOPDIR)/VERSION
	@echo "#define HD_VERSION_STRING \"`cat $(TOPDIR)/VERSION`\"" >$@

hd.h: $(TOPDIR)/VERSION
	@perl -pi -e "s/define\s+HD_VERSION\s+\d+/define HD_VERSION\t$(LIBHD_MAJOR_VERSION)/" $@
	@perl -pi -e "s/define\s+HD_MINOR_VERSION\s+\d+/define HD_MINOR_VERSION\t$(LIBHD_MINOR_VERSION)/" $@

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

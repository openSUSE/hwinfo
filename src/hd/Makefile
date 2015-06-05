TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= version.h

include $(TOPDIR)/Makefile.common

CFLAGS		+= $(shell pkg-config --cflags dbus-1)

version.h: $(TOPDIR)/VERSION
	@echo "#define HD_VERSION_STRING \"`cat $(TOPDIR)/VERSION`\"" >$@

hd.h: $(TOPDIR)/VERSION
	@perl -pi -e "s/define\s+HD_VERSION\s+\d+/define HD_VERSION\t$(LIBHD_MAJOR_VERSION)/" $@

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

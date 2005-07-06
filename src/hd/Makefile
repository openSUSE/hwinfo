TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= version.h

include $(TOPDIR)/Makefile.common

CFLAGS		+= -I /usr/include/dbus-1.0 -I /usr/lib64/dbus-1.0/include -I /usr/lib/dbus-1.0/include

version.h: $(TOPDIR)/VERSION
	@echo "#define HD_VERSION_STRING \"`cat $(TOPDIR)/VERSION`\"" >$@

hd.h: $(TOPDIR)/VERSION
	@perl -pi -e "s/define\s+HD_VERSION\s+\d+/define HD_VERSION\t$(LIBHD_MAJOR_VERSION)/" $@

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

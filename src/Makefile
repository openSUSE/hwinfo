TOPDIR		= ..
SUBDIRS		= hd ids isdn pnpdump
TARGETS		= touch $(LIBHD)
CLEANFILES	= $(LIBHD) $(LIBHD_D) $(LIBHD_SO)

.PHONY: touch

include $(TOPDIR)/Makefile.common

#ifeq "$(ARCH)" "alpha"
#SUBDIRS	:= $(filter-out isdn, $(SUBDIRS))
#endif

ifeq "$(findstring $(ARCH), i386 alpha)" ""
SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
endif

ifeq "$(ARCH)" "s390"
SUBDIRS	:= $(filter-out isdn, $(SUBDIRS))
endif

$(LIBHD): $(OBJS)
	ar r $@ $?
	@rm -f $(LIBHD_D)

subdirs: touch

touch:
	@touch -r $(LIBHD) $(LIBHD_D) 2>/dev/null || true

TOPDIR		= ..
SUBDIRS		= hd ids isdn pnpdump smp
TARGETS		= touch $(LIBHD)
CLEANFILES	= $(LIBHD) $(LIBHD_D) $(LIBHD_T)

.PHONY: touch

include $(TOPDIR)/Makefile.common

#ifeq "$(ARCH)" "alpha"
#SUBDIRS	:= $(filter-out isdn, $(SUBDIRS))
#endif

ifeq "$(ARCH)" "ppc"
SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
endif

ifeq "$(ARCH)" "sparc"
SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
endif

ifeq "$(ARCH)" "sparc64"
SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
endif

ifeq "$(ARCH)" "s390"
SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
SUBDIRS	:= $(filter-out isdn, $(SUBDIRS))
endif

$(LIBHD): $(OBJS)
	ar r $@ $?
	@rm -f $(LIBHD_D)

subdirs: touch

touch:
	@touch -r $(LIBHD) $(LIBHD_D) 2>/dev/null || true

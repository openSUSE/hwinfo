TOPDIR		= ..
SUBDIRS		= hd ids pnpdump smp
TARGETS		= touch $(LIBHD)
CLEANFILES	= $(LIBHD) $(LIBHD_D) $(LIBHD_T)

.PHONY: touch

include $(TOPDIR)/Makefile.common

# remove the isa-pnp stuff for ppc's
ifeq "$(ARCH)" "ppc"
SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
endif

# remove the isa-pnp stuff for sparc's
ifeq "$(ARCH)" "sparc"
SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
endif

$(LIBHD): $(OBJS)
	ar r $@ $?
	@rm -f $(LIBHD_D)

subdirs: touch

touch:
	@touch -r $(LIBHD) $(LIBHD_D) 2>/dev/null || true

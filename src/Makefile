TOPDIR		= ..
SUBDIRS		= hd isdn ids
TARGETS		= touch $(LIBHD) $(LIBHD_SO)
CLEANFILES	= $(LIBHD) $(LIBHD_D) $(LIBHD_SO) libhd.so*
LIBS		= $(LIBHD)

.PHONY: touch

include $(TOPDIR)/Makefile.common

ifneq "$(findstring $(ARCH), i386 x86_64 ia64)" ""
SUBDIRS	+= x86emu int10
LIBS	+= libx86emu.a
endif

#ifeq "$(findstring $(ARCH), i386 alpha)" ""
#SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
#endif

$(LIBHD): $(OBJS)
	ar r $@ $?
	@rm -f $(LIBHD_D)

ifdef SHARED_FLAGS
$(LIBHD_SO): $(LIBS)
	echo $(SHARED_FLAGS)
	$(CC) -shared -Wl,--whole-archive $(LIBS) -Wl,--no-whole-archive \
		-Wl,-soname=libhd.so.$(LIBHD_MAJOR_VERSION) \
		-o $(LIBHD_SO) 
	ln -snf libhd.so.$(LIBHD_VERSION) libhd.so.$(LIBHD_MAJOR_VERSION)
	ln -snf libhd.so.$(LIBHD_MAJOR_VERSION) libhd.so
else
$(LIBHD_SO):
endif

subdirs: touch

touch:
	@touch -r $(LIBHD) $(LIBHD_D) 2>/dev/null || true

TOPDIR		= ..
SUBDIRS		= hd isdn pnpdump ids
TARGETS		= touch $(LIBHD) $(LIBHD_SO)
CLEANFILES	= $(LIBHD) $(LIBHD_D) $(LIBHD_SO) libhd.so.$(LIBHD_MAJOR_VERSION) libhd.so

.PHONY: touch

include $(TOPDIR)/Makefile.common

ifeq "$(ARCH)" "alpha"
SUBDIRS	:= $(filter-out isdn, $(SUBDIRS))
endif

ifeq "$(ARCH)" "i386"
SUBDIRS	+= int10
endif

ifeq "$(findstring $(ARCH), i386 alpha)" ""
SUBDIRS	:= $(filter-out pnpdump, $(SUBDIRS))
endif

ifeq "$(ARCH)" "s390"
SUBDIRS	:= $(filter-out isdn, $(SUBDIRS))
endif

ifeq "$(ARCH)" "s390x"
SUBDIRS	:= $(filter-out isdn, $(SUBDIRS))
endif

$(LIBHD): $(OBJS)
	ar r $@ $?
	@rm -f $(LIBHD_D)

ifdef SHARED_FLAGS
$(LIBHD_SO): $(LIBHD)
	echo $(SHARED_FLAGS)
	$(CC) -shared -Wl,--whole-archive $(LIBHD) -Wl,--no-whole-archive \
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

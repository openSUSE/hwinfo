TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hw
CLEANFILES	= hw

include Makefile.common

hw: hw.o $(LIBHD)
	$(CC) hw.o $(LDFLAGS) -o $@

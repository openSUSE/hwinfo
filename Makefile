TOPDIR		= $(CURDIR)
SUBDIRS		= src
TARGETS		= hw
CLEANFILES	= hw

include Makefile.common

hw: hw.o $(LIBHD)
	$(CC) hw.o $(LDFLAGS) -o $@

install:
	install -d -m 755 /usr/sbin /usr/lib
	install -m 755 -s hw /usr/sbin
	install -m 644 $(LIBHD) /usr/lib

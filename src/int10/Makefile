TOPDIR	= ../..
TARGETS	= $(LIBHD_D)

include $(TOPDIR)/Makefile.common

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

i10_v86.o: i10_v86.c
	$(CC) -c $(CFLAGS) -fno-strict-aliasing $<

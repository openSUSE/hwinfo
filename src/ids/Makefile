TOPDIR		= ../..
TARGETS		= $(LIBHD_D)
CLEANFILES	= *.c *.err id_list id_list_r

include $(TOPDIR)/Makefile.common

IDFILES = id_list.bus id_list.class id_list.monitor id_list.eisa \
	  pci.ids id_list.pci id_list.compat \
	  pci.driver Identity

OBJS	+= id_list.o id_list_r.o

$(LIBHD_D): $(OBJS)
	ar r $(LIBHD) $?

id_list.c: $(IDFILES)
	./mk_ids $(IDFILES) 2>$*.err >$*
	./mk_include $* >$@

id_list_r.c: $(IDFILES)
	@./mk_ids -r $(IDFILES) 2>$*.err >$*
	@./mk_include $* >$@

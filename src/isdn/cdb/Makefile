TOPDIR		= ../../..
TARGETS		= isdn_cdb.h
CLEANFILES	= isdn_cdb isdn_cdb.h lex.yy.c
CDB_ISDN_FILE   = ISDN.$(ARCH).txt

include $(TOPDIR)/Makefile.common

isdn_cdb.h: isdn_cdb $(CDB_ISDN_FILE)
	rm -f isdn_cdb.h
	./isdn_cdb $(CDB_ISDN_FILE) isdn_cdb.h

lex.yy.c: isdn_cdb.lex
	lex isdn_cdb.lex

TOPDIR		= ../../..
TARGETS		= isdn_cdb.h
CLEANFILES	= isdn_cdb isdn_cdb.h lex.yy.c ISDN.CDB.txt

include $(TOPDIR)/Makefile.common

ISDN.CDB.txt:
	if [ -f ISDN.$(ARCH).txt ]; then \
		ln -s ISDN.$(ARCH).txt ISDN.CDB.txt; \
	else \
		ln -s ISDN.ppc.txt ISDN.CDB.txt; \
	fi

isdn_cdb.h: isdn_cdb ISDN.CDB.txt
	rm -f isdn_cdb.h
	./isdn_cdb ISDN.CDB.txt isdn_cdb.h

lex.yy.c: isdn_cdb.lex
	lex isdn_cdb.lex

TOPDIR		= ../../..
TARGETS		= mk_isdnhwdb isdn_cdb.h ISDN.CDB.hwdb
CLEANFILES	= isdn_cdb isdn_cdb.h lex.yy.c ISDN.CDB.txt mk_isdnhwdb ISDN.CDB.hwdb

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

isdn_cdb: isdn_cdb.o cdb_read.o

mk_isdnhwdb: mk_isdnhwdb.o cdb_read.o

cdb_read.o: cdb_read.c cdb_read.h

isdn_cdb.o: isdn_cdb.c cdb_read.h lex.yy.c

mk_isdnhwdb.o: mk_isdnhwdb.c cdb_read.h lex.yy.c

ISDN.CDB.hwdb: mk_isdnhwdb ISDN.CDB.txt
	rm -f  ISDN.CDB.hwdb
	./mk_isdnhwdb ISDN.CDB.txt ISDN.CDB.hwdb

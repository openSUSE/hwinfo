TOPDIR		= ../../..
TARGETS		= ihw_db.h
CLEANFILES	= makelib makelib.log ihw_db.h

include $(TOPDIR)/Makefile.common

ihw_db.h: makelib isdncards
	rm -f ihw_db.h
	./makelib isdncards ihw_db.h

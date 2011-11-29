noinst_LTLIBRARIES = libephyhistory.la

libephyhistory_la_SOURCES = \
	ephy-history-service.c		    \
	ephy-history-service.h		    \
	ephy-history-service-hosts-table.c  \
	ephy-history-service-private.h	    \
	ephy-history-service-urls-table.c   \
	ephy-history-service-visits-table.c \
	ephy-history-types.c 		    \
	ephy-history-types.h

libephyhistory_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	$(AM_CPPFLAGS)

libephyhistory_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)	\
	$(AM_CFLAGS)

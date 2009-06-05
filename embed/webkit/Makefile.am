noinst_LTLIBRARIES = libephywebkitembed.la

libephywebkitembed_la_SOURCES = 	\
	webkit-embed-persist.c          \
	webkit-embed-persist.h		\
	webkit-embed-single.c	        \
	webkit-embed-single.h

libephywebkitembed_la_CPPFLAGS = \
	-I$(top_srcdir)/lib	    		\
	-I$(top_srcdir)/embed	    		\
	-I$(top_srcdir)		    		\
	-DSHARE_DIR=\"$(pkgdatadir)\"   	\
	-DBUILDING_GDK__=1 \
	-DBUILDING_CAIRO__=1 \
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CPPFLAGS)


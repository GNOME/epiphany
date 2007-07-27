noinst_LTLIBRARIES = libephywebkitembed.la

libephywebkitembed_la_SOURCES = 	\
	webkit-embed.cpp	        \
	webkit-embed.h			\
	webkit-embed-find.cpp           \
	webkit-embed-find.h		\
	webkit-embed-persist.cpp        \
	webkit-embed-persist.h		\
	webkit-embed-single.cpp	        \
	webkit-embed-single.h

libephywebkitembed_la_CPPFLAGS = \
	-I$(top_srcdir)/lib	    		\
	-I$(top_srcdir)/embed	    		\
	-I$(top_srcdir)		    		\
	$(WEBKIT_CFLAGS)                        \
	-DSHARE_DIR=\"$(pkgdatadir)\"   	\
	-DBUILDING_GDK__=1 \
	-DBUILDING_CAIRO__=1 \
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CPPFLAGS)


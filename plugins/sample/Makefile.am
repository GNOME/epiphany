plugindir = $(libdir)/epiphany/plugins

INCLUDES = \
	-I$(top_srcdir)/src 		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/src/bookmarks   \
	$(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"

plugin_LTLIBRARIES = libsample.la

libsample_la_SOURCES = \
	sample.c

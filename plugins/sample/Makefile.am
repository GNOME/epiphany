plugindir = $(libdir)/epiphany/plugins

INCLUDES = \
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/bookmarks	\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/embed		\
	$(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

plugin_LTLIBRARIES = libephy.la

libephy_la_SOURCES = \
	sample.c

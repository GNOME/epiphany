plugindir = $(libdir)/epiphany/plugins

INCLUDES = \
	-I/opt/gnome/include/epiphany-1.0 \
	$(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

plugin_LTLIBRARIES = libsample.la

libsample_la_SOURCES = \
	sample.c

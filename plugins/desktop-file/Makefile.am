plugindir = $(libdir)/epiphany/$(EPIPHANY_MAJOR)/plugins
plugin_LTLIBRARIES = libdesktopfileplugin.la

libdesktopfileplugin_la_SOURCES = \
	plugin.cpp

gecko_include_subdirs = \
	.	\
	dom	\
	plugin

libdesktopfileplugin_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"ephy-desktop-file-plugin\" \
	-I$(top_srcdir)/lib \
	$(addprefix -I$(GECKO_INCLUDE_ROOT)/,$(gecko_include_subdirs)) \
	$(AM_CPPFLAGS)

libdesktopfileplugin_la_CXXFLAGS = \
	$(DESKTOP_FILE_PLUGIN_DEP_CFLAGS)	\
	$(AM_CXXFLAGS)

libdesktopfileplugin_la_LIBADD = \
	$(DESKTOP_FILE_PLUGIN_DEP_LIBS)

libdesktopfileplugin_la_LDFLAGS = \
	-module -avoid-version \
	-export-symbols $(srcdir)/plugin.symbols \
	$(AM_LDFLAGS)

if !HAVE_GECKO_XPCOM_GLUE
	libdesktopfileplugin_la_LDFLAGS += \
		-R$(GECKO_HOME) \
		$(NULL)
endif

EXTRA_DIST = \
	plugin.symbols

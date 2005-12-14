plugindir = $(libdir)/epiphany/$(EPIPHANY_MAJOR)/plugins
plugin_LTLIBRARIES = libdesktopfileplugin.la

libdesktopfileplugin_la_SOURCES = \
	plugin.cpp

libdesktopfileplugin_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"ephy-desktop-file-plugin\" \
	-I$(top_srcdir)/lib \
	-I$(MOZILLA_INCLUDE_ROOT) \
	$(AM_CPPFLAGS)

libdesktopfileplugin_la_CXXFLAGS = \
	$(DESKTOP_FILE_PLUGIN_DEP_CFLAGS)	\
	$(AM_CXXFLAGS)

libdesktopfileplugin_la_LIBADD = \
	$(DESKTOP_FILE_PLUGIN_DEP_LIBS)

libdesktopfileplugin_la_LDFLAGS = \
	-module -avoid-version \
	-export-symbols $(srcdir)/plugin.symbols \
	-R$(MOZILLA_HOME) \
	$(AM_LDFLAGS)

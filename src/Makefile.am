SUBDIRS = bookmarks

INCLUDES = \
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/widgets   	\
	-I$(top_srcdir)/src/bookmarks   \
	$(WARN_CFLAGS)			\
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	-DSHARE_DIR=\"$(pkgdatadir)\" 	\
	-DPLUGINS_DIR=\""$(libdir)/epiphany/plugins"\" 	\
	-DDATADIR=\""$(datadir)"\" 	\
	-DPIXMAP_DIR=\""$(datadir)/pixmaps"\" 		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

bin_PROGRAMS = epiphany-bin
bin_SCRIPTS = epiphany

CXXLD = $(CXX)
LINK = $(LIBTOOL) --mode=link $(CXXLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

CORBA_SOURCE = \
	EphyAutomation-common.c	  \
	EphyAutomation-stubs.c	  \
	EphyAutomation-skels.c	  \
	EphyAutomation.h

idls = $(top_srcdir)/idl/EphyAutomation.idl

idl_flags = -I$(LIBBONOBO_IDL) -I$(BONOBO_ACTIVATION_IDL) 

$(CORBA_SOURCE): $(idls) $(ORBIT_IDL)
	$(ORBIT_IDL) $(idl_flags) $(idls)

if ENABLE_NAUTILUS_VIEW
nautilus_view_sources = \
	ephy-nautilus-view.c \
	ephy-nautilus-view.h
else
nautilus_view_sources =
endif

headerdir = $(prefix)/include/epiphany-@EPIPHANY_MAJOR@/epiphany
header_DATA = $(INST_H_FILES)

NOINST_H_FILES =					\
	ephy-automation.h				\
	ephy-encoding-menu.h				\
	ephy-favicon-action.h				\
	ephy-favorites-menu.h				\
	ephy-go-action.h				\
	ephy-history-window.h				\
	ephy-location-action.h				\
	ephy-navigation-action.h			\
	ephy-plugin.h					\
	ephy-spinner-action.h				\
	ephy-tabs-menu.h				\
	ephy-toolbars-model.h				\
	language-editor.h				\
	pdm-dialog.h					\
	popup-commands.h				\
	prefs-dialog.h					\
	ppview-toolbar.h				\
	statusbar.h					\
	toolbar.h					\
	window-commands.h

INST_H_FILES = 				    		\
	ephy-notebook.h					\
	ephy-shell.h					\
	ephy-tab.h					\
	ephy-window.h					\
	session.h

epiphany_bin_SOURCES = 				    	\
	$(CORBA_SOURCE)				 	\
	$(nautilus_view_sources)			\
	ephy-automation.c				\
	ephy-encoding-menu.c				\
	ephy-favicon-action.c				\
	ephy-favorites-menu.c				\
	ephy-go-action.c				\
	ephy-history-window.c				\
	ephy-location-action.c				\
	ephy-main.c					\
	ephy-navigation-action.c			\
	ephy-notebook.c					\
	ephy-plugin.c					\
	ephy-shell.c					\
	ephy-spinner-action.c				\
	ephy-tab.c					\
	ephy-tab.h					\
	ephy-tabs-menu.c				\
	ephy-toolbars-model.c				\
	ephy-toolbars-model.h				\
	ephy-window.c					\
	language-editor.c				\
	pdm-dialog.c					\
	popup-commands.c				\
	prefs-dialog.c					\
	ppview-toolbar.c				\
	session.c					\
	statusbar.c					\
	toolbar.c					\
	window-commands.c				\
	$(INST_H_FILES)					\
	$(NOINST_H_FILES)

epiphany_bin_LDADD = \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/libephy.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(MOZILLA_COMPONENT_LIBS)	\
	$(EPIPHANY_DEPENDENCY_LIBS) \
	$(INTLLIBS)

BUILT_SOURCES = $(CORBA_SOURCE)

CLEANFILES = $(CORBA_SOURCE)

EXTRA_DIST = $(top_srcdir)/idl/EphyAutomation.idl

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

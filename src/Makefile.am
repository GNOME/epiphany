SUBDIRS = bookmarks

noinst_LTLIBRARIES = libephymain.la

bin_PROGRAMS = epiphany

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

headerdir = $(prefix)/include/epiphany-@EPIPHANY_MAJOR@/epiphany
header_DATA = $(INST_H_FILES)

NOINST_H_FILES =					\
	ephy-automation.h				\
	ephy-encoding-dialog.h				\
	ephy-encoding-menu.h				\
	ephy-favicon-action.h				\
	ephy-go-action.h				\
	ephy-history-window.h				\
	ephy-location-action.h				\
	ephy-navigation-action.h			\
	ephy-tabs-menu.h				\
	ephy-toolbars-model.h				\
	pdm-dialog.h					\
	popup-commands.h				\
	prefs-dialog.h					\
	ppview-toolbar.h				\
	toolbar.h					\
	window-commands.h

INST_H_FILES = 				    		\
	ephy-extension.h				\
	ephy-extensions-manager.h			\
	ephy-notebook.h					\
	ephy-session.h					\
	ephy-shell.h					\
	ephy-statusbar.h				\
	ephy-tab.h					\
	ephy-window.h

libephymain_la_SOURCES =				\
	$(CORBA_SOURCE)				 	\
	ephy-automation.c				\
	ephy-completion-model.c				\
	ephy-completion-model.h				\
	ephy-encoding-dialog.c				\
	ephy-encoding-menu.c				\
	ephy-extension.c				\
	ephy-extensions-manager.c			\
	ephy-favicon-action.c				\
	ephy-go-action.c				\
	ephy-history-window.c				\
	ephy-location-action.c				\
	ephy-navigation-action.c			\
	ephy-notebook.c					\
	ephy-session.c					\
	ephy-shell.c					\
	ephy-statusbar.c				\
	ephy-tab.c					\
	ephy-tab.h					\
	ephy-tabs-menu.c				\
	ephy-toolbars-model.c				\
	ephy-window.c					\
	pdm-dialog.c					\
	popup-commands.c				\
	prefs-dialog.c					\
	ppview-toolbar.c				\
	toolbar.c					\
	window-commands.c				\
	$(INST_H_FILES)					\
	$(NOINST_H_FILES)

libephymain_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/widgets   	\
	-I$(top_srcdir)/src/bookmarks   \
	-DSHARE_DIR=\"$(pkgdatadir)\" 	\
	-DEXTENSIONS_DIR=\""$(libdir)/epiphany-$(EPIPHANY_MAJOR)/extensions"\" 	\
	-DDATADIR=\""$(datadir)"\" 	\
	-DPIXMAP_DIR=\""$(datadir)/pixmaps"\"	\
	$(AM_CPPFLAGS)

libephymain_la_CFLAGS = \
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	$(AM_CFLAGS)

epiphany_SOURCES = ephy-main.c

epiphany_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/widgets   	\
	-I$(top_srcdir)/src/bookmarks   \
	-DSHARE_DIR=\"$(pkgdatadir)\" 	\
	-DEXTENSIONS_DIR=\""$(libdir)/epiphany-$(EPIPHANY_MAJOR)/extensions"\" 	\
	-DDATADIR=\""$(datadir)"\" 	\
	-DPIXMAP_DIR=\""$(datadir)/pixmaps"\"	\
	$(AM_CPPFLAGS)

epiphany_CFLAGS = \
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	$(AM_CFLAGS)

epiphany_LDFLAGS = -R$(MOZILLA_HOME) -dlopen self

epiphany_LDADD = \
	$(top_builddir)/src/libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembedfactory.la \
	$(top_builddir)/embed/mozilla/libephymozillaembed.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(MOZILLA_COMPONENT_LIBS)	\
	$(EPIPHANY_DEPENDENCY_LIBS) \
	$(INTLLIBS)

BUILT_SOURCES = $(CORBA_SOURCE)

CLEANFILES = $(CORBA_SOURCE)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

SUBDIRS = bookmarks

INCLUDES = \
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/widgets   	\
	-I$(top_srcdir)/lib/toolbar     \
	-I$(top_srcdir)/src/bookmarks   \
	$(WARN_CFLAGS)			\
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	-DSHARE_DIR=\"$(pkgdatadir)\" 	\
	-DDATADIR=\""$(datadir)"\" 	\
	-DPIXMAP_DIR=\""$(datadir)/pixmaps"\" 		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

bin_PROGRAMS = epiphany

CXXLD = $(CXX)
LINK = $(LIBTOOL) --mode=link $(CXXLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@


ephy_automation_interface_idl_sources = \
	EphyAutomation-common.c	  \
	EphyAutomation-stubs.c	  \
	EphyAutomation-skels.c	  \
	EphyAutomation.h		

if ENABLE_NAUTILUS_VIEW
nautilus_view_sources = \
	ephy-nautilus-view.c \
	ephy-nautilus-view.h
else
nautilus_view_sources =
endif

epiphany_SOURCES = 				    	\
	$(ephy_automation_interface_idl_sources) 	\
	$(nautilus_view_sources)			\
	appearance-prefs.c				\
	appearance-prefs.h				\
	ephy-automation.c				\
	ephy-automation.h				\
	ephy-favicon-action.c				\
	ephy-favicon-action.h				\
	ephy-favorites-menu.c				\
	ephy-favorites-menu.h				\
	ephy-history-model.c				\
	ephy-history-model.h				\
	ephy-location-action.c				\
	ephy-location-action.h				\
	ephy-main.c					\
	ephy-navigation-action.c			\
	ephy-shell.c					\
	ephy-shell.h					\
	ephy-spinner-action.c				\
	ephy-spinner-action.h				\
	ephy-tab.c					\
	ephy-tab.h					\
	ephy-window.c					\
	ephy-window.h					\
	general-prefs.c					\
	general-prefs.h					\
	history-dialog.c				\
	history-dialog.h				\
	language-editor.c				\
	language-editor.h				\
	pdm-dialog.c					\
	pdm-dialog.h					\
	popup-commands.c				\
	popup-commands.h				\
	prefs-dialog.c					\
	prefs-dialog.h					\
	ppview-toolbar.c				\
	ppview-toolbar.h				\
	session.c					\
	session.h					\
	statusbar.c					\
	statusbar.h					\
	toolbar.c					\
	toolbar.h					\
	ui-prefs.c					\
	ui-prefs.h					\
	window-commands.c				\
	window-commands.h

epiphany_LDADD = \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/libephy.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(MOZILLA_COMPONENT_LIBS)	\
	$(EPIPHANY_DEPENDENCY_LIBS) \
	$(INTLLIBS)


BUILT_SOURCES=	\
	EphyAutomation.h EphyAutomation-common.c EphyAutomation-stubs.c EphyAutomation-skels.c

CLEAN_FILES = $(BUILT_SOURCES)

EphyAutomation-common.c EphyAutomation-stubs.c EphyAutomation-skels.c EphyAutomation.h: $(top_srcdir)/idl/EphyAutomation.idl
	$(ORBIT_IDL) -I $(LIBBONOBO_IDL) -I $(BONOBO_ACTIVATION_IDL) $(top_srcdir)/idl/EphyAutomation.idl

EXTRA_DIST = $(top_srcdir)/idl/EphyAutomation.idl

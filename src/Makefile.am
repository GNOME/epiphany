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
	-DPLUGINS_DIR=\""$(libdir)/epiphany/plugins"\" 	\
	-DDATADIR=\""$(datadir)"\" 	\
	-DPIXMAP_DIR=\""$(datadir)/pixmaps"\" 		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

bin_PROGRAMS = epiphany-bin
bin_SCRIPTS = epiphany

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

epiphany_bin_SOURCES = 				    	\
	$(ephy_automation_interface_idl_sources) 	\
	$(nautilus_view_sources)			\
	ephy-automation.c				\
	ephy-automation.h				\
	ephy-encoding-menu.c				\
	ephy-encoding-menu.h				\
	ephy-favicon-action.c				\
	ephy-favicon-action.h				\
	ephy-favorites-menu.c				\
	ephy-favorites-menu.h				\
	ephy-go-action.c				\
	ephy-go-action.h				\
	ephy-history-window.c				\
	ephy-history-window.h				\
	ephy-location-action.c				\
	ephy-location-action.h				\
	ephy-main.c					\
	ephy-navigation-action.c			\
	ephy-navigation-action.h			\
	ephy-notebook.c					\
	ephy-notebook.h					\
	ephy-plugin.c					\
	ephy-plugin.h					\
	ephy-shell.c					\
	ephy-shell.h					\
	ephy-spinner-action.c				\
	ephy-spinner-action.h				\
	ephy-tab.c					\
	ephy-tab.h					\
	ephy-tabs-menu.c				\
	ephy-tabs-menu.h				\
	ephy-toolbars-model.c				\
	ephy-toolbars-model.h				\
	ephy-window.c					\
	ephy-window.h					\
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
	window-commands.c				\
	window-commands.h

epiphany_bin_LDADD = \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/libephy.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(MOZILLA_COMPONENT_LIBS)	\
	$(EPIPHANY_DEPENDENCY_LIBS) \
	$(INTLLIBS)

CLEANFILES = \
	$(ephy_automation_interface_idl_sources) \
	ephy_automation_interface_idl_stamp

$(ephy_automation_interface_idl_sources): ephy_automation_interface_idl_stamp

ephy_automation_interface_idl_stamp: $(top_srcdir)/idl/EphyAutomation.idl $(ORBIT_IDL)
	$(ORBIT_IDL) -I $(LIBBONOBO_IDL) -I $(BONOBO_ACTIVATION_IDL) -I$(top_srcdir) $<
	touch $@

EXTRA_DIST = $(top_srcdir)/idl/EphyAutomation.idl

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
header_DATA = \
	ephy-type-builtins.h	\
	$(INST_H_FILES)

NOINST_H_FILES = \
	ephy-automation.h		\
	ephy-encoding-dialog.h		\
	ephy-encoding-menu.h		\
	ephy-go-action.h		\
	ephy-history-window.h		\
	ephy-home-action.h		\
	ephy-link.h			\
	ephy-link-action.h		\
	ephy-location-action.h		\
	ephy-navigation-action.h	\
	ephy-tabs-menu.h		\
	ephy-toolbars-model.h		\
	ephy-toolbar-editor.h		\
	pdm-dialog.h			\
	popup-commands.h		\
	prefs-dialog.h			\
	ppview-toolbar.h		\
	toolbar.h			\
	window-commands.h

INST_H_FILES = \
	ephy-extension.h		\
	ephy-extensions-manager.h	\
	ephy-notebook.h			\
	ephy-session.h			\
	ephy-shell.h			\
	ephy-statusbar.h		\
	ephy-tab.h			\
	ephy-window.h

libephymain_la_SOURCES = \
	$(BUILT_SOURCES)		\
	ephy-automation.c		\
	ephy-completion-model.c		\
	ephy-completion-model.h		\
	ephy-encoding-dialog.c		\
	ephy-encoding-menu.c		\
	ephy-extension.c		\
	ephy-extensions-manager.c	\
	ephy-go-action.c		\
	ephy-home-action.c		\
	ephy-history-window.c		\
	ephy-link.c			\
	ephy-link-action.c		\
	ephy-location-action.c		\
	ephy-navigation-action.c	\
	ephy-notebook.c			\
	ephy-session.c			\
	ephy-shell.c			\
	ephy-statusbar.c		\
	ephy-tab.c			\
	ephy-tab.h			\
	ephy-tabs-menu.c		\
	ephy-toolbars-model.c		\
	ephy-toolbar-editor.c		\
	ephy-window.c			\
	pdm-dialog.c			\
	popup-commands.c		\
	prefs-dialog.c			\
	ppview-toolbar.c		\
	toolbar.c			\
	window-commands.c		\
	$(INST_H_FILES)			\
	$(NOINST_H_FILES)

libephymain_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/egg	\
	-I$(top_builddir)/embed		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/widgets   	\
	-I$(top_srcdir)/src/bookmarks   \
	-DEXTENSIONS_DIR=\""$(libdir)/epiphany-$(EPIPHANY_MAJOR)/extensions"\" 	\
	-DLOADER_DIR=\""$(libdir)/epiphany-$(EPIPHANY_MAJOR)/loaders"\"		\
	-DDATADIR=\""$(datadir)"\" 	\
	$(AM_CPPFLAGS)

libephymain_la_CFLAGS = \
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	$(AM_CFLAGS)

if ENABLE_DBUS
libephymain_la_CFLAGS += $(DBUS_CFLAGS)
endif

epiphany_SOURCES = ephy-main.c

epiphany_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/src/bookmarks   \
	-DDATADIR=\""$(datadir)"\" 	\
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

if ENABLE_DBUS
epiphany_LDADD += $(DBUS_LIBS)
endif

BUILT_SOURCES =	\
	$(CORBA_SOURCE)	\
	$(TYPES_SOURCE)

TYPES_SOURCE = \
	ephy-type-builtins.c	\
	ephy-type-builtins.h

stamp_files = \
	stamp-ephy-type-builtins.c	\
	stamp-ephy-type-builtins.h

ephy-type-builtins.c: stamp-ephy-type-builtins.c
	@true
stamp-ephy-type-builtins.c: $(INST_H_FILES) $(NOINST_H_FILES)
	( cd $(srcdir) && $(GLIB_MKENUMS) \
		--fhead "#include \"ephy-type-builtins.h\"\n\n" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--fprod "\n#include \"@filename@\"" \
		--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
		--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "  { 0, NULL, NULL }\n};\n\n" \
		--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
		--vtail "  static GType type = 0;\n\n" \
		--vtail "  if (G_UNLIKELY (type == 0))\n" \
		--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
		--vtail "  return type;\n}\n\n" \
		$(^F) ) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

ephy-type-builtins.h: stamp-ephy-type-builtins.h
	@true
stamp-ephy-type-builtins.h: $(INST_H_FILES) $(NOINST_H_FILES)
	( cd $(srcdir) && $(GLIB_MKENUMS) \
		--fhead "#ifndef EPHY_TYPE_BUILTINS_H\n" \
		--fhead "#define EPHY_TYPE_BUILTINS_H 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* EPHY_TYPE_BUILTINS_H */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define EPHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(^F) ) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = $(stamp_files) $(BUILT_SOURCES)

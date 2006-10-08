SUBDIRS = bookmarks

noinst_LTLIBRARIES = libephymain.la

if ENABLE_PYTHON
noinst_LTLIBRARIES += libpyphany.la
endif

bin_PROGRAMS = epiphany

EXTRA_DIST =

headerdir = $(prefix)/include/epiphany/@EPIPHANY_MAJOR@/epiphany
header_DATA = \
	ephy-type-builtins.h	\
	$(INST_H_FILES)

NOINST_H_FILES = \
	ephy-action-helper.h		\
	ephy-activation.h		\
	ephy-encoding-dialog.h		\
	ephy-encoding-menu.h		\
	ephy-find-toolbar.h		\
	ephy-fullscreen-popup.h		\
	ephy-go-action.h		\
	ephy-history-window.h		\
	ephy-home-action.h		\
	ephy-link-action.h		\
	ephy-lockdown.h			\
	ephy-location-action.h		\
	ephy-navigation-action.h	\
	ephy-tabs-menu.h		\
	ephy-toolbars-model.h		\
	ephy-toolbar.h			\
	ephy-toolbar-editor.h		\
	languages.h			\
	pdm-dialog.h			\
	popup-commands.h		\
	prefs-dialog.h			\
	ppview-toolbar.h		\
	window-commands.h

INST_H_FILES = \
	ephy-dbus.h			\
	ephy-extension.h		\
	ephy-extensions-manager.h	\
	ephy-link.h			\
	ephy-notebook.h			\
	ephy-session.h			\
	ephy-shell.h			\
	ephy-statusbar.h		\
	ephy-tab.h			\
	ephy-window.h

libephymain_la_SOURCES = \
	ephy-activation.c		\
	ephy-action-helper.c		\
	ephy-completion-model.c		\
	ephy-completion-model.h		\
	ephy-dbus.c			\
	ephy-dbus.h			\
	ephy-encoding-dialog.c		\
	ephy-encoding-menu.c		\
	ephy-extension.c		\
	ephy-extensions-manager.c	\
	ephy-find-toolbar.c		\
	ephy-fullscreen-popup.c		\
	ephy-go-action.c		\
	ephy-home-action.c		\
	ephy-history-window.c		\
	ephy-link.c			\
	ephy-link-action.c		\
	ephy-location-action.c		\
	ephy-lockdown.c			\
	ephy-navigation-action.c	\
	ephy-notebook.c			\
	ephy-session.c			\
	ephy-shell.c			\
	ephy-statusbar.c		\
	ephy-tab.c			\
	ephy-tab.h			\
	ephy-tabs-menu.c		\
	ephy-toolbars-model.c		\
	ephy-toolbar.c			\
	ephy-toolbar-editor.c		\
	ephy-window.c			\
	pdm-dialog.c			\
	popup-commands.c		\
	prefs-dialog.c			\
	ppview-toolbar.c		\
	window-commands.c		\
	$(INST_H_FILES)			\
	$(NOINST_H_FILES)

nodist_libephymain_la_SOURCES = \
	$(TYPES_SOURCE)

if ENABLE_CERTIFICATE_MANAGER
NOINST_H_FILES += \
	ephy-cert-manager-dialog.h

libephymain_la_SOURCES += \
	ephy-cert-manager-dialog.c
endif

libephymain_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/egg	\
	-I$(top_builddir)/embed		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/widgets   	\
	-I$(top_srcdir)/src/bookmarks   \
	-DEXTENSIONS_DIR=\""$(libdir)/epiphany/$(EPIPHANY_MAJOR)/extensions"\" 	\
	-DLOADER_DIR=\""$(libdir)/epiphany/$(EPIPHANY_MAJOR)/loaders"\"		\
	-DDATADIR=\""$(datadir)"\" 	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(AM_CPPFLAGS)

libephymain_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	\
	$(DBUS_CFLAGS)		\
	$(AM_CFLAGS)

if ENABLE_NETWORK_MANAGER
libephymain_la_SOURCES += \
	ephy-net-monitor.c	\
	ephy-net-monitor.h
endif

if ENABLE_PYTHON
NOINST_H_FILES += \
	ephy-python.h		\
	ephy-python-extension.h	\
	ephy-python-loader.h

BUILT_PYPHANY_SOURCE = epiphany.c

libpyphany_la_SOURCES = \
	ephy-python.c		\
	ephy-python.h		\
	ephy-python-extension.c	\
	ephy-python-extension.h	\
	ephy-python-loader.c	\
	ephy-python-loader.h

nodist_libpyphany_la_SOURCES = \
	$(BUILT_PYPHANY_SOURCE)

libpyphany_la_CPPFLAGS = \
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/widgets	\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/embed		\
	-I$(top_srcdir)/embed/mozilla	\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/bookmarks	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/widgets	\
	-I$(top_builddir)/lib/egg	\
	-I$(top_builddir)/embed		\
	-I$(top_builddir)/embed/mozilla	\
	-I$(top_builddir)/src		\
	-I$(top_builddir)/src/bookmarks	\
	$(PYTHON_INCLUDES)		\
	-DEXTENSIONS_DIR=\""$(libdir)/epiphany/$(EPIPHANY_MAJOR)/extensions"\" 	\
	-DLOADER_DIR=\""$(libdir)/epiphany/$(EPIPHANY_MAJOR)/loaders"\"		\
	-DDATADIR=\""$(datadir)"\" 	\
	$(AM_CPPFLAGS)

libpyphany_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)		\
	$(NO_STRICT_ALIASING_CFLAGS)	\
	$(DBUS_CFLAGS)			\
	$(PYGTK_CFLAGS)			\
	$(AM_CFLAGS)

libpyphany_la_LDFLAGS = \
	-export-symbols-regex ephy_python_init

libpyphany_la_LIBADD = \
	$(DEPENDENCIES_LIBS)			\
	$(PYTHON_LIB_LOC) $(PYTHON_LIBS)	\
	$(PYTHON_EXTRA_LIBS)			\
	$(PYGTK_LIBS)
endif

epiphany_SOURCES = ephy-main.c

epiphany_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/src/bookmarks   \
	-DDATADIR=\""$(datadir)"\" 	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(INCINTL)			\
	$(AM_CPPFLAGS)

epiphany_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	\
	$(DBUS_CFLAGS)		\
	$(AM_CFLAGS)

epiphany_LDFLAGS = -R$(MOZILLA_HOME) -dlopen self

epiphany_LDADD = \
	libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembedfactory.la \
	$(top_builddir)/embed/mozilla/libephymozillaembed.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(GECKO_LIBS) \
	$(MOZILLA_EXTRA_LIBS) \
	$(DEPENDENCIES_LIBS) \
	$(DBUS_LIBS) \
	$(LIBINTL)

if ENABLE_PYTHON
epiphany_LDADD += \
	libpyphany.la				\
	$(PYTHON_LIB_LOC) $(PYTHON_LIBS)	\
	$(PYTHON_EXTRA_LIBS)			\
	$(PYGTK_LIBS)

pydefsdir = $(datadir)/pygtk/2.0/defs
pydefs_DATA = epiphany.defs

EXTRA_DIST += \
	epiphany.override	\
	$(pydefs_DATA)
endif

if ENABLE_NETWORK_MANAGER
epiphany_LDADD += \
	$(NETWORK_MANAGER_LIBS)
endif

BUILT_SOURCES =	\
	$(TYPES_SOURCE)			\
	ephy-dbus-client-bindings.h	\
	ephy-dbus-server-bindings.h


TYPES_SOURCE = \
	ephy-type-builtins.c	\
	ephy-type-builtins.h

if ENABLE_PYTHON
BUILT_SOURCES += $(BUILT_PYPHANY_SOURCE)
endif

stamp_files = \
	stamp-ephy-type-builtins.c	\
	stamp-ephy-type-builtins.h	\
	stamp-ephy-dbus-server-bindings.h	\
	stamp-ephy-dbus-client-bindings.h

ephy-type-builtins.c: stamp-ephy-type-builtins.c
	@true
stamp-ephy-type-builtins.c: $(INST_H_FILES) $(NOINST_H_FILES)
	( cd $(srcdir) && $(GLIB_MKENUMS) \
		--fhead "#include \"ephy-type-builtins.h\"\n\n" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--fprod "\n#include \"@filename@\"" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n" \
		--vhead "  static GType type = 0;\n\n" \
		--vhead "  if (G_UNLIKELY (type == 0))\n  {\n" \
		--vhead "    const G@Type@Value _@enum_name@_values[] = {" \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n\n" \
		--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n  }\n\n" \
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

ephy-dbus-server-bindings.h: stamp-ephy-dbus-server-bindings.h
	@true
stamp-ephy-dbus-server-bindings.h: $(top_srcdir)/data/epiphany-service.xml Makefile
	$(DBUS_BINDING_TOOL) --prefix=ephy_activation --mode=glib-server $< > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

ephy-dbus-client-bindings.h: stamp-ephy-dbus-client-bindings.h
	@true
stamp-ephy-dbus-client-bindings.h: $(top_srcdir)/data/epiphany-service.xml Makefile
	$(DBUS_BINDING_TOOL) --prefix=ephy_activation --mode=glib-client $< > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

epiphany.c: epiphany.defs epiphany.override
	( cd $(srcdir) && $(PYGTK_CODEGEN) \
		--register $(PYGTK_DEFSDIR)/pango-types.defs \
		--register $(PYGTK_DEFSDIR)/gdk-types.defs \
		--register $(PYGTK_DEFSDIR)/gtk-types.defs \
		--override $*.override \
		--prefix py$* $(<F) ) > $@ 

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)
DISTCLEANFILES = $(stamp_files) $(BUILT_SOURCES)
MAINTAINERCLEANFILES = $(stamp_files) $(BUILT_SOURCES)

# update rules for python bindings
if ENABLE_PYTHON

BINDING_HEADERS_SRCDIR_IN = \
	embed/ephy-command-manager.h			\
	embed/ephy-cookie-manager.h			\
	embed/ephy-embed-event.h			\
	embed/ephy-embed-factory.h			\
	embed/ephy-embed.h				\
	embed/ephy-embed-persist.h			\
	embed/ephy-embed-shell.h			\
	embed/ephy-embed-single.h			\
	embed/ephy-favicon-cache.h			\
	embed/ephy-history.h				\
	embed/ephy-password-manager.h			\
	embed/ephy-permission-manager.h			\
	lib/ephy-dialog.h				\
	lib/ephy-node-db.h				\
	lib/ephy-node.h					\
	lib/ephy-state.h				\
	lib/egg/egg-editable-toolbar.h			\
	lib/egg/egg-toolbars-model.h			\
	lib/widgets/ephy-search-entry.h			\
	lib/widgets/ephy-spinner.h			\
	src/bookmarks/ephy-bookmarks.h			\
	src/ephy-extension.h				\
	src/ephy-extensions-manager.h			\
	src/ephy-find-toolbar.h				\
	src/ephy-link-action.h				\
	src/ephy-link.h					\
	src/ephy-location-action.h			\
	src/ephy-notebook.h				\
	src/ephy-session.h				\
	src/ephy-shell.h				\
	src/ephy-statusbar.h				\
	src/ephy-tab.h					\
	src/ephy-toolbar.h				\
	src/ephy-window.h

BINDING_HEADERS_SRCDIR_IGNORE_IN = \
	embed/downloader-view.h				\
	embed/ephy-download.h				\
	embed/ephy-embed-dialog.h			\
	embed/ephy-embed-find.h				\
	embed/ephy-embed-prefs.h			\
	embed/ephy-encodings.h				\
	embed/print-dialog.h				\
	lib/eel-gconf-extensions.h			\
	lib/ephy-debug.h				\
	lib/ephy-dnd.h					\
	lib/ephy-file-chooser.h				\
	lib/ephy-file-helpers.h				\
	lib/ephy-glade.h				\
	lib/ephy-gui.h					\
	lib/widgets/ephy-icon-entry.h			\
	lib/ephy-langs.h				\
	lib/ephy-loader.h				\
	lib/ephy-module.h				\
	lib/ephy-node-common.h				\
	lib/ephy-node-filter.h				\
	lib/ephy-object-helpers.h			\
	lib/ephy-prefs.h				\
	lib/ephy-shlib-loader.h				\
	lib/ephy-signal-accumulator.h			\
	lib/ephy-stock-icons.h				\
	lib/ephy-string.h				\
	lib/ephy-zoom.h					\
	lib/egg/eggintl.h				\
	lib/egg/eggmarshalers.h				\
	lib/egg/eggstatusicon.h				\
	lib/egg/eggtrayicon.h				\
	lib/egg/eggtraymanager.h			\
	lib/egg/eggtreemultidnd.h			\
	lib/egg/egg-toolbar-editor.h			\
	lib/widgets/ephy-location-entry.h		\
	lib/widgets/ephy-node-view.h			\
	lib/widgets/ephy-tree-model-node.h		\
	lib/widgets/ephy-tree-model-sort.h		\
	lib/widgets/ephy-zoom-action.h			\
	lib/widgets/ephy-zoom-control.h			\
	src/bookmarks/ephy-bookmark-action.h		\
	src/bookmarks/ephy-bookmark-properties.h	\
	src/bookmarks/ephy-bookmarksbar-model.h		\
	src/bookmarks/ephy-bookmarks-editor.h		\
	src/bookmarks/ephy-bookmarks-export.h		\
	src/bookmarks/ephy-bookmarks-import.h		\
	src/bookmarks/ephy-bookmarks-menu.h		\
	src/bookmarks/ephy-favorites-menu.h		\
	src/bookmarks/ephy-topic-action.h		\
	src/bookmarks/ephy-topics-palette.h		\
	src/bookmarks/ephy-bookmark-action-group.h	\
	src/bookmarks/ephy-bookmark-factory-action.h	\
	src/bookmarks/ephy-bookmarks-ui.h		\
	src/bookmarks/ephy-nodes-cover.h		\
	src/bookmarks/ephy-open-tabs-action.h		\
	src/bookmarks/ephy-related-action.h		\
	src/bookmarks/ephy-topic-action-group.h		\
	src/bookmarks/ephy-topic-factory-action.h	\
	src/ephy-action-helper.h			\
	src/ephy-activation.h				\
	src/ephy-completion-model.h			\
	src/ephy-dbus.h					\
	src/ephy-encoding-dialog.h			\
	src/ephy-encoding-menu.h			\
	src/ephy-fullscreen-popup.h			\
	src/ephy-go-action.h				\
	src/ephy-history-window.h			\
	src/ephy-home-action.h				\
	src/ephy-lockdown.h				\
	src/ephy-navigation-action.h			\
	src/ephy-python-extension.h			\
	src/ephy-python.h				\
	src/ephy-python-loader.h			\
	src/ephy-tabs-menu.h				\
	src/ephy-toolbar-editor.h			\
	src/ephy-toolbars-model.h			\
	src/languages.h					\
	src/pdm-dialog.h				\
	src/popup-commands.h				\
	src/ppview-toolbar.h				\
	src/prefs-dialog.h				\
	src/window-commands.h

BINDING_HEADERS_BUILDDIR_IN = \
	embed/ephy-embed-type-builtins.h		\
	lib/egg/eggtypebuiltins.h			\
	lib/ephy-lib-type-builtins.h			\
	src/bookmarks/ephy-bookmarks-type-builtins.h	\
	src/ephy-type-builtins.h

BINDING_HEADERS_BUILDDIR_IGNORE_IN = \
        config.h					\
        embed/stamp-ephy-embed-type-builtins.h		\
	lib/ephy-marshal.h				\
        lib/egg/eggmarshalers.h				\
        lib/egg/eggtypebuiltins.h			\
	lib/egg/stamp-eggmarshalers.h			\
	lib/egg/stamp-eggtypebuiltins.h			\
        lib/ephy-marshal.h				\
        lib/stamp-ephy-lib-type-builtins.h		\
        lib/stamp-ephy-marshal.h			\
        src/EphyAutomation.h				\
        src/stamp-ephy-type-builtins.h			\
	src/bookmarks/stamp-ephy-bookmarks-type-builtins.h

BINDING_HEADERS_SRCDIR		:= $(addprefix $(top_srcdir)/,$(BINDING_HEADERS_SRCDIR_IN))
BINDING_HEADERS_SRCDIR_IGNORE	:= $(addprefix $(top_srcdir)/,$(BINDING_HEADERS_SRCDIR_IGNORE_IN))
BINDING_HEADERS_BUILDDIR	:= $(addprefix $(top_builddir)/,$(BINDING_HEADERS_BUILDDIR_IN))
BINDING_HEADERS_BUILDDIR_IGNORE	:= $(addprefix $(top_builddir)/,$(BINDING_HEADERS_BUILDDIR_IGNORE_IN))

BINDING_HEADERS_ALL = $(shell find $(top_srcdir) $(top_builddir) -type f -name "*.h"  \
	               -a ! -wholename "*/mozilla/*" -print| sort | uniq)

BINDING_HEADERS_ALL_KNOWN = \
	$(BINDING_HEADERS_SRCDIR) 		\
	$(BINDING_HEADERS_SRCDIR_IGNORE)	\
	$(BINDING_HEADERS_BUILDDIR)		\
	$(BINDING_HEADERS_BUILDDIR_IGNORE)

BINDING_HEADERS_ALL_UNKNOWN = $(filter-out $(BINDING_HEADERS_ALL_KNOWN),$(BINDING_HEADERS_ALL))

regenerate-python-binding:
	$(PYGTK_H2DEF) $(sort $(BINDING_HEADERS_SRCDIR) $(BINDING_HEADERS_BUILDDIR)) > epiphany.defs.new

check-python-binding:
	@echo "$(BINDING_HEADERS_ALL_UNKNOWN)"
	@test -z "$(BINDING_HEADERS_ALL_UNKNOWN)"

#check-local: check-python-binding

endif

dist-check-python-binding:
if !ENABLE_PYTHON
	@echo "Python bindings must be enabled in order to make dist!"
	@false
endif

dist-hook: dist-check-python-binding

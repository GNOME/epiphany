SUBDIRS = bookmarks

NULL =

AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

noinst_LTLIBRARIES = libephymain.la

bin_PROGRAMS = epiphany

EXTRA_DIST = \
	org.gnome.ShellSearchProvider2.xml \
	epiphany.gresource.xml \
	$(RESOURCE_FILES)      \
	$(NULL)

TYPES_H_FILES = \
	ephy-link.h				\
	ephy-bookmark-properties-grid.h		\
	ephy-session.h				\
	ephy-shell.h				\
	ephy-window.h				\
	ephy-title-box.h			\
	$(NULL)

libephymain_la_SOURCES = \
	ephy-bookmark.c				\
	ephy-bookmark.h				\
	ephy-bookmark-properties-grid.c		\
	ephy-bookmark-properties-grid.c		\
	ephy-bookmark-row.c 			\
	ephy-bookmark-row.h 			\
	ephy-bookmarks-manager.c		\
	ephy-bookmarks-manager.h		\
	ephy-bookmarks-popover.c 		\
	ephy-bookmarks-popover.h 		\
	clear-data-dialog.c			\
	clear-data-dialog.h			\
	cookies-dialog.c			\
	cookies-dialog.h			\
	ephy-action-helper.c			\
	ephy-action-helper.h			\
	ephy-bookmark.c				\
	ephy-bookmark.h				\
	ephy-bookmark-row.c			\
	ephy-bookmark-row.h			\
	ephy-bookmarks-popover.c 		\
	ephy-bookmarks-popover.h 		\
	ephy-completion-model.c			\
	ephy-completion-model.h			\
	ephy-encoding-dialog.c			\
	ephy-encoding-dialog.h			\
	ephy-encoding-row.c			\
	ephy-encoding-row.h			\
	ephy-history-window.c			\
	ephy-history-window.h			\
	ephy-sync-window.c			\
	ephy-sync-window.h			\
	ephy-sync-service.c			\
	ephy-sync-service.h			\
	ephy-sync-crypto.c			\
	ephy-sync-crypto.h			\
	ephy-link.c				\
	ephy-link.h				\
	ephy-location-controller.c		\
	ephy-location-controller.h		\
	ephy-lockdown.c				\
	ephy-lockdown.h				\
	ephy-notebook.c				\
	ephy-notebook.h				\
	ephy-private.h				\
	ephy-search-provider.h			\
	ephy-session.c				\
	ephy-session.h				\
	ephy-shell.c				\
	ephy-shell.h				\
	ephy-title-box.c			\
	ephy-title-box.h			\
	ephy-toolbar.c				\
	ephy-toolbar.h				\
	ephy-window.c				\
	ephy-window.h				\
	languages.h				\
	passwords-dialog.c			\
	passwords-dialog.h			\
	popup-commands.c			\
	popup-commands.h			\
	prefs-dialog.c				\
	prefs-dialog.h				\
	window-commands.c			\
	window-commands.h			\
	$(NULL)

nodist_libephymain_la_SOURCES = \
	$(TYPES_SOURCE)

libephymain_la_LIBADD = \
	$(top_builddir)/gvdb/libgvdb.la

libephymain_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/embed		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/history	\
	-I$(top_srcdir)/lib/widgets   	\
	-I$(top_builddir)/lib/widgets	\
	-I$(top_srcdir)/src/bookmarks   \
	-I$(top_srcdir)/gvdb		\
	-DLOADER_DIR=\""$(pkglibdir)/$(EPIPHANY_MAJOR)/loaders"\"		\
	-DDATADIR=\""$(pkgdatadir)"\" 	\
	-DLOCALEDIR=\"$(localedir)\"	\
	$(AM_CPPFLAGS)

libephymain_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	  \
	$(WEBKIT_CFLAGS)	  \
	$(CODE_COVERAGE_CFLAGS)   \
	$(AM_CFLAGS)

RESOURCE_FILES = \
	resources/about.css			  					\
	resources/bookmark-properties.ui						\
	resources/channel-insecure-symbolic.png						\
	resources/computer-fail-symbolic.png						\
	resources/clear-data-dialog.ui							\
	resources/cookies-dialog.ui							\
	resources/encoding-dialog.ui							\
	resources/encoding-row.ui							\
	resources/epiphany-application-menu.ui						\
	resources/epiphany-bookmark-editor-ui.xml					\
	resources/epiphany.css								\
	resources/error.html								\
	resources/history-dialog.ui							\
	resources/incognito.png								\
	resources/missing-thumbnail.png							\
	resources/passwords-dialog.ui							\
	resources/prefs-dialog.ui							\
	resources/prefs-lang-dialog.ui							\
	resources/shortcuts-dialog.ui							\
	resources/icons/hicolor/16x16/actions/ephy-bookmark-empty.png			\
	resources/icons/hicolor/16x16/actions/ephy-bookmark-full.png			\
	resources/icons/hicolor/scalable/actions/ephy-bookmark-tag-symbolic.svg		\
	resources/icons/hicolor/scalable/actions/ephy-bookmarks-symbolic.svg		\
	resources/gtk/bookmark-properties-grid.ui					\
	resources/gtk/bookmark-row.ui							\
	resources/gtk/bookmarks-popover.ui						\
	resources/gtk/menus.ui								\
	$(NULL)

epiphany-resources.c: epiphany.gresource.xml $(RESOURCE_FILES)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(srcdir)/resources --generate-source --c-name epiphany $(srcdir)/epiphany.gresource.xml

epiphany-resources.h: epiphany.gresource.xml $(RESOURCE_FILES)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(srcdir)/resources --generate-header --c-name epiphany $(srcdir)/epiphany.gresource.xml

EPIPHANY_RESOURCES = \
	epiphany-resources.c \
	epiphany-resources.h \
	$(NULL)

BUILT_SOURCES = \
	$(dbus_shell_search_provider_built_sources) \
	$(EPIPHANY_RESOURCES) \
	$(TYPES_SOURCE)       \
	$(NULL)

epiphany_SOURCES = ephy-main.c \
	$(EPIPHANY_RESOURCES)  \
	$(NULL)

epiphany_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/history	\
	-I$(top_srcdir)/lib/widgets	\
	-I$(top_builddir)/lib/widgets	\
	-I$(top_srcdir)/src/bookmarks   \
	-DDATADIR=\""$(datadir)"\" 	\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DLOCALEDIR=\"$(localedir)\"	\
	$(AM_CPPFLAGS)

epiphany_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CFLAGS)

epiphany_LDFLAGS = \
	-dlopen self \
	$(AM_LDFLAGS)

epiphany_LDADD = \
	libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/history/libephyhistory.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(DEPENDENCIES_LIBS) \
	$(CODE_COVERAGE_LDFLAGS)

libexec_PROGRAMS = epiphany-search-provider

epiphany_search_provider_SOURCES = \
	$(dbus_shell_search_provider_built_sources) \
	ephy-search-provider.c \
	ephy-search-provider-main.c

epiphany_search_provider_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/embed		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/history	\
	-I$(top_srcdir)/src/bookmarks   \
	-DDATADIR=\""$(datadir)"\" 	\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DLOCALEDIR=\"$(localedir)\"	\
	$(AM_CPPFLAGS)

epiphany_search_provider_CFLAGS = $(epiphany_CFLAGS)

epiphany_search_provider_LDADD = \
	libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/history/libephyhistory.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(DEPENDENCIES_LIBS) \
	$(CODE_COVERAGE_LDFLAGS)

TYPES_SOURCE = \
	ephy-type-builtins.c	\
	ephy-type-builtins.h

stamp_files = \
	stamp-ephy-type-builtins.c	\
	stamp-ephy-type-builtins.h

ephy-type-builtins.c: stamp-ephy-type-builtins.c Makefile
	@true
stamp-ephy-type-builtins.c: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#include <config.h>\n\n" \
		--fhead "#include \"ephy-type-builtins.h\"\n\n" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--fprod "\n#include \"@filename@\"" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n" \
		--vhead "  static GType type = 0;\n\n" \
		--vhead "  if (G_UNLIKELY (type == 0))\n  {\n" \
		--vhead "    static const G@Type@Value _@enum_name@_values[] = {" \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n\n" \
		--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n  }\n\n" \
		--vtail "  return type;\n}\n\n" \
		$(filter-out $<,$^) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

ephy-type-builtins.h: stamp-ephy-type-builtins.h Makefile
	@true
stamp-ephy-type-builtins.h: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#ifndef EPHY_TYPE_BUILTINS_H\n" \
		--fhead "#define EPHY_TYPE_BUILTINS_H 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* EPHY_TYPE_BUILTINS_H */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define EPHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(filter-out $<,$^) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

dbus_shell_search_provider_built_sources =	\
	ephy-shell-search-provider-generated.c	\
	ephy-shell-search-provider-generated.h

# The upstream for the DBus interface definition is
# at http://git.gnome.org/browse/gnome-shell/plain/data/org.gnome.ShellSearchProvider2.xml
$(dbus_shell_search_provider_built_sources) : Makefile.am $(srcdir)/org.gnome.ShellSearchProvider2.xml
	gdbus-codegen							\
		--interface-prefix org.gnome.				\
		--c-namespace Ephy					\
		--generate-c-code ephy-shell-search-provider-generated	\
		$(srcdir)/org.gnome.ShellSearchProvider2.xml		\
		$(NULL)

CLEANFILES = \
	$(stamp_files) \
	$(BUILT_SOURCES) \
	resources/.sass-cache

-include $(top_srcdir)/git.mk

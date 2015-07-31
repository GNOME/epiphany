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

NOINST_H_FILES = \
	ephy-action-helper.h			\
	ephy-combined-stop-reload-action.h	\
	ephy-encoding-dialog.h			\
	ephy-encoding-menu.h			\
	ephy-history-window.h			\
	ephy-home-action.h			\
	ephy-link-action.h			\
	ephy-lockdown.h				\
	ephy-location-controller.h		\
	ephy-navigation-history-action.h	\
	ephy-private.h				\
	ephy-search-provider.h			\
	ephy-title-box.h			\
	ephy-toolbar.h				\
	ephy-window-action.h			\
	languages.h				\
	clear-data-dialog.h			\
	cookies-dialog.h			\
	passwords-dialog.h			\
	popup-commands.h			\
	prefs-dialog.h				\
	window-commands.h

INST_H_FILES = \
	ephy-link.h			\
	ephy-notebook.h			\
	ephy-session.h			\
	ephy-shell.h			\
	ephy-window.h			\
	$(NULL)

libephymain_la_SOURCES = \
	ephy-action-helper.c			\
	ephy-completion-model.c			\
	ephy-completion-model.h			\
	ephy-combined-stop-reload-action.c	\
	ephy-encoding-dialog.c			\
	ephy-encoding-menu.c			\
	ephy-home-action.c			\
	ephy-history-window.c			\
	ephy-link.c				\
	ephy-link-action.c			\
	ephy-location-controller.c		\
	ephy-lockdown.c				\
	ephy-navigation-history-action.c	\
	ephy-notebook.c				\
	ephy-session.c				\
	ephy-shell.c				\
	ephy-title-box.c			\
	ephy-toolbar.c				\
	ephy-window.c				\
	ephy-window-action.c			\
	clear-data-dialog.c			\
	cookies-dialog.c			\
	passwords-dialog.c			\
	popup-commands.c			\
	prefs-dialog.c				\
	window-commands.c			\
	$(INST_H_FILES)				\
	$(NOINST_H_FILES)			\
	$(NULL)

nodist_libephymain_la_SOURCES = \
	$(TYPES_SOURCE)

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
	resources/about.css			  \
	resources/clear-data-dialog.ui		  \
	resources/cookies-dialog.ui		  \
	resources/encoding-dialog.ui		  \
	resources/epiphany-application-menu.ui	  \
	resources/epiphany-bookmark-editor-ui.xml \
	resources/epiphany-ui.xml		  \
	resources/epiphany.css                    \
	resources/error.html			  \
	resources/history-dialog.ui		  \
	resources/incognito.png                   \
	resources/missing-thumbnail.png		  \
	resources/passwords-dialog.ui		  \
	resources/prefs-dialog.ui		  \
	resources/prefs-lang-dialog.ui		  \
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
stamp-ephy-type-builtins.c: Makefile $(INST_H_FILES) $(NOINST_H_FILES)
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
stamp-ephy-type-builtins.h: Makefile $(INST_H_FILES) $(NOINST_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)\n" \
		--fhead "#error Only <epiphany/epiphany.h> can be included directly.\n" \
		--fhead "#endif\n\n" \
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

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

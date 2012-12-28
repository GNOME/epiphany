SUBDIRS = bookmarks

NULL =

noinst_LTLIBRARIES = libephymain.la

bin_PROGRAMS = epiphany

EXTRA_DIST = \
	epiphany.gresource.xml \
	$(UI_FILES)            \
	$(NULL)

NOINST_H_FILES = \
	ephy-action-helper.h			\
	ephy-combined-stop-reload-action.h	\
	ephy-encoding-dialog.h			\
	ephy-encoding-menu.h			\
	ephy-find-toolbar.h			\
	ephy-history-window.h			\
	ephy-home-action.h			\
	ephy-link-action.h			\
	ephy-lockdown.h				\
	ephy-location-controller.h		\
	ephy-navigation-history-action.h	\
	ephy-page-menu-action.h			\
	ephy-password-info.h			\
	ephy-private.h				\
	ephy-toolbar.h				\
	ephy-window-action.h			\
	languages.h				\
	pdm-dialog.h				\
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
	ephy-find-toolbar.c			\
	ephy-home-action.c			\
	ephy-history-window.c			\
	ephy-link.c				\
	ephy-link-action.c			\
	ephy-location-controller.c		\
	ephy-lockdown.c				\
	ephy-navigation-history-action.c	\
	ephy-notebook.c				\
	ephy-page-menu-action.c			\
	ephy-password-info.c	        	\
	ephy-session.c				\
	ephy-shell.c				\
	ephy-toolbar.c				\
	ephy-window.c				\
	ephy-window-action.c			\
	pdm-dialog.c				\
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
	-I$(top_srcdir)/src/bookmarks   \
	-DLOADER_DIR=\""$(pkglibdir)/$(EPIPHANY_MAJOR)/loaders"\"		\
	-DDATADIR=\""$(pkgdatadir)"\" 	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	$(AM_CPPFLAGS)

if WITH_WEBKIT2
libephymain_la_CPPFLAGS += -DHAVE_WEBKIT2
endif

libephymain_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	  \
	$(WEBKIT_CFLAGS)	  \
	$(CODE_COVERAGE_CFLAGS)   \
	$(AM_CFLAGS)

UI_FILES = \
	resources/epiphany-bookmark-editor-ui.xml \
	resources/epiphany-history-window-ui.xml  \
	resources/epiphany-ui.xml		  \
	resources/epiphany.ui			  \
	resources/epiphany-application-menu.ui	  \
	resources/prefs-dialog.ui		  \
	resources/epiphany.css                    \
	$(NULL)

epiphany-resources.c: epiphany.gresource.xml $(UI_FILES)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(srcdir)/resources --generate-source --c-name epiphany $(srcdir)/epiphany.gresource.xml

epiphany-resources.h: epiphany.gresource.xml $(UI_FILES)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(srcdir)/resources --generate-header --c-name epiphany $(srcdir)/epiphany.gresource.xml

EPIPHANY_RESOURCES = \
	epiphany-resources.c \
	epiphany-resources.h \
	$(NULL)

BUILT_SOURCES = \
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
	-I$(top_srcdir)/src/bookmarks   \
	-DDATADIR=\""$(datadir)"\" 	\
	-DGNOMELOCALEDIR=\"$(datadir)/locale\"	\
	-DEPHY_WEB_EXTENSIONS_DIR=\""$(libdir)/epiphany/$(EPIPHANY_MAJOR)/web-extensions"\" \
	$(INCINTL)			\
	$(AM_CPPFLAGS)

epiphany_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CFLAGS)

epiphany_LDFLAGS = -dlopen self

epiphany_LDADD = \
	libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/history/libephyhistory.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(DEPENDENCIES_LIBS) \
	$(CODE_COVERAGE_LDFLAGS) \
	$(LIBINTL)

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

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)
DISTCLEANFILES = $(stamp_files) $(BUILT_SOURCES)
MAINTAINERCLEANFILES = $(stamp_files) $(BUILT_SOURCES)

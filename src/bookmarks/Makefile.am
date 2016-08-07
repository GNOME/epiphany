noinst_LTLIBRARIES = libephybookmarks.la

AM_CFLAGS = $(CODE_COVERAGE_CFLAGS) $(WARN_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LIBS) $(WARN_LDFLAGS)

TYPES_H_FILES = \
	ephy-bookmarks.h

libephybookmarks_la_SOURCES = 		\
	ephy-bookmarks.c		\
	ephy-bookmarks.h		\
	ephy-bookmark-action.c		\
	ephy-bookmark-action.h		\
	ephy-bookmarks-editor.c		\
	ephy-bookmarks-editor.h		\
	ephy-bookmarks-export.c		\
	ephy-bookmarks-export.h		\
	ephy-bookmarks-import.c 	\
	ephy-bookmarks-import.h		\
	ephy-bookmarks-ui.c		\
	ephy-bookmarks-ui.h		\
	ephy-bookmarks-menu.c		\
	ephy-bookmarks-menu.h		\
	ephy-bookmark-properties.c	\
	ephy-bookmark-properties.h	\
	ephy-link-action.c		\
	ephy-link-action.h		\
	ephy-topic-action.c		\
	ephy-topic-action.h		\
	ephy-open-tabs-action.c		\
	ephy-open-tabs-action.h		\
	ephy-topic-action-group.c	\
	ephy-topic-action-group.h	\
	ephy-bookmark-action-group.c	\
	ephy-bookmark-action-group.h	\
	ephy-topics-entry.c  		\
	ephy-topics-entry.h		\
	ephy-topics-palette.c  		\
	ephy-topics-palette.h		\
	ephy-nodes-cover.c		\
	ephy-nodes-cover.h		\
	ephy-window-action.c		\
	ephy-window-action.h

nodist_libephybookmarks_la_SOURCES = 	\
	$(BUILT_SOURCES)

BUILT_SOURCES =	$(TYPES_SOURCE)

TYPES_SOURCE = \
	ephy-bookmarks-type-builtins.c	\
	ephy-bookmarks-type-builtins.h

stamp_files = \
	stamp-ephy-bookmarks-type-builtins.c	\
	stamp-ephy-bookmarks-type-builtins.h

ephy-bookmarks-type-builtins.c: stamp-ephy-bookmarks-type-builtins.c Makefile
	@true
stamp-ephy-bookmarks-type-builtins.c: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#include <config.h>\n\n" \
		--fhead "#include \"ephy-bookmarks-type-builtins.h\"\n\n" \
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

ephy-bookmarks-type-builtins.h: stamp-ephy-bookmarks-type-builtins.h Makefile
	@true
stamp-ephy-bookmarks-type-builtins.h: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#ifndef EPHY_BOOKMARKS_TYPE_BUILTINS_H\n" \
		--fhead "#define EPHY_BOOKMARKS_TYPE_BUILTINS_H 1\n\n" \
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

libephybookmarks_la_CFLAGS = \
	$(AVAHI_CLIENT_CFLAGS)	\
	$(AVAHI_GOBJECT_CFLAGS)	\
	$(GDK_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(GLIB_CFLAGS)		\
	$(GTK_CFLAGS) 		\
	$(LIBXML_CFLAGS)	\
	$(LIBXSLT_CFLAGS)	\
	$(WEBKIT2GTK_CFLAGS)	\
	$(AM_CFLAGS)

libephybookmarks_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/widgets	\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/embed		\
	-I$(top_srcdir)/lib/widgets	\
	-I$(top_srcdir)/lib/history	\
	-I$(top_srcdir)/lib/egg		\
	-DPKGDATADIR=\"$(pkgdatadir)\" 	\
	$(AM_CPPFLAGS)

libephybookmarks_la_LDFLAGS = \
	$(AM_LDFLAGS)

libephybookmarks_la_LIBADD = \
	$(AVAHI_CLIENT_LIBS)	\
	$(AVAHI_GOBJECT_LIBS)	\
	$(GDK_LIBS)		\
	$(GIO_LIBS)		\
	$(GLIB_LIBS)		\
	$(GTK_LIBS) 		\
	$(LIBXML_LIBS)		\
	$(LIBXSLT_LIBS)		\
	$(WEBKIT2GTK_LIBS)

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

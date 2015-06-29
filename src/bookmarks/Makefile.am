noinst_LTLIBRARIES = libephybookmarks.la

AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

INST_H_FILES = \
	ephy-bookmarks.h

NOINST_H_FILES =		 	\
	ephy-bookmark-action.h		\
	ephy-bookmarks-editor.h		\
	ephy-bookmarks-export.h		\
	ephy-bookmarks-import.h		\
	ephy-bookmarks-ui.h		\
	ephy-bookmarks-menu.h		\
	ephy-bookmark-properties.h	\
	ephy-topic-action.h		\
	ephy-open-tabs-action.h		\
	ephy-topic-action-group.h	\
	ephy-bookmark-action-group.h	\
	ephy-topics-entry.h  		\
	ephy-topics-palette.h  		\
	ephy-nodes-cover.h

libephybookmarks_la_SOURCES = 		\
	ephy-bookmarks.c		\
	ephy-bookmark-action.c		\
	ephy-bookmarks-editor.c		\
	ephy-bookmarks-export.c		\
	ephy-bookmarks-import.c 	\
	ephy-bookmarks-ui.c		\
	ephy-bookmarks-menu.c		\
	ephy-bookmark-properties.c	\
	ephy-topic-action.c		\
	ephy-open-tabs-action.c		\
	ephy-topic-action-group.c	\
	ephy-bookmark-action-group.c	\
	ephy-topics-entry.c  		\
	ephy-topics-palette.c  		\
	ephy-nodes-cover.c      	\
	$(NOINST_H_FILES)		\
	$(INST_H_FILES)

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
stamp-ephy-bookmarks-type-builtins.c: Makefile $(INST_H_FILES) $(NOINST_H_FILES)
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
stamp-ephy-bookmarks-type-builtins.h: Makefile $(INST_H_FILES) $(NOINST_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)\n" \
		--fhead "#error Only <epiphany/epiphany.h> can be included directly.\n" \
		--fhead "#endif\n\n" \
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

libephybookmarks_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/embed		\
	-I$(top_srcdir)/lib/widgets	\
	-I$(top_builddir)/lib/widgets	\
	-I$(top_srcdir)/lib/history	\
	-I$(top_srcdir)/lib/egg		\
	-DDATADIR=\""$(pkgdatadir)"\" 	\
	$(AM_CPPFLAGS)

libephybookmarks_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	\
	$(CODE_COVERAGE_CFLAGS) \
	$(AM_CFLAGS)

libephybookmarks_la_LIBADD =

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

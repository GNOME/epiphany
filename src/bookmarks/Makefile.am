noinst_LTLIBRARIES = libephybookmarks.la

TYPES_H_FILES = \
	ephy-bookmark-properties-grid.h

libephybookmarks_la_SOURCES = \
	ephy-add-bookmark-popover.c	\
	ephy-add-bookmark-popover.h	\
	ephy-bookmark.c			\
	ephy-bookmark.h			\
	ephy-bookmark-properties-grid.c	\
	ephy-bookmark-properties-grid.h	\
	ephy-bookmark-row.c		\
	ephy-bookmark-row.h		\
	ephy-bookmarks-manager.c	\
	ephy-bookmarks-manager.h	\
	ephy-bookmarks-popover.c 	\
	ephy-bookmarks-popover.h

libephybookmarks_la_CPPFLAGS = \
	-I$(top_builddir)/lib				\
	-I$(top_builddir)/lib/widgets			\
	-I$(top_srcdir)/embed				\
	-I$(top_srcdir)/gvdb				\
	-I$(top_srcdir)/lib				\
	-I$(top_srcdir)/lib/history			\
	-I$(top_srcdir)/lib/widgets			\
	-I$(top_srcdir)/src				\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\"	\
	-DLOCALEDIR=\"$(localedir)\"			\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(LIBSECRET_CFLAGS)				\
	$(LIBSOUP_CFLAGS)				\
	$(WEBKIT2GTK_CFLAGS)

libephybookmarks_la_LIBADD = \
	$(top_builddir)/gvdb/libgvdb.la			\
	$(top_builddir)/lib/libephymisc.la		\
	$(top_builddir)/lib/widgets/libephywidgets.la	\
	$(GLIB_LIBS)					\
	$(GTK_LIBS)					\
	$(LIBSECRET_LIBS)				\
	$(LIBSOUP_LIBS)					\
	$(WEBKIT2GTK_LIBS)

libephybookmarks_la_CFLAGS = \
	$(WARN_CFLAGS)

libephybookmarks_la_LDFLAGS = \
	$(WARN_LDFLAGS)

nodist_libephybookmarks_la_SOURCES = \
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

CLEANFILES = \
	$(stamp_files) \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

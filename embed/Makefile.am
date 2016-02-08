NULL =

SUBDIRS = web-extension

AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

noinst_LTLIBRARIES = libephyembed.la

TYPES_H_FILES = \
	ephy-download.h			\
	ephy-embed-shell.h		\
	ephy-web-view.h

BUILT_SOURCES = \
	ephy-embed-type-builtins.c	\
	ephy-embed-type-builtins.h

libephyembed_la_SOURCES = \
	ephy-about-handler.c		\
	ephy-about-handler.h		\
	ephy-downloads-manager.c	\
	ephy-downloads-manager.h	\
	ephy-download.c			\
	ephy-download.h			\
	ephy-embed.c			\
	ephy-embed.h			\
	ephy-embed-container.c          \
	ephy-embed-container.h		\
	ephy-embed-event.c		\
	ephy-embed-event.h		\
	ephy-embed-prefs.h		\
	ephy-embed-private.h		\
	ephy-embed-shell.c		\
	ephy-embed-shell.h		\
	ephy-embed-utils.c              \
	ephy-embed-utils.h		\
	ephy-encoding.c			\
	ephy-encoding.h			\
	ephy-encodings.c		\
	ephy-encodings.h		\
	ephy-file-monitor.c		\
	ephy-file-monitor.h		\
	ephy-find-toolbar.c		\
	ephy-find-toolbar.h		\
	ephy-embed-prefs.c		\
	ephy-web-view.c			\
	ephy-web-view.h			\
	ephy-web-extension-proxy.c	\
	ephy-web-extension-proxy.h

nodist_libephyembed_la_SOURCES = \
	$(BUILT_SOURCES)

libephyembed_la_CPPFLAGS = \
	-I$(top_srcdir)/embed/web-extension \
	-I$(top_builddir)/lib       	    \
	-I$(top_srcdir)/lib       	    \
	-I$(top_srcdir)/lib/egg       	    \
	-I$(top_srcdir)/lib/history	    \
	-I$(top_srcdir)/lib/widgets         \
	-I$(top_builddir)/lib/widgets       \
	$(AM_CPPFLAGS)

libephyembed_la_CFLAGS = \
	-DDATADIR="\"$(datadir)\""	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	-DEPHY_WEB_EXTENSIONS_DIR=\""$(libdir)/epiphany/$(EPIPHANY_MAJOR)/web-extensions"\" \
	$(DEPENDENCIES_CFLAGS) 	\
	$(CODE_COVERAGE_CFLAGS) \
	$(AM_CFLAGS)

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)

stamp_files = \
	stamp-ephy-embed-type-builtins.c	\
	stamp-ephy-embed-type-builtins.h

ephy-embed-type-builtins.c: stamp-ephy-embed-type-builtins.c Makefile
	@true
stamp-ephy-embed-type-builtins.c: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#include <config.h>\n\n" \
		--fhead "#include \"ephy-embed-type-builtins.h\"\n\n" \
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

ephy-embed-type-builtins.h: stamp-ephy-embed-type-builtins.h Makefile
	@true
stamp-ephy-embed-type-builtins.h: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#ifndef EPHY_EMBED_TYPE_BUILTINS_H\n" \
		--fhead "#define EPHY_EMBED_TYPE_BUILTINS_H 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* EPHY_EMBED_TYPE_BUILTINS_H */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define EPHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(filter-out $<,$^) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

-include $(top_srcdir)/git.mk

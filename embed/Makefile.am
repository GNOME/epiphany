NULL =

SUBDIRS = web-extension

AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

noinst_LTLIBRARIES = libephyembed.la

NOINST_H_FILES = \
	ephy-about-handler.h		\
	ephy-embed-dialog.h		\
	ephy-embed-private.h		\
	ephy-encoding.h			\
	ephy-encodings.h		\
	ephy-file-monitor.h

INST_H_FILES = \
	ephy-download.h			\
	ephy-embed.h			\
	ephy-embed-container.h          \
	ephy-embed-event.h		\
	ephy-embed-prefs.h		\
	ephy-embed-shell.h		\
	ephy-embed-utils.h              \
	ephy-find-toolbar.h		\
	ephy-web-view.h			\
	ephy-web-extension-proxy.h


BUILT_SOURCES = \
	ephy-embed-type-builtins.c	\
	ephy-embed-type-builtins.h

libephyembed_la_SOURCES = \
	ephy-about-handler.c		\
	ephy-download.c			\
	ephy-embed.c			\
	ephy-embed-container.c          \
	ephy-embed-dialog.c		\
	ephy-embed-event.c		\
	ephy-embed-shell.c		\
	ephy-embed-utils.c              \
	ephy-encoding.c			\
	ephy-encodings.c		\
	ephy-file-monitor.c		\
	ephy-find-toolbar.c		\
	ephy-embed-prefs.c		\
	ephy-web-view.c			\
	ephy-web-extension-proxy.c	\
	$(INST_H_FILES)			\
	$(NOINST_H_FILES)

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
stamp-ephy-embed-type-builtins.c: Makefile $(INST_H_FILES)
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
stamp-ephy-embed-type-builtins.h: Makefile $(INST_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)\n" \
		--fhead "#error Only <epiphany/epiphany.h> can be included directly.\n" \
		--fhead "#endif\n\n" \
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

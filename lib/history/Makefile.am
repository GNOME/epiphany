AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

noinst_LTLIBRARIES = libephyhistory.la

libephyhistory_la_SOURCES = \
	ephy-history-service.c		    \
	ephy-history-service.h		    \
	ephy-history-service-hosts-table.c  \
	ephy-history-service-private.h	    \
	ephy-history-service-urls-table.c   \
	ephy-history-service-visits-table.c \
	ephy-history-types.c 		    \
	ephy-history-types.h

nodist_libephyhistory_la_SOURCES = \
	$(BUILT_SOURCES)

libephyhistory_la_CPPFLAGS = \
	-I$(top_builddir)/lib/history	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	$(AM_CPPFLAGS)

libephyhistory_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)	\
	$(CODE_COVERAGE_CFLAGS) \
	$(AM_CFLAGS)

TYPES_H_FILES = ephy-history-types.h

BUILT_SOURCES = \
	ephy-history-type-builtins.c	\
	ephy-history-type-builtins.h

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)

stamp_files = \
	stamp-ephy-history-type-builtins.c	\
	stamp-ephy-history-type-builtins.h

ephy-history-type-builtins.c: stamp-ephy-history-type-builtins.c Makefile
	@true

stamp-ephy-history-type-builtins.c: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#include <config.h>\n\n" \
		--fhead "#include \"ephy-history-type-builtins.h\"\n\n" \
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

ephy-history-type-builtins.h: stamp-ephy-history-type-builtins.h Makefile
	@true

stamp-ephy-history-type-builtins.h: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)\n" \
		--fhead "#error Only <epiphany/epiphany.h> can be included directly.\n" \
		--fhead "#endif\n\n" \
		--fhead "#ifndef EPHY_HISTORY_TYPE_BUILTINS_H\n" \
		--fhead "#define EPHY_HISTORY_TYPE_BUILTINS_H 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* EPHY_HISTORY_TYPE_BUILTINS_H */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define EPHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(filter-out $<,$^) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

-include $(top_srcdir)/git.mk

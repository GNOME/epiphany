NULL =

EGGSOURCES = \
	eggtreemultidnd.c \
        eggdesktopfile.c \
        eggsmclient.c \
	eggsmclient-xsmp.c \
	egg-editable-toolbar.c \
	egg-toolbars-model.c \
	egg-toolbar-editor.c \
        $(NULL)

EGGHEADERS = \
	eggtreemultidnd.h \
        eggdesktopfile.h \
        eggsmclient.h \
        eggsmclient-private.h \
	egg-editable-toolbar.h \
	egg-toolbars-model.h \
	egg-toolbar-editor.h \
        $(NULL)

noinst_HEADERS = \
	$(EGGHEADERS) \
	eggmarshalers.h

noinst_LTLIBRARIES = libegg.la

libegg_la_SOURCES = \
	$(EGGSOURCES) \
	$(EGGHEADERS)

nodist_libegg_la_SOURCES = \
	$(BUILT_SOURCES)

libegg_la_CPPFLAGS = \
	-I$(top_builddir)/lib/egg		\
	-DCURSOR_DIR=\"$(pkgdatadir)/art\"	\
        -DG_LOG_DOMAIN=\""Egg"\"		\
	-DEGG_SM_CLIENT_BACKEND_XSMP=1		\
	$(AM_CPPFLAGS)

libegg_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)		\
	$(AM_CFLAGS)

libegg_la_LIBADD =

BUILT_SOURCES = \
	eggmarshalers.c \
	eggmarshalers.h \
	eggtypebuiltins.c \
	eggtypebuiltins.h

stamp_files = \
	stamp-eggmarshalers.c \
	stamp-eggmarshalers.h \
	stamp-eggtypebuiltins.c \
	stamp-eggtypebuiltins.h

eggmarshalers.h: stamp-eggmarshalers.h
	@true
stamp-eggmarshalers.h: eggmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_egg_marshal $(srcdir)/eggmarshalers.list --header > eggmarshalers.h \
	&& echo timestamp > $(@F)

eggmarshalers.c: stamp-eggmarshalers.c
	@true
stamp-eggmarshalers.c: eggmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_egg_marshal $(srcdir)/eggmarshalers.list --header --body > eggmarshalers.c \
	&& echo timestamp > $(@F)

eggtypebuiltins.c: stamp-eggtypebuiltins.c Makefile
	@true
stamp-eggtypebuiltins.c: Makefile $(EGGHEADERS)
	$(GLIB_MKENUMS) \
		--fhead "#include <config.h>\n\n" \
		--fhead "#include \"eggtypebuiltins.h\"\n\n" \
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

eggtypebuiltins.h: stamp-eggtypebuiltins.h Makefile
	@true
stamp-eggtypebuiltins.h: Makefile $(EGGHEADERS)
	$(GLIB_MKENUMS) \
		--fhead "#ifndef __EGGTYPEBUILTINS_H__\n" \
		--fhead "#define __EGGTYPEBUILTINS_H__ 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* __EGGTYPEBUILTINS_H__ */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define EGG_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(filter-out $<,$^) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

EXTRA_DIST = \
	eggmarshalers.list

EGGFILES=$(EGGSOURCES) $(EGGHEADERS)
EGGDIR=$(srcdir)/../../../libegg/libegg

regenerate-built-sources:
	EGGFILES="$(EGGFILES) eggmarshalers.list" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)
DISTCLEANFILES = $(stamp_files) $(BUILT_SOURCES)
MAINTAINERCLEANFILES = $(stamp_files) $(BUILT_SOURCES)

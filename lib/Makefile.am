SUBDIRS = widgets egg

noinst_LTLIBRARIES = libephymisc.la

headerdir = $(prefix)/include/epiphany/$(EPIPHANY_MAJOR)/epiphany
header_DATA = \
	ephy-lib-type-builtins.h	\
	$(INST_H_FILES)

NOINST_H_FILES = \
	eel-gconf-extensions.h			\
	ephy-debug.h				\
	ephy-dnd.h				\
	ephy-file-chooser.h			\
	ephy-file-helpers.h			\
	ephy-gui.h				\
	ephy-langs.h				\
	ephy-module.h				\
	ephy-node-filter.h			\
	ephy-node-common.h			\
	ephy-object-helpers.h			\
	ephy-password-dialog.h			\
	ephy-prefs.h				\
	ephy-print-utils.h			\
	ephy-shlib-loader.h			\
	ephy-signal-accumulator.h		\
	ephy-string.h				\
	ephy-stock-icons.h			\
	ephy-zoom.h

TYPES_H_FILES = \
	ephy-adblock.h				\
	ephy-node.h				\
	ephy-password-dialog.h			\
	ephy-state.h

INST_H_FILES = \
	ephy-adblock.h				\
	ephy-dialog.h				\
	ephy-loader.h				\
	ephy-node.h				\
	ephy-node-db.h				\
	ephy-state.h

libephymisc_la_SOURCES = \
	eel-gconf-extensions.c			\
	ephy-adblock.c				\
	ephy-debug.c				\
	ephy-dialog.c				\
	ephy-dnd.c				\
	ephy-file-chooser.c			\
	ephy-file-helpers.c			\
	ephy-gui.c				\
	ephy-langs.c				\
	ephy-loader.c				\
	ephy-module.c				\
	ephy-node.c				\
	ephy-node.h				\
	ephy-node-filter.c			\
	ephy-node-common.h			\
	ephy-node-db.c				\
	ephy-object-helpers.c			\
	ephy-password-dialog.c			\
	ephy-prefs.h				\
	ephy-print-utils.c			\
	ephy-shlib-loader.c			\
	ephy-signal-accumulator.c		\
	ephy-state.c				\
	ephy-string.c				\
	ephy-stock-icons.c			\
	ephy-zoom.c				\
	$(INST_H_FILES)				\
	$(NOINST_H_FILES)

nodist_libephymisc_la_SOURCES = \
	$(BUILT_SOURCES)

libephymisc_la_CPPFLAGS = \
	-I$(top_builddir)/lib			\
	-DSHARE_DIR=\"$(pkgdatadir)\" 		\
	-DEXTENSIONS_DIR=\""$(libdir)/epiphany/$(EPIPHANY_MAJOR)/extensions"\" 	\
	$(AM_CPPFLAGS)

libephymisc_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)	\
	$(AM_CFLAGS)

libephymisc_la_LIBADD =

if ENABLE_SPELLCHECKER
libephymisc_la_SOURCES += \
	ephy-spell-check.c	\
	ephy-spell-check.h

libephymisc_la_CFLAGS += \
	$(SPELLCHECKER_CFLAGS)

libephymisc_la_LIBADD += \
	$(SPELLCHECKER_LIBS)
endif

BUILT_SOURCES = \
	ephy-lib-type-builtins.c	\
	ephy-lib-type-builtins.h	\
	ephy-marshal.c			\
	ephy-marshal.h

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)
DISTCLEANFILES = $(stamp_files) $(BUILT_SOURCES)
MAINTAINERCLEANFILES = $(stamp_files) $(BUILT_SOURCES)

stamp_files = \
	stamp-ephy-lib-type-builtins.c	\
	stamp-ephy-lib-type-builtins.h	\
	stamp-ephy-marshal.c		\
	stamp-ephy-marshal.h

ephy-marshal.c: stamp-ephy-marshal.c
	@true
stamp-ephy-marshal.c: ephy-marshal.list
	$(GLIB_GENMARSHAL) --prefix=ephy_marshal $(srcdir)/ephy-marshal.list --header --body > ephy-marshal.c \
	&& echo timestamp > $(@F)

ephy-marshal.h: stamp-ephy-marshal.h
	@true
stamp-ephy-marshal.h: ephy-marshal.list
	$(GLIB_GENMARSHAL) --prefix=ephy_marshal $(srcdir)/ephy-marshal.list --header > ephy-marshal.h \
	&& echo timestamp > $(@F)

ephy-lib-type-builtins.c: stamp-ephy-lib-type-builtins.c
	@true
stamp-ephy-lib-type-builtins.c: $(TYPES_H_FILES)
	( cd $(srcdir) && $(GLIB_MKENUMS) \
		--fhead "#include \"ephy-lib-type-builtins.h\"\n\n" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--fprod "\n#include \"@filename@\"" \
		--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
		--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "  { 0, NULL, NULL }\n};\n\n" \
		--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
		--vtail "  static GType type = 0;\n\n" \
		--vtail "  if (G_UNLIKELY (type == 0))\n" \
		--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
		--vtail "  return type;\n}\n\n" \
		$(^F) ) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

ephy-lib-type-builtins.h: stamp-ephy-lib-type-builtins.h
	@true
stamp-ephy-lib-type-builtins.h: $(TYPES_H_FILES)
	( cd $(srcdir) && $(GLIB_MKENUMS) \
		--fhead "#ifndef EPHY_LIB_TYPE_BUILTINS_H\n" \
		--fhead "#define EPHY_LIB_TYPE_BUILTINS_H 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* EPHY_LIB_TYPE_BUILTINS_H */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define EPHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(^F) ) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

EXTRA_DIST = 			\
	ephy-marshal.list

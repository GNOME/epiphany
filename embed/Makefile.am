SUBDIRS = $(with_engine)

DIST_SUBDIRS = mozilla webkit

NULL =

noinst_LTLIBRARIES = libephyembed.la libephyembedfactory.la

headerdir = $(prefix)/include/epiphany/$(EPIPHANY_MAJOR)/epiphany
header_DATA = \
	ephy-embed-type-builtins.h	\
	$(INST_H_FILES)

NOINST_H_FILES = \
	downloader-view.h		\
	ephy-download.h			\
	ephy-embed-dialog.h		\
	ephy-embed-find.h		\
	ephy-encodings.h		\
	ephy-favicon-cache.h

INST_H_FILES = \
	ephy-adblock.h			\
	ephy-adblock-manager.h		\
	ephy-base-embed.h               \
	ephy-command-manager.h		\
	ephy-cookie-manager.h		\
	ephy-embed.h			\
	ephy-embed-container.h          \
	ephy-embed-event.h		\
	ephy-embed-factory.h		\
	ephy-embed-persist.h		\
	ephy-embed-prefs.h		\
	ephy-embed-single.h		\
	ephy-embed-shell.h		\
	ephy-embed-utils.h              \
	ephy-history.h			\
	ephy-history-item.h             \
	ephy-password-manager.h		\
	ephy-permission-manager.h

BUILT_SOURCES = \
	ephy-embed-type-builtins.c	\
	ephy-embed-type-builtins.h

libephyembed_la_SOURCES = \
	ephy-adblock.c			\
	ephy-adblock-manager.c		\
	ephy-base-embed.c               \
	downloader-view.c		\
	ephy-command-manager.c		\
	ephy-cookie-manager.c		\
	ephy-download.c			\
	ephy-embed.c			\
	ephy-embed-container.c          \
	ephy-embed-dialog.c		\
	ephy-embed-event.c		\
	ephy-embed-find.c		\
	ephy-embed-persist.c		\
	ephy-embed-single.c		\
	ephy-embed-shell.c		\
	ephy-embed-utils.c              \
	ephy-encodings.c		\
	ephy-favicon-cache.c		\
	ephy-history.c			\
	ephy-history-item.c             \
	ephy-password-manager.c		\
	ephy-permission-manager.c	\
	$(INST_H_FILES)			\
	$(NOINST_H_FILES)

nodist_libephyembed_la_SOURCES = \
	$(BUILT_SOURCES)

libephyembed_la_CPPFLAGS = \
	-I$(top_builddir)/lib       	\
	-I$(top_srcdir)/lib       	\
	-I$(top_srcdir)/lib/egg       	\
	-I$(top_srcdir)/lib/widgets     \
	$(AM_CPPFLAGS)

libephyembed_la_CFLAGS = \
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CFLAGS)

libephyembedfactory_la_SOURCES =	\
	ephy-embed-factory.c		\
	ephy-embed-factory.h

libephyembedfactory_la_CPPFLAGS = \
	-I$(top_srcdir)/embed/$(with_engine)		\
	$(AM_CPPFLAGS)

if WITH_XULRUNNER_ENGINE
libephyembedfactory_la_CPPFLAGS += \
	-I$(srcdir)/xulrunner/src		\
	-I$(srcdir)/xulrunner/embed		\
	-I$(top_builddir)/embed/xulrunner/src	\
	-I$(top_builddir)/embed/xulrunner/embed	\
	$(LIBXUL_INCLUDES)			\
	$(NULL)
endif

if WITH_GECKO_ENGINE
libephyembedfactory_la_CPPFLAGS += -I$(GECKO_INCLUDE_ROOT)/gtkembedmoz
endif

libephyembedfactory_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CFLAGS)

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)
DISTCLEANFILES = $(stamp_files) $(BUILT_SOURCES)
MAINTAINERCLEANFILES = $(stamp_files) $(BUILT_SOURCES)

stamp_files = \
	stamp-ephy-embed-type-builtins.c	\
	stamp-ephy-embed-type-builtins.h

ephy-embed-type-builtins.c: stamp-ephy-embed-type-builtins.c Makefile
	@true
stamp-ephy-embed-type-builtins.c: Makefile $(INST_H_FILES)
	$(GLIB_MKENUMS) \
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
	$(GLIB_MKENUMS) \
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

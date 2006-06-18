SUBDIRS = mozilla

noinst_LTLIBRARIES = libephyembed.la libephyembedfactory.la

headerdir = $(prefix)/include/epiphany/@EPIPHANY_MAJOR@/epiphany
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
	ephy-adblock-manager.h		\
	ephy-command-manager.h		\
	ephy-cookie-manager.h		\
	ephy-embed.h			\
	ephy-embed-event.h		\
	ephy-embed-factory.h		\
	ephy-embed-persist.h		\
	ephy-embed-prefs.h		\
	ephy-embed-single.h		\
	ephy-embed-shell.h		\
	ephy-history.h			\
	ephy-password-manager.h		\
	ephy-permission-manager.h

BUILT_SOURCES = \
	ephy-embed-type-builtins.c	\
	ephy-embed-type-builtins.h

libephyembed_la_SOURCES = \
	ephy-adblock-manager.c		\
	downloader-view.c		\
	ephy-command-manager.c		\
	ephy-cookie-manager.c		\
	ephy-download.c			\
	ephy-embed.c			\
	ephy-embed-dialog.c		\
	ephy-embed-event.c		\
	ephy-embed-find.c		\
	ephy-embed-persist.c		\
	ephy-embed-single.c		\
	ephy-embed-shell.c		\
	ephy-encodings.c		\
	ephy-favicon-cache.c		\
	ephy-history.c			\
	ephy-password-manager.c		\
	ephy-permission-manager.c	\
	$(INST_H_FILES)			\
	$(NOINST_H_FILES)

if ENABLE_CERTIFICATE_MANAGER
INST_H_FILES += \
	ephy-certificate-manager.h      \
	ephy-x509-cert.h

libephyembed_la_SOURCES += \
	ephy-certificate-manager.c      \
	ephy-x509-cert.c
endif

nodist_libephyembed_la_SOURCES = \
	$(BUILT_SOURCES)

libephyembed_la_CPPFLAGS = \
	-I$(top_builddir)/lib       	\
	-I$(top_srcdir)/embed/mozilla   \
	-I$(top_srcdir)/lib       	\
	-I$(top_srcdir)/lib/egg       	\
	-I$(top_srcdir)/lib/widgets     \
	$(AM_CPPFLAGS)

libephyembed_la_CFLAGS = \
	-I$(MOZILLA_INCLUDE_ROOT)/gtkembedmoz	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CFLAGS)

libephyembedfactory_la_SOURCES =	\
	ephy-embed-factory.c		\
	ephy-embed-factory.h

libephyembedfactory_la_CPPFLAGS = \
	-I$(top_srcdir)/embed/mozilla		\
	-I$(MOZILLA_INCLUDE_ROOT)/gtkembedmoz	\
	$(AM_CPPFLAGS)

libephyembedfactory_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CFLAGS)

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)
DISTCLEANFILES = $(stamp_files) $(BUILT_SOURCES)
MAINTAINERCLEANFILES = $(stamp_files) $(BUILT_SOURCES)

stamp_files = \
	stamp-ephy-embed-type-builtins.c	\
	stamp-ephy-embed-type-builtins.h

ephy-embed-type-builtins.c: stamp-ephy-embed-type-builtins.c
	@true
stamp-ephy-embed-type-builtins.c: $(INST_H_FILES)
	( cd $(srcdir) && $(GLIB_MKENUMS) \
		--fhead "#include \"ephy-embed-type-builtins.h\"\n\n" \
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

ephy-embed-type-builtins.h: stamp-ephy-embed-type-builtins.h
	@true
stamp-ephy-embed-type-builtins.h: $(INST_H_FILES)
	( cd $(srcdir) && $(GLIB_MKENUMS) \
		--fhead "#ifndef EPHY_EMBED_TYPE_BUILTINS_H\n" \
		--fhead "#define EPHY_EMBED_TYPE_BUILTINS_H 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* EPHY_EMBED_TYPE_BUILTINS_H */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define EPHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(^F) ) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

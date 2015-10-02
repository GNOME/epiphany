SUBDIRS = widgets egg history

AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

noinst_LTLIBRARIES = libephymisc.la

NOINST_H_FILES = \
	ephy-debug.h				\
	ephy-dnd.h				\
	ephy-favicon-helpers.h			\
	ephy-file-chooser.h			\
	ephy-file-helpers.h			\
	ephy-form-auth-data.h			\
	ephy-gui.h				\
	ephy-langs.h				\
	ephy-node-filter.h			\
	ephy-node-common.h			\
	ephy-prefs.h				\
	ephy-profile-utils.h			\
	ephy-security-levels.h			\
	ephy-signal-accumulator.h		\
	ephy-smaps.h				\
	ephy-sqlite.h				\
	ephy-sqlite-connection.h		\
	ephy-sqlite-statement.h			\
	ephy-string.h				\
	ephy-snapshot-service.h			\
	ephy-time-helpers.h			\
	ephy-uri-helpers.h			\
	ephy-web-app-utils.h			\
	ephy-web-dom-utils.h			\
	ephy-zoom.h

TYPES_H_FILES = \
	ephy-initial-state.h			\
	ephy-node.h				\
	ephy-security-levels.h

INST_H_FILES = \
	ephy-dialog.h				\
	ephy-initial-state.h			\
	ephy-node.h				\
	ephy-node-db.h				\
	ephy-settings.h

libephymisc_la_SOURCES = \
	ephy-debug.c				\
	ephy-dialog.c				\
	ephy-dnd.c				\
	ephy-favicon-helpers.c			\
	ephy-file-chooser.c			\
	ephy-file-helpers.c			\
	ephy-form-auth-data.c			\
	ephy-gui.c				\
	ephy-initial-state.c			\
	ephy-langs.c				\
	ephy-node.c				\
	ephy-node.h				\
	ephy-node-filter.c			\
	ephy-node-common.h			\
	ephy-node-db.c				\
	ephy-prefs.h				\
	ephy-profile-utils.c			\
	ephy-profile-utils.h			\
	ephy-security-levels.c			\
	ephy-settings.c				\
	ephy-signal-accumulator.c		\
	ephy-smaps.c				\
	ephy-snapshot-service.c			\
	ephy-sqlite-connection.c		\
	ephy-sqlite-statement.c			\
	ephy-string.c				\
	ephy-time-helpers.c			\
	ephy-uri-helpers.c			\
	ephy-web-app-utils.c			\
	ephy-web-dom-utils.c			\
	ephy-zoom.c				\
	$(INST_H_FILES)				\
	$(NOINST_H_FILES)

nodist_libephymisc_la_SOURCES = \
	$(BUILT_SOURCES)

libephymisc_la_CPPFLAGS = \
	-I$(top_builddir)/lib			    \
	-I$(top_srcdir)/lib/egg			    \
	-DDATADIR="\"$(datadir)\""		    \
	-DSHARE_DIR=\"$(pkgdatadir)\" 		    \
	-DTOP_SRC_DATADIR=\"$(top_srcdir)/data\"    \
	-DABS_TOP_BUILD_DIR=\"$(abs_top_builddir)\" \
	-DLOCALEDIR=\"$(localedir)\"		    \
	$(AM_CPPFLAGS)

libephymisc_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)	\
	$(CODE_COVERAGE_CFLAGS) \
	$(AM_CFLAGS)

libephymisc_la_LIBADD = \
	-lm \
	$(CODE_COVERAGE_LDFLAGS) \
	$(DEPENDENCIES_LIBS)

bin_PROGRAMS = ephy-profile-migrator

ephy_profile_migrator_SOURCES = \
	ephy-profile-migrator.c

ephy_profile_migrator_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) \
	$(NSS_CFLAGS) \
	$(AM_CFLAGS)

ephy_profile_migrator_LDADD = \
	$(top_builddir)/lib/history/libephyhistory.la \
	libephymisc.la \
	$(DEPENDENCIES_LIBS)

if ENABLE_NSS
ephy_profile_migrator_SOURCES += \
	ephy-nss-glue.c \
	ephy-nss-glue.h
ephy_profile_migrator_LDADD += $(NSS_LIBS)
endif # ENABLE_NSS

ephy_profile_migrator_CPPFLAGS = \
	-I$(top_builddir)/lib \
	-I$(srcdir)/history \
	-DSHARE_DIR=\"$(pkgdatadir)\" \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	$(AM_CPPFLAGS)

BUILT_SOURCES = \
	ephy-lib-type-builtins.c	\
	ephy-lib-type-builtins.h

CLEANFILES = $(stamp_files) $(BUILT_SOURCES)

stamp_files = \
	stamp-ephy-lib-type-builtins.c	\
	stamp-ephy-lib-type-builtins.h

ephy-lib-type-builtins.c: stamp-ephy-lib-type-builtins.c Makefile
	@true
stamp-ephy-lib-type-builtins.c: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#include <config.h>\n\n" \
		--fhead "#include \"ephy-lib-type-builtins.h\"\n\n" \
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

ephy-lib-type-builtins.h: stamp-ephy-lib-type-builtins.h Makefile
	@true
stamp-ephy-lib-type-builtins.h: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)\n" \
		--fhead "#error Only <epiphany/epiphany.h> can be included directly.\n" \
		--fhead "#endif\n\n" \
		--fhead "#ifndef EPHY_LIB_TYPE_BUILTINS_H\n" \
		--fhead "#define EPHY_LIB_TYPE_BUILTINS_H 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* EPHY_LIB_TYPE_BUILTINS_H */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define EPHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(filter-out $<,$^) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

-include $(top_srcdir)/git.mk

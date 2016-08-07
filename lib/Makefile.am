SUBDIRS = widgets egg history

AM_CFLAGS = $(CODE_COVERAGE_CFLAGS) $(WARN_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS) $(WARN_LDFLAGS)

noinst_LTLIBRARIES = libephymisc.la

TYPES_H_FILES = \
	ephy-initial-state.h			\
	ephy-node.h				\
	ephy-security-levels.h

libephymisc_la_SOURCES = \
	ephy-dbus-util.c			\
	ephy-dbus-util.h			\
	ephy-debug.c				\
	ephy-debug.h				\
	ephy-dnd.c				\
	ephy-dnd.h				\
	ephy-favicon-helpers.c			\
	ephy-favicon-helpers.h			\
	ephy-file-helpers.c			\
	ephy-file-helpers.h			\
	ephy-form-auth-data.c			\
	ephy-form-auth-data.h			\
	ephy-gui.c				\
	ephy-gui.h				\
	ephy-initial-state.c			\
	ephy-initial-state.h			\
	ephy-langs.c				\
	ephy-langs.h				\
	ephy-node.c				\
	ephy-node.h				\
	ephy-node-common.h			\
	ephy-node-filter.c			\
	ephy-node-filter.h			\
	ephy-node-db.c				\
	ephy-node-db.h				\
	ephy-prefs.h				\
	ephy-profile-utils.c			\
	ephy-profile-utils.h			\
	ephy-security-levels.c			\
	ephy-security-levels.h			\
	ephy-settings.c				\
	ephy-settings.h				\
	ephy-signal-accumulator.c		\
	ephy-signal-accumulator.h		\
	ephy-smaps.c				\
	ephy-smaps.h				\
	ephy-snapshot-service.c			\
	ephy-snapshot-service.h			\
	ephy-sqlite.h				\
	ephy-sqlite-connection.c		\
	ephy-sqlite-connection.h		\
	ephy-sqlite-statement.c			\
	ephy-sqlite-statement.h			\
	ephy-string.c				\
	ephy-string.h				\
	ephy-time-helpers.c			\
	ephy-time-helpers.h			\
	ephy-uri-helpers.c			\
	ephy-uri-helpers.h			\
	ephy-web-app-utils.c			\
	ephy-web-app-utils.h			\
	ephy-zoom.c				\
	ephy-zoom.h

nodist_libephymisc_la_SOURCES = \
	$(BUILT_SOURCES)

libephymisc_la_CPPFLAGS = \
	-DABS_TOP_BUILD_DIR=\"$(abs_top_builddir)\" \
	-DDATADIR=\"$(datadir)\"		    \
	-DLOCALEDIR=\"$(localedir)\"		    \
	-DSHARE_DIR=\"$(pkgdatadir)\" 		    \
	-DTOP_SRC_DATADIR=\"$(top_srcdir)/data\"    \
	-I$(top_builddir)/lib			    \
	-I$(top_srcdir)/lib/egg			    \
	$(AM_CPPFLAGS)

libephymisc_la_CFLAGS = \
	$(CAIRO_CFLAGS)		\
	$(GDK_PIXBUF_CFLAGS)	\
	$(GDK_X11_CFLAGS)	\
	$(GIO_CFLAGS)		\
	$(GLIB_CFLAGS)		\
	$(GNOME_DESKTOP_CFLAGS)	\
	$(GTK_CFLAGS)		\
	$(LIBSECRET_CFLAGS)	\
	$(LIBSOUP_CFLAGS)	\
	$(LIBXML_CFLAGS)	\
	$(SQLITE3_CFLAGS)	\
	$(WEBKIT2GTK_CFLAGS)	\
	$(AM_CFLAGS)

libephymisc_la_LDFLAGS = \
	$(AM_LDFLAGS)

libephymisc_la_LIBADD = \
	$(CAIRO_LIBS)		\
	$(GDK_PIXBUF_LIBS)	\
	$(GDK_X11_LIBS)		\
	$(GIO_LIBS)		\
	$(GLIB_LIBS)		\
	$(GNOME_DESKTOP_LIBS)	\
	$(GTK_LIBS)		\
	$(LIBSECRET_LIBS)	\
	$(LIBSOUP_LIBS)		\
	$(LIBXML_LIBS)		\
	$(LIBM)			\
	$(SQLITE3_LIBS)		\
	$(WEBKIT2GTK_LIBS)

bin_PROGRAMS = ephy-profile-migrator

ephy_profile_migrator_SOURCES = \
	ephy-profile-migrator.c

ephy_profile_migrator_CFLAGS = \
	$(GLIB_CFLAGS)		\
	$(GTK_CFLAGS)		\
	$(LIBSECRET_CFLAGS)	\
	$(LIBSOUP_CFLAGS)	\
	$(WEBKIT2GTK_CFLAGS)	\
	$(AM_CFLAGS)

ephy_profile_migrator_CPPFLAGS = \
	-I$(top_builddir)/lib \
	-I$(srcdir)/history \
	-DSHARE_DIR=\"$(pkgdatadir)\" \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	$(AM_CPPFLAGS)

ephy_profile_migrator_LDADD = \
	$(top_builddir)/lib/history/libephyhistory.la \
	libephymisc.la		\
	$(GLIB_LIBS)		\
	$(GTK_LIBS)		\
	$(LIBSECRET_LIBS)	\
	$(LIBSOUP_LIBS)		\
	$(WEBKIT2GTK_LIBS)

ephy_profile_migrator_LDFLAGS = \
	$(AM_LDFLAGS)

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

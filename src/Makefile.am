SUBDIRS = . search-provider profile-migrator

pkglib_LTLIBRARIES = libephymain.la

bin_PROGRAMS = epiphany

dist_noinst_DATA = \
	resources/epiphany.gresource.xml \
	$(RESOURCE_FILES)

TYPES_H_FILES = \
	bookmarks/ephy-bookmark-properties-grid.h	\
	ephy-link.h					\
	ephy-session.h					\
	ephy-shell.h					\
	ephy-window.h

libephymain_la_SOURCES = \
	clear-data-dialog.c			\
	clear-data-dialog.h			\
	cookies-dialog.c			\
	cookies-dialog.h			\
	ephy-action-helper.c			\
	ephy-action-helper.h			\
	bookmarks/ephy-add-bookmark-popover.c	\
	bookmarks/ephy-add-bookmark-popover.h	\
	bookmarks/ephy-bookmark.c		\
	bookmarks/ephy-bookmark.h		\
	bookmarks/ephy-bookmark-properties-grid.c	\
	bookmarks/ephy-bookmark-properties-grid.h	\
	bookmarks/ephy-bookmark-row.c		\
	bookmarks/ephy-bookmark-row.h		\
	bookmarks/ephy-bookmarks-export.c	\
	bookmarks/ephy-bookmarks-export.h	\
	bookmarks/ephy-bookmarks-import.c	\
	bookmarks/ephy-bookmarks-import.h	\
	bookmarks/ephy-bookmarks-manager.c	\
	bookmarks/ephy-bookmarks-manager.h	\
	bookmarks/ephy-bookmarks-popover.c 	\
	bookmarks/ephy-bookmarks-popover.h	\
	ephy-completion-model.c			\
	ephy-completion-model.h			\
	ephy-encoding-dialog.c			\
	ephy-encoding-dialog.h			\
	ephy-encoding-row.c			\
	ephy-encoding-row.h			\
	ephy-header-bar.c			\
	ephy-header-bar.h			\
	ephy-history-dialog.c			\
	ephy-history-dialog.h			\
	ephy-link.c				\
	ephy-link.h				\
	ephy-location-controller.c		\
	ephy-location-controller.h		\
	ephy-lockdown.c				\
	ephy-lockdown.h				\
	ephy-notebook.c				\
	ephy-notebook.h				\
	ephy-search-engine-dialog.c		\
	ephy-search-engine-dialog.h		\
	ephy-session.c				\
	ephy-session.h				\
	ephy-shell.c				\
	ephy-shell.h				\
	ephy-window.c				\
	ephy-window.h				\
	languages.h				\
	passwords-dialog.c			\
	passwords-dialog.h			\
	popup-commands.c			\
	popup-commands.h			\
	prefs-dialog.c				\
	prefs-dialog.h				\
	window-commands.c			\
	window-commands.h

if ENABLE_SYNC
libephymain_la_SOURCES += \
	sync/ephy-sync-crypto.c			\
	sync/ephy-sync-crypto.h			\
	sync/ephy-sync-secret.c			\
	sync/ephy-sync-secret.h			\
	sync/ephy-sync-service.c		\
	sync/ephy-sync-service.h		\
	sync/ephy-sync-utils.c			\
	sync/ephy-sync-utils.h
endif

nodist_libephymain_la_SOURCES = \
	$(TYPES_SOURCE)

libephymain_la_CPPFLAGS = \
	-I$(top_builddir)/embed		\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/widgets	\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/gvdb/gvdb	\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/history	\
	-I$(top_srcdir)/lib/widgets   	\
	-I$(top_srcdir)/src/bookmarks	\
	-I$(top_srcdir)/src/sync	\
	-DLOCALEDIR=\"$(localedir)\"	\
	-DPKGDATADIR=\"$(pkgdatadir)\"	\
	$(GDK_CFLAGS)			\
	$(GIO_CFLAGS)			\
	$(GLIB_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(HOGWEED_CFLAGS)		\
	$(JSON_GLIB_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(LIBSECRET_CFLAGS)		\
	$(LIBSOUP_CFLAGS)		\
	$(LIBXML_CFLAGS)		\
	$(NETTLE_CFLAGS)		\
	$(WEBKIT2GTK_CFLAGS)

libephymain_la_CFLAGS = \
	$(WARN_CFLAGS)

libephymain_la_LDFLAGS = \
	$(WARN_LDFLAGS) -avoid-version -no-undefined

libephymain_la_LIBADD = \
	$(top_builddir)/embed/libephyembed.la			\
	$(top_builddir)/gvdb/libgvdb.la				\
	$(top_builddir)/lib/libephymisc.la			\
	$(top_builddir)/lib/egg/libegg.la			\
	$(top_builddir)/lib/widgets/libephywidgets.la		\
	$(GDK_LIBS)		\
	$(GIO_LIBS)		\
	$(GLIB_LIBS)		\
	$(GTK_LIBS)		\
	$(HOGWEED_LIBS)		\
	$(JSON_GLIB_LIBS)	\
	$(LIBNOTIFY_LIBS)	\
	$(LIBSECRET_LIBS)	\
	$(LIBSOUP_LIBS)		\
	$(LIBXML_LIBS)		\
	$(NETTLE_LIBS)		\
	$(WEBKIT2GTK_LIBS)

RESOURCE_FILES = \
	resources/about.css			  	\
	resources/about.ini				\
	resources/channel-insecure-symbolic.png		\
	resources/computer-fail-symbolic.png		\
	resources/ephy-bookmark-tag-symbolic.svg	\
	resources/ephy-bookmarks-symbolic.svg		\
	resources/epiphany.css				\
	resources/error.css				\
	resources/error.html				\
	resources/gtk/application-menu.ui		\
	resources/gtk/bookmark-properties-grid.ui	\
	resources/gtk/bookmark-row.ui			\
	resources/gtk/bookmarks-popover.ui		\
	resources/gtk/clear-data-dialog.ui		\
	resources/gtk/cookies-dialog.ui			\
	resources/gtk/encoding-dialog.ui		\
	resources/gtk/encoding-row.ui			\
	resources/gtk/history-dialog.ui			\
	resources/gtk/menus.ui				\
	resources/gtk/page-menu-popover.ui		\
	resources/gtk/passwords-dialog.ui		\
	resources/gtk/prefs-dialog.ui			\
	resources/gtk/prefs-lang-dialog.ui		\
	resources/gtk/search-engine-dialog.ui		\
	resources/gtk/shortcuts-dialog.ui		\
	resources/incognito.png				\
	resources/mime-types-permissions.xml		\
	resources/missing-thumbnail.png			\
	resources/network-error-symbolic.png

epiphany-resources.c: resources/epiphany.gresource.xml $(RESOURCE_FILES)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir)/resources --generate-source --c-name epiphany $(srcdir)/resources/epiphany.gresource.xml

epiphany-resources.h: resources/epiphany.gresource.xml $(RESOURCE_FILES)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir)/resources --generate-header --c-name epiphany $(srcdir)/resources/epiphany.gresource.xml

EPIPHANY_RESOURCES = \
	epiphany-resources.c \
	epiphany-resources.h

BUILT_SOURCES = \
	$(EPIPHANY_RESOURCES) \
	$(TYPES_SOURCE)

epiphany_SOURCES = ephy-main.c \
	$(EPIPHANY_RESOURCES)

epiphany_CFLAGS = \
	$(WARN_CFLAGS)

epiphany_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/embed 		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/history	\
	-I$(top_srcdir)/lib/widgets	\
	-I$(top_builddir)/lib/widgets	\
	-I$(top_srcdir)/src/bookmarks	\
	-I$(top_srcdir)/src/sync	\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DLOCALEDIR=\"$(localedir)\"	\
	$(GLIB_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(HOGWEED_CFLAGS)		\
	$(JSON_GLIB_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(LIBXML_CFLAGS)		\
	$(NETTLE_CFLAGS)		\
	$(WEBKIT2GTK_CFLAGS)

epiphany_LDADD = \
	$(top_builddir)/lib/libephymisc.la	\
	libephymain.la		\
	$(GLIB_LIBS)		\
	$(GTK_LIBS)		\
	$(HOGWEED_LIBS)		\
	$(JSON_GLIB_LIBS)	\
	$(LIBNOTIFY_LIBS)	\
	$(LIBXML_LIBS)		\
	$(NETTLE_LIBS)		\
	$(WEBKIT2GTK_LIBS)

epiphany_LDFLAGS = \
	-dlopen self	\
	$(WARN_LDFLAGS)

TYPES_SOURCE = \
	ephy-type-builtins.c	\
	ephy-type-builtins.h

stamp_files = \
	stamp-ephy-type-builtins.c	\
	stamp-ephy-type-builtins.h

ephy-type-builtins.c: stamp-ephy-type-builtins.c Makefile
	@true
stamp-ephy-type-builtins.c: Makefile $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) \
		--fhead "#include <config.h>\n\n" \
		--fhead "#include \"ephy-type-builtins.h\"\n\n" \
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

ephy-type-builtins.h: stamp-ephy-type-builtins.h Makefile
	@true
stamp-ephy-type-builtins.h: Makefile $(TYPES_H_FILES)
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
	$(BUILT_SOURCES) \
	resources/.sass-cache

-include $(top_srcdir)/git.mk

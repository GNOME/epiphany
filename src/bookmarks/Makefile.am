INCLUDES = \
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/embed		\
	-I$(top_srcdir)/lib/widgets	\
	-I$(top_srcdir)/lib/egg		\
	$(WARN_CFLAGS)			\
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	-DSHARE_DIR=\"$(pkgdatadir)\" 	\
	-DDATADIR=\""$(datadir)"\" 	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephybookmarks.la

libephybookmarks_la_SOURCES = 		\
	ephy-bookmarks.c		\
	ephy-bookmarks.h		\
	ephy-bookmark-action.c		\
	ephy-bookmark-action.h		\
	ephy-bookmarks-editor.c		\
	ephy-bookmarks-editor.h		\
	ephy-bookmarks-import.c 	\
	ephy-bookmarks-import.h		\
	ephy-bookmark-properties.c	\
	ephy-bookmark-properties.h	\
	ephy-bookmarks-toolbar.c	\
	ephy-bookmarks-toolbar.h	\
	ephy-keywords-entry.c		\
	ephy-keywords-entry.h		\
	ephy-new-bookmark.c		\
	ephy-new-bookmark.h		\
	ephy-topic-action.c		\
	ephy-topic-action.h		\
	ephy-topics-selector.c  	\
	ephy-topics-selector.h

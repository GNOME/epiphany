INCLUDES = \
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/embed		\
	-I$(top_srcdir)/lib/widgets	\
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

libephybookmarks_la_SOURCES = \
	ephy-bookmarks.c	\
	ephy-bookmarks.h	\
	ephy-bookmarks-editor.c	\
	ephy-bookmarks-editor.h	\
	ephy-bookmarks-import.c \
	ephy-bookmarks-import.h	\
	ephy-keywords-entry.c	\
	ephy-keywords-entry.h	\
	ephy-new-bookmark.c	\
	ephy-new-bookmark.h	\
	ephy-node-view.c	\
	ephy-node-view.h	\
	ephy-tree-model-node.c	\
	ephy-tree-model-node.h



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
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephybookmarks.la

headerdir = $(prefix)/include/epiphany-@EPIPHANY_MAJOR@/epiphany
header_DATA = $(INST_H_FILES)

INST_H_FILES =		 	\
	ephy-bookmarks.h

NOINST_H_FILES =		 	\
	ephy-bookmark-action.h		\
	ephy-bookmarks-editor.h		\
	ephy-bookmarks-export.h		\
	ephy-bookmarks-import.h		\
	ephy-bookmarks-menu.h		\
	ephy-bookmark-properties.h	\
	ephy-new-bookmark.h		\
	ephy-topic-action.h		\
	ephy-topics-selector.h

libephybookmarks_la_SOURCES = 		\
	ephy-bookmarks.c		\
	ephy-bookmark-action.c		\
	ephy-bookmarks-editor.c		\
	ephy-bookmarks-export.c		\
	ephy-bookmarks-import.c 	\
	ephy-bookmarks-menu.c		\
	ephy-bookmark-properties.c	\
	ephy-new-bookmark.c		\
	ephy-topic-action.c		\
	ephy-topics-selector.c  	\
	$(NOINST_H_FILES)		\
	$(INST_H_FILES)

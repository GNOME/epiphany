SUBDIRS = widgets egg

INCLUDES = \
        $(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"

if MAINTAINER_MODE
INCLUDES += -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGNOME_DISABLE_DEPRECATED -DBONOBO_DISABLE_DEPRECATED $(WARN_CFLAGS)
endif

noinst_LTLIBRARIES = libephymisc.la

headerdir = $(prefix)/include/epiphany-@EPIPHANY_MAJOR@/epiphany
header_DATA = $(INST_H_FILES)

NOINST_H_FILES = \
	eel-gconf-extensions.h			\
	ephy-bonobo-extensions.h		\
	ephy-debug.h				\
	ephy-dnd.h				\
	ephy-file-chooser.h			\
	ephy-file-helpers.h			\
	ephy-glade.h				\
	ephy-gui.h				\
	ephy-langs.h				\
	ephy-marshal.h				\
	ephy-module-loader.h			\
	ephy-node-filter.h			\
	ephy-node-common.h			\
	ephy-prefs.h				\
	ephy-state.h				\
	ephy-string.h				\
	ephy-stock-icons.h			\
	ephy-thread-helpers.h			\
	ephy-zoom.h

INST_H_FILES = \
	ephy-dialog.h				\
	ephy-node.h				\
	ephy-node-db.h

libephymisc_la_SOURCES = \
	eel-gconf-extensions.c			\
	ephy-bonobo-extensions.c		\
	ephy-debug.c				\
	ephy-dialog.c				\
	ephy-dnd.c				\
	ephy-file-chooser.c			\
	ephy-file-helpers.c			\
	ephy-glade.c				\
	ephy-gui.c				\
	ephy-gui.h				\
	ephy-langs.c				\
	ephy-module-loader.c			\
	ephy-marshal.c				\
	ephy-node.c				\
	ephy-node.h				\
	ephy-node-filter.c			\
	ephy-node-common.h			\
	ephy-node-db.c				\
	ephy-prefs.h				\
	ephy-state.c				\
	ephy-string.c				\
	ephy-stock-icons.c			\
	ephy-thread-helpers.c			\
	ephy-types.h				\
	ephy-zoom.c				\
	$(INST_H_FILES)				\
	$(NOINST_H_FILES)

BUILT_SOURCES=ephy-marshal.c ephy-marshal.h

CLEAN_FILES = $(BUILT_SOURCES)

ephy-marshal.c: ephy-marshal.list
	@GLIB_GENMARSHAL@ --prefix=ephy_marshal $(srcdir)/ephy-marshal.list --header --body > ephy-marshal.c

ephy-marshal.h: ephy-marshal.list
	@GLIB_GENMARSHAL@ --prefix=ephy_marshal $(srcdir)/ephy-marshal.list --header > ephy-marshal.h

EXTRA_DIST = 			\
	ephy-marshal.list


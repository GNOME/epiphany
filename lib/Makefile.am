SUBDIRS = widgets egg

INCLUDES = \
	$(WARN_CFLAGS)			\
        $(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephy.la

headerdir = $(prefix)/include/epiphany-@EPIPHANY_MAJOR@/epiphany
header_DATA = $(INST_H_FILES)

NOINST_H_FILES = \
	eel-gconf-extensions.h			\
	ephy-autocompletion.h			\
	ephy-autocompletion-source.h		\
	ephy-bonobo-extensions.h		\
	ephy-debug.h				\
	ephy-dnd.h				\
	ephy-file-helpers.h			\
	ephy-glade.h				\
	ephy-gui.h				\
	ephy-marshal.h				\
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
	ephy-langs.h				\
	ephy-node.h				\
	ephy-node-db.h				\
	ephy-types.h

libephy_la_SOURCES = \
	eel-gconf-extensions.c			\
	eel-gconf-extensions.h			\
	ephy-autocompletion.c			\
	ephy-autocompletion.h			\
	ephy-autocompletion-source.c		\
	ephy-autocompletion-source.h		\
	ephy-bonobo-extensions.h		\
	ephy-bonobo-extensions.c		\
	ephy-debug.c				\
	ephy-debug.h				\
	ephy-dialog.c				\
	ephy-dialog.h				\
	ephy-dnd.c				\
	ephy-dnd.h				\
	ephy-file-helpers.c			\
	ephy-file-helpers.h			\
	ephy-glade.c				\
	ephy-glade.h				\
	ephy-gui.c				\
	ephy-gui.h				\
	ephy-langs.h				\
	ephy-langs.c				\
	ephy-marshal.c				\
	ephy-marshal.h				\
	ephy-node.c				\
	ephy-node.h				\
	ephy-node-filter.c			\
	ephy-node-filter.h			\
	ephy-node-common.h			\
	ephy-node-db.c				\
	ephy-node-db.h				\
	ephy-prefs.h				\
	ephy-state.c				\
	ephy-state.h				\
	ephy-string.c				\
	ephy-string.h				\
	ephy-stock-icons.c			\
	ephy-stock-icons.h			\
	ephy-thread-helpers.c			\
	ephy-thread-helpers.h			\
	ephy-types.h				\
	ephy-zoom.h				\
	ephy-zoom.c

libephy_la_LIBADD = \
	$(top_builddir)/lib/widgets/libephywidgets.la	\
	$(top_builddir)/lib/egg/libegg.la

BUILT_SOURCES=ephy-marshal.c ephy-marshal.h

CLEAN_FILES = $(BUILT_SOURCES)

ephy-marshal.c: ephy-marshal.list
	@GLIB_GENMARSHAL@ --prefix=ephy_marshal $(srcdir)/ephy-marshal.list --header --body > ephy-marshal.c

ephy-marshal.h: ephy-marshal.list
	@GLIB_GENMARSHAL@ --prefix=ephy_marshal $(srcdir)/ephy-marshal.list --header > ephy-marshal.h

EXTRA_DIST = 			\
	ephy-marshal.list


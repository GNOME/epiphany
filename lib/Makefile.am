SUBDIRS = widgets egg

INCLUDES = \
	$(WARN_CFLAGS)			\
        $(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephy.la

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
	ephy-filesystem-autocompletion.c	\
	ephy-filesystem-autocompletion.h	\
	ephy-glade.c				\
	ephy-glade.h				\
	ephy-gobject-misc.h			\
	ephy-gui.c				\
	ephy-gui.h				\
	ephy-marshal.c				\
	ephy-marshal.h				\
	ephy-node.c				\
	ephy-node.h				\
	ephy-node-filter.c			\
	ephy-node-filter.h			\
	ephy-prefs.h				\
	ephy-prefs-utils.c			\
	ephy-prefs-utils.h			\
	ephy-start-here.c			\
	ephy-start-here.h			\
	ephy-state.c				\
	ephy-state.h				\
	ephy-string.c				\
	ephy-string.h				\
	ephy-stock-icons.c			\
	ephy-stock-icons.h			\
	ephy-thread-helpers.c			\
	ephy-thread-helpers.h			\
	ephy-types.h

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


INCLUDES = \
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/egg		\
	$(WARN_CFLAGS)			\
	$(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephywidgets.la

libephywidgets_la_SOURCES = \
	ephy-arrow-toolbutton.c			\
	ephy-arrow-toolbutton.h			\
	ephy-autocompletion-window.c		\
	ephy-autocompletion-window.h		\
	ephy-ellipsizing-label.c		\
	ephy-ellipsizing-label.h		\
	ephy-location-entry.c			\
	ephy-location-entry.h			\
	ephy-node-view.c			\
	ephy-node-view.h			\
	ephy-search-entry.c			\
	ephy-search-entry.h			\
	ephy-spinner.c				\
	ephy-spinner.h				\
	ephy-tree-model-node.c			\
	ephy-tree-model-node.h			\
	ephy-tree-model-sort.c			\
	ephy-tree-model-sort.h			\
	ephy-zoom-action.h			\
	ephy-zoom-action.c			\
	ephy-zoom-control.c			\
	ephy-zoom-control.h

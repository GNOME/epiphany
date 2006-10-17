noinst_LTLIBRARIES = libephywidgets.la

libephywidgets_la_SOURCES = \
	ephy-icon-entry.c			\
	ephy-icon-entry.h			\
	ephy-location-entry.c			\
	ephy-location-entry.h			\
	ephy-node-view.c			\
	ephy-node-view.h			\
	ephy-search-entry.c			\
	ephy-search-entry.h			\
	ephy-spinner.c				\
	ephy-spinner.h				\
	ephy-spinner-tool-item.c		\
	ephy-spinner-tool-item.h		\
	ephy-tree-model-node.c			\
	ephy-tree-model-node.h			\
	ephy-tree-model-sort.c			\
	ephy-tree-model-sort.h			\
	ephy-zoom-action.h			\
	ephy-zoom-action.c			\
	ephy-zoom-control.c			\
	ephy-zoom-control.h

libephywidgets_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/egg		\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	$(AM_CPPFLAGS)

libephywidgets_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)	\
	$(AM_CFLAGS)

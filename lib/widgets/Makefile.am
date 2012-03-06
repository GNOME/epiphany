noinst_LTLIBRARIES = libephywidgets.la

libephywidgets_la_SOURCES = \
	ephy-download-widget.c			\
	ephy-download-widget.h			\
	ephy-history-view.c			\
	ephy-history-view.h			\
	ephy-hosts-store.c			\
	ephy-hosts-store.h			\
	ephy-hosts-view.c			\
	ephy-hosts-view.h			\
	ephy-location-entry.c			\
	ephy-location-entry.h			\
	ephy-middle-clickable-button.c		\
	ephy-middle-clickable-button.h		\
	ephy-node-view.c			\
	ephy-node-view.h			\
	ephy-search-entry.c			\
	ephy-search-entry.h			\
	ephy-tree-model-node.c			\
	ephy-tree-model-node.h			\
	ephy-tree-model-sort.c			\
	ephy-tree-model-sort.h			\
	ephy-urls-store.c			\
	ephy-urls-store.h			\
	ephy-urls-view.c			\
	ephy-urls-view.h			\
	ephy-zoom-action.h			\
	ephy-zoom-action.c			\
	nautilus-floating-bar.c			\
	nautilus-floating-bar.h			\
	totem-glow-button.c			\
	totem-glow-button.h

libephywidgets_la_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/widgets	\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/embed		\
	-I$(top_srcdir)/lib/egg		\
	-I$(top_srcdir)/lib/history	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	$(AM_CPPFLAGS)

libephywidgets_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)	\
	$(AM_CFLAGS)

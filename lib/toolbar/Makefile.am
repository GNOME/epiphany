INCLUDES = \
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/widgets	\
	$(WARN_CFLAGS)			\
	$(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephytoolbar.la

libephytoolbar_la_SOURCES = \
	ephy-toolbar.h				\
	ephy-toolbar.c				\
	ephy-toolbar-item.h			\
	ephy-toolbar-item.c			\
	ephy-toolbar-item-factory.h		\
	ephy-toolbar-item-factory.c		\
	ephy-tbi-zoom.h				\
	ephy-tbi-zoom.c				\
	ephy-tbi-separator.h			\
	ephy-tbi-separator.c			\
	ephy-tbi-favicon.h			\
	ephy-tbi-favicon.c			\
	ephy-tbi-spinner.h			\
	ephy-tbi-spinner.c			\
	ephy-tbi-location.h			\
	ephy-tbi-location.c			\
	ephy-tbi-navigation-history.h		\
	ephy-tbi-navigation-history.c		\
	ephy-tbi-std-toolitem.h			\
	ephy-tbi-std-toolitem.c			\
	ephy-toolbar-bonobo-view.c		\
	ephy-toolbar-bonobo-view.h		\
	ephy-toolbar-tree-model.h		\
	ephy-toolbar-tree-model.c		\
	ephy-toolbar-editor.h			\
	ephy-toolbar-editor.c			\
	ephy-tb-button.c			\
	ephy-tb-button.h

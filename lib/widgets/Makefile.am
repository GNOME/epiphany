INCLUDES = \
	-I$(top_srcdir)/lib		\
	$(WARN_CFLAGS)			\
	$(EPIPHANY_DEPENDENCY_CFLAGS)	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephywidgets.la

libephywidgets_la_SOURCES = \
	ephy-ellipsizing-label.c		\
	ephy-ellipsizing-label.h		\
	ephy-notebook.c				\
	ephy-notebook.h				\
	ephy-location-entry.c			\
	ephy-location-entry.h			\
	ephy-autocompletion-window.c		\
	ephy-autocompletion-window.h		\
	ephy-spinner.c				\
	ephy-spinner.h				\
	eggtreemultidnd.c			\
	eggtreemultidnd.h			\
	ephy-tree-model-sort.c			\
	ephy-tree-model-sort.h			\
	eggtreemodelfilter.c			\
	eggtreemodelfilter.h

SUBDIRS = mozilla

INCLUDES = \
	-I$(top_srcdir)/embed/mozilla     	\
	-I$(top_srcdir)/lib       		\
	-I$(top_srcdir)/lib/widgets      	\
	-I$(MOZILLA_INCLUDE_ROOT)/gtkembedmoz 	\
	$(WARN_CFLAGS)				\
	$(EPIPHANY_DEPENDENCY_CFLAGS) 		\
	-DSHARE_DIR=\"$(pkgdatadir)\" 		\
	-DG_DISABLE_DEPRECATED			\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED		\
	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephyembed.la

libephyembed_la_SOURCES = \
	downloader-view.c		\
	downloader-view.h		\
	ephy-embed.c			\
	ephy-embed.h			\
	ephy-embed-dialog.c		\
	ephy-embed-dialog.h		\
	ephy-embed-event.c		\
	ephy-embed-event.h		\
	ephy-embed-favicon.c		\
	ephy-embed-favicon.h		\
	ephy-embed-persist.c		\
	ephy-embed-persist.h		\
	ephy-embed-popup.c		\
	ephy-embed-popup.h		\
	ephy-embed-popup-bw.c		\
	ephy-embed-popup-bw.h		\
	ephy-embed-popup-control.c	\
	ephy-embed-popup-control.h	\
	ephy-embed-prefs.h		\
	ephy-embed-shell.c		\
	ephy-embed-shell.h		\
	ephy-embed-types.h		\
	ephy-embed-utils.c		\
	ephy-embed-utils.h		\
	ephy-favicon.c			\
	ephy-favicon.h			\
	ephy-favicon-cache.c		\
	ephy-favicon-cache.h		\
	ephy-history.c			\
	ephy-history.h			\
	find-dialog.c			\
	find-dialog.h			\
	print-dialog.c			\
	print-dialog.h

libephyembed_la_LIBADD = \
	$(top_builddir)/embed/mozilla/libephymozillaembed.la

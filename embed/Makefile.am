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
	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
	-DGNOME_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libephyembed.la

headerdir = $(prefix)/include/epiphany-@EPIPHANY_MAJOR@/epiphany
header_DATA = $(INST_H_FILES)

NOINST_H_FILES = \
	downloader-view.h		\
	ephy-download.h			\
	ephy-embed-dialog.h		\
	ephy-embed-popup-control.h	\
	ephy-embed-utils.h		\
	ephy-favicon-cache.h		\
	find-dialog.h			\
	print-dialog.h

INST_H_FILES = \
	ephy-embed.h			\
	ephy-embed-event.h		\
	ephy-embed-persist.h		\
	ephy-embed-prefs.h		\
	ephy-embed-single.h		\
	ephy-embed-shell.h		\
	ephy-embed-types.h		\
	ephy-history.h

libephyembed_la_SOURCES = \
	downloader-view.c		\
	ephy-download.c			\
	ephy-embed.c			\
	ephy-embed-dialog.c		\
	ephy-embed-event.c		\
	ephy-embed-persist.c		\
	ephy-embed-popup-control.c	\
	ephy-embed-single.c		\
	ephy-embed-shell.c		\
	ephy-embed-utils.c		\
	ephy-favicon-cache.c		\
	ephy-history.c			\
	find-dialog.c			\
	print-dialog.c			\
	$(INST_H_FILES)			\
	$(NOINST_H_FILES)

libephyembed_la_LIBADD = \
	$(top_builddir)/embed/mozilla/libephymozillaembed.la

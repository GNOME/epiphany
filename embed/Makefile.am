SUBDIRS = mozilla 

noinst_LTLIBRARIES = libephyembed.la libephyembedfactory.la

headerdir = $(prefix)/include/epiphany-@EPIPHANY_MAJOR@/epiphany
header_DATA = $(INST_H_FILES)

NOINST_H_FILES = \
	downloader-view.h		\
	ephy-download.h			\
	ephy-embed-dialog.h		\
	ephy-encodings.h		\
	ephy-favicon-cache.h		\
	find-dialog.h			\
	print-dialog.h

INST_H_FILES = \
	ephy-command-manager.h		\
	ephy-cookie-manager.h		\
	ephy-embed.h			\
	ephy-embed-event.h		\
	ephy-embed-factory.h		\
	ephy-embed-persist.h		\
	ephy-embed-prefs.h		\
	ephy-embed-single.h		\
	ephy-embed-shell.h		\
	ephy-history.h			\
	ephy-password-manager.h		\
	ephy-permission-manager.h

libephyembed_la_SOURCES = \
	downloader-view.c		\
	ephy-command-manager.c		\
	ephy-cookie-manager.c		\
	ephy-download.c			\
	ephy-embed.c			\
	ephy-embed-dialog.c		\
	ephy-embed-event.c		\
	ephy-embed-persist.c		\
	ephy-embed-single.c		\
	ephy-embed-shell.c		\
	ephy-encodings.c		\
	ephy-favicon-cache.c		\
	ephy-history.c			\
	ephy-password-manager.c		\
	ephy-permission-manager.c	\
	find-dialog.c			\
	print-dialog.c			\
	$(INST_H_FILES)			\
	$(NOINST_H_FILES)

libephyembed_la_CPPFLAGS = \
	-I$(top_builddir)/lib       	\
	-I$(top_srcdir)/embed/mozilla   \
	-I$(top_srcdir)/lib       	\
	-I$(top_srcdir)/lib/egg       	\
	-I$(top_srcdir)/lib/widgets     \
	$(AM_CPPFLAGS)

libephyembed_la_CFLAGS = \
	-I$(MOZILLA_INCLUDE_ROOT)/gtkembedmoz	\
	-DSHARE_DIR=\"$(pkgdatadir)\"	\
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	$(AM_CFLAGS)

libephyembedfactory_la_SOURCES =	\
	ephy-embed-factory.c		\
	ephy-embed-factory.h

libephyembedfactory_la_CPPFLAGS = \
	-I$(top_srcdir)/embed/mozilla		\
	-I$(MOZILLA_INCLUDE_ROOT)/gtkembedmoz	\
	$(AM_CPPFLAGS)

libephyembedfactory_la_CFLAGS = \
	$(EPIPHANY_DEPENDENCY_CFLAGS) 	\
	$(AM_CFLAGS)

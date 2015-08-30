AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

webextension_LTLIBRARIES = libephywebextension.la

webextensiondir = \
	$(libdir)/epiphany/$(EPIPHANY_MAJOR)/web-extensions

libephywebextension_la_SOURCES = \
	ephy-embed-form-auth.c \
	ephy-embed-form-auth.h \
	ephy-web-extension.c \
	ephy-web-extension.h \
	ephy-web-extension-main.c \
	ephy-web-extension-names.h \
	ephy-web-overview.h \
	ephy-web-overview.c \
	ephy-web-overview-model.h \
	ephy-web-overview-model.c \
	uri-tester.c \
	uri-tester.h

libephywebextension_la_CPPFLAGS = \
	-I$(top_srcdir)/lib \
	$(AM_CPPFLAGS)

libephywebextension_la_CFLAGS = \
	-DSHARE_DIR=\"$(pkgdatadir)\" \
	-DTOP_SRC_DATADIR=\"$(top_srcdir)/data\" \
	$(AM_CFLAGS) \
	$(WEB_EXTENSION_CFLAGS)

libephywebextension_la_LIBADD = \
	$(top_builddir)/lib/libephymisc.la \
	$(WEB_EXTENSION_LIBS)

libephywebextension_la_LDFLAGS = \
	-module -avoid-version -no-undefined

-include $(top_srcdir)/git.mk

webextension_LTLIBRARIES = libephywebextension.la

INCLUDES = \
	-I$(top_srcdir)/lib

webextensiondir = \
	$(libdir)/epiphany/$(EPIPHANY_MAJOR)/web-extensions

libephywebextension_la_SOURCES = \
	ephy-web-extension.c \
	ephy-web-extension.h \
	$(top_srcdir)/lib/ephy-web-dom-utils.c \
	$(top_srcdir)/lib/ephy-web-dom-utils.h

libephywebextension_la_CPPFLAG = \
	$(AM_CPPFLAGS)

libephywebextension_la_CFLAGS = \
	$(WEB_EXTENSION_CFLAGS)

libephywebextension_la_LIBADD = \
	$(WEB_EXTENSION_LIBS)

libephywebextension_la_LDFLAGS = \
	-module -avoid-version -no-undefined

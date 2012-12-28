webextension_LTLIBRARIES = libephywebextension.la

webextensiondir = \
	$(libdir)/epiphany/$(EPIPHANY_MAJOR)/web-extensions

libephywebextension_la_SOURCES = \
	ephy-web-extension.c \
	ephy-web-extension.h

libephywebextension_la_CPPFLAG = \
	$(AM_CPPFLAGS)

libephywebextension_la_CFLAGS = \
	$(WEB_EXTENSION_CFLAGS)

libephywebextension_la_LIBADD = \
	$(WEB_EXTENSION_LIBS)

libephywebextension_la_LDFLAGS = \
	-module -avoid-version -no-undefined

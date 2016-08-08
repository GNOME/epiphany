AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)

webextension_LTLIBRARIES = libephywebextension.la

webextensiondir = $(libdir)/epiphany/$(EPIPHANY_MAJOR)/web-extensions

libephywebextension_la_SOURCES = \
	ephy-embed-form-auth.c		\
	ephy-embed-form-auth.h		\
	ephy-uri-tester.c		\
	ephy-uri-tester.h		\
	ephy-web-dom-utils.c		\
	ephy-web-dom-utils.h		\
	ephy-web-extension.c		\
	ephy-web-extension.h		\
	ephy-web-extension-main.c	\
	ephy-web-extension-names.h	\
	ephy-web-overview.h		\
	ephy-web-overview.c		\
	ephy-web-overview-model.h	\
	ephy-web-overview-model.c

libephywebextension_la_CPPFLAGS = \
	-I$(top_srcdir)/lib	\
	$(AM_CPPFLAGS)

libephywebextension_la_CFLAGS = \
	$(GIO_CFLAGS)					\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(LIBSECRET_CFLAGS)				\
	$(LIBSOUP_CFLAGS)				\
	$(WEBKIT2GTK_WEB_EXTENSION_CFLAGS)		\
	$(AM_CFLAGS)

libephywebextension_la_LDFLAGS = \
	$(AM_LDFLAGS) -module -avoid-version -no-undefined

libephywebextension_la_LIBADD = \
	$(top_builddir)/lib/libephymisc.la \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBSECRET_LIBS) \
	$(LIBSOUP_LIBS) \
	$(WEBKIT2GTK_WEB_EXTENSION_LIBS)

-include $(top_srcdir)/git.mk

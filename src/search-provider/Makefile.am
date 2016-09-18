BUILT_SOURCES = \
	ephy-shell-search-provider-generated.c	\
	ephy-shell-search-provider-generated.h

dist_noinst_DATA = org.gnome.ShellSearchProvider2.xml

libexec_PROGRAMS = epiphany-search-provider

epiphany_search_provider_SOURCES = \
	ephy-search-provider.h		\
	ephy-search-provider.c		\
	ephy-search-provider-main.c	\
	$(BUILT_SOURCES)

epiphany_search_provider_CFLAGS = \
	$(WARN_CFLAGS)

epiphany_search_provider_CPPFLAGS = \
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/embed		\
	-I$(top_srcdir)/lib   		\
	-I$(top_srcdir)/lib/history	\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/bookmarks   \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	-DLOCALEDIR=\"$(localedir)\"	\
	$(GIO_CFLAGS)			\
	$(GLIB_CFLAGS)			\
	$(GTK_CFLAGS)			\
	$(LIBSECRET_CFLAGS)		\
	$(LIBSOUP_CFLAGS)		\
	$(LIBXML_CFLAGS)		\
	$(WEBKIT2GTK_CFLAGS)

epiphany_search_provider_LDADD = \
	$(top_builddir)/lib/libephymisc.la	\
	$(top_builddir)/src/libephymain.la	\
	$(GIO_LIBS)		\
	$(GLIB_LIBS)		\
	$(GTK_LIBS)		\
	$(LIBSECRET_LIBS)	\
	$(LIBSOUP_LIBS)		\
	$(LIBXML_LIBS)		\
	$(WEBKIT2GTK_LIBS)

epiphany_search_provider_LDFLAGS = \
	-dlopen self	\
	$(WARN_LDFLAGS)

# FIXME: Use a stamp file or something so it doesn't get run twice
#        https://www.gnu.org/software/automake/manual/automake.html#Multiple-Outputs
# The upstream for the DBus interface definition is
# at http://git.gnome.org/browse/gnome-shell/plain/data/org.gnome.ShellSearchProvider2.xml
$(BUILT_SOURCES) : Makefile.am $(srcdir)/org.gnome.ShellSearchProvider2.xml
	$(AM_V_GEN) gdbus-codegen							\
		--interface-prefix org.gnome.				\
		--c-namespace Ephy					\
		--generate-c-code ephy-shell-search-provider-generated	\
		$(srcdir)/org.gnome.ShellSearchProvider2.xml

CLEANFILES = $(BUILT_SOURCES)

pkglibexec_PROGRAMS = ephy-profile-migrator

ephy_profile_migrator_SOURCES = \
	ephy-profile-migrator.c

ephy_profile_migrator_CFLAGS = \
	$(WARN_CFLAGS)

ephy_profile_migrator_CPPFLAGS = \
	-I$(top_builddir)/lib				\
	-I$(top_srcdir)/embed				\
	-I$(top_srcdir)/lib   				\
	-I$(top_srcdir)/lib/history			\
	-I$(top_srcdir)/src/				\
	-I$(top_srcdir)/src/bookmarks			\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" 	\
	-DLOCALEDIR=\"$(localedir)\"			\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(LIBSECRET_CFLAGS)				\
	$(LIBSOUP_CFLAGS)				\
	$(LIBXML_CFLAGS)				\
	$(WEBKIT2GTK_CFLAGS)

ephy_profile_migrator_LDADD = \
	$(top_builddir)/lib/libephymisc.la	\
	$(top_builddir)/src/libephymain.la	\
	$(GLIB_LIBS)				\
	$(GTK_LIBS)				\
	$(LIBSECRET_LIBS)			\
	$(LIBSOUP_LIBS)				\
	$(LIBXML_LIBS)				\
	$(WEBKIT2GTK_LIBS)

ephy_profile_migrator_LDFLAGS = \
	$(WARN_LDFLAGS)

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk

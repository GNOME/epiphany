NULL =

noinst_PROGRAMS = \
	testephysearchentry \
	testephyiconentry \
	$(NULL)

testephysearchentry_SOURCES = \
	testephysearchentry.c \
	$(NULL)

testephysearchentry_CPPFLAGS = \
	-I$(top_srcdir)/lib/widgets \
	-I$(top_builddir)/lib/widgets \
	$(AM_CPPFLAGS)

testephysearchentry_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) \
	$(AM_CFLAGS)

testephysearchentry_LDADD = \
    $(top_builddir)/lib/widgets/libephywidgets.la \
    $(DEPENDENCIES_LIBS)

testephyiconentry_SOURCES = \
	testephyiconentry.c \
	$(NULL)

testephyiconentry_CPPFLAGS = \
	-I$(top_srcdir)/lib/widgets \
	-I$(top_builddir)/lib/widgets \
	$(AM_CPPFLAGS)

testephyiconentry_CFLAGS = \
	$(DEPENDENCIES_CFLAGS) \
	$(AM_CFLAGS)

testephyiconentry_LDADD = \
    $(top_builddir)/lib/widgets/libephywidgets.la \
    $(DEPENDENCIES_LIBS)

noinst_PROGRAMS = \
	testephysearchentry \
	testephylocationentry \
	testephyiconentry \
	testephyzoomcontrol

INCLUDES = \
	-I$(top_srcdir)/lib		 \
	-I$(top_srcdir)/lib/widgets

CFLAGS = \
	$(DEPENDENCIES_CFLAGS) \
	$(AM_CFLAGS)

LDADD = \
	$(DEPENDENCIES_LIBS) 

testephysearchentry_SOURCES = \
	testephysearchentry.c
testephysearchentry_LDADD = \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(DEPENDENCIES_LIBS) 

testephyiconentry_SOURCES = \
	testephyiconentry.c
testephyiconentry_LDADD = \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(DEPENDENCIES_LIBS) 

testephylocationentry_SOURCES = \
	testephylocationentry.c
testephylocationentry_LDADD = \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(top_builddir)/lib/libephymisc.la \
	$(DEPENDENCIES_LIBS) 

testephyzoomcontrol_SOURCES = \
	testephyzoomcontrol.c
testephyzoomcontrol_LDADD = \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/libephymisc.la \
	$(DEPENDENCIES_LIBS)

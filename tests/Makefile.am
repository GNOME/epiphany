noinst_PROGRAMS = \
	testephysearchentry \
	testephyiconentry

INCLUDES = \
	-I$(top_builddir)/lib/widgets

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


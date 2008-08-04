noinst_PROGRAMS = \
	testephysearchentry

testephysearchentry_SOURCES = \
	testephysearchentry.c
testephysearchentry_LDADD = \
    $(top_builddir)/lib/widgets/libephywidgets.la \
    $(DEPENDENCIES_LIBS) 

INCLUDES = \
	-I$(top_builddir)/lib/widgets

CFLAGS = \
	$(DEPENDENCIES_CFLAGS) \
	$(AM_CFLAGS)

LDADD = \
    $(DEPENDENCIES_LIBS) 

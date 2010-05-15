noinst_PROGRAMS = \
	testephyembedpersist \
	testephysearchentry \
	testephylocationentry \
	testephyzoomcontrol

INCLUDES = \
	-I$(top_srcdir)/embed    \
	-I$(top_srcdir)/lib      \
	-I$(top_srcdir)/lib/widgets \
	-I$(top_srcdir)/src      \
	-I$(top_srcdir)/src/bookmarks

CFLAGS = \
	$(DEPENDENCIES_CFLAGS) \
	$(AM_CFLAGS)

LDADD = \
	$(top_builddir)/src/libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(DEPENDENCIES_LIBS) 

if ENABLE_NSS
CFLAGS += \
	$(NSS_CFLAGS)
LDADD += \
	$(NSS_LIBS)
endif

if ENABLE_SEED
CFLAGS += \
	$(SEED_CFLAGS)
LDADD += \
	$(SEED_LIBS)
endif

testephyembedpersist_SOURCES = \
	testephyembedpersist.c

testephysearchentry_SOURCES = \
	testephysearchentry.c

testephylocationentry_SOURCES = \
	testephylocationentry.c

testephyzoomcontrol_SOURCES = \
	testephyzoomcontrol.c

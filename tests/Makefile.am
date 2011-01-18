noinst_PROGRAMS = \
	test-ephy-download \
	test-ephy-embed-single \
	test-ephy-location-entry \
	test-ephy-search-entry \
	test-ephy-zoom-control

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
	$(DBUS_LIBS) \
	$(DEPENDENCIES_LIBS) 

if ENABLE_SEED
CFLAGS += \
	$(SEED_CFLAGS)
LDADD += \
	$(SEED_LIBS)
endif

test_ephy_download_SOURCES = \
	ephy-download.c

test_ephy_embed_single_SOURCES = \
	ephy-embed-single.c

test_ephy_location_entry_SOURCES = \
	ephy-location-entry.c

test_ephy_search_entry_SOURCES = \
	ephy-search-entry.c

test_ephy_zoom_control_SOURCES = \
	ephy-zoom-control.c

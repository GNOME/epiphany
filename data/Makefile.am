SUBDIRS = icons

desktop_in_files = org.gnome.Epiphany.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_SERVER_RULE@
@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_XML_RULE@

gsettings_ENUM_NAMESPACE = org.gnome.Epiphany
gsettings_ENUM_FILES = $(top_srcdir)/lib/ephy-prefs.h

gsettings_SCHEMAS = org.gnome.epiphany.gschema.xml
@GSETTINGS_RULES@

appstream_in_files = org.gnome.Epiphany.appdata.xml.in
appstream_XML = $(appstream_in_files:.xml.in=.xml)
@APPSTREAM_XML_RULES@

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.EpiphanySearchProvider.service.in
service_DATA = $(service_in_files:.service.in=.service)
 
# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir[@]|$(libexecdir)|g" $< > $@

# Default bookmarks
# We don't put translations in the resulting rdf since the code can get
# the translations via gettext (although it can also get translations
# from the rdf if they are there and not available via gettext)
default_bookmarksdir = $(pkgdatadir)
default_bookmarks_in_files = default-bookmarks.rdf.in
default_bookmarks_DATA = $(default_bookmarks_in_files:.rdf.in=.rdf)
$(default_bookmarks_DATA): $(default_bookmarks_in_files) Makefile $(INTLTOOL_MERGE)
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@.tmp
	sed -e "s|\@pkgdatadir\@|$(pkgdatadir)|g" $@.tmp > $@ || rm -f $@.tmp
	rm -f $@.tmp

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = epiphany-search-provider.ini

dist_man_MANS = epiphany.1

EXTRA_DIST = \
	$(appstream_in_files)		\
	$(service_in_files)		\
	$(desktop_in_files)		\
	$(default_bookmarks_in_files)	\
	$(gsettings_SCHEMAS)		\
	epiphany-service.xml

CLEANFILES = \
	$(appstream_XML)		\
	$(service_DATA)			\
	$(desktop_DATA)			\
	$(default_bookmarks_DATA)

-include $(top_srcdir)/git.mk

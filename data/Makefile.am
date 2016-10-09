SUBDIRS = icons pages

desktop_in_files = org.gnome.Epiphany.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

gsettings_ENUM_NAMESPACE = org.gnome.Epiphany
gsettings_ENUM_FILES = $(top_srcdir)/lib/ephy-prefs.h

gsettings_SCHEMAS = org.gnome.epiphany.gschema.xml org.gnome.epiphany.host.gschema.xml
@GSETTINGS_RULES@

appstream_in_files = org.gnome.Epiphany.appdata.xml.in
appstream_XML = $(appstream_in_files:.xml.in=.xml)
@APPSTREAM_XML_RULES@

$(appstream_XML): $(appstream_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

aboutdialogdir = $(pkgdatadir)
dist_aboutdialog_DATA = about.ini

mimepermissiondir = $(pkgdatadir)
dist_mimepermission_DATA = mime-types-permissions.xml

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Epiphany.service.in
service_DATA = $(service_in_files:.service.in=.service)
 
# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir[@]|$(libexecdir)|g" $< > $@

# Default bookmarks
default_bookmarksdir = $(pkgdatadir)
default_bookmarks_in_files = default-bookmarks.rdf.in
default_bookmarks_DATA = $(default_bookmarks_in_files:.rdf.in=.rdf)
$(default_bookmarks_DATA): $(default_bookmarks_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@pkgdatadir[@]|$(pkgdatadir)|g" $< > $@

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = epiphany-search-provider.ini

EXTRA_DIST = \
	$(appstream_in_files)		\
	$(service_in_files)		\
	$(desktop_in_files)		\
	$(default_bookmarks_in_files)	\
	$(gsettings_SCHEMAS)		\
	epiphany-service.xml		\
	check-mime.py

CLEANFILES = \
	$(appstream_XML)		\
	$(service_DATA)			\
	$(desktop_DATA)			\
	$(default_bookmarks_DATA)

-include $(top_srcdir)/git.mk

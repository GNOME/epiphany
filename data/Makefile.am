SUBDIRS = art glade icons ui

@INTLTOOL_SERVER_RULE@
@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@
@INTLTOOL_XML_RULE@

desktop_in_files = epiphany.desktop.in.in bme.desktop.in.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = epiphany.schemas.in epiphany-lockdown.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

aboutdir = $(pkgdatadir)
about_DATA = epiphany.xhtml

aboutdialogdir = $(pkgdatadir)
aboutdialog_DATA = about.ini

mimepermissiondir = $(pkgdatadir)
mimepermission_DATA = mime-types-permissions.xml

m4datadir = $(datadir)/aclocal
m4data_DATA = epiphany.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = epiphany-$(EPIPHANY_API_VERSION).pc

xsldir = $(pkgdatadir)
xsl_DATA = \
	epiphany-bookmarks-html.xsl

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Epiphany.service.in
service_DATA = $(service_in_files:.service.in=.service)
 
# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

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

install-data-local: $(schema_DATA)
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $^ ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p 2>&1 > /dev/null; \
	done \
	fi
endif

EXTRA_DIST = \
	$(aboutdialog_DATA)		\
	$(service_in_files)		\
	$(schema_in_files)		\
	$(desktop_in_files)		\
	$(about_DATA)			\
	$(mimepermission_DATA)		\
	$(pkgconfig_DATA)		\
	$(xsl_DATA)			\
	$(service_DATA)			\
	$(m4data_DATA)			\
	$(default_bookmarks_in_files)	\
	epiphany-service.xml		\
	check-mime.py

DISTCLEANFILES = \
	$(service_DATA)		\
	$(schema_in_files:.schemas.in=.schemas)	\
	$(pkgconfig_DATA)	\
	$(desktop_DATA)		\
	$(default_bookmarks_DATA)

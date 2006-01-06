SUBDIRS = ui art glade

@INTLTOOL_SERVER_RULE@
@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@
@INTLTOOL_XML_RULE@

default_prefs_in_FILES = \
	default-prefs-common.js		\
	default-prefs-mozilla.js	\
	default-prefs-toolkit.js	\
	default-prefs-gecko-1-8.js

default_prefs_files = \
	default-prefs-common.js			\
	default-prefs-$(MOZILLA_FLAVOUR).js

if HAVE_GECKO_1_8
default_prefs_files += default-prefs-gecko-1-8.js
endif

default-prefs.js: $(default_prefs_files)
	cat $^ > $@

default_prefsdir = $(pkgdatadir)
default_prefs_DATA = default-prefs.js

desktop_in_files = epiphany.desktop.in.in bme.desktop.in.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

schemadir = $(GCONF_SCHEMA_FILE_DIR)
font_schema_files = epiphany-fonts.schemas
schema_in_files = epiphany.schemas.in epiphany-lockdown.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas) $(font_schema_files)

aboutdir = $(pkgdatadir)
about_DATA = epiphany.xhtml

mimepermissiondir = $(pkgdatadir)
mimepermission_DATA = mime-types-permissions.xml

m4datadir = $(datadir)/aclocal
m4data_DATA = epiphany.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = epiphany-1.10.pc

xsldir = $(pkgdatadir)
xsl_DATA = \
	epiphany-bookmarks-html.xsl	\
	ephy-xml2ini.xsl

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Epiphany.service.in
service_DATA = $(service_in_files:.service.in=.service)
 
# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@


install-data-local: $(schema_DATA)
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $^ ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p >&1 > /dev/null; \
	done \
	fi
endif

EXTRA_DIST = \
	$(server_in_files)		\
	$(schema_in_files)		\
	$(font_schema_files)		\
	$(desktop_in_files)		\
	$(about_DATA)			\
	$(mimepermission_DATA)		\
	$(pkgconfig_DATA)		\
	$(default_prefs_in_FILES)	\
	$(xsl_DATA)			\
	$(service_DATA)			\
	$(m4data_DATA)			\
	epiphany-service.xml		\
	check-mime.py			\
	generate-font-schemas.py

DISTCLEANFILES = \
	$(server_DATA)		\
	$(schema_DATA)		\
	$(pkgconfig_DATA)	\
	$(default_prefs_DATA)	\
	$(desktop_DATA)

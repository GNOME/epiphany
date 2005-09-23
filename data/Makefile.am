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

server_in_files = GNOME_Epiphany_Automation.server.in
server_DATA = GNOME_Epiphany_Automation.server
serverdir = $(libdir)/bonobo/servers

DESKTOP_IN_FILES= epiphany.desktop.in bme.desktop.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

applicationsdir = $(datadir)/application-registry
applications_DATA = epiphany.applications

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = epiphany.schemas.in epiphany-lockdown.schemas.in
schema_DATA = epiphany.schemas epiphany-lockdown.schemas

extensionschemadir = $(pkgdatadir)
extensionschema_DATA = epiphany-extension.xsd

aboutdir = $(pkgdatadir)
about_DATA = epiphany.xhtml

mimepermissiondir = $(pkgdatadir)
mimepermission_DATA = mime-types-permissions.xml

m4datadir=$(datadir)/aclocal
m4data_DATA=epiphany.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = epiphany-1.10.pc

xsldir = $(pkgdatadir)
xsl_DATA = epiphany-bookmarks-html.xsl

install-data-local:
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $(schema_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p >&1 > /dev/null; \
	done \
        fi
endif

EXTRA_DIST = \
	     $(server_in_files)     	\
	     $(schema_in_files)         \
	     $(DESKTOP_IN_FILES)	\
	     $(about_DATA)		\
	     $(applications_DATA)	\
	     $(mimepermission_DATA)	\
	     $(extensionschema_DATA)	\
	     $(pkgconfig_DATA)		\
	     $(default_prefs_in_FILES)	\
	     $(xsl_DATA)		\
	     $(m4data_DATA)		\
	     check-mime.py

DISTCLEANFILES = \
	$(server_DATA)		\
	$(schema_DATA)		\
	$(pkgconfig_DATA)	\
	$(default_prefs_DATA)	\
	$(DESKTOP_FILES)

SUBDIRS = ui art glade

@INTLTOOL_SERVER_RULE@
@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@

dist_pkgdata_DATA = default-prefs.js

NAUTILUS_VIEW_SERVER = GNOME_Epiphany_NautilusView.server.in

if ENABLE_NAUTILUS_VIEW
nautilus_server_in_files = $(NAUTILUS_VIEW_SERVER)
nautilus_server_DATA = GNOME_Epiphany_NautilusView.server
nautilus_serverdir = $(libdir)/bonobo/servers
endif

server_in_files = GNOME_Epiphany_Automation.server.in
server_DATA = GNOME_Epiphany_Automation.server
serverdir = $(libdir)/bonobo/servers

DESKTOP_IN_FILES= epiphany.desktop.in bme.desktop.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

applicationsdir = $(datadir)/application-registry
applications_DATA = epiphany.applications

schemadir = $(sysconfdir)/gconf/schemas
schema_in_files = epiphany.schemas.in epiphany-lockdown.schemas.in
schema_DATA = epiphany.schemas epiphany-lockdown.schemas

aboutdir = $(pkgdatadir)
about_DATA = epiphany.xhtml conspiracy.xhtml

mimepermissiondir = $(pkgdatadir)
mimepermission_DATA = mime-types-permissions.xml

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = epiphany-1.0.pc

install-data-local:
	if test -z "$(DESTDIR)" ; then \
	for p in $(schema_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p; \
	done \
        fi

EXTRA_DIST = $(NAUTILUS_VIEW_SERVER)	\
	     $(server_in_files)     	\
	     $(schema_in_files)         \
	     $(DESKTOP_IN_FILES)	\
	     $(about_DATA)		\
	     $(applications_DATA)	\
	     $(mimepermission_DATA)	\
	     $(pkgconfig_DATA)		\
	     check-mime.py

DISTCLEANFILES = \
	$(nautilus_server_DATA) \
	$(server_DATA)		\
	$(Applications_DATA)	\
	$(schema_DATA)		\
	$(pkgconfig_DATA)	\
	$(DESKTOP_FILES)

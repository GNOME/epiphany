SUBDIRS = ui art glade starthere

@INTLTOOL_SERVER_RULE@
@INTLTOOL_DESKTOP_RULE@
@INTLTOOL_SCHEMAS_RULE@

server_in_files = GNOME_Epiphany_Automation.server.in GNOME_Epiphany_NautilusView.server.in
server_DATA = GNOME_Epiphany_Automation.server GNOME_Epiphany_NautilusView.server
serverdir = $(libdir)/bonobo/servers

DESKTOP_IN_FILES=epiphany.desktop.in
DESKTOP_FILES=$(DESKTOP_IN_FILES:.desktop.in=.desktop)

Applicationsdir = $(datadir)/applications
Applications_DATA = $(DESKTOP_FILES)

schemadir = $(sysconfdir)/gconf/schemas
schema_in_files = epiphany.schemas.in
schema_DATA = epiphany.schemas

aboutdir = $(pkgdatadir)
about_DATA = epiphany.html

EXTRA_DIST = $(glade_DATA)

install-data-local:
	if test -z "$(DESTDIR)" ; then \
	for p in $(schema_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p; \
	done \
        fi

CLEANFILES = $(server_DATA) $(Applications_DATA) $(schema_DATA)

EXTRA_DIST = $(server_in_files)     	\
	     $(schema_in_files)         \
	     $(DESKTOP_IN_FILES)


SUBDIRS = ui art glade

@INTLTOOL_SERVER_RULE@
@INTLTOOL_DESKTOP_RULE@

server_in_files = GNOME_Epiphany_Automation.server.in GNOME_Epiphany_NautilusView.server.in
server_DATA = GNOME_Epiphany_Automation.server GNOME_Epiphany_NautilusView.server
serverdir = $(libdir)/bonobo/servers

schemadir = $(sysconfdir)/gconf/schemas
schema_DATA = epiphany.schemas

startheredir = $(pkgdatadir)
starthere_DATA = start_here.html

EXTRA_DIST = $(glade_DATA)

install-data-local:
	if test -z "$(DESTDIR)" ; then \
	for p in $(schema_DATA) ; do \
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p; \
	done \
        fi

CLEANFILES = GNOME_Epiphany_Automation.server

EXTRA_DIST = $(server_in_files)     \
	     $(schema_DATA)         \
	     $(starthere_DATA)

uixmldir = $(pkgdatadir)
uixml_in_files = epiphany-ui.xml.in           \
		 epiphany-bookmark-editor-ui.xml.in \
                 nautilus-epiphany-view.xml.in \
		 epiphany-toolbar.xml.in \
		 epiphany-toolbar-popup-ui.xml.in

uixml_DATA = $(uixml_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(uixml_in_files)

CLEANFILES = $(uixml_DATA)

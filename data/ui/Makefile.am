uixmldir = $(pkgdatadir)
uixml_in_files = nautilus-epiphany-view.xml.in
uixml_DATA = $(uixml_in_files:.xml.in=.xml)

xmldir = $(pkgdatadir)
xml_DATA = \
	epiphany-fs-toolbar.xml		\
	epiphany-bookmark-editor-ui.xml	\
	epiphany-ui.xml			\
	epiphany-toolbar.xml		\
	epiphany-history-window-ui.xml

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(uixml_in_files)

CLEANFILES = $(uixml_DATA)

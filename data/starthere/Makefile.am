@INTLTOOL_XML_RULE@

xmldir = $(pkgdatadir)/starthere
xml_in_files = index.xml.in smartbookmarks.xml.in
xml_DATA = $(xml_in_files:.xml.in=.xml)

pagedir = $(pkgdatadir)/starthere
page_DATA = section.xsl section.css

EXTRA_DIST = $(xml_DATA)	    	\
	     $(xml_in_files)		\
	     $(page_DATA)


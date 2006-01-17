manifestdir = $(pkgdatadir)/chrome
manifest_in_files = epiphany.manifest.in
manifest_DATA = $(manifest_in_files:.manifest.in=.manifest)

%.manifest: %.manifest.in
	@sed -e "s!@CHROMEDIR@!$(pkgdatadir)/chrome!g" $< > $@

brandingdir = $(manifestdir)/branding
branding_in_files = \
	brand.dtd.in		\
	brand.properties.in
branding_DATA = $(branding_in_files:%.in=%)

globalchromedir = $(manifestdir)/global
globalchrome_DATA = about.xhtml

EXTRA_DIST = \
	$(manifest_in_files)	\
	$(branding_in_files)	\
	$(globalchrome_DATA)

CLEANFILES = \
	$(manifest_DATA)

DISTCLEANFILES = \
	$(manifest_DATA)	\
	$(branding_DATA)

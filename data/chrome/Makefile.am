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

EXTRA_DIST = \
	$(manifest_in_files)	\
	$(branding_in_files)

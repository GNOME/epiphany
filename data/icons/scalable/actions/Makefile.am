themedir = $(pkgdatadir)/icons/hicolor
size = scalable
context = actions

iconsdir = $(themedir)/$(size)/$(context)

icons_DATA = \
	epiphany-download.svg


EXTRA_DIST =                            \
	$(icons_DATA)

MAINTAINERCLEANFILES =                  \
	Makefile.in

install-data-local: install-iconsDATA
	if  test -n "$(ICONMAP)" ; then \
		(cd $(DESTDIR)$(themedir)/$(size) && $(ICONMAP) -c $(context)) ; \
	fi

uninstall-am: uninstall-iconsDATA 
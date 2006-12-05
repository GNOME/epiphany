themedir = $(pkgdatadir)/icons/hicolor
size = 24x24
context = status

iconsdir = $(themedir)/$(size)/$(context)

icons_DATA = \
	popup-hidden.png


EXTRA_DIST =                            \
	$(icons_DATA)

MAINTAINERCLEANFILES =                  \
	Makefile.in

install-data-local: install-iconsDATA
	if  test -n "$(ICONMAP)" ; then \
		(cd $(DESTDIR)$(themedir)/$(size) && $(ICONMAP) -c $(context)) ; \
	fi

uninstall-am: uninstall-iconsDATA 
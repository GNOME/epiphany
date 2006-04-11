art_DATA = \
	epiphany-entry.png \
	epiphany-download.png \
	epiphany-history.png	\
	epiphany-popup-hidden.png \
	epiphany-bookmarks.png

appicon_DATA = epiphany-bookmarks.png
appicondir = $(datadir)/icons/hicolor/48x48/apps

artdir = $(pkgdatadir)/art

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating GTK+ icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = $(appicon_DATA) $(art_DATA)

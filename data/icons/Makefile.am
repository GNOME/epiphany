NULL =

public_icons_themes = \
	hicolor	\
	HighContrastLargePrint \
	HighContrastLargePrintInverse \
	LowContrastLargePrint \
	$(NULL)

public_icons = \
	hicolor_apps_16x16_gnome-web-browser.png \
	hicolor_apps_22x22_gnome-web-browser.png \
	hicolor_apps_24x24_gnome-web-browser.png \
	hicolor_apps_32x32_gnome-web-browser.png \
	hicolor_apps_scalable_gnome-web-browser.svg \
	HighContrastLargePrint_apps_48x48_gnome-web-browser.png \
	HighContrastLargePrintInverse_apps_48x48_gnome-web-browser.png \
	LowContrastLargePrint_apps_48x48_gnome-web-browser.png \
	$(NULL)

private_icons = \
	hicolor_actions_16x16_bookmark-view.png \
	hicolor_actions_16x16_history-view.png \
	hicolor_actions_16x16_location-entry.png \
	hicolor_actions_22x22_bookmark-view.png \
	hicolor_actions_22x22_history-view.png \
	hicolor_actions_22x22_location-entry.png \
	hicolor_actions_24x24_bookmark-view.png \
	hicolor_actions_24x24_history-view.png \
	hicolor_actions_24x24_location-entry.png \
	hicolor_actions_32x32_bookmark-view.png \
	hicolor_actions_32x32_history-view.png \
	hicolor_actions_32x32_location-entry.png \
	hicolor_actions_32x32_location-entry.svg \
	hicolor_actions_scalable_bookmark-view.svg \
	hicolor_actions_scalable_history-view.svg \
	hicolor_actions_scalable_location-entry.svg \
	hicolor_places_16x16_bookmark-web.png \
	hicolor_places_22x22_bookmark-web.png \
	hicolor_places_24x24_bookmark-web.png \
	hicolor_status_16x16_lock-broken.png \
	hicolor_status_16x16_lock-insecure.png \
	hicolor_status_16x16_lock-secure-checked.png \
	hicolor_status_16x16_lock-secure.png \
	hicolor_status_16x16_popup-hidden.png \
	hicolor_status_22x22_popup-hidden.png \
	hicolor_status_24x24_lock-broken.png \
	hicolor_status_24x24_lock-insecure.png \
	hicolor_status_24x24_lock-secure-checked.png \
	hicolor_status_24x24_lock-secure.png \
	hicolor_status_24x24_popup-hidden.png \
	hicolor_status_32x32_popup-hidden.png \
	hicolor_status_48x48_lock-broken.png \
	hicolor_status_48x48_lock-insecure.png \
	hicolor_status_48x48_lock-secure-checked.png \
	hicolor_status_48x48_lock-secure.png \
	hicolor_status_scalable_popup-hidden.svg \
	$(NULL)

noinst_DATA = \
	hicolor_actions_16x16_history-view.svg \
	hicolor_actions_22x22_history-view.svg \
	hicolor_actions_32x32_bookmark-view.svg \
	hicolor_actions_32x32_history-view.svg \
	hicolor_actions_16x16_bookmark-view.xcf \
	hicolor_actions_22x22_bookmark-view.xcf \
	hicolor_apps_16x16_gnome-web-browser.svg \
	hicolor_apps_22x22_gnome-web-browser.svg \
	hicolor_apps_32x32_gnome-web-browser.svg \
	hicolor_places_16x16_bookmark-web.svg \
	hicolor_places_22x22_bookmark-web.svg \
	hicolor_status_16x16_popup-hidden.xcf \
	hicolor_status_22x22_popup-hidden.xcf \
	hicolor_status_32x32_popup-hidden.svg \
	$(NULL)

EXTRA_DIST = \
	$(public_icons)		\
	$(private_icons)	\
	$(noinst_DATA)		\
	$(NULL)

###############################################################################

gtk_update_icon_cache = gtk-update-icon-cache -f -t

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		for theme in $(public_icons_themes); do \
			$(gtk_update_icon_cache) $(datadir)/icons/$$theme; \
		done; \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		for theme in $(public_icons_themes); do \
			echo "***   $(gtk_update_icon_cache) $(datadir)/icons/$$theme"; \
		done; \
	fi

install-icons:
	for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons update-icon-cache

uninstall-local: uninstall-icons update-icon-cache

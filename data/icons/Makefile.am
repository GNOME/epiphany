NULL =

private_icons = \
	hicolor_actions_16x16_bookmark-view.png \
	hicolor_actions_16x16_history-view.png \
	hicolor_actions_22x22_bookmark-view.png \
	hicolor_actions_22x22_history-view.png \
	hicolor_actions_24x24_bookmark-view.png \
	hicolor_actions_24x24_history-view.png \
	hicolor_actions_32x32_bookmark-view.png \
	hicolor_actions_32x32_history-view.png \
	hicolor_actions_scalable_bookmark-view.svg \
	hicolor_actions_scalable_history-view.svg \
	$(NULL)

noinst_DATA = \
	hicolor_actions_16x16_history-view.svg \
	hicolor_actions_22x22_history-view.svg \
	hicolor_actions_32x32_bookmark-view.svg \
	hicolor_actions_32x32_history-view.svg \
	hicolor_actions_16x16_bookmark-view.xcf \
	hicolor_actions_22x22_bookmark-view.xcf \
	$(NULL)

EXTRA_DIST = \
	$(private_icons)	\
	$(noinst_DATA)		\
	$(NULL)

###############################################################################

install-icons:
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons

uninstall-local: uninstall-icons

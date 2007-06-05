# The name of the module
DOC_MODULE=epiphany

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
DOC_SOURCE_DIR=../..

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref.
FIXXREF_OPTIONS=

# Used for dependencies.
HFILE_GLOB=$(top_srcdir)/lib/*.h
CFILE_GLOB=$(top_srcdir)/lib/*.c

# Header files to ignore when scanning.
IGNORE_HFILES = \
	config.h \
	eel-gconf-extensions.h \
	ephy-debug.h \
	ephy-dialog.h \
	ephy-dnd.h \
	ephy-file-chooser.h \
	ephy-file-helpers.h \
	ephy-gui.h \
	ephy-langs.h \
	ephy-marshal.h \
	ephy-node-common.h \
	ephy-node-filter.h \
	ephy-node.h \
	ephy-prefs.h \
	ephy-state.h \
	ephy-stock-icons.h \
	ephy-string.h \
	ephy-zoom.h \
	ephy-location-entry.h \
	ephy-node-view.h \
	ephy-search-entry.h \
	ephy-spinner.h \
	ephy-tree-model-node.h \
	ephy-tree-model-sort.h \
	ephy-zoom-action.h \
	ephy-zoom-control.h \
	downloader-view.h \
	ephy-download.h \
	ephy-embed-dialog.h \
	ephy-embed-event.h \
	ephy-embed-popup-control.h \
	ephy-embed-prefs.h \
	ephy-embed-shell.h \
	ephy-encodings.h \
	ephy-favicon-cache.h \
	ephy-history.h \
	find-dialog.h \
	print-dialog.h \
	ContentHandler.h \
	EphyAboutRedirector.h \
	EphyBrowser.h \
	EphyHeaderSniffer.h \
	EventContext.h \
	FilePicker.h \
	GlobalHistory.h \
	GtkNSSClientAuthDialogs.h \
	GtkNSSDialogs.h \
	GtkNSSKeyPairDialogs.h \
	MozDownload.h \
	mozilla-download.h \
	mozilla-embed.h \
	mozilla-embed-persist.h \
	mozilla-embed-single.h \
	mozilla-notifiers.h \
	MozillaPrivate.h \
	MozRegisterComponents.h \
	PrintingPromptService.h \
	egg-editable-toolbar.h \
	eggintl.h \
	eggmarshalers.h \
	eggstatusicon.h \
	egg-toolbar-editor.h \
	egg-toolbars-model.h \
	eggtrayicon.h \
	eggtraymanager.h \
	eggtreemultidnd.h \
	ephy-bookmark-action.h \
	ephy-bookmark-properties.h \
	ephy-bookmarks-editor.h \
	ephy-bookmarks-export.h \
	ephy-bookmarks.h \
	ephy-bookmarks-import.h \
	ephy-bookmarks-menu.h \
	ephy-new-bookmark.h \
	ephy-topic-action.h \
	ephy-topics-selector.h \
	ephy-automation.h \
	EphyAutomation.h \
	ephy-completion-model.h \
	ephy-encoding-dialog.h \
	ephy-encoding-menu.h \
	ephy-extension.h \
	ephy-favicon-action.h \
	ephy-favorites-menu.h \
	ephy-go-action.h \
	ephy-history-window.h \
	ephy-location-action.h \
	ephy-navigation-action.h \
	ephy-notebook.h \
	ephy-statusbar.h \
	ephy-tabs-menu.h \
	ephy-toolbars-model.h \
	pdm-dialog.h \
	popup-commands.h \
	ppview-toolbar.h \
	prefs-dialog.h \
	ephy-toolbar.h \
	ephy-fullscreen-popup.h \
	ephy-link.h \
	window-commands.h

# Images to copy into HTML directory.
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files =

# Other files to distribute.
extra_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = -I$(top_srcdir)/src \
		-I$(top_srcdir)/lib \
		-I$(top_builddir)/lib \
		-I$(top_srcdir)/lib/widgets \
		-I$(top_srcdir)/embed \
		-I$(top_builddir)/embed \
		-I$(top_srcdir)/embed/mozilla \
		-I$(top_srcdir)/src/bookmarks \
		-I$(top_srcdir)/lib/egg \
		-I$(top_builddir)/lib/egg \
		-I$(MOZILLA_INCLUDE_ROOT) \
		-I$(MOZILLA_INCLUDE_ROOT)/gtkembedmoz \
		$(DEPENDENCIES_CFLAGS) \
		$(GECKO_CFLAGS) \
		$(DBUS_CFLAGS) \
		$(INCINTL)

GTKDOC_LIBS = \
	$(top_builddir)/src/libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembedfactory.la \
	$(top_builddir)/embed/mozilla/libephymozillaembed.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(GECKO_LIBS)	\
	$(MOZILLA_EXTRA_LIBS) \
	$(MOZILLA_GLUE_LIBS) \
	$(DBUS_LIBS) \
	$(LIBINTL)

if ENABLE_PYTHON
GTKDOC_LIBS += \
	$(top_builddir)/src/libpyphany.la	\
	$(PYTHON_LIB_LOC) $(PYTHON_LIBS)	\
	$(PYTHON_EXTRA_LIBS)			\
	$(PYGTK_LIBS)
endif

# If you need to override some of the declarations, place them in the
# $(DOC_MODULE)-overrides.txt file and uncomment the second line here.
DOC_OVERRIDES =

# include common portion ...
# include $(top_srcdir)/gtk-doc.make
# We need to copy/paste this to be able to change LD_LIBRARY_PATH

if GTK_DOC_USE_LIBTOOL
GTKDOC_CC = $(LIBTOOL) --mode=compile $(CC) $(INCLUDES) $(AM_CFLAGS) $(CFLAGS)
GTKDOC_LD = $(LIBTOOL) --mode=link $(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS)
else
GTKDOC_CC = $(CC) $(INCLUDES) $(AM_CFLAGS) $(CFLAGS)
GTKDOC_LD = $(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS)
endif

# We set GPATH here; this gives us semantics for GNU make
# which are more like other make's VPATH, when it comes to
# whether a source that is a target of one rule is then
# searched for in VPATH/GPATH.
#
GPATH = $(srcdir)

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

EXTRA_DIST = 				\
	$(content_files)		\
	$(HTML_IMAGES)			\
	$(DOC_MAIN_SGML_FILE)		\
	$(DOC_MODULE).types		\
	$(DOC_MODULE)-sections.txt	\
	$(DOC_MODULE)-overrides.txt

DOC_STAMPS=scan-build.stamp tmpl-build.stamp sgml-build.stamp html-build.stamp \
	   $(srcdir)/tmpl.stamp $(srcdir)/sgml.stamp $(srcdir)/html.stamp

SCANOBJ_FILES = 		 \
	$(DOC_MODULE).args 	 \
	$(DOC_MODULE).hierarchy  \
	$(DOC_MODULE).interfaces \
	$(DOC_MODULE).prerequisites \
	$(DOC_MODULE).signals

CLEANFILES = $(SCANOBJ_FILES) $(DOC_MODULE)-scan.o $(DOC_MODULE)-unused.txt $(DOC_STAMPS)

if ENABLE_GTK_DOC
all-local: html-build.stamp

#### scan ####

scan-build.stamp: $(HFILE_GLOB)
	@echo '*** Scanning header files ***'
	@-chmod -R u+w $(srcdir)
	if grep -l '^..*$$' $(srcdir)/$(DOC_MODULE).types > /dev/null ; then \
	    LD_LIBRARY_PATH=$(MOZILLA_HOME):$$LD_LIBRARY_PATH CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" CFLAGS="$(GTKDOC_CFLAGS)" LDFLAGS="$(GTKDOC_LIBS)" gtkdoc-scangobj $(SCANGOBJ_OPTIONS) --module=$(DOC_MODULE) --output-dir=$(srcdir) ; \
	else \
	    cd $(srcdir) ; \
	    for i in $(SCANOBJ_FILES) ; do \
               test -f $$i || touch $$i ; \
	    done \
	fi
	cd $(srcdir) && \
	  gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" $(SCAN_OPTIONS) $(EXTRA_HFILES)
	touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES): scan-build.stamp
	@true

#### templates ####

tmpl-build.stamp: $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt
	@echo '*** Rebuilding template files ***'
	@-chmod -R u+w $(srcdir)
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)
	touch tmpl-build.stamp

tmpl.stamp: tmpl-build.stamp
	@true

#### xml ####

sgml-build.stamp: tmpl.stamp $(CFILE_GLOB) $(srcdir)/tmpl/*.sgml
	@echo '*** Building XML ***'
	@-chmod -R u+w $(srcdir)
	cd $(srcdir) && \
	gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --output-format=xml $(MKDB_OPTIONS)
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(DOC_MAIN_SGML_FILE) $(content_files)
	@echo '*** Building HTML ***'
	@-chmod -R u+w $(srcdir)
	rm -rf $(srcdir)/html 
	mkdir $(srcdir)/html
	cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	test "x$(HTML_IMAGES)" = "x" || ( cd $(srcdir) && cp $(HTML_IMAGES) html )
	@echo '-- Fixing Crossreferences' 
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)
	touch html-build.stamp
endif

##############

clean-local:
	rm -f *~ *.bak

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf xml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*`; \
	if test "$$installfiles" = '$(srcdir)/html/*'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR) || :; \
	fi)

uninstall-local:
	rm -f $(DESTDIR)$(TARGET_DIR)/*

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc dist-hook-local
	mkdir $(distdir)/tmpl
	mkdir $(distdir)/xml
	mkdir $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/xml/*.xml $(distdir)/xml
	-cp $(srcdir)/html/* $(distdir)/html

.PHONY : dist-hook-local

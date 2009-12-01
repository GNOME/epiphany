# The name of the module
DOC_MODULE=epiphany

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
DOC_SOURCE_DIR=../..

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS="--type-init-func=g_thread_init(NULL);g_type_init()"

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS=--sgml-mode --output-format=xml --source-suffixes=h,c

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
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
	ephy-node-view.h \
	ephy-tree-model-node.h \
	ephy-tree-model-sort.h \
	ephy-zoom-action.h \
	downloader-view.h \
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
GTKDOC_CFLAGS = -DEPIPHANY_COMPILATION \
		-I$(top_srcdir)/src \
		-I$(top_srcdir)/lib \
		-I$(top_builddir)/lib \
		-I$(top_srcdir)/lib/widgets \
		-I$(top_srcdir)/embed \
		-I$(top_builddir)/embed \
		-I$(top_srcdir)/src/bookmarks \
		-I$(top_srcdir)/lib/egg \
		-I$(top_builddir)/lib/egg \
		$(DEPENDENCIES_CFLAGS) \
		$(DBUS_CFLAGS) \
		$(INCINTL)

GTKDOC_LIBS = \
	$(top_builddir)/src/libephymain.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/widgets/libephywidgets.la \
	$(top_builddir)/lib/libephymisc.la \
	$(top_builddir)/lib/egg/libegg.la \
	$(DEPENDENCIES_LIBS) \
	$(DBUS_LIBS) \
	$(LIBINTL)

if ENABLE_SEED
GTKDOC_LIBS += \
	$(SEED_LIBS)
endif

# If you need to override some of the declarations, place them in the
# $(DOC_MODULE)-overrides.txt file and uncomment the second line here.
DOC_OVERRIDES =

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

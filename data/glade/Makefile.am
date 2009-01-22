gladedir = $(pkgdatadir)/glade
glade_in_files = \
	certificate-dialogs.glade	\
	epiphany.glade			\
	form-signing-dialog.glade	\
	prefs-dialog.glade		\
	print.glade

%.ui: %.glade
	$(GTK_BUILDER_CONVERT) $< $@

glade_DATA = $(glade_in_files:.glade=.ui)

CLEANFILES= $(glade_DATA)
EXTRA_DIST = $(glade_in_files)

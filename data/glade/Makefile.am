gladedir = $(pkgdatadir)/glade
glade_DATA = \
	certificate-dialogs.glade	\
	epiphany.glade			\
	form-signing-dialog.glade	\
	prefs-dialog.glade		\
	print.glade

if ENABLE_CERTIFICATE_MANAGER
glade_DATA += \
	certs-manager.glade
endif

EXTRA_DIST = $(glade_DATA) certs-manager.glade

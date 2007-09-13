noinst_LTLIBRARIES = libephyxulrunnerembed.la

libephyxulrunnerembed_la_SOURCES = 	\
	AutoJSContextStack.cpp		\
	AutoJSContextStack.h		\
	AutoWindowModalState.cpp	\
	AutoWindowModalState.h		\
	ContentHandler.cpp		\
	ContentHandler.h		\
	EphyAboutModule.cpp		\
	EphyAboutModule.h		\
	EphyBadCertRejector.cpp		\
	EphyBadCertRejector.h		\
	EphyContentPolicy.cpp		\
	EphyContentPolicy.h		\
	EphyDirectoryProvider.cpp       \
	EphyDirectoryProvider.h		\
	EphyHeaderSniffer.cpp		\
	EphyHeaderSniffer.h		\
	EphyBrowser.cpp                 \
	EphyBrowser.h	                \
	EphyFind.cpp			\
	EphyFind.h			\
	EphyHistoryListener.cpp		\
	EphyHistoryListener.h		\
	EphyPromptService.cpp		\
	EphyPromptService.h		\
	EphyRedirectChannel.cpp		\
	EphyRedirectChannel.h		\
	EphySidebar.cpp			\
	EphySidebar.h			\
	EphySingle.cpp			\
	EphySingle.h			\
	EphyUtils.cpp			\
	EphyUtils.h			\
	EventContext.cpp                \
	EventContext.h                  \
	GeckoCookiePromptService.cpp	\
	GeckoCookiePromptService.h	\
	GeckoFormSigningDialog.cpp	\
	GeckoFormSigningDialog.h	\
	GeckoPrintService.cpp		\
	GeckoPrintService.h		\
	GeckoPrintSession.cpp		\
	GeckoPrintSession.h		\
	GlobalHistory.cpp               \
	GlobalHistory.h                 \
	MozDownload.cpp			\
	MozDownload.h			\
	MozRegisterComponents.cpp       \
	MozRegisterComponents.h         \
	mozilla-download.cpp		\
	mozilla-download.h		\
	mozilla-embed.cpp	        \
	mozilla-embed.h			\
	mozilla-embed-event.cpp		\
	mozilla-embed-event.h		\
	mozilla-embed-find.cpp		\
	mozilla-embed-find.h		\
	mozilla-embed-persist.cpp       \
	mozilla-embed-persist.h		\
	mozilla-embed-single.cpp	\
	mozilla-embed-single.h	        \
	mozilla-notifiers.cpp		\
	mozilla-notifiers.h

# if ENABLE_FILEPICKER
# libephyxulrunnerembed_la_SOURCES += 	\
# 	FilePicker.cpp			\
# 	FilePicker.h
# endif

# if HAVE_MOZILLA_PSM
libephyxulrunnerembed_la_SOURCES += 	\
	GtkNSSClientAuthDialogs.cpp	\
	GtkNSSClientAuthDialogs.h	\
	GtkNSSDialogs.cpp		\
	GtkNSSDialogs.h			\
	GtkNSSKeyPairDialogs.cpp	\
	GtkNSSKeyPairDialogs.h		\
	GtkNSSSecurityWarningDialogs.cpp\
	GtkNSSSecurityWarningDialogs.h
# endif

# if ENABLE_CERTIFICATE_MANAGER
# libephyxulrunnerembed_la_SOURCES += \
# 	mozilla-x509-cert.cpp		\
# 	mozilla-x509-cert.h
# endif

# if ENABLE_SPELLCHECKER
libephyxulrunnerembed_la_SOURCES += \
	GeckoSpellCheckEngine.cpp	\
	GeckoSpellCheckEngine.h
# endif

libephyxulrunnerembed_la_CPPFLAGS = \
	-I$(top_srcdir)/lib	    		\
	-I$(top_srcdir)/embed	    		\
	-I$(top_srcdir)		    		\
	-I$(top_srcdir)/embed/xulrunner/src	\
	-I$(top_builddir)/embed/xulrunner/src	\
	-DSHARE_DIR=\"$(pkgdatadir)\"   	\
	-DPLUGINDIR=\"$(libdir)/epiphany/$(EPIPHANY_MAJOR)/plugins\"		\
	-DMOZILLA_HOME=\"$(LIBXUL_LIBDIR)\"	\
	-DMOZILLA_PREFIX=\"$(LIBXUL_PREFIX)\"	\
	-DMOZILLA_NATIVE_PLUGINSDIR=\"$(libdir)/mozilla/plugins\"		\
	-DUA_VERSION=\"$(EPIPHANY_UA_VERSION)\"	\
	-DALLOW_PRIVATE_API			\
	$(LIBXUL_CXXCPPFLAGS)			\
	$(LIBXUL_INCLUDES)			\
	$(AM_CPPFLAGS)

libephyxulrunnerembed_la_CXXFLAGS = \
	$(LIBXUL_CXXFLAGS)	\
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CXXFLAGS)

libephyxulrunnerembed_la_LIBADD = \
	$(top_builddir)/embed/xulrunner/src/libgnomegeckoembed.la

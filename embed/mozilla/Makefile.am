noinst_LTLIBRARIES = libephymozillaembed.la

libephymozillaembed_la_SOURCES = 	\
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
	MozillaPrivate.cpp		\
	MozillaPrivate.h		\
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

if ENABLE_FILEPICKER
libephymozillaembed_la_SOURCES += 	\
	FilePicker.cpp			\
	FilePicker.h
endif

if HAVE_MOZILLA_PSM
libephymozillaembed_la_SOURCES += 	\
	GtkNSSClientAuthDialogs.cpp	\
	GtkNSSClientAuthDialogs.h	\
	GtkNSSDialogs.cpp		\
	GtkNSSDialogs.h			\
	GtkNSSKeyPairDialogs.cpp	\
	GtkNSSKeyPairDialogs.h		\
	GtkNSSSecurityWarningDialogs.cpp\
	GtkNSSSecurityWarningDialogs.h
endif

if ENABLE_CERTIFICATE_MANAGER
libephymozillaembed_la_SOURCES += \
	mozilla-x509-cert.cpp		\
	mozilla-x509-cert.h
endif

if ENABLE_SPELLCHECKER
libephymozillaembed_la_SOURCES += \
	GeckoSpellCheckEngine.cpp	\
	GeckoSpellCheckEngine.h
endif

mozilla_include_subdirs = \
	.			\
	caps			\
	chardet			\
	chrome			\
	commandhandler		\
	content			\
	cookie			\
	docshell		\
	dom			\
	exthandler		\
	fastfind		\
	helperAppDlg		\
	find			\
	gfx			\
	gtkembedmoz		\
	history			\
	js			\
	layout			\
	locale			\
	mimetype		\
	necko			\
	nkcache			\
	passwordmgr		\
	pipboot			\
	pipnss			\
	pref			\
	shistory		\
	sidebar			\
	spellchecker		\
	uriloader		\
	uconv			\
	wallet			\
	webbrowserpersist	\
	webbrwsr		\
	widget			\
	windowwatcher		\
	xmlextras		\
	xpcom			\
	xpconnect

libephymozillaembed_la_CPPFLAGS = \
	-I$(top_srcdir)/lib	    		\
	-I$(top_srcdir)/embed	    		\
	-I$(top_srcdir)		    		\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(mozilla_include_subdirs))	\
	-DSHARE_DIR=\"$(pkgdatadir)\"   	\
	-DPLUGINDIR=\"$(libdir)/epiphany/$(EPIPHANY_MAJOR)/plugins\"		\
	-DMOZILLA_HOME=\"$(MOZILLA_HOME)\"	\
	-DMOZILLA_PREFIX=\"$(MOZILLA_PREFIX)\"	\
	-DUA_VERSION=\"$(EPIPHANY_UA_VERSION)\"	\
	-DALLOW_PRIVATE_API			\
	$(AM_CPPFLAGS)

libephymozillaembed_la_CXXFLAGS = \
	$(GECKO_CFLAGS)		\
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CXXFLAGS)

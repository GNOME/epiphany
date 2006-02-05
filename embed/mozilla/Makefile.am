noinst_LTLIBRARIES = libephymozillaembed.la

libephymozillaembed_la_SOURCES = 	\
	AutoJSContextStack.cpp		\
	AutoJSContextStack.h		\
	ContentHandler.cpp		\
	ContentHandler.h		\
	EphyAboutModule.cpp		\
	EphyAboutModule.h		\
	EphyContentPolicy.cpp		\
	EphyContentPolicy.h		\
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
	GlobalHistory.cpp               \
	GlobalHistory.h                 \
	MozDownload.cpp			\
	MozDownload.h			\
	MozillaPrivate.cpp		\
	MozillaPrivate.h		\
	MozRegisterComponents.cpp       \
	MozRegisterComponents.h         \
	PrintingPromptService.cpp	\
	PrintingPromptService.h		\
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

if HAVE_GECKO_1_8
libephymozillaembed_la_SOURCES += \
	EphyBadCertRejector.cpp		\
	EphyBadCertRejector.h

if HAVE_MOZILLA_TOOLKIT
libephymozillaembed_la_SOURCES += \
	EphyDirectoryProvider.cpp	\
	EphyDirectoryProvider.h
endif

endif

mozilla_include_subdirs = \
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
	shistory		\
	sidebar			\
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

# Grr, damn gentoo has a dodgy header layout
# http://bugs.gentoo.org/show_bug.cgi?id=100804
mozilla_include_subdirs += \
	find			\
	pref

libephymozillaembed_la_CPPFLAGS = \
	-I$(top_srcdir)/lib	    		\
	-I$(top_srcdir)/embed	    		\
	-I$(top_srcdir)		    		\
	-I$(MOZILLA_INCLUDE_ROOT)		\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(mozilla_include_subdirs))	\
	-DSHARE_DIR=\"$(pkgdatadir)\"   	\
	-DPLUGINDIR=\"$(libdir)/epiphany/$(EPIPHANY_MAJOR)/plugins\"		\
	-DMOZILLA_HOME=\"$(MOZILLA_HOME)\"	\
	-DMOZILLA_PREFIX=\"$(MOZILLA_PREFIX)\"	\
	-DEPIPHANY_MAJOR=\"$(EPIPHANY_MAJOR)\"	\
	-DALLOW_PRIVATE_API			\
	$(AM_CPPFLAGS)

libephymozillaembed_la_CXXFLAGS = \
	$(GECKO_CFLAGS)		\
	$(DEPENDENCIES_CFLAGS) 	\
	$(AM_CXXFLAGS)

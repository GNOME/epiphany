INCLUDES = \
	-I$(top_srcdir)/lib	    			\
	-I$(top_srcdir)/embed	    			\
	-I$(top_srcdir)		    			\
	$(MOZILLA_COMPONENT_CFLAGS)			\
	-I$(MOZILLA_INCLUDE_ROOT)			\
	-I$(MOZILLA_INCLUDE_ROOT)/caps			\
	-I$(MOZILLA_INCLUDE_ROOT)/chardet		\
	-I$(MOZILLA_INCLUDE_ROOT)/commandhandler	\
	-I$(MOZILLA_INCLUDE_ROOT)/content		\
	-I$(MOZILLA_INCLUDE_ROOT)/cookie		\
	-I$(MOZILLA_INCLUDE_ROOT)/docshell		\
	-I$(MOZILLA_INCLUDE_ROOT)/dom			\
	-I$(MOZILLA_INCLUDE_ROOT)/exthandler		\
	-I$(MOZILLA_INCLUDE_ROOT)/gfx			\
	-I$(MOZILLA_INCLUDE_ROOT)/helperAppDlg		\
	-I$(MOZILLA_INCLUDE_ROOT)/js			\
	-I$(MOZILLA_INCLUDE_ROOT)/history		\
	-I$(MOZILLA_INCLUDE_ROOT)/layout		\
	-I$(MOZILLA_INCLUDE_ROOT)/locale		\
	-I$(MOZILLA_INCLUDE_ROOT)/mimetype		\
	-I$(MOZILLA_INCLUDE_ROOT)/necko			\
	-I$(MOZILLA_INCLUDE_ROOT)/nkcache		\
	-I$(MOZILLA_INCLUDE_ROOT)/passwordmgr		\
	-I$(MOZILLA_INCLUDE_ROOT)/pipboot		\
	-I$(MOZILLA_INCLUDE_ROOT)/pipnss		\
	-I$(MOZILLA_INCLUDE_ROOT)/shistory		\
	-I$(MOZILLA_INCLUDE_ROOT)/uriloader		\
	-I$(MOZILLA_INCLUDE_ROOT)/uconv			\
	-I$(MOZILLA_INCLUDE_ROOT)/webbrowserpersist	\
	-I$(MOZILLA_INCLUDE_ROOT)/webbrwsr		\
	-I$(MOZILLA_INCLUDE_ROOT)/widget		\
	-I$(MOZILLA_INCLUDE_ROOT)/windowwatcher		\
	-I$(MOZILLA_INCLUDE_ROOT)/xmlextras		\
	-I$(MOZILLA_INCLUDE_ROOT)/xpcom			\
	-I$(MOZILLA_INCLUDE_ROOT)/xpconnect		\
	$(EPIPHANY_DEPENDENCY_CFLAGS) 			\
	-DSHARE_DIR=\"$(pkgdatadir)\"   		\
	-DMOZILLA_HOME=\"$(MOZILLA_HOME)\"		\
	-DMOZILLA_PREFIX=\"$(MOZILLA_PREFIX)\"		\
	-DALLOW_PRIVATE_API

if MAINTAINER_MODE
INCLUDES += -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DBONOBO_DISABLE_DEPRECATED $(MOZILLA_WARN_CXXFLAGS)
endif

noinst_LTLIBRARIES = libephymozillaembed.la

libephymozillaembed_la_SOURCES = 	\
	ContentHandler.cpp		\
	ContentHandler.h		\
	EphyAboutRedirector.cpp		\
	EphyAboutRedirector.h		\
	EphyContentPolicy.cpp		\
	EphyContentPolicy.h		\
	EphyHeaderSniffer.cpp		\
	EphyHeaderSniffer.h		\
	EphyBrowser.cpp                 \
	EphyBrowser.h	                \
	EphySingle.cpp			\
	EphySingle.h			\
	EphyUtils.cpp			\
	EphyUtils.h			\
	EventContext.cpp                \
	EventContext.h                  \
	GlobalHistory.cpp               \
	GlobalHistory.h                 \
	GtkNSSClientAuthDialogs.cpp	\
	GtkNSSClientAuthDialogs.h	\
	GtkNSSDialogs.cpp		\
	GtkNSSDialogs.h			\
	GtkNSSKeyPairDialogs.cpp	\
	GtkNSSKeyPairDialogs.h		\
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

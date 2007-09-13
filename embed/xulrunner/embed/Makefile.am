noinst_LTLIBRARIES = libephyxulrunnerembed.la

libephyxulrunnerembed_la_SOURCES = 	\
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
	EphySingle.cpp			\
	EphySingle.h			\
	EphyUtils.cpp			\
	EphyUtils.h			\
	EventContext.cpp                \
	EventContext.h                  \
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

# if ENABLE_CERTIFICATE_MANAGER
# libephyxulrunnerembed_la_SOURCES += \
# 	mozilla-x509-cert.cpp		\
# 	mozilla-x509-cert.h
# endif

libephyxulrunnerembed_la_CPPFLAGS = \
	-I$(top_srcdir)/lib	    		\
	-I$(top_srcdir)/embed	    		\
	-I$(top_srcdir)/embed/xulrunner/src	\
	-I$(top_srcdir)/embed/xulrunner/utils	\
	-I$(top_srcdir)/embed/xulrunner/components	\
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
	$(top_builddir)/embed/xulrunner/utils/libephyxulrunnerutils.la	\
	$(top_builddir)/embed/xulrunner/src/libgnomegeckoembed.la

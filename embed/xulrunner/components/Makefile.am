NULL =

noinst_LTLIBRARIES = libephycomponents.la

libephycomponents_la_SOURCES = \
	ContentHandler.cpp		\
	ContentHandler.h		\
	EphyAboutModule.cpp		\
	EphyAboutModule.h		\
	EphyContentPolicy.cpp		\
	EphyContentPolicy.h		\
	EphyHistoryListener.cpp		\
	EphyHistoryListener.h		\
	EphyRedirectChannel.cpp		\
	EphyRedirectChannel.h		\
	EphySidebar.cpp			\
	EphySidebar.h			\
	GeckoCookiePromptService.cpp	\
	GeckoCookiePromptService.h	\
	GeckoFormSigningDialog.cpp	\
	GeckoFormSigningDialog.h	\
	GeckoPrintService.cpp		\
	GeckoPrintService.h		\
	GeckoPrintSession.cpp		\
	GeckoPrintSession.h		\
	GeckoPromptService.cpp		\
	GeckoPromptService.h		\
	GlobalHistory.cpp               \
	GlobalHistory.h                 \
	MozDownload.cpp			\
	MozDownload.h			\
	MozRegisterComponents.cpp       \
	MozRegisterComponents.h         \
	$(NULL)

# if ENABLE_FILEPICKER
# libephycomponents_la_SOURCES += 	\
# 	FilePicker.cpp			\
# 	FilePicker.h
# endif

# NOTE & FIXME: Most of these are GPL not LGPL
# if HAVE_MOZILLA_PSM
libephycomponents_la_SOURCES += 	\
	GtkNSSClientAuthDialogs.cpp	\
	GtkNSSClientAuthDialogs.h	\
	GtkNSSDialogs.cpp		\
	GtkNSSDialogs.h			\
	GtkNSSKeyPairDialogs.cpp	\
	GtkNSSKeyPairDialogs.h		\
	GtkNSSSecurityWarningDialogs.cpp\
	GtkNSSSecurityWarningDialogs.h
# endif

# if ENABLE_SPELLCHECKER
libephycomponents_la_SOURCES += \
	GeckoSpellCheckEngine.cpp	\
	GeckoSpellCheckEngine.h
# endif

libephycomponents_la_CPPFLAGS = \
	-I$(top_srcdir)/lib	    		\
	-I$(top_srcdir)/embed	    		\
	-I$(top_srcdir)		    		\
	-I$(top_srcdir)/embed/xulrunner/src	\
	-I$(top_srcdir)/embed/xulrunner/embed	\
	-I$(top_srcdir)/embed/xulrunner/utils	\
	-I$(top_builddir)/embed/xulrunner/src	\
	-I$(top_builddir)/embed/xulrunner/embed	\
	-DSHARE_DIR=\"$(pkgdatadir)\"   	\
	-DPLUGINDIR=\"$(libdir)/epiphany/$(EPIPHANY_MAJOR)/plugins\"		\
	-DMOZILLA_HOME=\"$(LIBXUL_LIBDIR)\"	\
	-DMOZILLA_PREFIX=\"$(LIBXUL_PREFIX)\"	\
	-DMOZILLA_NATIVE_PLUGINSDIR=\"$(libdir)/mozilla/plugins\"		\
	-DUA_VERSION=\"$(EPIPHANY_UA_VERSION)\"	\
	-DALLOW_PRIVATE_API			\
	$(LIBXUL_CXXCPPFLAGS)	\
	$(LIBXUL_INCLUDES)	\
	$(AM_CPPFLAGS)

libephycomponents_la_CXXFLAGS = \
	$(LIBXUL_CXXFLAGS)	\
	$(GTK_CFLAGS)		\
	$(GTKPRINT_CFLAGS)	\
	$(GNOMEVFS_CFLAGS)	\
	$(GCONF_CFLAGS)		\
	$(GLADE_CFLAGS)		\
	$(GNOME_CFLAGS)		\
	$(AM_CXXFLAGS)

libephycomponents_la_LDFLAGS = \
	$(AM_LDFLAGS)

libephycomponents_la_LIBADD = \
	$(top_builddir)/embed/xulrunner/utils/libephyxulrunnerutils.la	\
	$(LIBXUL_LIBS)

CLEANFILES =
	$(NULL)

EXTRA_DIST = \
	$(NULL)

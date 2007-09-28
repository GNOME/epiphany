NULL =

noinst_LTLIBRARIES = libephyxulrunnerutils.la

libephyxulrunnerutils_la_SOURCES = \
	AutoJSContextStack.cpp		\
	AutoJSContextStack.h		\
	AutoWindowModalState.cpp	\
	AutoWindowModalState.h		\
	EphyBadCertRejector.cpp		\
	EphyBadCertRejector.h		\
	EphyUtils.cpp			\
	EphyUtils.h			\
	GeckoUtils.cpp			\
	GeckoUtils.h			\
	ggeAutoModalDialog.cpp		\
	ggeAutoModalDialog.h		\
	ggeAutoSurvivalArea.cpp		\
	ggeAutoSurvivalArea.h		\
	$(NULL)

libephyxulrunnerutils_la_CPPFLAGS = \
	-I$(top_srcdir)/embed/xulrunner/src	\
	$(LIBXUL_CXXCPPFLAGS)	\
	$(LIBXUL_INCLUDES)	\
	$(AM_CPPFLAGS)

libephyxulrunnerutils_la_CXXFLAGS = \
	$(LIBXUL_CXXFLAGS)	\
	$(GTK_CFLAGS)		\
	$(AM_CXXFLAGS)

libephyxulrunnerutils_la_LDFLAGS = \
	$(AM_LDFLAGS)

libephyxulrunnerutils_la_LIBADD = \
	$(LIBXUL_LIBS)

CLEANFILES =
	$(NULL)

EXTRA_DIST = \
	$(NULL)

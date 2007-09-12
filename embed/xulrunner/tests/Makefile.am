bin_PROGRAMS = testgeckoembed

NULL =

testgeckoembed_SOURCES = \
	testgeckoembed.cpp

testgeckoembed_CPPFLAGS = \
	-I$(top_srcdir)				\
	-I$(top_srcdir)/embed/xulrunner/src	\
	-I$(top_builddir)/embed/xulrunner/src	\
	$(LIBXUL_CXXCPPFLAGS)			\
	$(LIBXUL_INCLUDES)			\
	-DGECKO_HOME=\"$(LIBXUL_PREFIX)\"	\
	$(AM_CPPFLAGS)

testgeckoembed_CXXFLAGS = \
	$(LIBXUL_CXXFLAGS)	\
	$(GTK_CFLAGS)		\
	$(AM_CXXFLAGS)

testgeckoembed_LDFLAGS = \
	-R$(LIBXUL_LIBDIR)	\
	$(AM_LDFLAGS)

testgeckoembed_LDADD = \
	$(top_builddir)/embed/xulrunner/src/libgnomegeckoembed-0.0.la \
	$(LIBXUL_LIBS)	\
	$(GTK_LIBS)	\
	$(NULL)

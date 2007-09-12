defsdir = $(datadir)/pygtk/2.0/defs
defs_DATA = gnomegeckoembed.defs

pkgpythondir = $(pyexecdir)/gtk-2.0
pkgpyexecdir = $(pyexecdir)/gtk-2.0

# gnomegeckoembed module
pkgpyexec_LTLIBRARIES = gnomegeckoembed.la

gnomegeckoembed_la_CPPFLAGS = \
	-I$(top_srcdir)				\
	-I$(top_srcdir)/embed/xulrunner/src	\
	-I$(top_builddir)/embed/xulrunner/src	\
	$(AM_CPPFLAGS)

gnomegeckoembed_la_CFLAGS = \
	$(GTK_CFLAGS)			\
	$(NO_STRICT_ALIASING_CFLAGS)	\
	$(PYGTK_CFLAGS)			\
	$(PYTHON_CFLAGS)		\
	$(AM_CFLAGS)

gnomegeckoembed_la_LIBADD = \
	$(top_builddir)/embed/xulrunner/src/libgnomegeckoembed-0.0.la	\
	$(GTK_LIBS)		\
	$(PYTHON_LIBS)		\
	$(PYTHON_EXTRA_LIBS)	\
	$(PYGTK_LIBS)

gnomegeckoembed_la_LDFLAGS = \
	-module -avoid-version	\
	-export-symbols-regex initgnomegeckoembed \
	-R$(GECKO_HOME)	\
	$(AM_LDFLAGS)

gnomegeckoembed_la_SOURCES = gnomegeckoembedmodule.c
nodist_gnomegeckoembed_la_SOURCES = gnomegeckoembed.c

gnomegeckoembed.c: gnomegeckoembed.defs gnomegeckoembed.override
CLEANFILES = gnomegeckoembed.c
EXTRA_DIST = gnomegeckoembed.override gnomegeckoembed.defs

.defs.c:
	(cd $(srcdir)\
	 && $(PYGTK_CODEGEN) \
	    --override $*.override \
	    --prefix py$* $*.defs) > gen-$*.c \
	&& cp gen-$*.c $*.c \
	&& rm -f gen-$*.c

BINDING_HEADERS_SRCDIR_IN = \
	src/gecko-embed.h \
	src/gecko-embed-single.h \
	src/gecko-embed-types.h

BINDING_HEADERS_BUILDDIR_IN =

BINDING_HEADERS_SRCDIR      := $(addprefix $(top_srcdir)/,$(BINDING_HEADERS_SRCDIR_IN))
BINDING_HEADERS_BUILDDIR    := $(addprefix $(top_builddir)/,$(BINDING_HEADERS_BUILDDIR_IN))

regenerate-python-binding:
	$(PYGTK_H2DEF) $(sort $(BINDING_HEADERS_SRCDIR) $(BINDING_HEADERS_BUILDDIR)) > gnomegeckoembed.defs.new

EGGSOURCES = \
	eggtreemultidnd.c \
	egg-editable-toolbar.c \
	egg-toolbars-model.c \
	egg-toolbar-editor.c \
	eggstatusicon.c \
	eggtrayicon.c \
	eggtraymanager.c

EGGHEADERS = \
	eggtreemultidnd.h \
	egg-editable-toolbar.h \
	egg-toolbars-model.h \
	egg-toolbar-editor.h \
	eggstatusicon.h \
	eggtrayicon.h \
	eggtraymanager.h

noinst_HEADERS = \
	$(EGGHEADERS) \
	eggmarshalers.h \
	eggintl.h

noinst_LTLIBRARIES = libegg.la

libegg_la_SOURCES = \
	$(EGGSOURCES) \
	eggmarshalers.c

libegg_la_CPPFLAGS = \
	-I$(top_builddir)/lib/egg		\
	-DCURSOR_DIR=\"$(pkgdatadir)/art\"	\
	$(AM_CPPFLAGS)

libegg_la_CFLAGS = \
	$(EPIPHANY_DEPENDENCY_CFLAGS)		\
	$(AM_CFLAGS)

BUILT_SOURCES = eggmarshalers.h eggmarshalers.c

eggmarshalers.h: eggmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_egg_marshal $(srcdir)/eggmarshalers.list --header > eggmarshalers.h

eggmarshalers.c: eggmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_egg_marshal $(srcdir)/eggmarshalers.list --header --body > eggmarshalers.c

EXTRA_DIST=			\
	eggmarshalers.list

EGGFILES=$(EGGSOURCES) $(EGGHEADERS)
EGGDIR=$(srcdir)/../../../libegg/libegg

regenerate-built-sources:
	EGGFILES="$(EGGFILES) eggmarshalers.list" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh

CLEANFILES = $(BUILT_SOURCES)
DISTCLEAN_FILES = $(BUILT_SOURCES)

INCLUDES = \
  -I$(top_builddir)/lib/egg \
  $(EPIPHANY_DEPENDENCY_CFLAGS) \
  -DCURSOR_DIR=\"$(pkgdatadir)/art\" \
  -DGTK_DISABLE_DEPRECATED \
  -DGDK_DISABLE_DEPRECATED \
  -DG_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libegg.la

EGGSOURCES = \
  eggtreemultidnd.c \
  egg-editable-toolbar.c \
  egg-toolbars-model.c \
  egg-toolbar-editor.c \
  eggstatusicon.c \
  eggtrayicon.c \
  eggtraymanager.c

libegg_la_SOURCES = \
  $(EGGSOURCES) \
  eggmarshalers.c

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

eggmarshalers.h: eggmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_egg_marshal $(srcdir)/eggmarshalers.list --header > eggmarshalers.h

eggmarshalers.c: eggmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_egg_marshal $(srcdir)/eggmarshalers.list --body > eggmarshalers.c

EXTRA_DIST=			\
	eggmarshalers.list

EGGFILES=$(EGGSOURCES) $(EGGHEADERS)
EGGDIR=$(srcdir)/../../../libegg/libegg

regenerate-built-sources:
	EGGFILES="$(EGGFILES) eggmarshalers.list" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh


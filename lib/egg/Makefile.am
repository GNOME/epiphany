INCLUDES = \
  $(EPIPHANY_DEPENDENCY_CFLAGS) \
  -DGTK_DISABLE_DEPRECATED \
  -DGDK_DISABLE_DEPRECATED \
  -DG_DISABLE_DEPRECATED

noinst_LTLIBRARIES = libegg.la

libegg_la_SOURCES = \
  egg-action.c \
  egg-action-group.c \
  egg-toggle-action.c \
  egg-radio-action.c \
  egg-markup.c \
  egg-menu-merge.c \
  egg-accel-dialog.c \
  eggradiotoolbutton.c \
  eggtoggletoolbutton.c \
  eggtoolitem.c	\
  eggseparatortoolitem.c \
  eggtoolbar.c \
  eggtoolbutton.c \
  eggmarshalers.c

noinst_HEADERS = \
  egg-menu.h \
  egg-action.h \
  egg-action-group.h \
  egg-toggle-action.h \
  egg-radio-action.h \
  egg-markup.h \
  egg-menu-merge.h \
  egg-accel-dialog.h \
  eggradiotoolbutton.h \
  eggtoggletoolbutton.h \
  eggtoolitem.h	\
  eggseparatortoolitem.h \
  eggtoolbar.h \
  eggtoolbutton.h

eggmarshalers.h:
	cd $(srcdir) \
	&& $(GLIB_GENMARSHAL) --prefix=_egg_marshal eggmarshalers.list --header > xgen-emh \
	&& cp xgen-emh eggmarshalers.h \
	&& rm -f xgen-emh xgen-emh~

eggmarshalers.c:
	cd $(srcdir) \
	&& $(GLIB_GENMARSHAL) --prefix=_egg_marshal eggmarshalers.list --body > xgen-emc \
	&& cp xgen-emc eggmarshalers.c \
	&& rm -f xgen-emc xgen-emc~

egg-marshal.c: eggmarshalers.h eggmarshalers.c

noinst_HEADERS = \
	eggmarshalers.h

EXTRA_DIST=			\
	eggmarshalers.list

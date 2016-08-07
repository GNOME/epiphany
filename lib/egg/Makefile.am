noinst_LTLIBRARIES = libegg.la

libegg_la_SOURCES = \
	eggtreemultidnd.c	\
	eggtreemultidnd.h

libegg_la_CFLAGS = $(GTK_CFLAGS)

libegg_la_CPPFLAGS = -DG_LOG_DOMAIN=\""Egg"\"

libegg_la_LIBADD = $(GTK_LIBS)

-include $(top_srcdir)/git.mk

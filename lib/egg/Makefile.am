NULL =

EGGSOURCES = \
	eggtreemultidnd.c \
        $(NULL)

EGGHEADERS = \
	eggtreemultidnd.h \
        $(NULL)

noinst_HEADERS = \
	$(EGGHEADERS) \
	$(NULL)

noinst_LTLIBRARIES = libegg.la

libegg_la_SOURCES = \
	$(EGGSOURCES) \
	$(EGGHEADERS)

libegg_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Egg"\"		\
	$(AM_CPPFLAGS)

libegg_la_CFLAGS = \
	$(DEPENDENCIES_CFLAGS)		\
	$(CODE_COVERAGE_CFLAGS)		\
	$(AM_CFLAGS)


-include $(top_srcdir)/git.mk

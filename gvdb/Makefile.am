AM_CPPFLAGS = \
    $(BUILD_CFLAGS) \
    $(GVDB_CFLAGS)

noinst_LTLIBRARIES = libgvdb.la

libgvdb_la_SOURCES = \
    gvdb-builder.c \
    gvdb-reader.c

noinst_HEADERS = \
    gvdb-builder.h \
    gvdb-format.h \
    gvdb-reader.h

libgvdb_la_LIBADD = \
    $(BUILD_LIBS) \
    $(GVDB_LIBS)

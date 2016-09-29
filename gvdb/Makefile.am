AM_CPPFLAGS = \
    $(BUILD_CFLAGS) \
    $(GVDB_CFLAGS)

noinst_LTLIBRARIES = libgvdb.la

libgvdb_la_SOURCES = \
    gvdb/gvdb-builder.c \
    gvdb/gvdb-reader.c

noinst_HEADERS = \
    gvdb/gvdb-builder.h \
    gvdb/gvdb-format.h \
    gvdb/gvdb-reader.h

libgvdb_la_LIBADD = \
    $(BUILD_LIBS) \
    $(GVDB_LIBS)

## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = 1.6

# The name of the module
DOC_MODULE=epiphany

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
DOC_SOURCE_DIR=../..

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref.
FIXXREF_OPTIONS=

# Used for dependencies.
HFILE_GLOB=$(top_srcdir)/lib/*.h
CFILE_GLOB=$(top_srcdir)/lib/*.c

# Header files to ignore when scanning.
IGNORE_HFILES= BaseProtocolContentHandler.h BaseProtocolHandler.h ContentHandler.h EphyAboutRedirector.h EphyBrowser.h EphyEventListener.h EphyHeaderSniffer.h EventContext.h ExternalProtocolHandlers.h ExternalProtocolService.h FilePicker.h GlobalHistory.h GtkNSSClientAuthDialogs.h GtkNSSDialogs.h GtkNSSKeyPairDialogs.h MozDownload.h mozilla-download.h mozilla-embed.h mozilla-embed-persist.h mozilla-embed-single.h mozilla-notifiers.h MozillaPrivate.h MozRegisterComponents.h nsUnicharUtils.h PrintingPromptService.h

# Images to copy into HTML directory.
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files =

# Other files to distribute.
extra_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = -I$(top_srcdir)/src \
		-I$(top_srcdir)/lib \
		-I$(top_srcdir)/lib/widgets \
		-I$(top_srcdir)/embed \
		-I$(top_srcdir)/embed/mozilla \
		-I$(top_srcdir)/src/bookmarks \
		$(EPIPHANY_DEPENDENCY_CFLAGS)

GTKDOC_LIBS = \
	$(top_builddir)/embed/libephyembed.la \
	$(top_builddir)/lib/libephy.la \
	$(top_builddir)/src/bookmarks/libephybookmarks.la \
	$(EPIPHANY_DEPENDENCY_LIBS)

# If you need to override some of the declarations, place them in the
# $(DOC_MODULE)-overrides.txt file and uncomment the second line here.
DOC_OVERRIDES =

# include common portion ...
include $(top_srcdir)/gtk-doc.make


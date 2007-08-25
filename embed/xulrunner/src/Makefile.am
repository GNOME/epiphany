NULL =

required = \
	.			\
	accessibility		\
	content			\
	embedcomponents		\
	docshell		\
	dom			\
	embed_base		\
	gfx			\
	intl			\
	js			\
	layout			\
	necko			\
	profdirserviceprovider	\
	shistory		\
	string			\
	uriloader		\
	webbrwsr		\
	webshell		\
	widget			\
	windowwatcher		\
	xpcom			\
	xpconnect		\
	xulapp			\
	$(NULL)

MARSHALERS = gecko-embed-marshal.h gecko-embed-marshal.cpp
TYPES_SOURCES = gecko-embed-type-builtins.h gecko-embed-type-builtins.cpp

BUILT_SOURCES= $(MARSHALERS) $(TYPES_SOURCES)

gecko-embed-marshal.cpp: gecko-embed-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gecko_embed_marshal $< --header --body > $@

gecko-embed-marshal.h: gecko-embed-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gecko_embed_marshal $< --header > $@

TYPES_H_FILES = \
	gecko-embed-types.h	\
	$(NULL)

stamp_files = \
	stamp-gecko-embed-type-builtins.cpp	\
	stamp-gecko-embed-type-builtins.h	\
	$(NULL)

gecko-embed-type-builtins.cpp: stamp-gecko-embed-type-builtins.cpp Makefile
	@true
stamp-gecko-embed-type-builtins.cpp: Makefile $(TYPES_H_FILES)
	$(GLIB_MKENUMS) \
		--fhead "#include \"gecko-embed-type-builtins.h\"\n\n" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--fprod "\n#include \"@filename@\"" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n" \
		--vhead "  static GType type = 0;\n\n" \
		--vhead "  if (G_UNLIKELY (type == 0))\n  {\n" \
		--vhead "    static const G@Type@Value _@enum_name@_values[] = {" \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n\n" \
		--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n  }\n\n" \
		--vtail "  return type;\n}\n\n" \
		$(filter-out $<,$^) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

gecko-embed-type-builtins.h: stamp-gecko-embed-type-builtins.h Makefile
	@true
stamp-gecko-embed-type-builtins.h: Makefile $(TYPES_H_FILES)
	$(GLIB_MKENUMS) \
		--fhead "#ifndef GECKO_TYPE_BUILTINS_H\n" \
		--fhead "#define GECKO_TYPE_BUILTINS_H 1\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* GECKO_TYPE_BUILTINS_H */\n" \
		--fprod "\n/* --- @filename@ --- */" \
		--eprod "#define GECKO_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
		--eprod "GType @enum_name@_get_type (void);\n" \
		$(filter-out $<,$^) > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $(@F:stamp-%=%) || cp xgen-$(@F) $(@F:stamp-%=%) ) \
	&& rm -f xgen-$(@F) \
	&& echo timestamp > $(@F)

lib_LTLIBRARIES = libgnomegeckoembed-0.0.la

libgnomegeckoembed_0_0_la_SOURCES = \
	$(MARSHALERS)				\
	$(TYPES_SOURCES)			\
	gecko-dom-event.cpp			\
	gecko-dom-event.h			\
	gecko-dom-event-internal.h		\
	gecko-dom-event-private.h		\
	gecko-embed.cpp				\
	gecko-embed.h				\
	gecko-embed-private.h			\
	gecko-embed-signals.h			\
	gecko-embed-single.cpp			\
	gecko-embed-single.h			\
	gecko-embed-single-private.h		\
	gecko-embed-types.h			\
	gecko-init.cpp               		\
	gecko-init.h               	 	\
	gecko-init-internal.h      	 	\
	gecko-init-private.h       	 	\
	AutoJSContextStack.cpp			\
	AutoJSContextStack.h			\
	AutoWindowModalState.cpp		\
	AutoWindowModalState.h			\
	EmbedContentListener.cpp		\
	EmbedContentListener.h			\
	EmbedEventListener.cpp			\
	EmbedEventListener.h			\
	EmbedProgress.cpp			\
	EmbedProgress.h				\
	EmbedWindow.cpp				\
	EmbedWindowCreator.cpp			\
	EmbedWindowCreator.h			\
	EmbedWindow.h				\
	GeckoBrowser.cpp			\
	GeckoBrowser.h				\
	GeckoPromptService.cpp			\
	GeckoPromptService.h			\
	GeckoSingle.cpp				\
	GeckoSingle.h				\
	GeckoUtils.cpp				\
	GeckoUtils.h				\
	$(NULL)

libgnomegeckoembed_0_0_la_CPPFLAGS = \
	$(addprefix -I$(GECKO_INCLUDE_ROOT)/,$(required)) \
	$(AM_CPPFLAGS)

#	-DDEBUG				\
#	-DXPCOM_GLUE
#-DMOZILLA_STRICT_API

libgnomegeckoembed_0_0_la_CXXFLAGS = \
	$(GGE_DEPENDENCY_CFLAGS)	\
	$(GECKO_CFLAGS)			\
	$(AM_CXXFLAGS)

libgnomegeckoembed_0_0_la_LIBADD = \
	$(GGE_DEPENDENCY_LIBS)	\
	$(GECKO_LIBS)			\
	$(GECKO_EXTRA_LIBS)		\
	$(GECKO_GLUE_LIBS)

libgnomegeckoembed_0_0_la_LDFLAGS = \
	$(AM_LDFLAGS)

gnomegeckoembedincludedir = $(includedir)/gnome-gecko-embed-0.0
gnomegeckoembedinclude_HEADERS =

gnomegeckoembedsubincludedir = $(includedir)/gnome-gecko-embed-0.0/gecko-embed
gnomegeckoembedsubinclude_HEADERS =

CLEANFILES = \
	$(stamp_files)		\
	$(BUILT_SOURCES)	\
	$(NULL)

EXTRA_DIST = \
	gecko-embed-marshal.list	\
	$(NULL)

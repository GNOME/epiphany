#!/usr/bin/env python
# -*- coding: UTF-8 -*-
#
#  Copyright (C) 2005 Christian Persch
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#  $Id$

import sys, codecs
from xml import dom;

sys.stdout = codecs.getwriter("utf-8")(sys.__stdout__)

def appendTextNode(doc, schemaNode, tag, text):
	node = doc.createElement(tag)
	schemaNode.appendChild(node)
	textNode = doc.createTextNode(text)
	node.appendChild(textNode)


def appendLocaleNode(doc, schemaNode, localeName, shortSchemaText):
	localeNode = doc.createElement("locale")
	localeNode.setAttribute("name", localeName)
	schemaNode.appendChild(localeNode)

	appendTextNode(doc, localeNode, "short", shortSchemaText)
	appendTextNode(doc, localeNode, "long", shortSchemaText)


def append_schema(doc, schemalistNode, key, datatype, default, description, schemaText):
	schemaNode = doc.createElement("schema")
	schemalistNode.appendChild(schemaNode)
	appendTextNode (doc, schemaNode, "key", "/schemas" + key)
	appendTextNode (doc, schemaNode, "applyto", key)
	appendTextNode (doc, schemaNode, "owner", "epiphany")
	appendTextNode (doc, schemaNode, "type", datatype)
	appendTextNode (doc, schemaNode, "default", default)
	appendLocaleNode (doc, schemaNode, "C", schemaText)


def append_schemas(docNode, schemalistNode, group):
	base = "/apps/epiphany/web/"
	append_schema(doc, schemalistNode, base + "fixed_font_size_" + group,
		      "int", "10", "Monospace font size",
		      "Monospaced font size for \"" + group + "\"")
	append_schema(doc, schemalistNode, base + "font_monospace_" + group,
		      "string", "monospace", "Monospace font",
		      "Monospaced font for \"" + group + "\"")
	append_schema(doc, schemalistNode, base + "variable_font_size_" + group,
		      "int", "10", "Proportional font size",
		      "Variable width font size for \"" + group + "\"")
	append_schema(doc, schemalistNode, base + "font_variable_" + group,
		      "string", "sans-serif", "Proportional font",
		      "Variable width font for \"" + group + "\"")
	append_schema(doc, schemalistNode, base + "minimum_font_size_" + group,
		      "int", "7", "Minimum font size",
		      "Minimum font size for \"" + group + "\"")


# keep this list in sync with lib/ephy-langs.c
font_languages = [
	"ar",
	"el",
	"he",
	"ja",
	"ko",
	"th",
	"tr",
	"x-armn",
	"x-baltic",
	"x-beng",
	"x-cans",
	"x-central-euro",
	"x-cyrillic",
	"x-devanagari",
	"x-ethi",
	"x-geor",
	"x-gujr",
	"x-guru",
	"x-khmr",
	"x-mlym",
	"x-tamil",
	"x-unicode",
	"x-western",
	"zh-CN",
	"zh-HK",
	"zh-TW" ]

doc = dom.getDOMImplementation().createDocument(None, "gconfschemafile", None)

schemalistNode = doc.createElement("schemalist")
doc.documentElement.appendChild(schemalistNode)

for lang in font_languages:
	append_schemas(doc, schemalistNode, lang)

doc.writexml(sys.stdout)

# Remember to pass the output through "xmllint --format"

/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ephy-bookmarks-export.h"
#include "ephy-node-common.h"
#include "ephy-file-helpers.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#include <libxml/globals.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

static inline xmlChar *
sanitise_string (const xmlChar *string)
{
	xmlChar *copy, *p;

	if (!string)
		return xmlStrdup ((const xmlChar *) "");

	/* http://www.w3.org/TR/REC-xml/#sec-well-formed :
	   Character Range
	   [2]	   Char	      ::=	   #x9 | #xA | #xD | [#x20-#xD7FF] |
	   [#xE000-#xFFFD] | [#x10000-#x10FFFF]
	   any Unicode character, excluding the surrogate blocks, FFFE, and FFFF.
	*/

	copy = xmlStrdup (string);
	for (p = copy; *p; p++)
	{
		xmlChar c = *p;
		if (G_UNLIKELY (c < 0x20 && c != 0xd && c != 0xa && c != 0x9)) {
			*p = 0x20;
		}
	}

	return copy;
}

static int
write_topics_list (EphyNode *topics,
		   EphyNode *bmk,
		   xmlTextWriterPtr writer)
{
	GPtrArray *children;
	GList *keywords = NULL, *l;
	int i;
	int ret = 0;

	children = ephy_node_get_children (topics);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		int priority;

		kid = g_ptr_array_index (children, i);

		priority = ephy_node_get_property_int (kid, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority == -1) priority = EPHY_NODE_NORMAL_PRIORITY;

		if (priority == EPHY_NODE_NORMAL_PRIORITY &&
		    ephy_node_has_child (kid, bmk))
		{
			keywords = g_list_prepend (keywords, kid);
		}
	}

	for (l = keywords; l != NULL; l = l->next)
	{
		EphyNode *node = l->data;
		const char *name;
		xmlChar *safeName;

		name = ephy_node_get_property_string
			(node, EPHY_NODE_KEYWORD_PROP_NAME);
		safeName = sanitise_string ((const xmlChar *) name);

		ret = xmlTextWriterWriteElementNS
			(writer, 
			 (xmlChar *) "dc",
			 (xmlChar *) "subject",
			 NULL,
			 safeName);
		xmlFree (safeName);

		if (ret < 0) break;
	}

	g_list_free (keywords);

	return ret >= 0 ? 0 : -1;
}

static int
write_rdf (EphyBookmarks *bookmarks,
	   GFile *file,
	   xmlTextWriterPtr writer)
{
	EphyNode *bmks, *topics, *smart_bmks;
	GPtrArray *children;
	char *file_uri;
	int i, ret;
	xmlChar *safeString;
#ifdef ENABLE_ZEROCONF
	EphyNode *local;
#endif

	START_PROFILER ("Writing RDF")

	ret = xmlTextWriterStartDocument (writer, "1.0", NULL, NULL);
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElementNS
		(writer,
		 (xmlChar *) "rdf",
		 (xmlChar *) "RDF",
		 (xmlChar *) "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteAttribute
		(writer,
		 (xmlChar *) "xmlns",
		 (xmlChar *) "http://purl.org/rss/1.0/");
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteAttributeNS
		(writer, 
		 (xmlChar *) "xmlns",
		 (xmlChar *) "dc",
		 NULL,
		 (xmlChar *) "http://purl.org/dc/elements/1.1/");
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteAttributeNS
		(writer,
		 (xmlChar *) "xmlns",
		 (xmlChar *) "ephy",
		 NULL,
		 (xmlChar *) "http://gnome.org/ns/epiphany#");
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElement (writer, (xmlChar *) "channel");
	if (ret < 0) goto out;

	/* FIXME: sanitise file_uri? */
	file_uri = g_file_get_uri (file);
	safeString = sanitise_string ((const xmlChar *) file_uri);
	g_free (file_uri);

	ret = xmlTextWriterWriteAttributeNS
		(writer,
		 (xmlChar *) "rdf",
		 (xmlChar *) "about",
		 NULL,
		 safeString);
	xmlFree (safeString);
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteElement 
		(writer,
		 (xmlChar *) "title",
		 (xmlChar *) "Web bookmarks");
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteElement
		(writer,
		 (xmlChar *) "link",
		 (xmlChar *) "https://wiki.gnome.org/Apps/Web");
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElement (writer, (xmlChar *) "items");
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElementNS
		(writer,
		 (xmlChar *) "rdf", 
		 (xmlChar *) "Seq",
		 NULL);
	if (ret < 0) goto out;

	bmks = ephy_bookmarks_get_bookmarks (bookmarks);
	topics = ephy_bookmarks_get_keywords (bookmarks);
	smart_bmks = ephy_bookmarks_get_smart_bookmarks (bookmarks);
#ifdef ENABLE_ZEROCONF
	local = ephy_bookmarks_get_local (bookmarks);
#endif

	children = ephy_node_get_children (bmks);
	for (i=0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *url;
		char *link = NULL;
		gboolean smart_url;
		xmlChar *safeLink;

		kid = g_ptr_array_index (children, i);

#ifdef ENABLE_ZEROCONF
		/* Don't export the local bookmarks */
		if (ephy_node_has_child (local, kid)) continue;
#endif

		smart_url = ephy_node_has_child (smart_bmks, kid);
		url = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_LOCATION);
		if (smart_url && url)
		{
			char *scheme;
			char *host_name;
			
			scheme = g_uri_parse_scheme (url);
			host_name = ephy_string_get_host_name (url);
			link = g_strconcat (scheme,
					    "://",
					    host_name,
					    NULL);

			g_free (scheme);
			g_free (host_name);
		}

		safeLink = sanitise_string (link ? (const xmlChar *) link : (const xmlChar *) url);
		g_free (link);

		ret = xmlTextWriterStartElementNS
			(writer,
			 (xmlChar *) "rdf",
			 (xmlChar *) "li",
			 NULL);
		if (ret < 0) break;

		ret = xmlTextWriterWriteAttributeNS
			(writer,
			 (xmlChar *) "rdf",
			 (xmlChar *) "resource",
			 NULL,
			 safeLink);
		xmlFree (safeLink);
		if (ret < 0) break;

		ret = xmlTextWriterEndElement (writer); /* rdf:li */
		if (ret < 0) break;
	}
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* rdf:Seq */
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* items */
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* channel */
	if (ret < 0) goto out;
	
	children = ephy_node_get_children (bmks);
	for (i=0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *url, *title;
		char *link = NULL;
		gboolean smart_url;
		xmlChar *safeLink, *safeTitle;

		kid = g_ptr_array_index (children, i);

#ifdef ENABLE_ZEROCONF
		/* Don't export the local bookmarks */
		if (ephy_node_has_child (local, kid)) continue;
#endif

		smart_url = ephy_node_has_child (smart_bmks, kid);
		url = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_LOCATION);
		title = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_TITLE);

		if (smart_url && url)
		{
			char *scheme;
			char *host_name;

			scheme = g_uri_parse_scheme (url);
			host_name = ephy_string_get_host_name (url);

			link = g_strconcat (scheme,
					    "://",
					    host_name,
					    NULL);
			g_free (scheme);
			g_free (host_name);
		}

		if (link == NULL)
		{
			link = g_strdup (url);
		}

		ret = xmlTextWriterStartElement (writer, (xmlChar *) "item");
		if (ret < 0) break;

		safeLink = sanitise_string ((const xmlChar *) link);
		g_free (link);

		ret = xmlTextWriterWriteAttributeNS
			(writer,
			 (xmlChar *) "rdf",
			 (xmlChar *) "about",
			 NULL,
			 safeLink);
		if (ret < 0)
		{
			xmlFree (safeLink);
			break;
		}

		safeTitle = sanitise_string ((const xmlChar *) title);
		ret = xmlTextWriterWriteElement
			(writer,
			 (xmlChar *) "title",
			 safeTitle);
		xmlFree (safeTitle);
		if (ret < 0) break;

		ret = xmlTextWriterWriteElement
			(writer,
			 (xmlChar *) "link",
			 safeLink);
		xmlFree (safeLink);
		if (ret < 0) break;

		if (smart_url)
		{
			xmlChar *safeSmartLink;

			safeSmartLink = sanitise_string ((const xmlChar *) url);
			ret = xmlTextWriterWriteElementNS
				(writer,
				 (xmlChar *) "ephy",
				 (xmlChar *) "smartlink",
				 NULL,
				 safeSmartLink);
			xmlFree (safeSmartLink);
			if (ret < 0) break;
		}

		ret = write_topics_list (topics, kid, writer);
		if (ret < 0) break;

		ret = xmlTextWriterEndElement (writer); /* item */
	}
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* rdf:RDF */
	if (ret < 0) goto out;

	ret = xmlTextWriterEndDocument (writer);

out:
	STOP_PROFILER ("Writing RDF")

	return ret;
}

void
ephy_bookmarks_export_rdf (EphyBookmarks *bookmarks,
			   const char *file_path)
{
	xmlTextWriterPtr writer;
	xmlBufferPtr buf;
	GFile *file;
	int ret;

	LOG ("Exporting as RDF to %s", file_path);

	START_PROFILER ("Exporting as RDF")

	buf = xmlBufferCreate ();
	if (buf == NULL)
	{
		return;
	}
	/* FIXME: do we want to turn on compression here? */
	writer = xmlNewTextWriterMemory (buf, 0);
	if (writer == NULL)
	{
		xmlBufferFree (buf);
		return;
	}

	ret = xmlTextWriterSetIndent (writer, 1);
	if (ret < 0) goto out;

	ret = xmlTextWriterSetIndentString (writer, (xmlChar *) "  ");
	if (ret < 0) goto out;
	
	file = g_file_new_for_path (file_path);
	ret = write_rdf (bookmarks, file, writer);
	g_object_unref (file);
	if (ret < 0) goto out;

	xmlFreeTextWriter (writer);
out:
	if (ret >= 0)
	{
		if (g_file_set_contents (file_path,
					 (const char *)buf->content,
					 buf->use,
					 NULL) == FALSE)
		{
			ret = -1;
		}
	}

	xmlBufferFree (buf);

	STOP_PROFILER ("Exporting as RDF")

	LOG ("Exporting as RDF %s.", ret >= 0 ? "succeeded" : "FAILED");
}

void
ephy_bookmarks_export_mozilla (EphyBookmarks *bookmarks,
			       const char *filename)
{
	xsltStylesheetPtr cur = NULL;
	xmlTextWriterPtr writer;
	xmlDocPtr doc = NULL, res;
	char *tmp_file_path, *template;
	GFile *tmp_file;
	int ret = -1;
	
	LOG ("Exporting as Mozilla to %s", filename);

	template = g_build_filename (g_get_tmp_dir (),
				     "export-bookmarks-XXXXXX", NULL);
	tmp_file_path = ephy_file_tmp_filename (template, "rdf");
	g_free (template);
	if (tmp_file_path == NULL) return;

	writer = xmlNewTextWriterDoc (&doc, 0);
	if (writer == NULL || doc == NULL)
	{
		g_free (tmp_file_path);
		return;
	}

	START_PROFILER ("Exporting as Mozilla");
	
	tmp_file = g_file_new_for_path (tmp_file_path);
	ret = write_rdf (bookmarks, tmp_file, writer);
	g_object_unref (tmp_file);

	if (ret < 0) goto out;

	/* Set up libxml stuff */
	xmlLoadExtDtdDefaultValue = 1;
	xmlSubstituteEntitiesDefault (1);
	
	cur = xsltParseStylesheetFile ((const xmlChar *) ephy_file ("epiphany-bookmarks-html.xsl"));
	if (cur == NULL) goto out;

	res = xsltApplyStylesheet (cur, doc, NULL);
	if (res == NULL)
	{
		xsltFreeStylesheet (cur);
		goto out;
	}

	ret = xsltSaveResultToFilename (filename, res, cur, FALSE);

	xsltFreeStylesheet (cur);
	xmlFreeDoc (res);

	/* Clean up libxslt stuff */
	xsltCleanupGlobals ();

out:
	xmlFreeTextWriter (writer);
	xmlFreeDoc (doc);
	g_free (tmp_file_path);

	STOP_PROFILER ("Exporting as Mozilla")
	
	LOG ("Exporting as Mozilla %s.", ret >= 0 ? "succeeded" : "FAILED");
}

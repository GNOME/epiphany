/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "ephy-bookmarks-export.h"
#include "ephy-node-common.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>

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
		EphyNodePriority priority;

		kid = g_ptr_array_index (children, i);

		priority = ephy_node_get_property_int (kid, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority == -1) priority = EPHY_NODE_NORMAL_PRIORITY;

		if (priority == EPHY_NODE_NORMAL_PRIORITY &&
		    ephy_node_has_child (kid, bmk))
		{
			keywords = g_list_prepend (keywords, kid);
		}
	}
	ephy_node_thaw (topics);

	for (l = keywords; l != NULL; l = l->next)
	{
		EphyNode *node = l->data;
		const char *name;

		name = ephy_node_get_property_string
			(node, EPHY_NODE_KEYWORD_PROP_NAME);

		ret = xmlTextWriterWriteElementNS
			(writer, "dc", "subject", NULL, name);
		if (ret < 0) break;
	}

	g_list_free (keywords);

	return ret >= 0 ? 0 : -1;
}

void
ephy_bookmarks_export_rdf (EphyBookmarks *bookmarks,
			   const char *filename)
{
	EphyNode *bmks, *topics, *smart_bmks;
	xmlTextWriterPtr writer;
	char *tmp_file;
	GPtrArray *children;
	char *file_uri;
	int i, ret;

	LOG ("Exporting as RDF to %s", filename)

	START_PROFILER ("Exporting as RDF")

	tmp_file = g_strconcat (filename, ".tmp", NULL);

	/* FIXME: do we want to turn on compression here? */
	writer = xmlNewTextWriterFilename (tmp_file, 0);
	if (writer == NULL) return;

	ret = xmlTextWriterStartDocument (writer, "1.0", NULL, NULL);
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElementNS
		(writer, "rdf", "RDF", "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteAttribute (writer, "xmlns", "http://purl.org/rss/1.0/");
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteAttributeNS
		(writer, "xmlns", "dc", NULL, "http://purl.org/dc/elements/1.1/");
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteAttributeNS
		(writer, "xmlns", "ephy", NULL, "http://gnome.org/ns/epiphany#");
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElement (writer, "channel");
	if (ret < 0) goto out;

	/* FIXME is this UTF-8 ? */
	file_uri = gnome_vfs_get_uri_from_local_path (filename);
	ret = xmlTextWriterWriteAttributeNS (writer, "rdf", "about", NULL, file_uri);
	g_free (file_uri);
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteElement (writer, "title", "Epiphany bookmarks");
	if (ret < 0) goto out;

	ret = xmlTextWriterWriteElement
		(writer, "link", "http://www.gnome.org/projects/epiphany/");
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElement (writer, "items");
	if (ret < 0) goto out;

	ret = xmlTextWriterStartElementNS (writer, "rdf", "Seq", NULL);
	if (ret < 0) goto out;

	bmks = ephy_bookmarks_get_bookmarks (bookmarks);
	topics = ephy_bookmarks_get_keywords (bookmarks);
	smart_bmks = ephy_bookmarks_get_smart_bookmarks (bookmarks);

	children = ephy_node_get_children (bmks);
	for (i=0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *url;
		char *link = NULL;
		gboolean smart_url;

		kid = g_ptr_array_index (children, i);

		smart_url = ephy_node_has_child (smart_bmks, kid);
		url = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_LOCATION);
		if (smart_url)
		{
			GnomeVFSURI *uri;

			uri = gnome_vfs_uri_new (url);

			if (uri)
			{
				link = g_strconcat (gnome_vfs_uri_get_scheme (uri),
						    "://",
						    gnome_vfs_uri_get_host_name (uri),
						    NULL);

				gnome_vfs_uri_unref (uri);
			}
		}

		if (link == NULL)
		{
			link = g_strdup (url);
		}

		ret = xmlTextWriterStartElementNS (writer, "rdf", "li", NULL);
		if (ret < 0) break;

		ret = xmlTextWriterWriteAttributeNS
			(writer, "rdf", "about", NULL, link);
		if (ret < 0) break;

		ret = xmlTextWriterEndElement (writer); /* rdf:li */
		if (ret < 0) break;
	}
	ephy_node_thaw (bmks);
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

		kid = g_ptr_array_index (children, i);

		smart_url = ephy_node_has_child (smart_bmks, kid);
		url = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_LOCATION);
		title = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_TITLE);

		if (smart_url)
		{
			GnomeVFSURI *uri;

			uri = gnome_vfs_uri_new (url);

			if (uri)
			{
				link = g_strconcat (gnome_vfs_uri_get_scheme (uri),
						    "://",
						    gnome_vfs_uri_get_host_name (uri),
						    NULL);

				gnome_vfs_uri_unref (uri);
			}
		}

		if (link == NULL)
		{
			link = g_strdup (url);
		}

		ret = xmlTextWriterStartElement (writer, "item");
		if (ret < 0) break;

		ret = xmlTextWriterWriteAttributeNS
			(writer, "rdf", "about", NULL, link);
		if (ret < 0) break;

		ret = xmlTextWriterWriteElement (writer, "title", title);
		if (ret < 0) break;

		ret = xmlTextWriterWriteElement (writer, "link", link);
		if (ret < 0) break;

		if (smart_url)
		{
			ret = xmlTextWriterWriteElementNS (writer, "ephy", "smartlink", NULL, url);
			if (ret < 0) break;
		}

		ret = write_topics_list (topics, kid, writer);
		if (ret < 0) break;

		ret = xmlTextWriterEndElement (writer); /* item */

		g_free (link);
	}
	ephy_node_thaw (bmks);
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* rdf:RDF */
	if (ret < 0) goto out;

	ret = xmlTextWriterEndDocument (writer);

out:
	xmlFreeTextWriter (writer);

	if (ret >= 0)
	{
		if (ephy_file_switch_temp_file (filename, tmp_file) == FALSE)
		{
			ret = -1;
		}
	}

	STOP_PROFILER ("Exporting as RDF")

	LOG ("Exporting as RDF %s.", ret >= 0 ? "succeeded" : "FAILED")
}

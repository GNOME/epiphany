/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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
 */

#include <libxml/tree.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "ephy-bookmarks-export.h"
#include "ephy-node-common.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

static void
add_topics_list (EphyNode *topics, EphyNode *bmk,
		 xmlNodePtr parent, xmlNsPtr dc_ns)
{
	GPtrArray *children;
	int i;
	GList *bmks = NULL, *l;

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
			bmks = g_list_append (bmks, kid);
		}
	}
	ephy_node_thaw (topics);

	if (bmks == NULL) return;

	for (l = bmks; l != NULL; l = l->next)
	{
		const char *name;
		EphyNode *node = l->data;
		xmlNodePtr item_node;

		name = ephy_node_get_property_string
			(node, EPHY_NODE_KEYWORD_PROP_NAME);
		item_node = xmlNewChild (parent, dc_ns, "subject", NULL);
		xmlNodeSetContent (item_node, name);
	}

	g_list_free (bmks);
}

void
ephy_bookmarks_export_rdf (EphyBookmarks *bookmarks,
			   const char *filename)
{
	EphyNode *bmks, *topics, *smart_bmks;
	xmlDocPtr doc;
	xmlNodePtr root, xml_node, channel_node, channel_seq_node;
	xmlNsPtr ephy_ns, rdf_ns, dc_ns;
	GPtrArray *children;
	char *file_uri;
	int i;

	LOG ("Exporting to rdf")

	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "rdf:RDF", NULL);
	xmlDocSetRootElement (doc, root);
	rdf_ns = xmlNewNs (root, "http://www.w3.org/1999/02/22-rdf-syntax-ns#", "rdf");
	xmlNewNs (root, "http://purl.org/rss/1.0/", NULL);
	dc_ns = xmlNewNs (root, "http://purl.org/dc/elements/1.1/", "dc");
	ephy_ns = xmlNewNs (root, "http://gnome.org/ns/epiphany#", "ephy");
	channel_node = xmlNewChild (root, NULL, "channel", NULL);
	file_uri = gnome_vfs_get_uri_from_local_path (filename);
	xmlSetProp (channel_node, "rdf:about", file_uri);
	g_free (file_uri);

	xml_node = xmlNewChild (channel_node, NULL, "title", NULL);
	xmlNodeSetContent (xml_node, "Epiphany bookmarks");

	xml_node = xmlNewChild (channel_node, NULL, "link", NULL);
	xmlNodeSetContent (xml_node, "http://epiphany.mozdev.org");

	xml_node = xmlNewChild (channel_node, NULL, "items", NULL);

	channel_seq_node = xmlNewChild (xml_node, rdf_ns, "Seq", NULL);

	bmks = ephy_bookmarks_get_bookmarks (bookmarks);
	topics = ephy_bookmarks_get_keywords (bookmarks);
	smart_bmks = ephy_bookmarks_get_smart_bookmarks (bookmarks);

	children = ephy_node_get_children (bmks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *url, *title;
		xmlNodePtr item_node;
		xmlChar *encoded_link;
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

		encoded_link = xmlEncodeEntitiesReentrant (doc, link);

		item_node = xmlNewChild (channel_seq_node, rdf_ns, "li", NULL);
		xmlSetNsProp (item_node, rdf_ns, "about", link);

		item_node = xmlNewChild (root, NULL, "item", NULL);
		xmlSetNsProp (item_node, rdf_ns, "about", link);

		xml_node = xmlNewChild (item_node, NULL, "title", title);

		xml_node = xmlNewChild (item_node, NULL, "link", encoded_link);

		if (smart_url)
		{
			xmlChar *encoded_url;
			encoded_url = xmlEncodeEntitiesReentrant (doc, url);
			xml_node = xmlNewChild (item_node, ephy_ns, "smartlink", encoded_url);
			xmlFree (encoded_url);
		}

		add_topics_list (topics, kid, item_node, dc_ns);

		xmlFree (encoded_link);
		g_free (link);
	}
	ephy_node_thaw (bmks);

	ephy_file_save_xml (filename, doc);
	xmlFreeDoc(doc);
}

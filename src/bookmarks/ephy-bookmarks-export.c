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

#include "ephy-bookmarks-export.h"
#include "ephy-node-common.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

static void
add_topics_list (EphyNode *topics, EphyNode *bmk, xmlNodePtr parent)
{
	GPtrArray *children;
	int i;
	GList *bmks = NULL, *l;
	xmlNodePtr xml_node, bag_node, item_node;

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

	xml_node = xmlNewChild (parent, NULL, "dc:subject", NULL);
	bag_node = xmlNewChild (xml_node, NULL, "rdf:Bag", NULL);

	for (l = bmks; l != NULL; l = l->next)
	{
		const char *name;
		EphyNode *node = l->data;

		name = ephy_node_get_property_string
			(node, EPHY_NODE_KEYWORD_PROP_NAME);
		item_node = xmlNewChild (bag_node, NULL, "rdf:li", NULL);
		xmlNodeSetContent (item_node, name);
	}

	g_list_free (bmks);
}

void
ephy_bookmarks_export_rdf (EphyBookmarks *bookmarks,
			   const char *filename)
{
	EphyNode *bmks;
	EphyNode *topics;
	xmlDocPtr doc;
	xmlNodePtr root, xml_node, channel_node, channel_seq_node;
	GPtrArray *children;
	int i;

	LOG ("Exporting to rdf")

	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "rdf:RDF", NULL);
	xmlDocSetRootElement (doc, root);
	xmlSetProp (root, "xmlns:rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
	xmlSetProp (root, "xmlns", "http://purl.org/rss/1.0/");
	xmlSetProp (root, "xmlns:dc", "http://purl.org/dc/elements/1.1/");

	channel_node = xmlNewChild (root, NULL, "channel", NULL);
	xmlSetProp (channel_node, "rdf:about", "http://epiphany.mozdev.org/bookmarks");

	xml_node = xmlNewChild (channel_node, NULL, "title", NULL);
	xmlNodeSetContent (xml_node, "Epiphany bookmarks");

	xml_node = xmlNewChild (channel_node, NULL, "link", NULL);
	xmlNodeSetContent (xml_node, "http://epiphany.mozdev.org");

	xml_node = xmlNewChild (channel_node, NULL, "items", NULL);

	channel_seq_node = xmlNewChild (xml_node, NULL, "rdf:Seq", NULL);

	bmks = ephy_bookmarks_get_bookmarks (bookmarks);
	topics = ephy_bookmarks_get_keywords (bookmarks);

	children = ephy_node_get_children (bmks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;
		const char *url, *title;
		xmlNodePtr item_node;

		kid = g_ptr_array_index (children, i);


		url = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_LOCATION);
		title = ephy_node_get_property_string
			(kid, EPHY_NODE_BMK_PROP_TITLE);

		item_node = xmlNewChild (channel_seq_node, NULL, "rdf:li", NULL);
		xmlSetProp (item_node, "rdf:about", url);

		item_node = xmlNewChild (root, NULL, "item", NULL);
		xmlSetProp (item_node, "rdf:about", url);

		xml_node = xmlNewChild (item_node, NULL, "title", NULL);
		xmlNodeSetContent (xml_node, title);

		xml_node = xmlNewChild (item_node, NULL, "link", NULL);
		xmlNodeSetContent (xml_node, url);

		add_topics_list (topics, kid, item_node);
	}
	ephy_node_thaw (bmks);

	ephy_file_save_xml (filename, doc);
	xmlFreeDoc(doc);
}

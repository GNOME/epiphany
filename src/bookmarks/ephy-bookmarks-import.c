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

#include <glib.h>
#include <libxml/HTMLtree.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include "ephy-bookmarks-import.h"
#include "ephy-debug.h"

typedef struct _XbelInfo
{
	char *title;
	char *smarturl;
} XbelInfo;

static EphyNode *
set_folder (EphyBookmarks *bookmarks,
	    EphyNode *bookmark,
	    const char *name)
{
	EphyNode *topic;

	topic = ephy_bookmarks_find_keyword (bookmarks, name, FALSE);
	if (topic == NULL)
	{
		topic = ephy_bookmarks_add_keyword (bookmarks, name);
	}

	ephy_bookmarks_set_keyword (bookmarks, topic, bookmark);

	return topic;
}

gboolean
ephy_bookmarks_import (EphyBookmarks *bookmarks,
		       const char *filename)
{
	char *type;

	type = gnome_vfs_get_mime_type (filename);

	LOG ("Importing bookmarks of type %s", type)

	if (type == NULL) return FALSE;

	if (strcmp (type, "application/x-mozilla-bookmarks") == 0)
	{
		return ephy_bookmarks_import_mozilla (bookmarks, filename);
	}
	else
	{
		return ephy_bookmarks_import_xbel (bookmarks, filename);
	}
}

static void
mozilla_parse_bookmarks (EphyBookmarks *bookmarks,
		         htmlNodePtr node,
			 char **keyword)
{
	htmlNodePtr child = node;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "h3"))
		{
			*keyword = xmlNodeGetContent (child);
		}
		else if (xmlStrEqual (child->name, "a"))
		{
			xmlChar *title, *url;
			EphyNode *bmk;

			title = xmlNodeGetContent (child);
			url = xmlGetProp (child, "href");
			bmk = ephy_bookmarks_add (bookmarks,
					          title,
					          url);
			set_folder (bookmarks, bmk, *keyword);
			xmlFree (title);
			xmlFree (url);
		}

		mozilla_parse_bookmarks (bookmarks,
				         child->children,
					 keyword);
		child = child->next;
	}
}


static void
xbel_parse_single_bookmark (EphyBookmarks *bookmarks,
			    xmlNodePtr node, XbelInfo *xbel)
{
	xmlNodePtr child = node;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "title"))
		{
			xbel->title = xmlNodeGetContent (child);
		}
		else if (xmlStrEqual (child->name, "info"))
		{
			xbel_parse_single_bookmark (bookmarks,
						    child->children,
						    xbel);
		}
		else if (xmlStrEqual (child->name, "metadata"))
		{
			xbel_parse_single_bookmark (bookmarks,
						    child->children,
						    xbel);
		}
		else if (xmlStrEqual (child->name, "smarturl"))
		{
			xbel->smarturl = xmlNodeGetContent (child);
		}

		child = child->next;
	}
}

static void
xbel_parse_folder (EphyBookmarks *bookmarks,
		   xmlNodePtr node)
{
	xmlNodePtr child = node;
	xmlChar *keyword = NULL;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "title"))
		{
			keyword = xmlNodeGetContent (child);
		}
		else if (xmlStrEqual (child->name, "bookmark"))
		{
			XbelInfo *xbel;
			xmlChar *url;
			EphyNode *bmk;

			xbel = g_new0 (XbelInfo, 1);
			xbel->title = NULL;
			xbel->smarturl = NULL;

			url = xmlGetProp (child, "href");

			xbel_parse_single_bookmark (bookmarks,
						    child->children,
						    xbel);

			/* FIXME need to import also smart bookmark */
			bmk = ephy_bookmarks_add (bookmarks,
					          xbel->title,
					          url);
			if (keyword)
			{
				set_folder (bookmarks, bmk, keyword);
			}

			xmlFree (url);

			if (xbel && xbel->title)
				xmlFree (xbel->title);

			if (xbel && xbel->smarturl)
				xmlFree (xbel->smarturl);

			g_free (xbel);
		}
		else if (xmlStrEqual (child->name, "folder"))
		{
			xbel_parse_folder (bookmarks,
					   child->children);

			g_free (keyword);
			keyword = NULL;
		}

		child = child->next;
	}

	g_free (keyword);
}


static void
xbel_parse_bookmarks (EphyBookmarks *bookmarks,
		      xmlNodePtr node)
{
	xmlNodePtr child = node;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "xbel"))
		{
			xbel_parse_folder (bookmarks,
					   child->children);
		}

		child = child->next;
	}
}

gboolean
ephy_bookmarks_import_mozilla (EphyBookmarks *bookmarks,
			       const char *filename)
{
	htmlDocPtr doc;
	htmlNodePtr child;
	char *keyword = NULL;

	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
		return FALSE;

	doc = htmlParseFile (filename, "UTF-8");
	g_assert (doc != NULL);

	child = doc->children;
	mozilla_parse_bookmarks (bookmarks, child, &keyword);

	g_free (keyword);
        xmlFreeDoc (doc);

	ephy_bookmarks_save (bookmarks);

	return TRUE;
}

gboolean
ephy_bookmarks_import_xbel (EphyBookmarks *bookmarks,
			    const char *filename)
{
	xmlDocPtr doc;
	xmlNodePtr child;

	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
		return FALSE;

	doc = xmlParseFile (filename);
	g_assert (doc != NULL);

	child = doc->children;
	xbel_parse_bookmarks (bookmarks, child);

	xmlFreeDoc (doc);

	ephy_bookmarks_save (bookmarks);

	return TRUE;
}

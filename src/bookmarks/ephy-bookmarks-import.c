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

#include "ephy-bookmarks-import.h"
#include "ephy-string.h"

typedef struct _XbelInfo
{
	char *title;
	char *smarturl;
} XbelInfo;

static char *
build_keyword (const char *folder)
{
	return ephy_str_replace_substring (folder, " ", "_");
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
			xmlChar *tmp;

			tmp = xmlNodeGetContent (child);
			g_free (*keyword);
			*keyword = build_keyword (tmp);
			xmlFree (tmp);
		}
		else if (xmlStrEqual (child->name, "a"))
		{
			xmlChar *title, *url;

			title = xmlNodeGetContent (child);
			url = xmlGetProp (child, "href");
			ephy_bookmarks_add (bookmarks,
					    title,
					    url,
					    NULL,
					    *keyword);
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
		   xmlNodePtr node,
		   const char *default_keyword)
{
	xmlNodePtr child = node;
	xmlChar *keyword = g_strdup (default_keyword);

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "title"))
		{
			xmlChar *tmp;

			tmp = xmlNodeGetContent (child);

			g_free (keyword);
			keyword = build_keyword (tmp);
			xmlFree (tmp);
		}
		else if (xmlStrEqual (child->name, "bookmark"))
		{
			XbelInfo *xbel;
			xmlChar *url;

			xbel = g_new0 (XbelInfo, 1);
			xbel->title = NULL;
			xbel->smarturl = NULL;

			url = xmlGetProp (child, "href");

			xbel_parse_single_bookmark (bookmarks,
						    child->children,
						    xbel);

			
			ephy_bookmarks_add (bookmarks,
					    xbel->title,
					    url,
					    xbel->smarturl,
					    keyword);

			if (url)
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
					   child->children,
					   keyword);

			if (keyword)
			{
				g_free (keyword);
				keyword = NULL;
			}
		}
		
		child = child->next;
	}

	g_free (keyword);
}


static void
xbel_parse_bookmarks (EphyBookmarks *bookmarks,
		      xmlNodePtr node,
		      const char *default_keyword)
{
	xmlNodePtr child = node;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "xbel"))
		{
			xbel_parse_bookmarks (bookmarks,
					      child->children,
					      default_keyword);
		}
		else if (xmlStrEqual (child->name, "folder"))
		{
			xbel_parse_folder (bookmarks,
					   child->children,
					   default_keyword);
		}
		else if (xmlStrEqual (child->name, "bookmark"))
		{
			xbel_parse_folder (bookmarks,
					   child,
					   default_keyword);
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

	return TRUE;
}

gboolean
ephy_bookmarks_import_xbel (EphyBookmarks *bookmarks,
			    const char *filename,
			    const char *default_keyword)
{
	xmlDocPtr doc;
	xmlNodePtr child;

	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
		return FALSE;

	doc = xmlParseFile (filename);
	g_assert (doc != NULL);

	child = doc->children;
	xbel_parse_bookmarks (bookmarks, child, default_keyword);

	xmlFreeDoc (doc);

	return TRUE;
}

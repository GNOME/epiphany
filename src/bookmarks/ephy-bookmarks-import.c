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

/**
 * NSItemType: netscape bookmark item type
 */
typedef enum
{
	NS_SITE,
	NS_NOTES,
	NS_FOLDER,
	NS_FOLDER_END,
	NS_SEPARATOR,
	NS_UNKNOWN
} NSItemType;

typedef struct _XbelInfo
{
	char *title;
	char *smarturl;
} XbelInfo;

static void
bookmark_add (EphyBookmarks *bookmarks,
	      const char *title,
	      const char *address,
	      const char *topic_name)
{
	EphyNode *topic;
	EphyNode *bmk;

	if (ephy_bookmarks_find_bookmark (bookmarks, address)) return;

	bmk = ephy_bookmarks_add (bookmarks, title, address);

	if (topic_name)
	{
		topic = ephy_bookmarks_find_keyword (bookmarks, topic_name, FALSE);
		if (topic == NULL)
		{
			topic = ephy_bookmarks_add_keyword (bookmarks, topic_name);
		}

		ephy_bookmarks_set_keyword (bookmarks, topic, bmk);
	}
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
	else if (strcmp (type, "application/xbel") == 0)
	{
		return ephy_bookmarks_import_xbel (bookmarks, filename);
	}
	else if (strstr (filename, MOZILLA_BOOKMARKS_DIR) != NULL)
	{
		return ephy_bookmarks_import_mozilla (bookmarks, filename);
	}
	else if (strstr (filename, GALEON_BOOKMARKS_DIR) != NULL ||
		 strstr (filename, KDE_BOOKMARKS_DIR) != NULL)
	{
		return ephy_bookmarks_import_xbel (bookmarks, filename);
	}

	return FALSE;
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

			xbel = g_new0 (XbelInfo, 1);
			xbel->title = NULL;
			xbel->smarturl = NULL;

			url = xmlGetProp (child, "href");

			xbel_parse_single_bookmark (bookmarks,
						    child->children,
						    xbel);

			bookmark_add (bookmarks, xbel->title, url, keyword);

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

static gchar *
gul_general_read_line_from_file (FILE *f)
{
	gchar *line = g_strdup ("");
	gchar *t;
	gchar *buf = g_new0 (gchar, 256);
	while ( ! ( strchr (buf, '\n') || feof (f) ) ) {
		fgets(buf, 256, f);
		t = line;
		line = g_strconcat (line, buf, NULL);
		g_free (t);
	}
	g_free (buf);
	return line;
}

static const gchar *
gul_string_ascii_strcasestr (const gchar *a, const gchar *b)
{
	gchar *down_a;
	gchar *down_b;
	gchar *ptr;

	/* copy and lower case the strings */
	down_a = g_strdup (a);
	down_b = g_strdup (b);
	g_ascii_strdown (down_a, -1);
	g_ascii_strdown (down_b, -1);

	/* compare */
	ptr = strstr (down_a, down_b);

	/* free allocated strings */
	g_free (down_a);
	g_free (down_b);

	/* return result of comparison */
	return ptr == NULL ? NULL : (a + (ptr - down_a));
}

/**
 * Parses a line of a mozilla/netscape bookmark file. File must be open.
 */
/* this has been tested fairly well */
static NSItemType
ns_get_bookmark_item (FILE *f, GString *name, GString *url)
{
	char *line = NULL;
	char *found;

	line = gul_general_read_line_from_file (f);

	if ((found = (char *) gul_string_ascii_strcasestr (line, "<A HREF=")))
	{  /* declare site? */
		g_string_assign (url, found+9);  /* url=URL+ ADD_DATE ... */
		g_string_truncate (url, strstr(url->str, "\"")-url->str);
		found = (char *) strstr (found+9+url->len, "\">");
		if (!found)
		{
			g_free (line);
			return NS_UNKNOWN;
		}
		g_string_assign (name, found+2);
		g_string_truncate (name, gul_string_ascii_strcasestr (name->str,
						       "</A>")-name->str);
		g_free (line);
		return NS_SITE;
	}
	else if ((found = (char *) gul_string_ascii_strcasestr (line, "<DT><H3")))
	{ /* declare folder? */
		found = (char *) strstr(found+7, ">");
		if (!found) return NS_UNKNOWN;
		g_string_assign (name, found+1);
		g_string_truncate (name, gul_string_ascii_strcasestr (name->str,
				   "</H3>") - name->str);
		g_free (line);
		return NS_FOLDER;
	}
	else if ((found = (char *) gul_string_ascii_strcasestr (line, "</DL>")))
	{     /* end folder? */
		g_free (line);
		return NS_FOLDER_END;
	}

	g_free (line);
	return NS_UNKNOWN;
}

/**
 * This function replaces some weird elements
 * like &amp; &le;, etc..
 * More info : http://www.w3.org/TR/html4/charset.html#h-5.3.2
 * NOTE : We don't support &#D or &#xH.
 * Patch courtesy of Almer S. Tigelaar <almer1@dds.nl>
 */
static char *
ns_parse_bookmark_item (GString *string)
{
	char *iterator, *temp;
	int cnt = 0;
	GString *result = g_string_new (NULL);

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (string->str != NULL, NULL);

	iterator = string->str;

	for (cnt = 0, iterator = string->str;
	     cnt <= (int)(strlen (string->str));
	     cnt++, iterator++) {
		if (*iterator == '&') {
			int jump = 0;
			int i;

			if (g_ascii_strncasecmp (iterator, "&amp;", 5) == 0)
			{
				g_string_append_c (result, '&');
				jump = 5;
			}
			else if (g_ascii_strncasecmp (iterator, "&lt;", 4) == 0)
			{
				g_string_append_c (result, '<');
				jump = 4;
			}
			else if (g_ascii_strncasecmp (iterator, "&gt;", 4) == 0)
			{
				g_string_append_c (result, '>');
				jump = 4;
			}
			else if (g_ascii_strncasecmp (iterator, "&quot;", 6) == 0)
			{
				g_string_append_c (result, '\"');
				jump = 6;
			}
			else
			{
				/* It must be some numeric thing now */

				iterator++;

				if (iterator && *iterator == '#') {
					int val;
					char *num, *tmp;

					iterator++;

					val = atoi (iterator);

					tmp = g_strdup_printf ("%d", val);
					jump = strlen (tmp);
					g_free (tmp);

					num = g_strdup_printf ("%c", (char) val);
					g_string_append (result, num);
					g_free (num);
				}
			}

			for (i = jump - 1; i > 0; i--)
			{
				iterator++;
				if (iterator == NULL)
					break;
			}
		}
		else
		{
			g_string_append_c (result, *iterator);
		}
	}
	temp = result->str;
	g_string_free (result, FALSE);
	return temp;
}

gboolean
ephy_bookmarks_import_mozilla (EphyBookmarks *bookmarks,
			       const char *filename)
{
	FILE *bf;  /* bookmark file */
	GString *name = g_string_new (NULL);
	gchar *parsedname;
	GString *url = g_string_new (NULL);
	char *current_folder = NULL;

	if (!(bf = fopen (filename, "r"))) {
		g_warning ("Failed to open file: %s\n", filename);
		return FALSE;
	}

	while (!feof (bf)) {
		NSItemType t;
		t = ns_get_bookmark_item (bf, name, url);
		switch (t)
		{
		case NS_FOLDER:
			g_free (current_folder);
			current_folder = g_strdup (name->str);
			break;
		case NS_SITE:
			parsedname = ns_parse_bookmark_item (name);

			bookmark_add (bookmarks, parsedname,
				      url->str, current_folder);
			break;
		default:
			break;
		}
	}
	fclose (bf);
	g_string_free (name, TRUE);
	g_string_free (url, TRUE);

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

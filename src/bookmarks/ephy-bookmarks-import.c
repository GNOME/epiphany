/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlreader.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include <glib/gi18n.h>

#include "ephy-bookmarks-import.h"
#include "ephy-debug.h"
#include "ephy-prefs.h"

#include "eel-gconf-extensions.h"

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

static EphyNode *
bookmark_add (EphyBookmarks *bookmarks,
	      const char *title,
	      const char *address)
{
	if (!ephy_bookmarks_find_bookmark (bookmarks, address))
	{
		return ephy_bookmarks_add (bookmarks, title, address);
	}
	else
	{
		return NULL;
	}
}

gboolean
ephy_bookmarks_import (EphyBookmarks *bookmarks,
		       const char *filename)
{
	char *type;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING)) return FALSE;

	type = gnome_vfs_get_mime_type (filename);

	LOG ("Importing bookmarks of type %s", type)

	if (type == NULL) return FALSE;

	if (strcmp (type, "application/x-mozilla-bookmarks") == 0)
	{
		return ephy_bookmarks_import_mozilla (bookmarks, filename);
	}
	else if (strcmp (type, "application/x-xbel") == 0)
	{
		return ephy_bookmarks_import_xbel (bookmarks, filename);
	}
	else if (strcmp (type, "text/rdf") == 0)
	{
		return ephy_bookmarks_import_rdf (bookmarks, filename);
	}
	else if (strstr (filename, MOZILLA_BOOKMARKS_DIR) != NULL ||
                 strstr (filename, FIREBIRD_BOOKMARKS_DIR) != NULL ||
		 strstr (filename, FIREFOX_BOOKMARKS_DIR) != NULL)
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

/* XBEL import */

typedef enum
{
	STATE_START,
	STATE_STOP,
	STATE_XBEL,
	STATE_FOLDER,
	STATE_BOOKMARK,
	STATE_TITLE,
	STATE_DESC,
	STATE_INFO,
	STATE_METADATA,
	STATE_SMARTURL
} EphyXBELImporterState;

static int
xbel_parse_bookmark (EphyBookmarks *eb, xmlTextReaderPtr reader, EphyNode **ret_node)
{
	EphyXBELImporterState state = STATE_BOOKMARK;
	EphyNode *node;
	xmlChar *title = NULL;
	xmlChar *address = NULL;
	int ret = 1;

	while (ret == 1)
	{
		const xmlChar *tag;
		xmlReaderTypes type;

		tag = xmlTextReaderConstName (reader);
		g_return_val_if_fail (tag != NULL, ret);

		type = xmlTextReaderNodeType (reader);

		if (xmlStrEqual (tag, "#text"))
		{
			if (state == STATE_TITLE && title == NULL)
			{
				title = xmlTextReaderValue (reader);
			}
			else if (state == STATE_SMARTURL)
			{
				xmlFree (address);
				address = xmlTextReaderValue (reader);
			}
			else
			{
				/* eat it */
			}
		}
		else if (xmlStrEqual (tag, "bookmark"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_BOOKMARK && address == NULL)
			{
				address = xmlTextReaderGetAttribute (reader, "href");
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_BOOKMARK)
			{
				/* we're done */

				break;
			}
		}
		else if (xmlStrEqual (tag, "title"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_BOOKMARK && title == NULL)
			{
				state = STATE_TITLE;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_TITLE)
			{
				state = STATE_BOOKMARK;
			}
		}
		else if (xmlStrEqual (tag, "desc"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_BOOKMARK)
			{
				state = STATE_DESC;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_DESC)
			{
				state = STATE_BOOKMARK;
			}
		}
		else if (xmlStrEqual (tag, "info"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_BOOKMARK)
			{
				state = STATE_INFO;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_INFO)
			{
				state = STATE_BOOKMARK;
			}
		}
		else if (xmlStrEqual (tag, "metadata"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_INFO)
			{
				state = STATE_METADATA;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_METADATA)
			{
				state = STATE_INFO;
			}
		}
		else if (xmlStrEqual (tag, "smarturl"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_METADATA)
			{
				state = STATE_SMARTURL;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_SMARTURL)
			{
				state = STATE_METADATA;
			}
		}

		/* next one, please */
		ret = xmlTextReaderRead (reader);
	}

	if (address == NULL)
	{
		return ret;
	}

	if (title == NULL)
	{
		title = xmlStrdup (_("Untitled"));
	}

	node = bookmark_add (eb, title, address);
	if (node == NULL)
	{
		/* probably a duplicate */
		node = ephy_bookmarks_find_bookmark (eb, address);		
	}

	xmlFree (title);
	xmlFree (address);

	*ret_node = node;

	return ret;
}

static int
xbel_parse_folder (EphyBookmarks *eb, xmlTextReaderPtr reader, GList **ret_list)
{
	EphyXBELImporterState state = STATE_FOLDER;
	EphyNode *keyword;
	GList *list = NULL, *l;
	xmlChar *title = NULL;
	int ret;

	ret = xmlTextReaderRead (reader);

	while (ret == 1)
	{
		const xmlChar *tag;
		xmlReaderTypes type;

		tag = xmlTextReaderConstName (reader);
		type = xmlTextReaderNodeType (reader);

		if (tag == NULL)
		{
			/* shouldn't happen but does anyway :) */
		}
		else if (xmlStrEqual (tag, "#text"))
		{
			if (state == STATE_TITLE && title == NULL)
			{
				title = xmlTextReaderValue (reader);
			}
			else
			{
				/* eat it */
			}
		}
		else if (xmlStrEqual (tag, "bookmark") && type == 1 && state == STATE_FOLDER)
		{
			EphyNode *node = NULL;

			ret = xbel_parse_bookmark (eb, reader, &node);

			if (EPHY_IS_NODE (node))
			{
				list = g_list_prepend (list, node);
			}

			if (ret != 1) break;
		}
		else if ((xmlStrEqual (tag, "folder"))
			&& state == STATE_FOLDER)
		{
			if (type == XML_READER_TYPE_ELEMENT)
			{
				GList *sublist = NULL;

				ret = xbel_parse_folder (eb, reader, &sublist);

				list = g_list_concat (list, sublist);
				
				if (ret != 1) break;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT)
			{
				/* we're done */

				break;
			}
		}
		else if (xmlStrEqual (tag, "title"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_FOLDER)
			{
				state = STATE_TITLE;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_TITLE)
			{
				state = STATE_FOLDER;
			}
		}
		else if (xmlStrEqual (tag, "info"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_FOLDER)
			{
				state = STATE_INFO;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_INFO)
			{
				state = STATE_FOLDER;
			}
		}
		else if (xmlStrEqual (tag, "desc"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_FOLDER)
			{
				state = STATE_DESC;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_DESC)
			{
				state = STATE_FOLDER;
			}
		}
		else
		{
			/* eat it */
		}

		/* next one, please */
		ret = xmlTextReaderRead (reader);
	}

	/* tag all bookmarks in the list with keyword %title */
	if (title == NULL || title[0] == '\0')
	{
		*ret_list = list;
		return ret;
	}

	keyword = ephy_bookmarks_find_keyword (eb, title, FALSE);

	if (keyword == NULL)
	{
		keyword = ephy_bookmarks_add_keyword (eb, title);
	}

	xmlFree (title);

	if (keyword == NULL)
	{
		*ret_list = list;
		return ret;
	}

	for (l = list; l != NULL; l = l->next)
	{
		EphyNode *node = (EphyNode *) l->data;

		ephy_bookmarks_set_keyword (eb, keyword, node);
	}

	*ret_list = list;

	return ret;
}

static int
xbel_parse_xbel (EphyBookmarks *eb, xmlTextReaderPtr reader)
{
	EphyXBELImporterState state = STATE_XBEL;
	int ret;

	ret = xmlTextReaderRead (reader);

	while (ret == 1 && state != STATE_STOP)
	{
		const xmlChar *tag;
		xmlReaderTypes type;

		tag = xmlTextReaderConstName (reader);
		type = xmlTextReaderNodeType (reader);

		if (tag == NULL)
		{
			/* shouldn't happen but does anyway :( */
		}
		else if (xmlStrEqual (tag, "bookmark") && type == XML_READER_TYPE_ELEMENT
			 && state == STATE_FOLDER)
		{
			EphyNode *node = NULL;

			/* this will eat the </bookmark> too */
			ret = xbel_parse_bookmark (eb, reader, &node);

			if (ret != 1) break;
		}
		else if (xmlStrEqual (tag, "folder") && type == XML_READER_TYPE_ELEMENT
			 && state == STATE_XBEL)
		{
			GList *list = NULL;

			/* this will eat the </folder> too */
			ret = xbel_parse_folder (eb, reader, &list);

			g_list_free (list);

			if (ret != 1) break;
		}
		else if ((xmlStrEqual (tag, "xbel")) && type == XML_READER_TYPE_ELEMENT
			 && state == STATE_START)
		{
			state = STATE_XBEL;
		}
		else if ((xmlStrEqual (tag, "xbel")) && type == XML_READER_TYPE_END_ELEMENT
			 && state == STATE_XBEL)
		{
			state = STATE_STOP;
		}
		else if (xmlStrEqual (tag, "title"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_XBEL)
			{
				state = STATE_TITLE;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_TITLE)
			{
				state = STATE_XBEL;
			}
		}
		else if (xmlStrEqual (tag, "info"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_XBEL)
			{
				state = STATE_INFO;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_INFO)
			{
				state = STATE_XBEL;
			}
		}
		else if (xmlStrEqual (tag, "desc"))
		{
			if (type == XML_READER_TYPE_ELEMENT && state == STATE_XBEL)
			{
				state = STATE_DESC;
			}
			else if (type == XML_READER_TYPE_END_ELEMENT && state == STATE_DESC)
			{
				state = STATE_XBEL;
			}
		}

		/* next one, please */
		ret = xmlTextReaderRead (reader);
	}

	return ret;
}

/* Mozilla/Netscape import */

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
 *
 * NOTE: The returned string must be freed.
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
				if (*iterator == '\0')
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

static char *
folders_list_to_topic_name (GList *folders)
{
	GString *topic;
	GList *l;

	g_return_val_if_fail (folders != NULL, NULL);

	topic = g_string_new (folders->data);

	for (l = folders->next; l != NULL; l = l->next)
	{
		g_string_append (topic, "/");
		g_string_append (topic, l->data);
	}

	return g_string_free (topic, FALSE);
}

gboolean
ephy_bookmarks_import_mozilla (EphyBookmarks *bookmarks,
			       const char *filename)
{
	FILE *bf;  /* bookmark file */
	GString *name, *url;
	char *parsedname, *topic;
	EphyNode *keyword;
	GList *folders = NULL;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING)) return FALSE;

	name = g_string_new (NULL);
	url = g_string_new (NULL);


	if (!(bf = fopen (filename, "r"))) {
		g_warning ("Failed to open file: %s\n", filename);
		return FALSE;
	}

	while (!feof (bf)) {
		EphyNode *node;
		NSItemType t;
		t = ns_get_bookmark_item (bf, name, url);
		switch (t)
		{
		case NS_FOLDER:
			folders = g_list_append (folders, ns_parse_bookmark_item (name));
			break;
		case NS_FOLDER_END:
			if (folders)
			{
				GList *last = g_list_last (folders);

				/* remove last entry */
				g_free (last->data);
				folders = g_list_delete_link (folders, last); 
			}
			break;
		case NS_SITE:
			parsedname = ns_parse_bookmark_item (name);

			node = bookmark_add (bookmarks, parsedname, url->str);

			if (node == NULL)
			{
				node = ephy_bookmarks_find_bookmark (bookmarks, url->str);
			}

			topic = folders_list_to_topic_name (folders);
			keyword = ephy_bookmarks_find_keyword (bookmarks, topic, FALSE);
			
			if (keyword == NULL)
			{
				keyword = ephy_bookmarks_add_keyword (bookmarks, topic);
			}

			if (node != NULL && keyword != NULL)
			{
				ephy_bookmarks_set_keyword (bookmarks, keyword, node);
			}

			g_free (topic);
			g_free (parsedname);

			break;
		default:
			break;
		}
	}
	fclose (bf);
	g_string_free (name, TRUE);
	g_string_free (url, TRUE);

	return TRUE;
}

gboolean
ephy_bookmarks_import_xbel (EphyBookmarks *bookmarks,
			    const char *filename)
{
	xmlTextReaderPtr reader;
	int ret;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING)) return FALSE;

	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
	{
		return FALSE;
	}
	
	reader = xmlNewTextReaderFilename (filename);
	if (reader == NULL)
	{
		return FALSE;
	}

	ret = xbel_parse_xbel (bookmarks, reader);

	xmlFreeTextReader (reader);

	return ret >= 0 ? TRUE : FALSE;
}

#define OLD_RDF_TEMPORARY_HACK

static void
parse_rdf_subjects (xmlNodePtr node,
		    GList **subjects)
{
	xmlChar *subject;

#ifdef OLD_RDF_TEMPORARY_HACK
	xmlNode *child;

	child = node->children;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "Bag"))
		{
			child = child->children;

			while (child != NULL)
			{
				if (xmlStrEqual (child->name, "li"))
				{
					subject = xmlNodeGetContent (child);
					*subjects = g_list_append (*subjects, subject);
				}

				child = child->next;
			}

			return;
		}

		child = child->next;
	}
#endif

	subject = xmlNodeGetContent (node);

	if (subject)
	{
		*subjects = g_list_append (*subjects, subject);
	}
}

static void
parse_rdf_item (EphyBookmarks *bookmarks,
		xmlNodePtr node)
{
	xmlChar *title = NULL;
	xmlChar *link = NULL;
	GList *subjects = NULL, *l = NULL;
	xmlNode *child;
	EphyNode *bmk;

	child = node->children;

#ifdef OLD_RDF_TEMPORARY_HACK
	link = xmlGetProp (node, "about");
#endif

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "title"))
		{
			title = xmlNodeGetContent (child);
		}
#ifndef OLD_RDF_TEMPORARY_HACK
		else if (xmlStrEqual (child->name, "link"))
		{
			link = xmlNodeGetContent (child);
		}
#endif
		else if (xmlStrEqual (child->name, "subject"))
		{
			parse_rdf_subjects (child, &subjects);
		}
		else if (xmlStrEqual (child->name, "smartlink"))
		{
			if (link) xmlFree (link);
			link = xmlNodeGetContent (child);
		}

		child = child->next;
	}

	bmk = bookmark_add (bookmarks, title, link);
	if (bmk)
	{
		l = subjects;
	}

	for (; l != NULL; l = l->next)
	{
		char *topic_name = l->data;
		EphyNode *topic;

		topic = ephy_bookmarks_find_keyword (bookmarks, topic_name, FALSE);

		if (topic == NULL)
		{
			topic = ephy_bookmarks_add_keyword (bookmarks, topic_name);
		}

		if (topic != NULL)
		{
			ephy_bookmarks_set_keyword (bookmarks, topic, bmk);
		}
	}

	xmlFree (title);
	xmlFree (link);

	g_list_foreach (subjects, (GFunc)xmlFree, NULL);
	g_list_free (subjects);
}

gboolean
ephy_bookmarks_import_rdf (EphyBookmarks *bookmarks,
			   const char *filename)
{
	xmlDocPtr doc;
	xmlNodePtr child;
	xmlNodePtr root;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING)) return FALSE;

	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
		return FALSE;

	doc = xmlParseFile (filename);
	if (doc == NULL)
	{
		/* FIXME: maybe put up a warning dialogue here, because this
		 * is a severe dataloss?
		 */
		g_warning ("Failed to re-import the bookmarks. All bookmarks lost!\n");
		return FALSE;
	}

	root = xmlDocGetRootElement (doc);

	child = root->children;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "item"))
		{
			parse_rdf_item (bookmarks, child);
		}

		child = child->next;
	}

	xmlFreeDoc (doc);

	return TRUE;
}

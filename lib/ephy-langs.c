/*
 *  Copyright © 2003, 2004 Christian Persch
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

#include "ephy-langs.h"

#include "ephy-debug.h"

#include <glib/gi18n.h>

#include <string.h>

#include <libxml/xmlreader.h>

/* sanitise the languages list according to the rules for HTTP accept-language
 * in RFC 2616, Sect. 14.4
 */
void
ephy_langs_sanitise (GArray *array)
{
	char *lang1, *lang2;
	int i, j;

	/* if we have 'xy-ab' in list but not 'xy', append 'xy' */
	for (i = 0; i < array->len; i++)
	{
		gboolean found = FALSE;
		char *dash, *prefix;

		lang1 = (char *) g_array_index (array,char *, i);

		dash = strchr (lang1, '-');
		if (dash == NULL) continue;

		for (j = i + 1; j < array->len; j++)
		{
			lang2 = (char *) g_array_index (array, char *, j);
			if (strchr (lang2, '-') == NULL &&
			    g_str_has_prefix (lang1, lang2))
			{
				found = TRUE;
			}
		}

		if (found == FALSE)
		{
			prefix = g_strndup (lang1, dash - lang1);
			g_array_append_val (array, prefix);
		}
	}

	/* uniquify */
	for (i = 0; i < (int) array->len - 1; i++)
	{
		for (j = (int) array->len - 1; j > i; j--)
		{
			lang1 = (char *) g_array_index (array,char *, i);
			lang2 = (char *) g_array_index (array, char *, j);

			if (strcmp (lang1, lang2) == 0)
			{
				g_array_remove_index (array, j);
				g_free (lang2);
			}
		}
	}

	/* move 'xy' code behind all 'xy-ab' codes */
	for (i = (int) array->len - 2; i >= 0; i--)
	{
		for (j = (int) array->len - 1; j > i; j--)
		{
			lang1 = (char *) g_array_index (array, char *, i);
			lang2 = (char *) g_array_index (array, char *, j);

			if (strchr (lang1, '-') == NULL &&
			    strchr (lang2, '-') != NULL &&
			    g_str_has_prefix (lang2, lang1))
			{
				g_array_insert_val (array, j + 1, lang1);
				g_array_remove_index (array, i);
				break;
			}
		}
	}
}

void
ephy_langs_append_languages (GArray *array)
{
	const char * const * languages;
	char *lang;
	int i;

	languages = g_get_language_names ();
	g_return_if_fail (languages != NULL);

	/* FIXME: maybe just use the first, instead of all of them? */
	for (i = 0; languages[i] != NULL; i++)
	{

		if (strstr (languages[i], ".") == 0 &&
		    strstr (languages[i], "@") == 0 &&
		    strcmp (languages[i], "C") != 0)
		{
			/* change to lowercase and '_' to '-' */
			lang = g_strdelimit (g_ascii_strdown
						(languages[i], -1), "_", '-');

			g_array_append_val (array, lang);
		}
	}

	/* Fallback: add "en" if list is empty */
	if (array->len == 0)
	{
		lang = g_strdup ("en");
		g_array_append_val (array, lang);
	}
}

char **
ephy_langs_get_languages (void)
{
	GArray *array;

	array = g_array_new (TRUE, FALSE, sizeof (char *));

	ephy_langs_append_languages (array);

	ephy_langs_sanitise (array);

	return (char **)(void *) g_array_free (array, FALSE);
}

static void
ephy_langs_bind_iso_domains (void)
{
	static gboolean bound = FALSE;

	if (bound == FALSE)
	{
	        bindtextdomain (ISO_639_DOMAIN, LOCALEDIR);
	        bind_textdomain_codeset (ISO_639_DOMAIN, "UTF-8");

	        bindtextdomain(ISO_3166_DOMAIN, LOCALEDIR);
	        bind_textdomain_codeset (ISO_3166_DOMAIN, "UTF-8");

		bound = TRUE;
	}
}

static void
read_iso_639_entry (xmlTextReaderPtr reader,
		    GHashTable *table)
{
	xmlChar *code, *name;

	code = xmlTextReaderGetAttribute (reader, (const xmlChar *) "iso_639_1_code");
	name = xmlTextReaderGetAttribute (reader, (const xmlChar *) "name");

	/* Get iso-639-2 code */
	if (code == NULL || code[0] == '\0')
	{
		xmlFree (code);
		/* FIXME: use the 2T or 2B code? */
		code = xmlTextReaderGetAttribute (reader, (const xmlChar *) "iso_639_2T_code");
	}

	if (code != NULL && code[0] != '\0' && name != NULL && name[0] != '\0')
	{
		g_hash_table_insert (table, code, name);
	}
	else
	{
		xmlFree (code);
		xmlFree (name);
	}
}

static void
read_iso_3166_entry (xmlTextReaderPtr reader,
		     GHashTable *table)
{
	xmlChar *code, *name;

	code = xmlTextReaderGetAttribute (reader, (const xmlChar *) "alpha_2_code");
	name = xmlTextReaderGetAttribute (reader, (const xmlChar *) "name");

	if (code != NULL && code[0] != '\0' && name != NULL && name[0] != '\0')
	{
		char *lcode;

		lcode = g_ascii_strdown ((char *) code, -1);
		xmlFree (code);

		g_hash_table_insert (table, lcode, name);
	}
	else
	{
		xmlFree (code);
		xmlFree (name);
	}

}

typedef enum
{
	STATE_START,
	STATE_STOP,
	STATE_ENTRIES,
} ParserState;

static void
load_iso_entries (int iso,
		  GFunc read_entry_func,
		  gpointer user_data)
{
	xmlTextReaderPtr reader;
	ParserState state = STATE_START;
	xmlChar iso_entries[32], iso_entry[32];
	char *filename;
	int ret = -1;

	LOG ("Loading ISO-%d codes", iso);

	START_PROFILER ("Loading ISO codes")

	filename = g_strdup_printf (ISO_CODES_PREFIX "/share/xml/iso-codes/iso_%d.xml", iso);
	reader = xmlNewTextReaderFilename (filename);
	if (reader == NULL) goto out;

	xmlStrPrintf (iso_entries, sizeof (iso_entries), (const xmlChar *)"iso_%d_entries", iso);
	xmlStrPrintf (iso_entry, sizeof (iso_entry), (const xmlChar *)"iso_%d_entry", iso);

	ret = xmlTextReaderRead (reader);

	while (ret == 1)
	{
		const xmlChar *tag;
		xmlReaderTypes type;

		tag = xmlTextReaderConstName (reader);
		type = xmlTextReaderNodeType (reader);

		if (state == STATE_ENTRIES &&
		    type == XML_READER_TYPE_ELEMENT &&
		    xmlStrEqual (tag, iso_entry))
		{
			read_entry_func (reader, user_data);
		}
		else if (state == STATE_START &&
			 type == XML_READER_TYPE_ELEMENT &&
			 xmlStrEqual (tag, iso_entries))
		{
			state = STATE_ENTRIES;
		}
		else if (state == STATE_ENTRIES &&
			 type == XML_READER_TYPE_END_ELEMENT &&
			 xmlStrEqual (tag, iso_entries))
		{
			state = STATE_STOP;
		}
		else if (type == XML_READER_TYPE_SIGNIFICANT_WHITESPACE ||
			 type == XML_READER_TYPE_WHITESPACE ||
			 type == XML_READER_TYPE_TEXT ||
			 type == XML_READER_TYPE_COMMENT)
		{
			/* eat it */
		}
		else
		{
			/* ignore it */
		}

		ret = xmlTextReaderRead (reader);
	}

	xmlFreeTextReader (reader);

out:
	if (ret < 0 || state != STATE_STOP)
	{
		g_warning ("Failed to load ISO-%d codes from %s!\n",
			   iso, filename);
	}

	g_free (filename);

	STOP_PROFILER ("Loading ISO codes")
}

GHashTable *
ephy_langs_iso_639_table (void)
{
	GHashTable *table;

	ephy_langs_bind_iso_domains ();
	table = g_hash_table_new_full (g_str_hash, g_str_equal,
				       (GDestroyNotify) xmlFree,
				       (GDestroyNotify) xmlFree);

	load_iso_entries (639, (GFunc) read_iso_639_entry, table);

	return table;
}

GHashTable *
ephy_langs_iso_3166_table (void)
{
	GHashTable *table;

	ephy_langs_bind_iso_domains ();
	table = g_hash_table_new_full (g_str_hash, g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) xmlFree);
	
	load_iso_entries (3166, (GFunc) read_iso_3166_entry, table);

	return table;
}

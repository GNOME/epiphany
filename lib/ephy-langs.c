/*
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

#include "config.h"

#include "ephy-langs.h"

#include <glib/gi18n.h>

#include <string.h>

static const EphyFontsLanguageInfo font_languages [] =
{
	{ N_("Arabic"),					"ar" },
	{ N_("Baltic"),					"x-baltic" },
	{ N_("Central European"),			"x-central-euro" },
	{ N_("Cyrillic"),				"x-cyrillic" },
	{ N_("Devanagari"),				"x-devanagari" },
	{ N_("Greek"),					"el" },
	{ N_("Hebrew"),					"he" },
	{ N_("Japanese"),				"ja" },
	{ N_("Korean"),					"ko" },
	{ N_("Simplified Chinese"),			"zh-CN" },
	{ N_("Tamil"),					"x-tamil" },
	{ N_("Thai"),					"th" },
	{ N_("Traditional Chinese"),			"zh-TW" },
	{ N_("Traditional Chinese (Hong Kong)"),	"zh-HK" },
	{ N_("Turkish"),				"tr" },
	{ N_("Unicode"),				"x-unicode" },
	{ N_("Western"),				"x-western" }
};
static const guint n_font_languages = G_N_ELEMENTS (font_languages);

const EphyFontsLanguageInfo *
ephy_font_languages (void)
{
	return font_languages;
}

guint			 
ephy_font_n_languages (void)
{
	return n_font_languages;
}

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
	const char *system_lang;
	char *lang;
	int i;

	/**
	* This is a comma separated list of language ranges, as specified
	* by RFC 2616, 14.4.
	* Always include the basic language code last.
	*
	* Examples:
	* "pt"    translation: "pt"
	* "pt_BR" translation: "pt-br,pt"
	* "zh_CN" translation: "zh-cn,zh"
	* "zh_HK" translation: "zh-hk,zh" or maybe "zh-hk,zh-tw,zh"
	*/
	system_lang = _("system-language");

	/* FIXME: use system_language when given, instead of g_get_language_names () ? */
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

	return (char **) g_array_free (array, FALSE);
}

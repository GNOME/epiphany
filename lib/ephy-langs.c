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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-langs.h"
#include "ephy-string.h"
#include <bonobo/bonobo-i18n.h>
#include <string.h>

static const FontsLanguageInfo font_languages[] =
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
#if MOZILLA_SNAPSHOT >= 11
	{ N_("Traditional Chinese (Hong Kong)"),	"zh-HK" },
#endif
	{ N_("Turkish"),				"tr" },
	{ N_("Unicode"),				"x-unicode" },
	{ N_("Western"),				"x-western" }
};
static const guint n_font_languages = G_N_ELEMENTS (font_languages);

static const
struct
{
	EphyLanguageGroup group;
	char *title;
}
lang_group_names[] =
{
        { LG_ARABIC,		N_("_Arabic")		},
        { LG_BALTIC,		N_("_Baltic")		},
        { LG_CENTRAL_EUROPEAN,	N_("Central _European") },
        { LG_CHINESE,		N_("Chi_nese")		},
        { LG_CYRILLIC,		N_("_Cyrillic")		},
        { LG_GREEK,		N_("_Greek")		},
        { LG_HEBREW,		N_("_Hebrew")		},
        { LG_INDIAN,		N_("_Indian")		},
        { LG_JAPANESE,		N_("_Japanese")		},
        { LG_KOREAN,		N_("_Korean")		},
        { LG_TURKISH,		N_("_Turkish")		},
        { LG_UNICODE,		N_("_Unicode")		},
        { LG_VIETNAMESE,	N_("_Vietnamese")	},
        { LG_WESTERN,		N_("_Western")		},
        { LG_OTHER,		N_("_Other")		}
};
static const guint n_lang_group_names = G_N_ELEMENTS (lang_group_names);

void
ephy_lang_group_info_free (EphyLanguageGroupInfo *info)
{
	g_return_if_fail (info != NULL);

	g_free (info->title);
	g_free (info->key);

	g_free (info);
}

static gint
lang_group_info_cmp (const EphyLanguageGroupInfo *i1, const EphyLanguageGroupInfo *i2)
{
	return strcmp (i1->key, i2->key);
}

GList *
ephy_lang_get_group_list (void)
{
	GList *list = NULL;
	guint i;

	for (i = 0; i < n_lang_group_names; i++)
	{
		EphyLanguageGroupInfo *info;
		char *elided = NULL;

		info = g_new0 (EphyLanguageGroupInfo, 1);

		info->title = g_strdup (_(lang_group_names[i].title));
		info->group = lang_group_names[i].group;

		/* collate without underscores */
		elided = ephy_string_elide_underscores (info->title);
		info->key = g_utf8_collate_key (elided, -1);
		g_free (elided);

		list = g_list_prepend (list, info);
	}

	return g_list_sort (list, (GCompareFunc) lang_group_info_cmp);
}

static int
fonts_language_info_cmp (const FontsLanguageInfo *i1, const FontsLanguageInfo *i2)
{
	return g_utf8_collate (i1->title, i2->title);
}

GList *
ephy_font_langs_get_codes_list (void)
{
	guint i;
	GList *list = NULL;

	for (i=0; i < n_font_languages; i++)
	{
		list = g_list_prepend (list, font_languages[i].code);
	}

	return list;
}

GList *
ephy_font_langs_get_list (void)
{
	GList *list = NULL;
	guint i;

	for (i = 0; i < n_font_languages; i++)
	{
		FontsLanguageInfo *info;

		info = g_new0 (FontsLanguageInfo, 1);
		info->title = _(font_languages[i].title);
		info->code = font_languages[i].code;

		list = g_list_prepend (list, info);
	}

	return g_list_sort (list, (GCompareFunc) fonts_language_info_cmp);
}

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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ephy-langs.h"
#include <bonobo/bonobo-i18n.h>

static const FontsLanguageInfo font_languages[] =
{
	{ N_("Arabic"),			"ar" },
	{ N_("Baltic"),			"x-baltic" },
	{ N_("Central European"),	"x-central-euro" },
	{ N_("Cyrillic"),		"x-cyrillic" },
	{ N_("Devanagari"),		"x-devanagari" },
	{ N_("Greek"),			"el" },
	{ N_("Hebrew"),			"he" },
	{ N_("Japanese"),		"ja" },
	{ N_("Korean"),			"ko" },
	{ N_("Simplified Chinese"),	"zh-CN" },
	{ N_("Tamil"),			"x-tamil" },
	{ N_("Thai"),			"th" },
	{ N_("Traditional Chinese"),	"zh-TW" },
	{ N_("Turkish"),		"tr" },
	{ N_("Unicode"),		"x-unicode" },
	{ N_("Western"),		"x-western" },
};
static const guint n_font_languages = G_N_ELEMENTS (font_languages);

void
language_group_info_free (LanguageGroupInfo *info)
{
	g_return_if_fail (info != NULL);

	g_free (info->title);
	g_free (info->key);

	g_free (info);
}

void
encoding_info_free (EncodingInfo *info)
{
	g_return_if_fail (info != NULL);

	g_free (info->title);
	g_free (info->key);
	g_free (info->encoding);

	g_free (info);
}

guint
ephy_langs_get_n_font_languages (void)
{
	return n_font_languages;
}

const FontsLanguageInfo *
ephy_langs_get_font_languages (void)
{
	return font_languages;
}

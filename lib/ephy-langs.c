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

#include <glib/gi18n.h>

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

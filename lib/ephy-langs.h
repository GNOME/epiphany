/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
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

#ifndef EPHY_LANGS_H
#define EPHY_LANGS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

G_BEGIN_DECLS

/* language groups */
typedef enum
{
	LG_ARABIC,
	LG_BALTIC,
	LG_CENTRAL_EUROPEAN,
	LG_CHINESE,
	LG_CYRILLIC,
	LG_GREEK,
	LG_HEBREW,
	LG_INDIAN,
	LG_JAPANESE,
	LG_KOREAN,
	LG_TURKISH,
	LG_UNICODE,
	LG_VIETNAMESE,
	LG_WESTERN,
	LG_OTHER,
	LG_ALL
} LanguageGroup;

typedef struct
{
	gchar *title;
	gchar *key;
	LanguageGroup group;
} LanguageGroupInfo;

typedef struct
{
	gchar *title;
	gchar *key;
	gchar *encoding;
	LanguageGroup group;
} EncodingInfo;

/* language encoding groups */
typedef struct
{
	gchar *title;
	gchar* code;
} FontsLanguageInfo;

guint			 ephy_langs_get_n_font_languages (void);

const FontsLanguageInfo *ephy_langs_get_font_languages   (void);

void			 language_group_info_free	 (LanguageGroupInfo *info);

void			 encoding_info_free		 (EncodingInfo *info);


G_END_DECLS

#endif

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

#include <glib.h>

G_BEGIN_DECLS

#define LANG_ENC_NUM 14

typedef enum {
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
	LG_OTHER
} LanguageGroup;

typedef struct {
	char *charset_title;
	char *charset_name;
	LanguageGroup lgroup;
} CharsetInfoPriv;

/* language groups names */
extern const char *lgroups[];
/* translated charset titles */
extern const CharsetInfoPriv charset_trans_array[];

/* FIXME */
extern const char *lang_encode_name[LANG_ENC_NUM];
extern const char *lang_encode_item[LANG_ENC_NUM];

gint get_translated_cscount (void);

G_END_DECLS

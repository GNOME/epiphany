/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "ephy-encodings.h"
#include "ephy-string.h"
#include <bonobo/bonobo-i18n.h>
#include <string.h>

/**
 * translatable encodings titles
 * NOTE: if you add /change encodings, please also update the schema file
 * epiphany.schemas.in
 */
static const 
struct
{
	char *title;
	char *name;
	EphyLanguageGroup group;
}
encodings[] =
{ 
	/* translators: access keys need only be unique within the same LG_group */
	{ N_("Arabic (_IBM-864)"),                  "IBM864",                LG_ARABIC },
	{ N_("Arabic (ISO-_8859-6)"),               "ISO-8859-6",            LG_ARABIC },
	{ N_("Arabic (_MacArabic)"),                "x-mac-arabic",          LG_ARABIC },
	{ N_("Arabic (_Windows-1256)"),             "windows-1256",          LG_ARABIC },
	{ N_("Baltic (_ISO-8859-13)"),              "ISO-8859-13",           LG_BALTIC },
	{ N_("Baltic (I_SO-8859-4)"),               "ISO-8859-4",            LG_BALTIC },
	{ N_("Baltic (_Windows-1257)"),             "windows-1257",          LG_BALTIC },
	{ N_("Central European (_IBM-852)"),        "IBM852",                LG_CENTRAL_EUROPEAN },
	{ N_("Central European (I_SO-8859-2)"),     "ISO-8859-2",	     LG_CENTRAL_EUROPEAN },
	{ N_("Central European (_MacCE)"),          "x-mac-ce",              LG_CENTRAL_EUROPEAN },
	{ N_("Central European (_Windows-1250)"),   "windows-1250",          LG_CENTRAL_EUROPEAN },
	{ N_("Croatian (Mac_Croatian)"),            "x-mac-croatian",        LG_CENTRAL_EUROPEAN },
	{ N_("Chinese Simplified (_GB18030)"),      "gb18030",               LG_CHINESE },
	{ N_("Chinese Simplified (G_B2312)"),       "GB2312",                LG_CHINESE },
	{ N_("Chinese Simplified (GB_K)"),          "x-gbk",                 LG_CHINESE },
	{ N_("Chinese Simplified (_HZ)"),           "HZ-GB-2312",	     LG_CHINESE },
	{ N_("Chinese Simplified (_ISO-2022-CN)"),  "ISO-2022-CN",           LG_CHINESE },
	{ N_("Chinese Traditional (Big_5)"),        "Big5",                  LG_CHINESE },
	{ N_("Chinese Traditional (Big5-HK_SCS)"),  "Big5-HKSCS",	     LG_CHINESE },
	{ N_("Chinese Traditional (_EUC-TW)"),      "x-euc-tw",              LG_CHINESE },
	{ N_("Cyrillic (_IBM-855)"),                "IBM855",                LG_CYRILLIC },
	{ N_("Cyrillic (I_SO-8859-5)"),             "ISO-8859-5",	     LG_CYRILLIC },
	{ N_("Cyrillic (IS_O-IR-111)"),             "ISO-IR-111",	     LG_CYRILLIC },
	{ N_("Cyrillic (_KOI8-R)"),                 "KOI8-R",                LG_CYRILLIC },
	{ N_("Cyrillic (_MacCyrillic)"),            "x-mac-cyrillic",        LG_CYRILLIC },
	{ N_("Cyrillic (_Windows-1251)"),           "windows-1251",          LG_CYRILLIC },
	{ N_("Cyrillic/Russian (_CP-866)"),         "IBM866",                LG_CYRILLIC },
	{ N_("Cyrillic/Ukrainian (_KOI8-U)"),       "KOI8-U",                LG_CYRILLIC },
	{ N_("Cyrillic/Ukrainian (Mac_Ukrainian)"), "x-mac-ukrainian",       LG_CYRILLIC },
	{ N_("Greek (_ISO-8859-7)"),                "ISO-8859-7",            LG_GREEK },
	{ N_("Greek (_MacGreek)"),                  "x-mac-greek",           LG_GREEK },
	{ N_("Greek (_Windows-1253)"),              "windows-1253",          LG_GREEK },
	{ N_("Gujarati (_MacGujarati)"),            "x-mac-gujarati",        LG_INDIAN },
	{ N_("Gurmukhi (Mac_Gurmukhi)"),            "x-mac-gurmukhi",        LG_INDIAN },
	{ N_("Hindi (Mac_Devanagari)"),             "x-mac-devanagari",      LG_INDIAN },
	{ N_("Hebrew (_IBM-862)"),                  "IBM862",                LG_HEBREW },
	{ N_("Hebrew (IS_O-8859-8-I)"),             "ISO-8859-8-I",          LG_HEBREW },
	{ N_("Hebrew (_MacHebrew)"),                "x-mac-hebrew",          LG_HEBREW },
	{ N_("Hebrew (_Windows-1255)"),             "windows-1255",          LG_HEBREW },
	{ N_("_Visual Hebrew (ISO-8859-8)"),        "ISO-8859-8",            LG_HEBREW },
	{ N_("Japanese (_EUC-JP)"),                 "EUC-JP",                LG_JAPANESE },
	{ N_("Japanese (_ISO-2022-JP)"),            "ISO-2022-JP",           LG_JAPANESE },
	{ N_("Japanese (_Shift-JIS)"),              "Shift_JIS",             LG_JAPANESE },
	{ N_("Korean (_EUC-KR)"),                   "EUC-KR",                LG_KOREAN },
	{ N_("Korean (_ISO-2022-KR)"),              "ISO-2022-KR",           LG_KOREAN },
	{ N_("Korean (_JOHAB)"),                    "x-johab",               LG_KOREAN },
	{ N_("Korean (_UHC)"),                      "x-windows-949",         LG_KOREAN },
	{ N_("Turkish (_IBM-857)"),                 "IBM857",                LG_TURKISH },
	{ N_("Turkish (I_SO-8859-9)"),              "ISO-8859-9",            LG_TURKISH },
	{ N_("Turkish (_MacTurkish)"),              "x-mac-turkish",         LG_TURKISH },
	{ N_("Turkish (_Windows-1254)"),            "windows-1254",          LG_TURKISH },
	{ N_("Unicode (UTF-_7)"),                   "UTF-7",                 LG_UNICODE },
	{ N_("Unicode (UTF-_8)"),                   "UTF-8",                 LG_UNICODE },
	{ N_("Vietnamese (_TCVN)"),                 "x-viet-tcvn5712",       LG_VIETNAMESE },
	{ N_("Vietnamese (_VISCII)"),               "VISCII",                LG_VIETNAMESE },
	{ N_("Vietnamese (V_PS)"),                  "x-viet-vps",            LG_VIETNAMESE },
	{ N_("Vietnamese (_Windows-1258)"),         "windows-1258",          LG_VIETNAMESE },
	{ N_("Western (_IBM-850)"),                 "IBM850",                LG_WESTERN },
	{ N_("Western (I_SO-8859-1)"),              "ISO-8859-1",            LG_WESTERN },
	{ N_("Western (IS_O-8859-15)"),             "ISO-8859-15",           LG_WESTERN },
	{ N_("Western (_MacRoman)"),                "x-mac-roman",           LG_WESTERN },
	{ N_("Western (_Windows-1252)"),            "windows-1252",          LG_WESTERN },
	{ N_("_Armenian (ARMSCII-8)"),              "armscii-8",             LG_OTHER },
	{ N_("_Celtic (ISO-8859-14)"),              "ISO-8859-14",           LG_OTHER },
	{ N_("_Farsi (MacFarsi)"),                  "x-mac-farsi",           LG_OTHER },
	{ N_("_Georgian (GEOSTD8)"),                "geostd8",               LG_OTHER },
	{ N_("_Icelandic (MacIcelandic)"),          "x-mac-icelandic",       LG_OTHER },
	{ N_("_Nordic (ISO-8859-10)"),              "ISO-8859-10",           LG_OTHER },
	{ N_("_Romanian (MacRomanian)"),            "x-mac-romanian",        LG_OTHER },
	{ N_("R_omanian (ISO-8859-16)"),            "ISO-8859-16",           LG_OTHER },
	{ N_("South _European (ISO-8859-3)"),       "ISO-8859-3",            LG_OTHER },
	{ N_("Thai (TIS-_620)"),                    "TIS-620",               LG_OTHER },
#if MOZILLA_SNAPSHOT >= 10 
	{ N_("Thai (IS_O-8859-11)"),                "iso-8859-11",           LG_OTHER },
	{ N_("_Thai (Windows-874)"),                "windows-874",           LG_OTHER },
#endif	
	{ N_("_User Defined"),                      "x-user-defined",        LG_OTHER },
};
static const guint n_encodings = G_N_ELEMENTS (encodings);

void
ephy_encoding_info_free (EphyEncodingInfo *info)
{
	g_return_if_fail (info != NULL);

	g_free (info->title);
	g_free (info->key);
	g_free (info->encoding);

	g_free (info);
}

static int
encodings_info_cmp (const EphyEncodingInfo *i1, const EphyEncodingInfo *i2)
{
	return strcmp (i1->key, i2->key);
}

GList *
ephy_encodings_get_list (EphyLanguageGroup group, gboolean elide_underscores)
{
	GList *list = NULL;
	guint i;

	for (i = 0; i < n_encodings; i++)
	{
		if (group == LG_ALL || group == encodings[i].group)
		{
			EphyEncodingInfo *info;
			char *elided = NULL;

			info = g_new0 (EphyEncodingInfo, 1);

			info->group = encodings[i].group;
			info->encoding = g_strdup (encodings[i].name);

			elided = ephy_string_elide_underscores (_(encodings[i].title));

			/* collate without underscores */
			info->key = g_utf8_collate_key (elided, -1);

			if (elide_underscores)
			{
				info->title = elided;
			}
			else
			{
				g_free (elided);

				info->title = g_strdup (_(encodings[i].title));
			}

			list = g_list_prepend (list, info);
		}
	}

	return g_list_sort (list, (GCompareFunc) encodings_info_cmp);
}

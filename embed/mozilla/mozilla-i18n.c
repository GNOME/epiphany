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

#include "mozilla-i18n.h"

#include <bonobo/bonobo-i18n.h>

const char *lgroups[] = {
        N_("Arabic"),
        N_("Baltic"),
        N_("Central European"),
        N_("Chinese"),
        N_("Cyrillic"),
        N_("Greek"),
        N_("Hebrew"),
        N_("Indian"),
        N_("Japanese"), 
        N_("Korean"),
        N_("Turkish"),
        N_("Unicode"),
        N_("Vietnamese"),
        N_("Western"),
        N_("Other"),
	NULL
};

const CharsetInfoPriv charset_trans_array[] = { 
	{N_("Arabic (IBM-864)"),                  "IBM864",                LG_ARABIC},
	{N_("Arabic (IBM-864-I)"),                "IBM864i",               LG_ARABIC},
	{N_("Arabic (ISO-8859-6)"),               "ISO-8859-6",            LG_ARABIC},
	{N_("Arabic (ISO-8859-6-E)"),             "ISO-8859-6-E",          LG_ARABIC},
	{N_("Arabic (ISO-8859-6-I)"),             "ISO-8859-6-I",          LG_ARABIC},
	{N_("Arabic (MacArabic)"),                "x-mac-arabic",          LG_ARABIC},
	{N_("Arabic (Windows-1256)"),             "windows-1256",          LG_ARABIC},
	{N_("Armenian (ARMSCII-8)"),              "armscii-8", 	           LG_OTHER},
	{N_("Baltic (ISO-8859-13)"),              "ISO-8859-13",           LG_BALTIC},
	{N_("Baltic (ISO-8859-4)"),               "ISO-8859-4",            LG_BALTIC},
	{N_("Baltic (Windows-1257)"),             "windows-1257",          LG_BALTIC},
	{N_("Celtic (ISO-8859-14)"),              "ISO-8859-14",           LG_OTHER},
	{N_("Central European (IBM-852)"),        "IBM852",                LG_CENTRAL_EUROPEAN},
	{N_("Central European (ISO-8859-2)"),     "ISO-8859-2",	           LG_CENTRAL_EUROPEAN},
	{N_("Central European (MacCE)"),          "x-mac-ce",              LG_CENTRAL_EUROPEAN},
	{N_("Central European (Windows-1250)"),   "windows-1250",          LG_CENTRAL_EUROPEAN},
	{N_("Chinese Simplified (GB18030)"),      "gb18030",               LG_CHINESE},
	{N_("Chinese Simplified (GB2312)"),       "GB2312",                LG_CHINESE},
	{N_("Chinese Simplified (GBK)"),          "x-gbk",                 LG_CHINESE},
	{N_("Chinese Simplified (HZ)"),           "HZ-GB-2312",	           LG_CHINESE},
	{N_("Chinese Simplified (Windows-936)"),  "windows-936",           LG_CHINESE},
	{N_("Chinese Traditional (Big5)"),        "Big5",                  LG_CHINESE},
	{N_("Chinese Traditional (Big5-HKSCS)"),  "Big5-HKSCS",	           LG_CHINESE},
	{N_("Chinese Traditional (EUC-TW)"),      "x-euc-tw",              LG_CHINESE},
	{N_("Croatian (MacCroatian)"),            "x-mac-croatian",        LG_CENTRAL_EUROPEAN},
	{N_("Cyrillic (IBM-855)"),                "IBM855",                LG_CYRILLIC},
	{N_("Cyrillic (ISO-8859-5)"),             "ISO-8859-5",	           LG_CYRILLIC},
	{N_("Cyrillic (ISO-IR-111)"),             "ISO-IR-111",	           LG_CYRILLIC},
	{N_("Cyrillic (KOI8-R)"),                 "KOI8-R",                LG_CYRILLIC},
	{N_("Cyrillic (MacCyrillic)"),            "x-mac-cyrillic",        LG_CYRILLIC},
	{N_("Cyrillic (Windows-1251)"),           "windows-1251",          LG_CYRILLIC},
	{N_("Cyrillic/Russian (CP-866)"),         "IBM866",                LG_CYRILLIC},
	{N_("Cyrillic/Ukrainian (KOI8-U)"),       "KOI8-U",                LG_CYRILLIC},
	{N_("Cyrillic/Ukrainian (MacUkrainian)"), "x-mac-ukrainian",       LG_CYRILLIC},
	{N_("English (US-ASCII)"),                "us-ascii",              LG_WESTERN},
	{N_("Farsi (MacFarsi)"),                  "x-mac-farsi",           LG_OTHER},
	{N_("Georgian (GEOSTD8)"),                "geostd8",               LG_OTHER},
	{N_("Greek (ISO-8859-7)"),                "ISO-8859-7",            LG_GREEK},
	{N_("Greek (MacGreek)"),                  "x-mac-greek",           LG_GREEK},
	{N_("Greek (Windows-1253)"),              "windows-1253",          LG_GREEK},
	{N_("Gujarati (MacGujarati)"),            "x-mac-gujarati",        LG_INDIAN},
	{N_("Gurmukhi (MacGurmukhi)"),            "x-mac-gurmukhi",        LG_INDIAN},
	{N_("Hebrew (IBM-862)"),                  "IBM862",                LG_HEBREW},
	{N_("Hebrew (ISO-8859-8-E)"),             "ISO-8859-8-E",          LG_HEBREW},
	{N_("Hebrew (ISO-8859-8-I)"),             "ISO-8859-8-I",          LG_HEBREW},
	{N_("Hebrew (MacHebrew)"),                "x-mac-hebrew",          LG_HEBREW},
	{N_("Hebrew (Windows-1255)"),             "windows-1255",          LG_HEBREW},
	{N_("Hindi (MacDevanagari)"),             "x-mac-devanagari",      LG_INDIAN},
	{N_("Icelandic (MacIcelandic)"),          "x-mac-icelandic",       LG_OTHER},
	{N_("Japanese (EUC-JP)"),                 "EUC-JP",                LG_JAPANESE},
	{N_("Japanese (ISO-2022-JP)"),            "ISO-2022-JP",           LG_JAPANESE},
	{N_("Japanese (Shift_JIS)"),              "Shift_JIS",             LG_JAPANESE},
	{N_("Korean (EUC-KR)"),                   "EUC-KR",                LG_KOREAN},
	{N_("Korean (ISO-2022-KR)"),              "ISO-2022-KR",           LG_KOREAN},
	{N_("Korean (JOHAB)"),                    "x-johab",               LG_KOREAN},
	{N_("Korean (UHC)"),                      "x-windows-949",         LG_KOREAN},
	{N_("Nordic (ISO-8859-10)"),              "ISO-8859-10",           LG_OTHER},
	{N_("Romanian (MacRomanian)"),            "x-mac-romanian",        LG_OTHER},
	{N_("Romanian (ISO-8859-16)"),            "ISO-8859-16",           LG_OTHER},
	{N_("South European (ISO-8859-3)"),       "ISO-8859-3",            LG_OTHER},
	{N_("Thai (TIS-620)"),                    "TIS-620",               LG_OTHER},
	{N_("Turkish (IBM-857)"),                 "IBM857",                LG_TURKISH},
	{N_("Turkish (ISO-8859-9)"),              "ISO-8859-9",            LG_TURKISH},
	{N_("Turkish (MacTurkish)"),              "x-mac-turkish",         LG_TURKISH},
	{N_("Turkish (Windows-1254)"),            "windows-1254",          LG_TURKISH},
	{N_("Unicode (UTF-7)"),                   "UTF-7",                 LG_UNICODE},
	{N_("Unicode (UTF-8)"),                   "UTF-8",                 LG_UNICODE},
	{N_("Unicode (UTF-16BE)"),                "UTF-16BE",              LG_UNICODE},
	{N_("Unicode (UTF-16LE)"),                "UTF-16LE",              LG_UNICODE},
	{N_("Unicode (UTF-32BE)"),                "UTF-32BE",              LG_UNICODE},
	{N_("Unicode (UTF-32LE)"),                "UTF-32LE",              LG_UNICODE},
	{N_("User Defined"),                      "x-user-defined",        LG_OTHER},
	{N_("Vietnamese (TCVN)"),                 "x-viet-tcvn5712",       LG_VIETNAMESE},
	{N_("Vietnamese (VISCII)"),               "VISCII",                LG_VIETNAMESE},
	{N_("Vietnamese (VPS)"),                  "x-viet-vps",            LG_VIETNAMESE},
	{N_("Vietnamese (Windows-1258)"),         "windows-1258",          LG_VIETNAMESE},
	{N_("Visual Hebrew (ISO-8859-8)"),        "ISO-8859-8",            LG_HEBREW},
	{N_("Western (IBM-850)"),                 "IBM850",                LG_WESTERN},
	{N_("Western (ISO-8859-1)"),              "ISO-8859-1",            LG_WESTERN},
	{N_("Western (ISO-8859-15)"),             "ISO-8859-15",           LG_WESTERN},
	{N_("Western (MacRoman)"),                "x-mac-roman",           LG_WESTERN},
	{N_("Western (Windows-1252)"),            "windows-1252",          LG_WESTERN},
	/* charsets whithout posibly translatable names */
	{"T61.8bit",                              "T61.8bit",              LG_OTHER},
	{"x-imap4-modified-utf7",                 "x-imap4-modified-utf7", LG_UNICODE},
	{"x-u-escaped",                           "x-u-escaped",           LG_OTHER}
};

const gchar *lang_encode_item[LANG_ENC_NUM] =
{
	"x-western",
	"x-central-euro",
	"ja",
	"zh-TW",
	"zh-CN",
	"ko",
	"x-cyrillic",
	"x-baltic",
	"el",
	"tr",
	"x-unicode",
	"th",
	"he",
	"ar"
};

const gchar *font_types[] =
{
        "serif",
        "sans-serif",
        "cursive",
        "fantasy",
        "monospace"
};

extern gint
get_translated_cscount(void)
{
	return sizeof (charset_trans_array) / sizeof ((charset_trans_array)[0]);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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

#include "ephy-encodings.h"
#include "ephy-node-db.h"
#include "ephy-file-helpers.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <string.h>

#define EPHY_ENCODINGS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ENCODINGS, EphyEncodingsPrivate))

struct _EphyEncodingsPrivate
{
	EphyNodeDb *db;
	EphyNode *root;
	EphyNode *encodings;
	GHashTable *hash;
	GSList *recent;
};

/*
 * translatable encodings titles
 * NOTE: if you add/remove/change encodings, please also update the schema in
 * epiphany.schemas.in
 */
static const 
struct
{
	char *title;
	char *code;
	EphyLanguageGroup groups;
}
encoding_entries [] =
{ 
	{ N_("Arabic (_IBM-864)"),                  "IBM864",                LG_ARABIC  },
	{ N_("Arabic (ISO-_8859-6)"),               "ISO-8859-6",            LG_ARABIC },
	{ N_("Arabic (_MacArabic)"),                "x-mac-arabic",          LG_ARABIC },
	{ N_("Arabic (_Windows-1256)"),             "windows-1256",          LG_ARABIC },
	{ N_("Baltic (_ISO-8859-13)"),              "ISO-8859-13",           LG_BALTIC },
	{ N_("Baltic (I_SO-8859-4)"),               "ISO-8859-4",            LG_BALTIC },
	{ N_("Baltic (_Windows-1257)"),             "windows-1257",          LG_BALTIC },
	{ N_("_Armenian (ARMSCII-8)"),              "armscii-8",             LG_CAUCASIAN },
	{ N_("_Georgian (GEOSTD8)"),                "geostd8",               LG_CAUCASIAN },
	{ N_("Central European (_IBM-852)"),        "IBM852",                LG_C_EUROPEAN },
	{ N_("Central European (I_SO-8859-2)"),     "ISO-8859-2",	     LG_C_EUROPEAN },
	{ N_("Central European (_MacCE)"),          "x-mac-ce",              LG_C_EUROPEAN },
	{ N_("Central European (_Windows-1250)"),   "windows-1250",          LG_C_EUROPEAN },
	{ N_("Chinese Simplified (_GB18030)"),      "gb18030",               LG_CHINESE_SIMP },
	{ N_("Chinese Simplified (G_B2312)"),       "GB2312",                LG_CHINESE_SIMP },
	{ N_("Chinese Simplified (GB_K)"),          "x-gbk",                 LG_CHINESE_SIMP },
	{ N_("Chinese Simplified (_HZ)"),           "HZ-GB-2312",	     LG_CHINESE_SIMP },
	{ N_("Chinese Simplified (_ISO-2022-CN)"),  "ISO-2022-CN",           LG_CHINESE_SIMP },
	{ N_("Chinese Traditional (Big_5)"),        "Big5",                  LG_CHINESE_TRAD },
	{ N_("Chinese Traditional (Big5-HK_SCS)"),  "Big5-HKSCS",	     LG_CHINESE_TRAD },
	{ N_("Chinese Traditional (_EUC-TW)"),      "x-euc-tw",              LG_CHINESE_TRAD },
	{ N_("Cyrillic (_IBM-855)"),                "IBM855",                LG_CYRILLIC },
	{ N_("Cyrillic (I_SO-8859-5)"),             "ISO-8859-5",	     LG_CYRILLIC },
	{ N_("Cyrillic (IS_O-IR-111)"),             "ISO-IR-111",	     LG_CYRILLIC },
	{ N_("Cyrillic (_KOI8-R)"),                 "KOI8-R",                LG_CYRILLIC },
	{ N_("Cyrillic (_MacCyrillic)"),            "x-mac-cyrillic",        LG_CYRILLIC },
	{ N_("Cyrillic (_Windows-1251)"),           "windows-1251",          LG_CYRILLIC },
	{ N_("Cyrillic/_Russian (IBM-866)"),        "IBM866",                LG_CYRILLIC },
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
	{ N_("_Celtic (ISO-8859-14)"),              "ISO-8859-14",           LG_NORDIC },
	{ N_("_Icelandic (MacIcelandic)"),          "x-mac-icelandic",       LG_NORDIC },
	{ N_("_Nordic (ISO-8859-10)"),              "ISO-8859-10",           LG_NORDIC },
	{ N_("_Persian (MacFarsi)"),                "x-mac-farsi",           LG_PERSIAN },
	{ N_("Croatian (Mac_Croatian)"),            "x-mac-croatian",        LG_SE_EUROPEAN },
	{ N_("_Romanian (MacRomanian)"),            "x-mac-romanian",        LG_SE_EUROPEAN },
	{ N_("R_omanian (ISO-8859-16)"),            "ISO-8859-16",           LG_SE_EUROPEAN },
	{ N_("South _European (ISO-8859-3)"),	    "ISO-8859-3",            LG_SE_EUROPEAN },
	{ N_("Thai (TIS-_620)"),                    "TIS-620",               LG_THAI },
	{ N_("Thai (IS_O-8859-11)"),                "iso-8859-11",           LG_THAI },
	{ N_("_Thai (Windows-874)"),                "windows-874",           LG_THAI },
	{ N_("Turkish (_IBM-857)"),                 "IBM857",                LG_TURKISH },
	{ N_("Turkish (I_SO-8859-9)"),              "ISO-8859-9",            LG_TURKISH },
	{ N_("Turkish (_MacTurkish)"),              "x-mac-turkish",         LG_TURKISH },
	{ N_("Turkish (_Windows-1254)"),            "windows-1254",          LG_TURKISH },
	{ N_("Unicode (UTF-_8)"),                   "UTF-8",                 LG_UNICODE },
	{ N_("Cyrillic/Ukrainian (_KOI8-U)"),       "KOI8-U",                LG_UKRAINIAN },
	{ N_("Cyrillic/Ukrainian (Mac_Ukrainian)"), "x-mac-ukrainian",       LG_UKRAINIAN },
	{ N_("Vietnamese (_TCVN)"),                 "x-viet-tcvn5712",       LG_VIETNAMESE },
	{ N_("Vietnamese (_VISCII)"),               "VISCII",                LG_VIETNAMESE },
	{ N_("Vietnamese (V_PS)"),                  "x-viet-vps",            LG_VIETNAMESE },
	{ N_("Vietnamese (_Windows-1258)"),         "windows-1258",          LG_VIETNAMESE },
	{ N_("Western (_IBM-850)"),                 "IBM850",                LG_WESTERN },
	{ N_("Western (_ISO-8859-1)"),              "ISO-8859-1",            LG_WESTERN },
	{ N_("Western (IS_O-8859-15)"),             "ISO-8859-15",           LG_WESTERN },
	{ N_("Western (_MacRoman)"),                "x-mac-roman",           LG_WESTERN },
	{ N_("Western (_Windows-1252)"),            "windows-1252",          LG_WESTERN },

	/* the following encodings are so rarely used that we don't want to pollute the "related"
	 * part of the encodings menu with them, so we set the language group to 0 here
	 */
	{ N_("English (_US-ASCII)"),                "us-ascii",              0 },
	{ N_("Unicode (UTF-_16 BE)"),               "UTF-16BE",              0 },
	{ N_("Unicode (UTF-1_6 LE)"),               "UTF-16LE",              0 },
	{ N_("Unicode (UTF-_32 BE)"),               "UTF-32BE",              0 },
	{ N_("Unicode (UTF-3_2 LE)"),               "UTF-32LE",              0 },
};

enum
{
	ALL_NODE_ID = 2,
	ENCODINGS_NODE_ID = 3,
	DETECTORS_NODE_ID = 5
};

#define RECENT_KEY	"/apps/epiphany/general/recent_encodings"
#define RECENT_MAX	4

static void ephy_encodings_class_init	(EphyEncodingsClass *klass);
static void ephy_encodings_init		(EphyEncodings *ma);

G_DEFINE_TYPE (EphyEncodings, ephy_encodings, G_TYPE_OBJECT)

static void
ephy_encodings_finalize (GObject *object)
{
	EphyEncodings *encodings = EPHY_ENCODINGS (object);

	g_hash_table_destroy (encodings->priv->hash);

	ephy_node_unref (encodings->priv->encodings);
	ephy_node_unref (encodings->priv->root);

	g_slist_foreach (encodings->priv->recent, (GFunc) g_free, NULL);
	g_slist_free (encodings->priv->recent);

	g_object_unref (encodings->priv->db);

	LOG ("EphyEncodings finalised");

	G_OBJECT_CLASS (ephy_encodings_parent_class)->finalize (object);
}

static void
ephy_encodings_class_init (EphyEncodingsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ephy_encodings_finalize;

	g_type_class_add_private (object_class, sizeof (EphyEncodingsPrivate));
}

/* copied from egg-toolbar-editor.c */
static char *
elide_underscores (const char *original)
{
	char *q, *result;
	const char *p;
	gboolean last_underscore;

	q = result = g_malloc (strlen (original) + 1);
	last_underscore = FALSE;

	for (p = original; *p; p++)
	{
		if (!last_underscore && *p == '_')
		{
			last_underscore = TRUE;
		}
		else
		{
			last_underscore = FALSE;
			*q++ = *p;
		}
	}

	*q = '\0';

	return result;
}

static EphyNode *
add_encoding (EphyEncodings *encodings,
	      const char *title,
	      const char *code,
	      EphyLanguageGroup groups)
{
	EphyNode *node;
	char *elided, *normalised;
	GValue value = { 0, };

	node = ephy_node_new (encodings->priv->db);

	ephy_node_set_property_string (node, EPHY_NODE_ENCODING_PROP_TITLE,
				       title);

	elided = elide_underscores (title);
	normalised = g_utf8_normalize (elided, -1, G_NORMALIZE_DEFAULT);

	g_value_init (&value, G_TYPE_STRING);
	g_value_take_string (&value, g_utf8_collate_key (normalised, -1));
	ephy_node_set_property (node, EPHY_NODE_ENCODING_PROP_COLLATION_KEY, &value);
	g_value_unset (&value);

	g_free (normalised);

	g_value_init (&value, G_TYPE_STRING);
	g_value_take_string (&value, elided);
	ephy_node_set_property (node, EPHY_NODE_ENCODING_PROP_TITLE_ELIDED, &value);
	g_value_unset (&value);

	ephy_node_set_property_string (node, EPHY_NODE_ENCODING_PROP_ENCODING,
				       code);

	ephy_node_set_property_int (node,
				    EPHY_NODE_ENCODING_PROP_LANGUAGE_GROUPS,
				    groups);
				    
	/* now insert the node in our structure */
	ephy_node_add_child (encodings->priv->root, node);
	g_hash_table_insert (encodings->priv->hash, g_strdup (code), node);

        ephy_node_add_child (encodings->priv->encodings, node);

	return node;
}

EphyNode *
ephy_encodings_get_node (EphyEncodings *encodings,
			 const char *code,
			 gboolean add_if_not_found)
{
	EphyNode *node;

	g_return_val_if_fail (EPHY_IS_ENCODINGS (encodings), NULL);

	node = g_hash_table_lookup (encodings->priv->hash, code);

	/* if it doesn't exist, add a node for it */
	if (!EPHY_IS_NODE (node) && add_if_not_found)
	{
		char *title;

		/* translators: this is the title that an unknown encoding will
		 * be displayed as.
		 */
		title = g_strdup_printf (_("Unknown (%s)"), code);
		node = add_encoding (encodings, title, code, 0);
		g_free (title);
	}

	return node;
}

GList *
ephy_encodings_get_encodings (EphyEncodings *encodings,
			      EphyLanguageGroup group_mask)
{
	GList *list = NULL;
	GPtrArray *children;
	int i, n_items;

	children = ephy_node_get_children (encodings->priv->encodings);
	n_items = children->len;
	for (i = 0; i < n_items; i++)
	{
		EphyNode *kid;
		EphyLanguageGroup group;
															     
		kid = g_ptr_array_index (children, i);
		group = ephy_node_get_property_int
			(kid, EPHY_NODE_ENCODING_PROP_LANGUAGE_GROUPS);
															      
		if ((group & group_mask) != 0)
		{
			list = g_list_prepend (list, kid);
		}
	}

	return list;
}

EphyNode *
ephy_encodings_get_all (EphyEncodings *encodings)
{
	g_return_val_if_fail (EPHY_IS_ENCODINGS (encodings), NULL);

	return encodings->priv->encodings;
}

void
ephy_encodings_add_recent (EphyEncodings *encodings,
			   const char *code)
{
	GSList *element;

	g_return_if_fail (EPHY_IS_ENCODINGS (encodings));
	g_return_if_fail (code != NULL);
	
	if (ephy_encodings_get_node (encodings, code, FALSE) == NULL) return;

	/* keep the list elements unique */
	element = g_slist_find_custom (encodings->priv->recent, code,
				       (GCompareFunc) strcmp);
	if (element != NULL)
	{
		g_free (element->data);
		encodings->priv->recent =
			g_slist_remove_link (encodings->priv->recent, element);
	}

	/* add the new code upfront */
	encodings->priv->recent =
		g_slist_prepend (encodings->priv->recent, g_strdup (code));

	/* truncate the list if necessary; it's at most 1 element too much */
	if (g_slist_length (encodings->priv->recent) > RECENT_MAX)
	{
		GSList *tail;

		tail = g_slist_last (encodings->priv->recent);
		g_free (tail->data);
		encodings->priv->recent =
			g_slist_remove_link (encodings->priv->recent, tail);
	}

	/* persist the list */
	eel_gconf_set_string_list (RECENT_KEY, encodings->priv->recent);
}

GList *
ephy_encodings_get_recent (EphyEncodings *encodings)
{
	GSList *l;
	GList *list = NULL;

	for (l = encodings->priv->recent; l != NULL; l = l->next)
	{
		EphyNode *node;

		node = ephy_encodings_get_node (encodings, (char *) l->data, FALSE);
		g_return_val_if_fail (EPHY_IS_NODE (node), NULL);

		list = g_list_prepend (list, node);
	}

	return list;
}

static void
ephy_encodings_init (EphyEncodings *encodings)
{
	EphyNodeDb *db;
	GSList *list, *l;
	guint i;

	encodings->priv = EPHY_ENCODINGS_GET_PRIVATE (encodings);

	LOG ("EphyEncodings initialising");

	db = ephy_node_db_new ("EncodingsDB");
	encodings->priv->db = db;

	encodings->priv->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						       (GDestroyNotify) g_free,
						       NULL);

	encodings->priv->root = ephy_node_new_with_id (db, ALL_NODE_ID);
	encodings->priv->encodings = ephy_node_new_with_id (db, ENCODINGS_NODE_ID);

	/* now fill the db */
	for (i = 0; i < G_N_ELEMENTS (encoding_entries); i++)
	{
		add_encoding (encodings,
                              _(encoding_entries[i].title),
			      encoding_entries[i].code,
			      encoding_entries[i].groups);
	}

	/* get the list of recently used encodings */
	list = eel_gconf_get_string_list (RECENT_KEY);

	/* make sure the list has no duplicates (GtkUIManager goes
	 * crazy otherwise), and only valid entries
	 */
	encodings->priv->recent = NULL;
	for (l = list; l != NULL; l = l->next)
	{
		if (g_slist_find (encodings->priv->recent, l->data) == NULL
		    && g_slist_length (encodings->priv->recent) < RECENT_MAX
		    && ephy_encodings_get_node (encodings, l->data, FALSE) != NULL)
		{
			encodings->priv->recent =
				g_slist_prepend (encodings->priv->recent,
						 l->data);
		}
		else
		{
			g_free (l->data);
		}
	}
	encodings->priv->recent = g_slist_reverse (encodings->priv->recent);
	g_slist_free (list);
}

EphyEncodings *
ephy_encodings_new (void)
{
	return g_object_new (EPHY_TYPE_ENCODINGS, NULL);
}

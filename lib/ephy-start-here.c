/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#include <libxml/tree.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "ephy-start-here.h"
#include "ephy-file-helpers.h"

//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

struct EphyStartHerePrivate
{
	const GList *langs;
};

static void
ephy_start_here_class_init (EphyStartHereClass *klass);
static void
ephy_start_here_init (EphyStartHere *tab);
static void
ephy_start_here_finalize (GObject *object);

static GObjectClass *parent_class = NULL;

GType
ephy_start_here_get_type (void)
{
        static GType ephy_start_here_type = 0;

        if (ephy_start_here_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyStartHereClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_start_here_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyStartHere),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_start_here_init
                };

                ephy_start_here_type = g_type_register_static (G_TYPE_OBJECT,
							      "EphyStartHere",
							      &our_info, 0);
        }

        return ephy_start_here_type;
}

static void
ephy_start_here_class_init (EphyStartHereClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_start_here_finalize;
}

static void
ephy_start_here_init (EphyStartHere *eb)
{
        eb->priv = g_new0 (EphyStartHerePrivate, 1);

	eb->priv->langs = gnome_i18n_get_language_list ("LC_MESSAGES");
}

static void
ephy_start_here_finalize (GObject *object)
{
        EphyStartHere *eb;

	g_return_if_fail (IS_EPHY_START_HERE (object));

	eb = EPHY_START_HERE (object);

        g_return_if_fail (eb->priv != NULL);

        g_free (eb->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyStartHere *
ephy_start_here_new ()
{
	EphyStartHere *tab;

	tab = EPHY_START_HERE (g_object_new (EPHY_START_HERE_TYPE, NULL));

	return tab;
}

static gboolean
is_my_lang (EphyStartHere *sh, char *my_lang)
{
	const GList *l;

	for (l = sh->priv->langs; l != NULL; l = l->next)
	{
		char *lang = l->data;
		int len;
		char *lg, *enc;

		lg = strchr (my_lang, '_');
		enc = strchr (my_lang, '.');

		len = strlen (my_lang);
		if (enc) len = enc - my_lang;
		if (lg) len = lg - my_lang;

		if (strncmp (lang, my_lang, len) == 0)
			return TRUE;
	}

	return FALSE;
}

static xmlNodePtr
drop_other_languages (EphyStartHere *sh,
		      const char *node_name,
		      xmlNodePtr node)
{
	xmlNodePtr default_node;
	xmlNodePtr res = NULL;
	gboolean use_default = TRUE;

	default_node = node->prev;
	while (default_node && !xmlStrEqual (default_node->name, node_name))
	{
		default_node = default_node->prev;
	}
	g_return_val_if_fail (default_node != NULL, NULL);

	while (node != NULL)
	{
		xmlChar *lang;

		res = node->next;

		lang = xmlNodeGetLang (node);
		if (!is_my_lang (sh, lang))
		{
			xmlUnlinkNode (node);
			xmlFreeNode (node);
		}
		else
		{
			use_default = FALSE;
		}
		xmlFree (lang);

		node = (res && xmlStrEqual (res->name, node_name)) ?
			res : NULL;
	}

	if (!use_default)
	{
		xmlUnlinkNode (default_node);
		xmlFreeNode (default_node);
	}

	return res;
}

static void
select_language (EphyStartHere *sh, xmlNodePtr node)
{
	while (node)
	{
		xmlChar *lang;

		lang = xmlNodeGetLang (node);
		if (lang)
		{
			node = drop_other_languages
				(sh, node->name, node);
			xmlFree (lang);
		}
		else
		{
			select_language (sh, node->children);
			node = node->next;
		}
	}
}

char *
ephy_start_here_get_page (EphyStartHere *sh, const char *id)
{
	xmlDocPtr doc;
        xmlNodePtr child;
	xmlNodePtr root;
	xmlBufferPtr buf;
	const char *xml_filepath;
	char *xml_filename;
	char *content;

	xml_filename = g_strconcat ("starthere/", id, ".xml", NULL);
	xml_filepath = ephy_file (xml_filename);
	g_free (xml_filename);

	doc = xmlParseFile (xml_filepath);
	root = xmlDocGetRootElement (doc);
	buf = xmlBufferCreate ();

	select_language (sh, root);

	child = doc->children;
	while (child)
	{
		xmlNodeDump (buf, doc, child, 1, 1);
		child = child->next;
	}

	content = g_strdup (xmlBufferContent (buf));
	xmlBufferFree (buf);
	xmlFreeDoc (doc);

	return content;
}

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

#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "ephy-start-here.h"
#include "ephy-file-helpers.h"

struct EphyStartHerePrivate
{
	const GList *langs;
	char *base_uri;
	xmlDocPtr doc;
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
	eb->priv->base_uri = g_strconcat ("file://",
					  ephy_file ("starthere/"),
					  NULL);
}

static void
ephy_start_here_finalize (GObject *object)
{
        EphyStartHere *eb;

	g_return_if_fail (IS_EPHY_START_HERE (object));

	eb = EPHY_START_HERE (object);

        g_return_if_fail (eb->priv != NULL);

	g_free (eb->priv->base_uri);

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
		char *lang = (char *)l->data;

		if (strcmp (lang, my_lang) == 0)
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

static char *
mozilla_bookmarks (void)
{
	GSList *l;
	char *dir;
	char *result;

	dir = g_build_filename (g_get_home_dir (), ".mozilla", NULL);
	l = ephy_file_find  (dir, "bookmarks.html", 4);
	g_free (dir);

	result = l ? g_strdup (l->data) : NULL;

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	return result;
}

static char *
galeon_bookmarks (void)
{
	GSList *l;
	char *dir;
	char *result;

	dir = g_build_filename (g_get_home_dir (), ".galeon", NULL);
	l = ephy_file_find  (dir, "bookmarks.xbel", 4);
	g_free (dir);

	result = l ? g_strdup (l->data) : NULL;

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	return result;
}

static char *
konqueror_bookmarks (void)
{
	GSList *l;
	char *dir;
	char *result;

	dir = g_build_filename (g_get_home_dir (), ".kde", "share",
				"apps", "konqueror", NULL);
	l = ephy_file_find  (dir, "bookmarks.xml", 4);
	g_free (dir);

	result = l ? g_strdup (l->data) : NULL;

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);

	return result;
}

static void
attach_content (EphyStartHere *sh, xmlNodePtr node, xmlChar *id)
{
	if (xmlStrEqual (id, "bookmarks-import"))
	{
		xmlNodePtr child;
		char *bmk_file;

		bmk_file = mozilla_bookmarks ();
		if (bmk_file)
		{
			child = xmlNewDocNode (sh->priv->doc, NULL, "action",
					       _("Import Mozilla bookmarks"));
			xmlSetProp (child, "id", "import-mozilla-bookmarks");
			xmlSetProp (child, "param", bmk_file);
			xmlAddChild (node, child);
		}
		g_free (bmk_file);

		bmk_file = galeon_bookmarks ();
		if (bmk_file)
		{
			child = xmlNewDocNode (sh->priv->doc, NULL, "action",
					       _("Import Galeon bookmarks"));
			xmlSetProp (child, "id", "import-galeon-bookmarks");
			xmlSetProp (child, "param", bmk_file);
			xmlAddChild (node, child);
		}
		g_free (bmk_file);

		bmk_file = konqueror_bookmarks ();
		if (bmk_file)
		{
			child = xmlNewDocNode (sh->priv->doc, NULL, "action",
					       _("Import Konqueror bookmarks"));
			xmlSetProp (child, "id", "import-konqueror-bookmarks");
			xmlSetProp (child, "param", bmk_file);
			xmlAddChild (node, child);
		}
		g_free (bmk_file);

	}
}

static char *
color_to_string (GdkColor color)
{
	return g_strdup_printf ("#%.2x%.2x%.2x",
			       color.red >> 8,
			       color.green >> 8,
			       color.blue >> 8);
}

static void
build_content (EphyStartHere *sh, xmlNodePtr node)
{
	while (node)
	{
		xmlChar *id;

		id = xmlGetProp (node, "id");
		if (id)
		{
			attach_content (sh, node, id);
			xmlFree (id);
		}

		if (xmlStrEqual (node->name, "section"))
		{
			GtkWidget *widget, *window;
			GdkColor color;
			char *str;

			/* create a random widget that we will use to get
			 * the current style
			 */
			window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
			widget = gtk_label_new ("");
			gtk_container_add (GTK_CONTAINER (window), widget);
			gtk_widget_realize (window);
			gtk_widget_realize (widget);
			gtk_widget_ensure_style (widget);

			color = widget->style->bg[GTK_STATE_NORMAL];
			str = color_to_string (color);
			xmlSetProp (node, "bg", str);
			g_free (str);

			color = widget->style->text[GTK_STATE_SELECTED];
			str = color_to_string (color);
			xmlSetProp (node, "title", str);
			g_free (str);

			color = widget->style->bg[GTK_STATE_ACTIVE];
			str = color_to_string (color);
			xmlSetProp (node, "title-bg", str);
			g_free (str);

			color = widget->style->bg[GTK_STATE_SELECTED];
			str = color_to_string (color);
			xmlSetProp (node, "link", str);
			g_free (str);

			color = widget->style->text[GTK_STATE_NORMAL];
			str = color_to_string (color);
			xmlSetProp (node, "text", str);
			g_free (str);

			gtk_widget_destroy (window);
		}

		build_content (sh, node->children);
		node = node->next;
	}
}
char *
ephy_start_here_get_page (EphyStartHere *sh, const char *id)
{
        xmlNodePtr root;
	const char *xml_filepath;
	char *xml_filename;
	xmlChar *content;
	int size;

	xml_filename = g_strconcat ("starthere/", id, ".xml", NULL);
	xml_filepath = ephy_file (xml_filename);
	if (!xml_filepath) return NULL;
	g_free (xml_filename);

	sh->priv->doc = xmlParseFile (xml_filepath);
	root = xmlDocGetRootElement (sh->priv->doc);

	select_language (sh, root);
	build_content (sh, root);

	xmlDocDumpMemory (sh->priv->doc, &content, &size);

	xmlFreeDoc (sh->priv->doc);

	return content;
}

const char *
ephy_start_here_get_base_uri (EphyStartHere *sh)
{
	return sh->priv->base_uri;
}

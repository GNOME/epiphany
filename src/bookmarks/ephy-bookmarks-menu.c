/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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
#include "config.h"
#endif

#include "ephy-bookmarks-menu.h"
#include "ephy-bookmark-action.h"
#include "ephy-shell.h"
#include "ephy-node-common.h"
#include "ephy-debug.h"

#include <string.h>
#include <stdlib.h>
#include <libxml/entities.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkuimanager.h>

#define EMPTY_ACTION_NAME "GoBookmarkEmpty"

/**
 * Private data
 */
struct _EphyBookmarksMenuPrivate
{
	EphyWindow *window;
	EphyBookmarks *bookmarks;
	GtkActionGroup *action_group;
	guint ui_id;
	guint update_tag;
};

/**
 * Private functions, only availble from this file
 */
static void	ephy_bookmarks_menu_class_init	  (EphyBookmarksMenuClass *klass);
static void	ephy_bookmarks_menu_init	  (EphyBookmarksMenu *menu);
static void	ephy_bookmarks_menu_finalize      (GObject *o);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static gpointer parent_class;

GType
ephy_bookmarks_menu_get_type (void)
{
        static GType ephy_bookmarks_menu_type = 0;

        if (ephy_bookmarks_menu_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyBookmarksMenuClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_bookmarks_menu_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyBookmarksMenu),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_bookmarks_menu_init
                };

                ephy_bookmarks_menu_type = g_type_register_static (G_TYPE_OBJECT,
							           "EphyBookmarksMenu",
							           &our_info, 0);
        }
        return ephy_bookmarks_menu_type;
}

static void
ephy_bookmarks_menu_clean (EphyBookmarksMenu *menu)
{
	EphyBookmarksMenuPrivate *p = menu->priv;
	GtkUIManager *merge = GTK_UI_MANAGER (p->window->ui_merge);

	if (p->ui_id > 0)
	{
		gtk_ui_manager_remove_ui (merge, p->ui_id);
		p->ui_id = 0;
	}

	if (p->action_group != NULL)
	{
		gtk_ui_manager_remove_action_group (merge, p->action_group);
		g_object_unref (p->action_group);
	}
}

static void
go_location_cb (GtkAction *action, char *location, EphyWindow *window)
{
	ephy_window_load_url (window, location);
}

static int
sort_topics (gconstpointer a, gconstpointer b)
{
	EphyNode *node_a = (EphyNode *)a;
	EphyNode *node_b = (EphyNode *)b;
	const char *title1, *title2;
	int retval;

	title1 = ephy_node_get_property_string (node_a, EPHY_NODE_KEYWORD_PROP_NAME);
	title2 = ephy_node_get_property_string (node_b, EPHY_NODE_KEYWORD_PROP_NAME);

	if (title1 == NULL)
	{
		retval = -1;
	}
	else if (title2 == NULL)
	{
		retval = 1;
	}
	else
	{
		char *str_a, *str_b;

		str_a = g_utf8_casefold (title1, -1);
		str_b = g_utf8_casefold (title2, -1);
		retval = g_utf8_collate (str_a, str_b);
		g_free (str_a);
		g_free (str_b);
	}

	return retval;
}

static int
sort_bookmarks (gconstpointer a, gconstpointer b)
{
	EphyNode *node_a = (EphyNode *)a;
	EphyNode *node_b = (EphyNode *)b;
	const char *title1, *title2;
	int retval;

	title1 = ephy_node_get_property_string (node_a, EPHY_NODE_BMK_PROP_TITLE);
	title2 = ephy_node_get_property_string (node_b, EPHY_NODE_BMK_PROP_TITLE);

	if (title1 == NULL)
	{
		retval = -1;
	}
	else if (title2 == NULL)
	{
		retval = 1;
	}
	else
	{
		char *str_a, *str_b;

		str_a = g_utf8_casefold (title1, -1);
		str_b = g_utf8_casefold (title2, -1);
		retval = g_utf8_collate (str_a, str_b);
		g_free (str_a);
		g_free (str_b);
	}

	return retval;
}

static void
add_bookmarks_menu (EphyBookmarksMenu *menu, EphyNode *node, GString *xml)
{
	GPtrArray *children;
	EphyBookmarksMenuPrivate *p = menu->priv;

	children = ephy_node_get_children (node);

	if (children->len < 1)
	{
		g_string_append (xml, "<menuitem name=\"");
		g_string_append (xml, EMPTY_ACTION_NAME"Menu");
		g_string_append (xml, "Menu");
		g_string_append (xml, "\" action=\"");
		g_string_append (xml, EMPTY_ACTION_NAME);
		g_string_append (xml, "\"/>\n");
	}
	else
	{
		GList *node_list = NULL, *l;
		int i;

		for (i = 0; i < children->len; ++i)
		{
			node_list = g_list_prepend (node_list,
						    g_ptr_array_index (children, i));
		}

		node_list = g_list_sort (node_list, (GCompareFunc)sort_bookmarks);

		for (l = node_list; l != NULL; l = l->next)
		{
			GtkAction *action;
			EphyNode *child;
			long id;
			char *verb;

			child = l->data;
			id = ephy_node_get_id (child);
			verb = g_strdup_printf ("OpenBookmark%ld", id);

			action = ephy_bookmark_action_new (verb, id);
			gtk_action_group_add_action (p->action_group, action);
			g_object_unref (action);
			g_signal_connect (action, "go_location",
					  G_CALLBACK (go_location_cb), p->window);

			g_string_append (xml, "<menuitem name=\"");
			g_string_append (xml, verb);
			g_string_append (xml, "Menu");
			g_string_append (xml, "\" action=\"");
			g_string_append (xml, verb);
			g_string_append (xml, "\"/>\n");

			g_free (verb);
		}

		g_list_free (node_list);
	}

	ephy_node_thaw (node);
}

static void
ephy_bookmarks_menu_rebuild (EphyBookmarksMenu *menu)
{
	EphyBookmarksMenuPrivate *p = menu->priv;
	GString *xml;
	gint i;
	EphyNode *topics;
	EphyNode *not_categorized;
	GPtrArray *children;
	GtkUIManager *merge = GTK_UI_MANAGER (p->window->ui_merge);
	GList *node_list = NULL, *l;
	GtkAction *empty;

	LOG ("Rebuilding bookmarks menu")

	START_PROFILER ("Rebuilding bookmarks menu")

	ephy_bookmarks_menu_clean (menu);

	topics = ephy_bookmarks_get_keywords (p->bookmarks);
	not_categorized = ephy_bookmarks_get_not_categorized (p->bookmarks);
	children = ephy_node_get_children (topics);

	xml = g_string_new (NULL);
	g_string_append (xml, "<ui><menubar><menu name=\"BookmarksMenu\">"
			      "<placeholder name=\"BookmarksTree\">"
			      "<separator name=\"BookmarksSep1\"/>");

	p->action_group = gtk_action_group_new ("BookmarksActions");
	gtk_ui_manager_insert_action_group (merge, p->action_group, 0);

	empty = g_object_new (GTK_TYPE_ACTION,
			      "name", EMPTY_ACTION_NAME,
			      /* This is the adjective, not the verb */
			      "label", _("Empty"),
			      "sensitive", FALSE,
			      NULL);
	gtk_action_group_add_action (p->action_group, empty);
	g_object_unref (empty);

	for (i = 0; i < children->len; ++i)
	{
		EphyNode *kid;
		EphyNodePriority priority;

		kid = g_ptr_array_index (children, i);

		priority = ephy_node_get_property_int
			(kid, EPHY_NODE_KEYWORD_PROP_PRIORITY);

		if (priority == EPHY_NODE_NORMAL_PRIORITY)
		{
			node_list = g_list_prepend (node_list, kid);
		}
	}
	ephy_node_thaw (topics);

	node_list = g_list_sort (node_list, (GCompareFunc)sort_topics);

	for (l = node_list; l != NULL; l = l->next)
	{
		char *verb;
		const char *title;
		EphyNode *child;
		GtkAction *action;

		child = l->data;
		title = ephy_node_get_property_string (child, EPHY_NODE_KEYWORD_PROP_NAME);

		verb = g_strdup_printf ("OpenTopic%ld", ephy_node_get_id (child));
		action = g_object_new (GTK_TYPE_ACTION,
				       "name", verb,
				       "label", title,
				       NULL);
		gtk_action_group_add_action (p->action_group, action);
		g_object_unref (action);

		g_string_append (xml, "<menu name=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "Menu");
		g_string_append (xml, "\" action=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "\">\n");

		add_bookmarks_menu (menu, child, xml);

		g_string_append (xml, "</menu>");

		g_free (verb);
	}

	if (ephy_node_get_n_children (not_categorized) > 0)
	{
		add_bookmarks_menu (menu, not_categorized, xml);
	}

	g_string_append (xml, "</placeholder></menu></menubar></ui>");

	if (children->len > 0)
	{
		GError *error = NULL;
		LOG ("Merging ui\n%s",xml->str);
		p->ui_id = gtk_ui_manager_add_ui_from_string
			(merge, xml->str, -1, &error);
	}

	g_string_free (xml, TRUE);
	g_list_free (node_list);

	STOP_PROFILER ("Rebuilding bookmarks menu")
}

static void
ephy_bookmarks_menu_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
        EphyBookmarksMenu *m = EPHY_BOOKMARKS_MENU (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        m->priv->window = g_value_get_object (value);
			ephy_bookmarks_menu_rebuild (m);
                        break;
        }
}

static void
ephy_bookmarks_menu_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
        EphyBookmarksMenu *m = EPHY_BOOKMARKS_MENU (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, m->priv->window);
                        break;
        }
}


static void
ephy_bookmarks_menu_class_init (EphyBookmarksMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_bookmarks_menu_finalize;
	object_class->set_property = ephy_bookmarks_menu_set_property;
	object_class->get_property = ephy_bookmarks_menu_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
}

static gboolean
do_updates (EphyBookmarksMenu *menu)
{
	ephy_bookmarks_menu_rebuild (menu);

	menu->priv->update_tag = 0;

	return FALSE;
}

static void
bookmarks_tree_changed_cb (EphyBookmarks *bookmarks, EphyBookmarksMenu *menu)
{
	if (menu->priv->update_tag == 0)
	{
		menu->priv->update_tag = g_idle_add((GSourceFunc)do_updates, menu);
	}
}

static void
ephy_bookmarks_menu_init (EphyBookmarksMenu *menu)
{
	EphyBookmarksMenuPrivate *p = g_new0 (EphyBookmarksMenuPrivate, 1);

	menu->priv = p;

	menu->priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	g_signal_connect_object (menu->priv->bookmarks, "tree_changed",
			         G_CALLBACK (bookmarks_tree_changed_cb),
			         menu, 0);

	menu->priv->ui_id = 0;
	menu->priv->action_group = NULL;
	menu->priv->update_tag = 0;
}

static void
ephy_bookmarks_menu_finalize (GObject *o)
{
	EphyBookmarksMenu *menu = EPHY_BOOKMARKS_MENU (o);
	EphyBookmarksMenuPrivate *p = menu->priv;

	if (menu->priv->update_tag != 0)
	{
		g_source_remove (menu->priv->update_tag);
	}

	if (p->action_group != NULL)
	{
		gtk_ui_manager_remove_action_group
			(GTK_UI_MANAGER (p->window->ui_merge),
			 p->action_group);
		g_object_unref (p->action_group);
	}

	g_free (p);

	G_OBJECT_CLASS (parent_class)->finalize (o);
}

EphyBookmarksMenu *
ephy_bookmarks_menu_new (EphyWindow *window)
{
	EphyBookmarksMenu *ret = g_object_new (EPHY_TYPE_BOOKMARKS_MENU,
					       "EphyWindow", window,
					       NULL);
	return ret;
}

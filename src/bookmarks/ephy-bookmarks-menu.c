/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
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

#include "ephy-bookmarks-menu.h"
#include "ephy-bookmark-action.h"
#include "ephy-shell.h"
#include "ephy-node-common.h"
#include "ephy-string.h"
#include "ephy-debug.h"

#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gtk/gtkuimanager.h>

#define BOOKMARKS_MENU_PATH "/menubar/BookmarksMenu"

#define EPHY_BOOKMARKS_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARKS_MENU, EphyBookmarksMenuPrivate))

struct _EphyBookmarksMenuPrivate
{
	GtkUIManager *merge;
	EphyWindow *window;
	EphyBookmarks *bookmarks;
	GtkActionGroup *action_group;
	guint ui_id;
	guint update_tag;
};

static void	ephy_bookmarks_menu_class_init	  (EphyBookmarksMenuClass *klass);
static void	ephy_bookmarks_menu_init	  (EphyBookmarksMenu *menu);
static void	ephy_bookmarks_menu_finalize      (GObject *o);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static GObjectClass *parent_class = NULL;

GType
ephy_bookmarks_menu_get_type (void)
{
        static GType type = 0;

        if (type == 0)
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

                type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyBookmarksMenu",
					       &our_info, 0);
        }

        return type;
}

static void
ephy_bookmarks_menu_clean (EphyBookmarksMenu *menu)
{
	EphyBookmarksMenuPrivate *p = menu->priv;

	START_PROFILER ("Cleaning bookmarks menu")

	if (p->ui_id > 0)
	{
		gtk_ui_manager_remove_ui (p->merge, p->ui_id);
		gtk_ui_manager_ensure_update (p->merge);
		p->ui_id = 0;
	}

	if (p->action_group != NULL)
	{
		gtk_ui_manager_remove_action_group (p->merge, p->action_group);
		g_object_unref (p->action_group);
	}

	STOP_PROFILER ("Cleaning bookmarks menu")
}

static void
open_bookmark_cb (GtkAction *action, char *location, EphyWindow *window)
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
create_menu (EphyBookmarksMenu *menu, EphyNode *node, const char *path)
{
	GPtrArray *children;
	EphyBookmarksMenuPrivate *p = menu->priv;
	GList *node_list = NULL, *l;
	int i;

	children = ephy_node_get_children (node);
	for (i = 0; i < children->len; ++i)
	{
		node_list = g_list_prepend (node_list,
					    g_ptr_array_index (children, i));
	}

	if (node_list != NULL)
	{
		node_list = g_list_sort (node_list, (GCompareFunc)sort_bookmarks);
		for (l = node_list; l != NULL; l = l->next)
		{
			GtkAction *action;
			EphyNode *child;
			long id;
			char verb[30], name[30], accel_path[60];

			child = l->data;
			id = ephy_node_get_id (child);
			g_snprintf (verb, sizeof (verb), 
				    "OpenBmk%d", ephy_node_get_id (child));
			g_snprintf (name, sizeof (name), "%sName", verb);
			g_snprintf (accel_path, sizeof (accel_path),
				    "<Actions>/BookmarksActions/%s", verb);

			action = ephy_bookmark_action_new (verb, id);
			gtk_action_set_accel_path (action, accel_path);
			gtk_action_group_add_action (p->action_group, action);
			g_object_unref (action);
			g_signal_connect (action, "open",
					  G_CALLBACK (open_bookmark_cb), p->window);

			gtk_ui_manager_add_ui (p->merge, p->ui_id, path,
					       name, verb,
					       GTK_UI_MANAGER_MENUITEM, FALSE);
		}
		g_list_free (node_list);
	}
}

static void
ensure_folder (EphyBookmarksMenu *menu, const char *path, const char *folder)
{
	EphyBookmarksMenuPrivate *p = menu->priv;
	char *action_name;
	GtkAction *action;

	g_return_if_fail (folder != NULL);

	action_name = g_strdup_printf ("TopicAction-%s\n", folder);
	action = gtk_action_group_get_action (p->action_group, action_name);

	if (action == NULL)
	{
		action = g_object_new (GTK_TYPE_ACTION,
				       "name", action_name,
				       "label", folder,
				       "hide_if_empty", FALSE,
				       NULL);
		gtk_action_group_add_action (p->action_group, action);
		g_object_unref (action);

		gtk_ui_manager_add_ui (p->merge, p->ui_id, path,
				       folder, action_name,
				       GTK_UI_MANAGER_MENU, FALSE);
	}

	g_free (action_name);
}

static char *
create_submenu (EphyBookmarksMenu *menu, EphyNode *topic)
{
	const char *tmp;
	char *title, *folder;
	char **folders;
	GString *path;
	int i;

	tmp = ephy_node_get_property_string (topic, EPHY_NODE_KEYWORD_PROP_NAME);
	title = ephy_string_double_underscores (tmp);

	g_return_val_if_fail (title != NULL, NULL);

	folders = g_strsplit (title, "/", -1);
	g_free (title);
	g_return_val_if_fail (folders != NULL, NULL); /* FIXME */

	path = g_string_new (BOOKMARKS_MENU_PATH);
	for (i = 0; folders[i] != NULL; i++)
	{
		folder = folders[i];

		ensure_folder (menu, path->str, folder);

		g_string_append (path, "/");
		g_string_append (path, folder);
	}

	g_strfreev (folders);

	return g_string_free (path, FALSE);
}

static void
ephy_bookmarks_menu_rebuild (EphyBookmarksMenu *menu)
{
	EphyBookmarksMenuPrivate *p = menu->priv;
	gint i;
	EphyNode *topics;
	EphyNode *not_categorized;
	GPtrArray *children;
	GList *node_list = NULL, *l;

	LOG ("Rebuilding bookmarks menu")

	ephy_bookmarks_menu_clean (menu);

	START_PROFILER ("Rebuilding bookmarks menu")

	topics = ephy_bookmarks_get_keywords (p->bookmarks);
	not_categorized = ephy_bookmarks_get_not_categorized (p->bookmarks);
	children = ephy_node_get_children (topics);

	p->ui_id = gtk_ui_manager_new_merge_id (p->merge);

	p->action_group = gtk_action_group_new ("BookmarksActions");
	gtk_ui_manager_insert_action_group (p->merge, p->action_group, 0);

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

	node_list = g_list_sort (node_list, (GCompareFunc)sort_topics);

	for (l = node_list; l != NULL; l = l->next)
	{
		char *path;

		path = create_submenu (menu, l->data);
		create_menu (menu, l->data, path);
		g_free (path);
	}

	if (ephy_node_get_n_children (not_categorized) > 0)
	{
		create_menu (menu, not_categorized, BOOKMARKS_MENU_PATH);
	}

	g_list_free (node_list);

	STOP_PROFILER ("Rebuilding bookmarks menu")
}

static void
ephy_bookmarks_menu_set_window (EphyBookmarksMenu *menu, EphyWindow *window)
{
	menu->priv->window = window;
	menu->priv->merge = GTK_UI_MANAGER (window->ui_merge);
	ephy_bookmarks_menu_rebuild (menu);
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
                        ephy_bookmarks_menu_set_window
				(m, g_value_get_object (value));
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
                                                              EPHY_TYPE_WINDOW,
                                                              G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyBookmarksMenuPrivate));
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
sync_topic_properties (GtkAction *action, EphyNode *bmk)
{
	const char *tmp;
	char *title;
	int priority;

	priority = ephy_node_get_property_int 
		(bmk, EPHY_NODE_KEYWORD_PROP_PRIORITY);

	tmp = ephy_node_get_property_string
       	        (bmk, EPHY_NODE_KEYWORD_PROP_NAME);

	title = ephy_string_double_underscores (tmp);

	g_object_set (action, "label", title, NULL);

	g_free (title);
}

static void
topic_child_changed_cb (EphyNode *node,
			EphyNode *child,
			guint property_id,
			EphyBookmarksMenu *menu)
{
	GtkAction *action;
	char name[64];

	if (menu->priv->update_tag != 0 || menu->priv->action_group == NULL)
	{
		return;
	}

	g_snprintf (name, sizeof (name), "OpenTopic%d", ephy_node_get_id (child));

	action = gtk_action_group_get_action (menu->priv->action_group, name);

	if (action != NULL)
	{
		sync_topic_properties (action, child);
	}
}

static void
ephy_bookmarks_menu_init (EphyBookmarksMenu *menu)
{
	EphyBookmarksMenuPrivate *p = EPHY_BOOKMARKS_MENU_GET_PRIVATE (menu);
	EphyNode *node;

	menu->priv = p;
	menu->priv->ui_id = 0;
	menu->priv->action_group = NULL;
	menu->priv->update_tag = 0;

	menu->priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	g_signal_connect_object (menu->priv->bookmarks, "tree_changed",
			         G_CALLBACK (bookmarks_tree_changed_cb),
			         menu, 0);

	node = ephy_bookmarks_get_keywords (menu->priv->bookmarks);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
				         (EphyNodeCallback) topic_child_changed_cb,
				         G_OBJECT (menu));
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
		g_object_unref (p->action_group);
	}

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

/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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
#include <string.h>

#define EPHY_BOOKMARKS_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARKS_MENU, EphyBookmarksMenuPrivate))

struct _EphyBookmarksMenuPrivate
{
	GtkUIManager *manager;
	EphyWindow *window;
	EphyBookmarks *bookmarks;
	GtkActionGroup *bmk_actions;
	GtkActionGroup *folder_actions;
	GSList *removed_bmks;
	guint ui_id;
	guint update_tag;
	gboolean needs_update;
};

#define BOOKMARKS_MENU_PATH "/menubar/BookmarksMenu"

/* 14 = strlen ("0000000000000000") - strlen ("%x")
 * FIXME: for 32bit, 6 is sufficient -> use some #if magic?
 */
#define MAXLEN	14

/* this %x is bookmark node id */
#define BMK_VERB_FORMAT		"Bmk%x"
#define BMK_VERB_FORMAT_LENGTH	strlen (BMK_VERB_FORMAT) + MAXLEN + 1

/* first %x is bookmark node id, second %x is g_str_hash of path */
#define BMK_NAME_FORMAT		"Bmk%x%x"
#define	BMK_NAME_FORMAT_LENGTH	strlen (BMK_NAME_FORMAT) + 2 * MAXLEN + 1

/* first %x is g_str_hash of folder name, 2nd %x is g_str_hash of path */
#define FOLDER_VERB_FORMAT		"Fld%x%x"
#define FOLDER_VERB_FORMAT_LENGTH	strlen (FOLDER_VERB_FORMAT) + 2 * MAXLEN + 1

#define GAZILLION	200
#define UPDATE_DELAY	5000 /* ms */
#define LABEL_WIDTH_CHARS	32

enum
{
	PROP_0,
	PROP_WINDOW
};

static void ephy_bookmarks_menu_class_init (EphyBookmarksMenuClass *klass);
static void ephy_bookmarks_menu_init	   (EphyBookmarksMenu *menu);

static GObjectClass *parent_class = NULL;

GType
ephy_bookmarks_menu_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0))
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
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy)
{
        if (GTK_IS_MENU_ITEM (proxy))
        {
		GtkLabel *label;

		label = (GtkLabel *) ((GtkBin *) proxy)->child;
		gtk_label_set_width_chars (label, LABEL_WIDTH_CHARS);
		gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
        }
}

static void
remove_action (gpointer idptr,
	       GtkActionGroup *action_group)
{
	GtkAction *action;
	char verb[BMK_VERB_FORMAT_LENGTH];

	g_snprintf (verb, sizeof (verb), BMK_VERB_FORMAT, GPOINTER_TO_UINT (idptr));
	action = gtk_action_group_get_action (action_group, verb);
	g_return_if_fail (action != NULL);

	gtk_action_group_remove_action (action_group, action);
}

static void
ephy_bookmarks_menu_clean (EphyBookmarksMenu *menu)
{
	EphyBookmarksMenuPrivate *p = menu->priv;

	START_PROFILER ("Cleaning bookmarks menu")

	if (p->ui_id != 0)
	{
		gtk_ui_manager_remove_ui (p->manager, p->ui_id);
		gtk_ui_manager_ensure_update (p->manager);
		p->ui_id = 0;
	}

	if (p->bmk_actions != NULL && menu->priv->removed_bmks != NULL)
	{
		/* now we can remove the actions for removed bookmarks */
		g_slist_foreach (menu->priv->removed_bmks, (GFunc) remove_action,
				 menu->priv->bmk_actions);
		g_slist_free (menu->priv->removed_bmks);
		menu->priv->removed_bmks = NULL;
	}

	if (p->folder_actions != NULL)
	{
		gtk_ui_manager_remove_action_group (p->manager, p->folder_actions);
		g_object_unref (p->folder_actions);
	}

	STOP_PROFILER ("Cleaning bookmarks menu")
}

static void
open_bookmark_cb (GtkAction *action,
		  const char *location,
		  EphyBookmarksMenu *menu)
{
	ephy_window_load_url (menu->priv->window, location);
}

#define BMK_ACCEL_PATH_PREFIX "<Actions>/BmkActions/"

static void
add_action_for_bookmark (EphyBookmarksMenu *menu,
			 EphyNode *bmk)
{
	GtkAction *action;
	char verb[BMK_VERB_FORMAT_LENGTH];
	char apath[strlen (BMK_ACCEL_PATH_PREFIX) + BMK_VERB_FORMAT_LENGTH];
	guint id;

	g_return_if_fail (bmk != NULL);

	id = ephy_node_get_id (bmk);

	g_snprintf (verb, sizeof (verb), BMK_VERB_FORMAT, id);
	g_snprintf (apath, sizeof (apath), BMK_ACCEL_PATH_PREFIX "%s", verb);

	action = ephy_bookmark_action_new (verb, bmk);

	gtk_action_set_accel_path (action, apath);

	g_signal_connect (action, "open",
			  G_CALLBACK (open_bookmark_cb), menu);

	gtk_action_group_add_action (menu->priv->bmk_actions, action);
	g_object_unref (action);
}

static void
ensure_bookmark_actions (EphyBookmarksMenu *menu)
{
	EphyNode *bookmarks, *bmk;
	GPtrArray *children;
	int i;

	if (menu->priv->bmk_actions != NULL) return;

	START_PROFILER ("Adding bookmarks actions")

	menu->priv->bmk_actions = gtk_action_group_new ("BmkActions");
	gtk_ui_manager_insert_action_group (menu->priv->manager,
					    menu->priv->bmk_actions, -1);

	g_signal_connect (menu->priv->bmk_actions, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	bookmarks = ephy_bookmarks_get_bookmarks (menu->priv->bookmarks);
	children = ephy_node_get_children (bookmarks);
	for (i = 0; i < children->len; i++)
	{
		bmk = g_ptr_array_index (children, i);

		add_action_for_bookmark (menu, bmk);
	}

	STOP_PROFILER ("Adding bookmarks actions")
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
create_menu (EphyBookmarksMenu *menu,
	     EphyNode *node,
	     const char *path)
{
	GPtrArray *children;
	EphyBookmarksMenuPrivate *p = menu->priv;
	GList *node_list = NULL, *l;
	guint phash;
	int i;

	phash = g_str_hash (path);

	children = ephy_node_get_children (node);
	for (i = 0; i < children->len; ++i)
	{
		node_list = g_list_prepend (node_list,
					    g_ptr_array_index (children, i));
	}

	node_list = g_list_sort (node_list, (GCompareFunc)sort_bookmarks);
	for (l = node_list; l != NULL; l = l->next)
	{
		char verb[BMK_VERB_FORMAT_LENGTH];
		char name[BMK_NAME_FORMAT_LENGTH];
		guint id;

		id = ephy_node_get_id ((EphyNode *) l->data);

		g_snprintf (verb, sizeof (verb), BMK_VERB_FORMAT, id);
		g_snprintf (name, sizeof (name), BMK_NAME_FORMAT, id, phash);

		gtk_ui_manager_add_ui (p->manager, p->ui_id, path,
				       name, verb,
				       GTK_UI_MANAGER_MENUITEM, FALSE);
	}
	g_list_free (node_list);
}

#define FOLDER_ACCEL_PATH_PREFIX "<Actions>/FolderActions/"

static char *
create_submenu (EphyBookmarksMenu *menu,
		EphyNode *topic)
{
	EphyBookmarksMenuPrivate *p = menu->priv;
	GtkAction *action;
	char verb[FOLDER_VERB_FORMAT_LENGTH];
	char apath[strlen (FOLDER_ACCEL_PATH_PREFIX) + FOLDER_VERB_FORMAT_LENGTH];
	const char *tmp;
	char *title, *folder;
	char **folders;
	GString *path;
	guint phash, fhash;
	int i;

	tmp = ephy_node_get_property_string (topic, EPHY_NODE_KEYWORD_PROP_NAME);
	g_return_val_if_fail (tmp != NULL, NULL);

	title = ephy_string_double_underscores (tmp);
	folders = g_strsplit (title, BOOKMARKS_HIERARCHY_SEP, -1);
	g_free (title);

	/* occurs if topic name was "" or BOOKMARKS_HIERARCHY_SEP */
	if (folders == NULL || folders[0] == NULL)
	{
		g_strfreev (folders);
		return g_strdup (BOOKMARKS_MENU_PATH);
	}

	path = g_string_new (BOOKMARKS_MENU_PATH);
	for (i = 0; folders[i] != NULL; i++)
	{
		folder = folders[i];

		/* happens for BOOKMARKS_HIERARCHY_SEP at start/end of title,
		 * or when occurring twice in succession.
		 * Treat as if it didn't occur/only occurred once.
		 */
		if (folders[i][0] == '\0') continue;

		phash = g_str_hash (path->str);
		fhash = g_str_hash (folder);
		g_snprintf (verb, sizeof (verb), FOLDER_VERB_FORMAT, fhash, phash);
	
		if (gtk_action_group_get_action (p->folder_actions, verb) == NULL)
		{
			g_snprintf (apath, sizeof (apath),
				    FOLDER_ACCEL_PATH_PREFIX "%s", verb);

			action = g_object_new (GTK_TYPE_ACTION,
					       "name", verb,
					       "label", folder,
					       "hide_if_empty", FALSE,
					       NULL);
			gtk_action_set_accel_path (action, apath);

			gtk_action_group_add_action (p->folder_actions, action);
			g_object_unref (action);

			gtk_ui_manager_add_ui (p->manager, p->ui_id, path->str,
					       verb, verb,
					       GTK_UI_MANAGER_MENU, FALSE);
		}

		g_string_append (path, "/");
		g_string_append (path, verb);
	}

	g_strfreev (folders);

	return g_string_free (path, FALSE);
}

static void
ephy_bookmarks_menu_rebuild (EphyBookmarksMenu *menu)
{
	EphyBookmarksMenuPrivate *p = menu->priv;
	EphyNode *topics, *not_categorized, *node;
	GPtrArray *children;
	GList *node_list = NULL, *l;
	char *path;
	int i;

	if (menu->priv->needs_update == FALSE)
	{
		LOG ("No update required")
	
		return;
	}

	LOG ("Rebuilding bookmarks menu")

	ephy_bookmarks_menu_clean (menu);

	START_PROFILER ("Rebuilding bookmarks menu")

	ensure_bookmark_actions (menu);
	p->folder_actions = gtk_action_group_new ("FolderActions");
	gtk_ui_manager_insert_action_group (p->manager, p->folder_actions, -1);

	g_signal_connect (p->folder_actions, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	p->ui_id = gtk_ui_manager_new_merge_id (p->manager);

	topics = ephy_bookmarks_get_keywords (p->bookmarks);
	children = ephy_node_get_children (topics);

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

	node_list = g_list_sort (node_list, (GCompareFunc) sort_topics);

	for (l = node_list; l != NULL; l = l->next)
	{
		node = (EphyNode *) l->data;

		path = create_submenu (menu, node);
		create_menu (menu, node, path);
		g_free (path);
	}

	not_categorized = ephy_bookmarks_get_not_categorized (p->bookmarks);

	if (ephy_node_get_n_children (not_categorized) > 0)
	{
		create_menu (menu, not_categorized, BOOKMARKS_MENU_PATH);
	}

	g_list_free (node_list);

	STOP_PROFILER ("Rebuilding bookmarks menu")

	menu->priv->needs_update = FALSE;
}

static gboolean
do_update_cb (EphyBookmarksMenu *menu)
{
	LOG ("do_update_cb")

	ephy_bookmarks_menu_rebuild (menu);
	menu->priv->update_tag = 0;

	/* don't run again */
	return FALSE;
}

static void
ephy_bookmarks_menu_maybe_update (EphyBookmarksMenu *menu)
{
	EphyNode *bookmarks;
	GPtrArray *children;

	menu->priv->needs_update = TRUE;

	/* FIXME: is there any way that we get here while the menu is popped up?
	 * if so, needs to do_update NOW
	 */

	/* if there are only a few bookmarks, update soon */
	bookmarks = ephy_bookmarks_get_bookmarks (menu->priv->bookmarks);
	children = ephy_node_get_children (bookmarks);
	if (children->len < GAZILLION)
	{
		if (menu->priv->update_tag == 0)
		{
			menu->priv->update_tag =
				g_timeout_add (UPDATE_DELAY,
					       (GSourceFunc) do_update_cb, menu);
		}
	}
	else if (menu->priv->update_tag != 0)
	{
		/* remove scheduled update, update on demand */
		g_source_remove (menu->priv->update_tag);
		menu->priv->update_tag = 0;
	}
}

static void
ephy_bookmarks_menu_set_window (EphyBookmarksMenu *menu,
				EphyWindow *window)
{
	menu->priv->window = window;
	menu->priv->manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
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
                case PROP_WINDOW:
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
                case PROP_WINDOW:
                        g_value_set_object (value, m->priv->window);
                        break;
        }
}

static void
bookmarks_tree_changed_cb (EphyBookmarks *bookmarks,
			   EphyBookmarksMenu *menu)
{
	LOG ("bookmarks_tree_changed_cb")

	ephy_bookmarks_menu_maybe_update (menu);
}

static void
topics_added_cb (EphyNode *keywords,
		 EphyNode *bmk,
		 EphyBookmarksMenu *menu)
{
	LOG ("topics_added_cb")

	ephy_bookmarks_menu_maybe_update (menu);
}

static void
topics_removed_cb (EphyNode *keywords,
		   EphyNode *child,
		   guint old_index,
		   EphyBookmarksMenu *menu)
{
	LOG ("topics_removed_cb")

	ephy_bookmarks_menu_maybe_update (menu);
}

static void
topic_child_changed_cb (EphyNode *node,
			EphyNode *child,
			guint property_id,
			EphyBookmarksMenu *menu)
{
	LOG ("topic_child_changed_cb id=%d property=%d",
	     ephy_node_get_id (child), property_id)

	if (property_id == EPHY_NODE_KEYWORD_PROP_NAME)
	{
		/* the title of the topic has changed, which may change the
		 * hierarchy.
		 */
		ephy_bookmarks_menu_maybe_update (menu);
	}
}

static void
bookmark_added_cb (EphyNode *bookmarks,
		   EphyNode *bmk,
		   EphyBookmarksMenu *menu)
{
	LOG ("bookmark_added_cb id=%d", ephy_node_get_id (bmk))

	if (menu->priv->bmk_actions != NULL)
	{
		/* If the new bookmark has the node ID of one scheduled to
		 * be removed, remove the old one first then add new one.
		 * This works since the action name depends only on the
		 * node ID. See bug #154805.
		 */
		GSList *l;

		l = g_slist_find (menu->priv->removed_bmks,
				 GUINT_TO_POINTER (ephy_node_get_id (bmk)));
		if (l != NULL)
		{
			remove_action (l->data, menu->priv->bmk_actions);

			menu->priv->removed_bmks = g_slist_delete_link
				(menu->priv->removed_bmks, l);
		}

		add_action_for_bookmark (menu, bmk);

		ephy_bookmarks_menu_maybe_update (menu);
	}
}

static void
bookmark_removed_cb (EphyNode *bookmarks,
		     EphyNode *bmk,
		     guint old_index,
		     EphyBookmarksMenu *menu)
{
	LOG ("bookmark_removed_cb id=%d", ephy_node_get_id (bmk))

	if (menu->priv->bmk_actions != NULL)
	{
		/* we cannot remove the action here since the menu might still
		 * reference it.
		 */
		menu->priv->removed_bmks =
			g_slist_prepend (menu->priv->removed_bmks,
					 GUINT_TO_POINTER (ephy_node_get_id (bmk)));

		ephy_bookmarks_menu_maybe_update (menu);
	}
}

static void
activate_cb (GtkAction *action,
	     EphyBookmarksMenu *menu)
{
	LOG ("activate_cb")

	ephy_bookmarks_menu_rebuild (menu);
}

static void
ephy_bookmarks_menu_init (EphyBookmarksMenu *menu)
{
	menu->priv = EPHY_BOOKMARKS_MENU_GET_PRIVATE (menu);
}

static GObject *
ephy_bookmarks_menu_constructor (GType type,
				 guint n_construct_properties,
				 GObjectConstructParam *construct_params)
{
	EphyBookmarksMenu *menu;
        GObject *object;
	GtkAction *action;
	EphyNode *node;

        object = parent_class->constructor (type, n_construct_properties,
                                            construct_params);

	menu = EPHY_BOOKMARKS_MENU (object);

	g_assert (menu->priv->window != NULL);

	menu->priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	g_signal_connect_object (menu->priv->bookmarks, "tree_changed",
			         G_CALLBACK (bookmarks_tree_changed_cb),
			         menu, 0);

	node = ephy_bookmarks_get_keywords (menu->priv->bookmarks);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
				         (EphyNodeCallback) topics_added_cb,
				         G_OBJECT (menu));
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) topics_removed_cb,
				         G_OBJECT (menu));
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_CHANGED,
				         (EphyNodeCallback) topic_child_changed_cb,
				         G_OBJECT (menu));

	node = ephy_bookmarks_get_bookmarks (menu->priv->bookmarks);
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback) bookmark_added_cb,
					 G_OBJECT (menu));
	ephy_node_signal_connect_object (node, EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) bookmark_removed_cb,
					 G_OBJECT (menu));

	action = gtk_ui_manager_get_action (menu->priv->manager,
					    BOOKMARKS_MENU_PATH);
	g_signal_connect_object (action, "activate",
				 G_CALLBACK (activate_cb), menu, 0);

	ephy_bookmarks_menu_maybe_update (menu);

	return object;
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

	g_slist_free (menu->priv->removed_bmks);

	if (p->bmk_actions != NULL)
	{
		g_object_unref (p->bmk_actions);
	}

	if (p->folder_actions != NULL)
	{
		g_object_unref (p->folder_actions);
	}

	LOG ("EphyBookmarksMenu finalised %p", o);

	G_OBJECT_CLASS (parent_class)->finalize (o);
}

static void
ephy_bookmarks_menu_class_init (EphyBookmarksMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->constructor = ephy_bookmarks_menu_constructor;
	object_class->finalize = ephy_bookmarks_menu_finalize;
	object_class->set_property = ephy_bookmarks_menu_set_property;
	object_class->get_property = ephy_bookmarks_menu_get_property;

	g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "Parent window",
                                                              EPHY_TYPE_WINDOW,
                                                              G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyBookmarksMenuPrivate));
}

EphyBookmarksMenu *
ephy_bookmarks_menu_new (EphyWindow *window)
{
	return EPHY_BOOKMARKS_MENU (g_object_new (EPHY_TYPE_BOOKMARKS_MENU,
						  "window", window,
						  NULL));
}

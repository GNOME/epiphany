/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#include "config.h"

#include "ephy-favorites-menu.h"
#include "ephy-bookmark-action.h"
#include "ephy-link.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <gtk/gtkmenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkuimanager.h>
#include <glib/gprintf.h>

#define LABEL_WIDTH_CHARS	32

#define EPHY_FAVORITES_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FAVORITES_MENU, EphyFavoritesMenuPrivate))

struct _EphyFavoritesMenuPrivate
{
	EphyWindow *window;
	EphyBookmarks *bookmarks;
	GtkActionGroup *action_group;
	guint ui_id;
	guint update_tag;
};

static void	ephy_favorites_menu_class_init	  (EphyFavoritesMenuClass *klass);
static void	ephy_favorites_menu_init	  (EphyFavoritesMenu *menu);
static void	ephy_favorites_menu_finalize      (GObject *o);

enum
{
	PROP_0,
	PROP_WINDOW
};

static gpointer parent_class;

GType
ephy_favorites_menu_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyFavoritesMenuClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_favorites_menu_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphyFavoritesMenu),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_favorites_menu_init
		};
		static const GInterfaceInfo link_info = 
		{
			NULL,
			NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyFavoritesMenu",
					       &our_info, 0);
		g_type_add_interface_static (type,
					     EPHY_TYPE_LINK,
					     &link_info);
	}

	return type;
}

static void
ephy_favorites_menu_clean (EphyFavoritesMenu *menu)
{
	EphyFavoritesMenuPrivate *p = menu->priv;
	GtkUIManager *merge = GTK_UI_MANAGER (ephy_window_get_ui_manager (p->window));

	if (p->ui_id > 0)
	{
		gtk_ui_manager_remove_ui (merge, p->ui_id);
		gtk_ui_manager_ensure_update (merge);
		p->ui_id = 0;
	}

	if (p->action_group != NULL)
	{
		gtk_ui_manager_remove_action_group (merge, p->action_group);
		g_object_unref (p->action_group);
	}
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
		gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars (label, LABEL_WIDTH_CHARS);
        }
}

static void
ephy_favorites_menu_rebuild (EphyFavoritesMenu *menu)
{
	EphyFavoritesMenuPrivate *p = menu->priv;
	gint i;
	EphyNode *fav;
	GPtrArray *children;
	GtkUIManager *merge = GTK_UI_MANAGER (ephy_window_get_ui_manager (p->window));

	LOG ("Rebuilding favorites menu");

	START_PROFILER ("Rebuild favorites menu")

	ephy_favorites_menu_clean (menu);

	fav = ephy_bookmarks_get_favorites (p->bookmarks);
	children = ephy_node_get_children (fav);

	p->action_group = gtk_action_group_new ("FavoritesActions");
	gtk_ui_manager_insert_action_group (merge, p->action_group, -1);
	g_signal_connect (p->action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	p->ui_id = gtk_ui_manager_new_merge_id (merge);

	for (i = 0; i < children->len; i++)
	{
		char verb[20];
		char name[20];
		char accel_path[48];
		EphyNode *node;
		GtkAction *action;

		g_snprintf (verb, sizeof (verb),"GoFav%d", i);
		g_snprintf (name, sizeof (name), "GoFav%dMenu", i);
		g_snprintf (accel_path, sizeof (accel_path),
			    "<Actions>/FavoritesActions/%s", verb);

		node = g_ptr_array_index (children, i);

		action = ephy_bookmark_action_new (verb, node);
		gtk_action_set_accel_path (action, accel_path);
		gtk_action_group_add_action (p->action_group, action);
		g_object_unref (action);
		g_signal_connect_swapped (action, "open-link",
				  	  G_CALLBACK (ephy_link_open), menu);

		gtk_ui_manager_add_ui (merge, p->ui_id,
				       "/menubar/GoMenu",
				       name, verb,
				       GTK_UI_MANAGER_MENUITEM, FALSE);
	}

	STOP_PROFILER ("Rebuild favorites menu")
}

static void
ephy_favorites_menu_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	EphyFavoritesMenu *menu = EPHY_FAVORITES_MENU (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			menu->priv->window = g_value_get_object (value);
			ephy_favorites_menu_rebuild (menu);
			break;
	}
}

static void
ephy_favorites_menu_get_property (GObject *object,
				  guint prop_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	EphyFavoritesMenu *menu = EPHY_FAVORITES_MENU (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, menu->priv->window);
			break;
	}
}


static void
ephy_favorites_menu_class_init (EphyFavoritesMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_favorites_menu_finalize;
	object_class->set_property = ephy_favorites_menu_set_property;
	object_class->get_property = ephy_favorites_menu_get_property;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyFavoritesMenuPrivate));
}

static gboolean
do_updates (EphyFavoritesMenu *menu)
{
	ephy_favorites_menu_rebuild (menu);

	menu->priv->update_tag = 0;

	/* don't run again */
	return FALSE;
}

static void
fav_removed_cb (EphyNode *node,
		EphyNode *child,
		guint old_index,
		EphyFavoritesMenu *menu)
{
	if (menu->priv->update_tag == 0)
	{
		menu->priv->update_tag = g_idle_add((GSourceFunc)do_updates, menu);
	}
}

static void
fav_added_cb (EphyNode *node,
	      EphyNode *child,
	      EphyFavoritesMenu *menu)
{
	if (menu->priv->update_tag == 0)
	{
		menu->priv->update_tag = g_idle_add((GSourceFunc)do_updates, menu);
	}
}

static void
ephy_favorites_menu_init (EphyFavoritesMenu *menu)
{
	EphyFavoritesMenuPrivate *p = EPHY_FAVORITES_MENU_GET_PRIVATE (menu);
	EphyNode *fav;
	menu->priv = p;

	menu->priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);

	fav = ephy_bookmarks_get_favorites (menu->priv->bookmarks);
	ephy_node_signal_connect_object (fav,
				         EPHY_NODE_CHILD_REMOVED,
				         (EphyNodeCallback) fav_removed_cb,
				         G_OBJECT (menu));
	ephy_node_signal_connect_object (fav,
				         EPHY_NODE_CHILD_ADDED,
				         (EphyNodeCallback) fav_added_cb,
				         G_OBJECT (menu));
}

static void
ephy_favorites_menu_finalize (GObject *o)
{
	EphyFavoritesMenu *menu = EPHY_FAVORITES_MENU (o);
	EphyNode *fav;

	fav = ephy_bookmarks_get_favorites (menu->priv->bookmarks);
	ephy_node_signal_disconnect_object (fav,
				 	    EPHY_NODE_CHILD_REMOVED,
				 	    (EphyNodeCallback) fav_removed_cb,
				 	    G_OBJECT (menu));
	ephy_node_signal_disconnect_object (fav,
					    EPHY_NODE_CHILD_ADDED,
					    (EphyNodeCallback) fav_added_cb,
					    G_OBJECT (menu));

	if (menu->priv->update_tag != 0)
	{
		g_source_remove (menu->priv->update_tag);
	}

	ephy_favorites_menu_clean (menu);

	G_OBJECT_CLASS (parent_class)->finalize (o);
}

EphyFavoritesMenu *
ephy_favorites_menu_new (EphyWindow *window)
{
	return g_object_new (EPHY_TYPE_FAVORITES_MENU,
			     "window", window,
			     NULL);
}

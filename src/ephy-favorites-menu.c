/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#include "ephy-favorites-menu.h"
#include "ephy-bookmark-action.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <gtk/gtkuimanager.h>
#include <glib/gprintf.h>

/**
 * Private data
 */

#define EPHY_FAVORITES_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FAVORITES_MENU, EphyFavoritesMenuPrivate))

struct _EphyFavoritesMenuPrivate
{
	EphyWindow *window;
	EphyBookmarks *bookmarks;
	GtkActionGroup *action_group;
	guint ui_id;
};

/**
 * Private functions, only availble from this file
 */
static void	ephy_favorites_menu_class_init	  (EphyFavoritesMenuClass *klass);
static void	ephy_favorites_menu_init	  (EphyFavoritesMenu *wrhm);
static void	ephy_favorites_menu_finalize      (GObject *o);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static gpointer parent_class;

GType
ephy_favorites_menu_get_type (void)
{
        static GType ephy_favorites_menu_type = 0;

        if (ephy_favorites_menu_type == 0)
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

                ephy_favorites_menu_type = g_type_register_static (G_TYPE_OBJECT,
							           "EphyFavoritesMenu",
							           &our_info, 0);
        }
        return ephy_favorites_menu_type;
}

static void
ephy_favorites_menu_clean (EphyFavoritesMenu *wrhm)
{
	EphyFavoritesMenuPrivate *p = wrhm->priv;
	GtkUIManager *merge = GTK_UI_MANAGER (p->window->ui_merge);

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
go_location_cb (GtkAction *action, char *location, EphyWindow *window)
{
	ephy_window_load_url (window, location);
}

static void
ephy_favorites_menu_rebuild (EphyFavoritesMenu *wrhm)
{
	EphyFavoritesMenuPrivate *p = wrhm->priv;
	gint i;
	EphyNode *fav;
	GPtrArray *children;
	GtkUIManager *merge = GTK_UI_MANAGER (p->window->ui_merge);

	LOG ("Rebuilding favorites menu")

	START_PROFILER ("Rebuild favorites menu")

	ephy_favorites_menu_clean (wrhm);

	fav = ephy_bookmarks_get_favorites (p->bookmarks);
	children = ephy_node_get_children (fav);

	p->action_group = gtk_action_group_new ("FavoritesActions");
	gtk_ui_manager_insert_action_group (merge, p->action_group, 0);
	p->ui_id = gtk_ui_manager_new_merge_id (merge);

	for (i = 0; i < children->len; i++)
	{
		char verb[20];
		char name[20];
		EphyNode *node;
		GtkAction *action;

		g_sprintf (verb, "GoFav%d", i);
		g_sprintf (name, "GoFav%dMenu", i);

		node = g_ptr_array_index (children, i);

		action = ephy_bookmark_action_new (verb,
						   ephy_node_get_id (node));
		gtk_action_group_add_action (p->action_group, action);
		g_object_unref (action);
		g_signal_connect (action, "go_location",
				  G_CALLBACK (go_location_cb), p->window);

		gtk_ui_manager_add_ui (merge, p->ui_id,
				       "/menubar/GoMenu/GoFavorites",
				       name, verb,
				       GTK_UI_MANAGER_MENUITEM, FALSE);
	}
	ephy_node_thaw (fav);

	STOP_PROFILER ("Rebuild favorites menu")
}

static void
ephy_favorites_menu_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
        EphyFavoritesMenu *m = EPHY_FAVORITES_MENU (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        m->priv->window = g_value_get_object (value);
			ephy_favorites_menu_rebuild (m);
                        break;
        }
}

static void
ephy_favorites_menu_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
        EphyFavoritesMenu *m = EPHY_FAVORITES_MENU (object);

        switch (prop_id)
        {
                case PROP_EPHY_WINDOW:
                        g_value_set_object (value, m->priv->window);
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
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
							      "Parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyFavoritesMenuPrivate));
}

static void
ephy_favorites_menu_init (EphyFavoritesMenu *wrhm)
{
	EphyFavoritesMenuPrivate *p = EPHY_FAVORITES_MENU_GET_PRIVATE (wrhm);
	wrhm->priv = p;

	wrhm->priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	wrhm->priv->ui_id = 0;
	wrhm->priv->action_group = NULL;
}

static void
ephy_favorites_menu_finalize (GObject *o)
{
	EphyFavoritesMenu *wrhm = EPHY_FAVORITES_MENU (o);
	EphyFavoritesMenuPrivate *p = wrhm->priv;

	if (p->action_group != NULL)
	{
		gtk_ui_manager_remove_action_group
			(GTK_UI_MANAGER (p->window->ui_merge),
			 p->action_group);
		g_object_unref (p->action_group);
	}

	G_OBJECT_CLASS (parent_class)->finalize (o);
}

EphyFavoritesMenu *
ephy_favorites_menu_new (EphyWindow *window)
{
	EphyFavoritesMenu *ret = g_object_new (EPHY_TYPE_FAVORITES_MENU,
					       "EphyWindow", window,
					       NULL);
	return ret;
}

void ephy_favorites_menu_update	(EphyFavoritesMenu *wrhm)
{
	ephy_favorites_menu_rebuild (wrhm);
}

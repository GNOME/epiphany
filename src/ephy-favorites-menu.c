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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-favorites-menu.h"
#include "ephy-gobject-misc.h"
#include "ephy-string.h"
#include "egg-menu-merge.h"
#include "ephy-marshal.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <string.h>
#include <stdlib.h>
#include <libxml/entities.h>

#define MAX_LABEL_LENGTH 30

/**
 * Private data
 */
struct _EphyFavoritesMenuPrivate
{
	EphyWindow *window;
	EphyBookmarks *bookmarks;
	EggActionGroup *action_group;
	guint ui_id;
};

typedef struct
{
	EphyWindow *window;
	const char *url;
} FavoriteData;

/**
 * Private functions, only availble from this file
 */
static void	ephy_favorites_menu_class_init	  (EphyFavoritesMenuClass *klass);
static void	ephy_favorites_menu_init	  (EphyFavoritesMenu *wrhm);
static void	ephy_favorites_menu_finalize_impl (GObject *o);
static void	ephy_favorites_menu_rebuild	  (EphyFavoritesMenu *wrhm);
static void     ephy_favorites_menu_set_property  (GObject *object,
						   guint prop_id,
						   const GValue *value,
						   GParamSpec *pspec);
static void	ephy_favorites_menu_get_property  (GObject *object,
						   guint prop_id,
						   GValue *value,
						   GParamSpec *pspec);

enum
{
	PROP_0,
	PROP_EPHY_WINDOW
};

static gpointer g_object_class;

/**
 * EphyFavoritesMenu object
 */
MAKE_GET_TYPE (ephy_favorites_menu,
	       "EphyFavoritesMenu", EphyFavoritesMenu,
	       ephy_favorites_menu_class_init, ephy_favorites_menu_init,
	       G_TYPE_OBJECT);

static void
ephy_favorites_menu_class_init (EphyFavoritesMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = ephy_favorites_menu_finalize_impl;
	g_object_class = g_type_class_peek_parent (klass);

	object_class->set_property = ephy_favorites_menu_set_property;
	object_class->get_property = ephy_favorites_menu_get_property;

	g_object_class_install_property (object_class,
                                         PROP_EPHY_WINDOW,
                                         g_param_spec_object ("EphyWindow",
                                                              "EphyWindow",
                                                              "Parent window",
                                                              EPHY_WINDOW_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
ephy_favorites_menu_init (EphyFavoritesMenu *wrhm)
{
	EphyFavoritesMenuPrivate *p = g_new0 (EphyFavoritesMenuPrivate, 1);
	wrhm->priv = p;

	wrhm->priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	wrhm->priv->ui_id = -1;
	wrhm->priv->action_group = NULL;
}

static void
ephy_favorites_menu_clean (EphyFavoritesMenu *wrhm)
{
	EphyFavoritesMenuPrivate *p = wrhm->priv;
	EggMenuMerge *merge = EGG_MENU_MERGE (p->window->ui_merge);

	if (p->ui_id >= 0)
	{
		egg_menu_merge_remove_ui (merge, p->ui_id);
	}

	if (p->action_group != NULL)
	{
		egg_menu_merge_remove_action_group (merge, p->action_group);
		g_object_unref (p->action_group);
	}
}

static void
ephy_favorites_menu_finalize_impl (GObject *o)
{
	EphyFavoritesMenu *wrhm = EPHY_FAVORITES_MENU (o);
	EphyFavoritesMenuPrivate *p = wrhm->priv;

	ephy_favorites_menu_clean (wrhm);

	g_free (p);

	G_OBJECT_CLASS (g_object_class)->finalize (o);
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

EphyFavoritesMenu *
ephy_favorites_menu_new (EphyWindow *window)
{
	EphyFavoritesMenu *ret = g_object_new (EPHY_TYPE_FAVORITES_MENU,
					       "EphyWindow", window,
					       NULL);
	return ret;
}

static void
ephy_favorites_menu_verb_cb (BonoboUIComponent *uic,
			     FavoriteData *data,
			     const char *cname)
{
	ephy_window_load_url (data->window, data->url);
}

static void
ephy_favorites_menu_rebuild (EphyFavoritesMenu *wrhm)
{
	EphyFavoritesMenuPrivate *p = wrhm->priv;
	GString *xml;
	gint i;
	EphyNode *fav;
	GPtrArray *children;
	EggMenuMerge *merge = EGG_MENU_MERGE (p->window->ui_merge);

	LOG ("Rebuilding recent history menu")

	fav = ephy_bookmarks_get_favorites (p->bookmarks);
	children = ephy_node_get_children (fav);

	xml = g_string_new (NULL);
	g_string_append (xml, "<Root><menu><submenu name=\"GoMenu\">"
			      "<placeholder name=\"GoFavorites\">");

	p->action_group = egg_action_group_new ("FavoritesActions");
	egg_menu_merge_insert_action_group (merge, p->action_group, 0);

	for (i = 0; i < children->len; i++)
	{
		char *verb = g_strdup_printf ("GoFav%d", i);
		char *title_s;
		const char *title;
		const char *url;
		xmlChar *label_x;
		EphyNode *child;
		FavoriteData *data;
		EggAction *action;

		child = g_ptr_array_index (children, i);
		title = ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_TITLE);
		url = ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_LOCATION);
		title_s = ephy_string_shorten (title, MAX_LABEL_LENGTH);
		label_x = xmlEncodeSpecialChars (NULL, title_s);

		data = g_new0 (FavoriteData, 1);
		data->window = wrhm->priv->window;
		data->url = url;

		action = g_object_new (EGG_TYPE_ACTION,
				       "name", verb,
				       "label", label_x,
				       "tooltip", "Hello",
				       "stock_id", NULL,
				       NULL);
		g_signal_connect_closure
			(action, "activate",
			 g_cclosure_new (G_CALLBACK (ephy_favorites_menu_verb_cb),
					 data,
					 (GClosureNotify)g_free),
			 FALSE);
		egg_action_group_add_action (p->action_group, action);
		g_object_unref (action);

		g_string_append (xml, "<menuitem name=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "Menu");
		g_string_append (xml, "\" verb=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "\"/>\n");

		xmlFree (label_x);
		g_free (title_s);
		g_free (verb);
	}

	ephy_node_thaw (fav);

	g_string_append (xml, "</placeholder></submenu></menu></Root>");

	if (children->len > 0)
	{
		GError *error = NULL;

		p->ui_id = egg_menu_merge_add_ui_from_string
			(merge, xml->str, -1, &error);
	}

	g_string_free (xml, TRUE);
}

void ephy_favorites_menu_update	(EphyFavoritesMenu *wrhm)
{
	ephy_favorites_menu_rebuild (wrhm);
}

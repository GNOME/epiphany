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
#include "ephy-bonobo-extensions.h"
#include "ephy-marshal.h"
#include "ephy-shell.h"

#include <string.h>
#include <stdlib.h>
#include <libxml/entities.h>

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

#define MAX_LABEL_LENGTH 30

/**
 * Private data
 */
struct _EphyFavoritesMenuPrivate
{
	gchar *path;
	EphyWindow *window;
	EphyBookmarks *bookmarks;
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
}

static void
ephy_favorites_menu_finalize_impl (GObject *o)
{
	EphyFavoritesMenu *wrhm = EPHY_FAVORITES_MENU (o);
	EphyFavoritesMenuPrivate *p = wrhm->priv;

	if (p->path)
	{
		g_free (p->path);
	}

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

void
ephy_favorites_menu_set_path (EphyFavoritesMenu *wrhm,
			      const gchar *path)
{
	EphyFavoritesMenuPrivate *p;

	g_return_if_fail (EPHY_IS_FAVORITES_MENU (wrhm));
	g_return_if_fail (path != NULL);

	p = wrhm->priv;

	if (p->path)
	{
		g_free (p->path);
	}
	p->path = g_strdup (path);

	ephy_favorites_menu_update (wrhm);
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
	BonoboUIComponent *uic = BONOBO_UI_COMPONENT (p->window->ui_component);

	if (!p->path) return;

	ephy_bonobo_clear_path (uic, p->path);

	DEBUG_MSG (("Rebuilding recent history menu\n"));

	fav = ephy_bookmarks_get_favorites (p->bookmarks);
	children = ephy_node_get_children (fav);

	xml = g_string_new (NULL);
	g_string_append_printf (xml, "<placeholder name=\"wrhm%x\">\n", (guint) wrhm);

	for (i = 0; i < children->len; i++)
	{
		char *verb = g_strdup_printf ("Wrhm%xn%d", (guint) wrhm, i);
		char *title_s;
		const char *title;
		const char *url;
		xmlChar *label_x;
		EphyNode *child;
		FavoriteData *data;

		child = g_ptr_array_index (children, i);
		title = ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_TITLE);
		url = ephy_node_get_property_string (child, EPHY_NODE_BMK_PROP_LOCATION);
		title_s = ephy_string_shorten (title, MAX_LABEL_LENGTH);
		label_x = xmlEncodeSpecialChars (NULL, title_s);

		g_string_append (xml, "<menuitem name=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "\" label=\"");
		g_string_append (xml, label_x);
		g_string_append (xml, "\" verb=\"");
		g_string_append (xml, verb);
		g_string_append (xml, "\"/>\n");

		data = g_new0 (FavoriteData, 1);
		data->window = wrhm->priv->window;
		data->url = url;
		bonobo_ui_component_add_verb_full (uic, verb, g_cclosure_new
				(G_CALLBACK (ephy_favorites_menu_verb_cb), data,
				(GClosureNotify)g_free));

		xmlFree (label_x);
		g_free (title_s);
		g_free (verb);
	}

	ephy_node_thaw (fav);

	g_string_append (xml, "</placeholder>\n");
	DEBUG_MSG (("\n%s\n", xml->str));
	if (children->len > 0)
	{
		bonobo_ui_component_set (uic, p->path,
					 xml->str, NULL);
	}
	g_string_free (xml, TRUE);
}

void ephy_favorites_menu_update	(EphyFavoritesMenu *wrhm)
{
	ephy_favorites_menu_rebuild (wrhm);
}

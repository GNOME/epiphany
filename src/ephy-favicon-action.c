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

#include "ephy-favicon-action.h"
#include "eggtoolitem.h"
#include "ephy-window.h"
#include "ephy-tab.h"
#include "ephy-dnd.h"
#include "ephy-favicon-cache.h"
#include "ephy-shell.h"

struct EphyFaviconActionPrivate
{
	EphyWindow *window;
	char *icon;
};

enum
{
	PROP_0,
	PROP_WINDOW,
	PROP_ICON
};

static void ephy_favicon_action_init       (EphyFaviconAction *action);
static void ephy_favicon_action_class_init (EphyFaviconActionClass *class);

static GObjectClass *parent_class = NULL;

GType
ephy_favicon_action_get_type (void)
{
	static GtkType type = 0;

	if (!type)
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyFaviconActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_favicon_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyFaviconAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_favicon_action_init,
		};

		type = g_type_register_static (EGG_TYPE_ACTION,
					       "EphyFaviconAction",
					       &type_info, 0);
	}
	return type;
}

static GtkWidget *
create_tool_item (EggAction *action)
{
	GtkWidget *image;
	GtkWidget *ebox;
	GtkWidget *item;

	item = (* EGG_ACTION_CLASS (parent_class)->create_tool_item) (action);

        ebox = gtk_event_box_new ();
	image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (ebox), image);
	gtk_container_set_border_width (GTK_CONTAINER (ebox), 2);
	gtk_container_add (GTK_CONTAINER (item), ebox);
	gtk_widget_show (image);
	gtk_widget_show (ebox);

	g_object_set_data (G_OBJECT (item), "image", image);

	return item;
}

static void
each_url_get_data_binder (EphyDragEachSelectedItemDataGet iteratee,
			  gpointer iterator_context, gpointer data)
{
	const char *location;
	EphyTab *tab;
	EphyWindow *window = EPHY_WINDOW(iterator_context);

	tab = ephy_window_get_active_tab (window);
	location = ephy_tab_get_location (tab);

	iteratee (location, -1, -1, -1, -1, data);
}

static void
favicon_drag_data_get_cb (GtkWidget *widget,
                          GdkDragContext *context,
                          GtkSelectionData *selection_data,
                          guint info,
                          guint32 time,
                          EphyWindow *window)
{
        g_assert (widget != NULL);
        g_return_if_fail (context != NULL);

        ephy_dnd_drag_data_get (widget, context, selection_data,
                info, time, window, each_url_get_data_binder);
}

static void
ephy_favicon_action_sync_icon (EggAction *action, GParamSpec *pspec,
			       GtkWidget *proxy)
{
	EphyFaviconAction *fav_action = EPHY_FAVICON_ACTION (action);
	char *url;
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	EphyFaviconCache *cache;

	cache = ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell));

	url = fav_action->priv->icon;
	image = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "image"));

	if (url)
	{
		pixbuf = ephy_favicon_cache_get (cache, url);
	}

	if (pixbuf)
	{
		gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	}
	else
	{
		gtk_image_set_from_stock (GTK_IMAGE (image),
					  GTK_STOCK_JUMP_TO,
					  GTK_ICON_SIZE_MENU);
	}
}

static void
connect_proxy (EggAction *action, GtkWidget *proxy)
{
	ephy_dnd_url_drag_source_set (proxy);

	g_signal_connect (proxy,
			  "drag_data_get",
			  G_CALLBACK (favicon_drag_data_get_cb),
			  EPHY_FAVICON_ACTION (action)->priv->window);
	g_signal_connect_object (action, "notify::icon",
				 G_CALLBACK (ephy_favicon_action_sync_icon),
				 proxy, 0);

	(* EGG_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
}

static void
ephy_favicon_action_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	EphyFaviconAction *fav;

	fav = EPHY_FAVICON_ACTION (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			fav->priv->window = EPHY_WINDOW (g_value_get_object (value));
			break;
		case PROP_ICON:
			g_free (fav->priv->icon);
			fav->priv->icon = g_strdup (g_value_get_string (value));
			g_object_notify(object, "icon");
			break;
	}
}

static void
ephy_favicon_action_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	EphyFaviconAction *fav;

	fav = EPHY_FAVICON_ACTION (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, fav->priv->window);
			break;
		case PROP_ICON:
			g_value_set_object (value, fav->priv->icon);
			break;
	}
}

static void
ephy_favicon_action_class_init (EphyFaviconActionClass *class)
{
	EggActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->set_property = ephy_favicon_action_set_property;
	object_class->get_property = ephy_favicon_action_get_property;

	parent_class = g_type_class_peek_parent (class);
	action_class = EGG_ACTION_CLASS (class);

	action_class->toolbar_item_type = EGG_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "The window",
                                                              G_TYPE_OBJECT,
                                                              G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_ICON,
                                         g_param_spec_string  ("icon",
                                                               "Icon",
                                                               "The icon",
                                                               NULL,
                                                               G_PARAM_READWRITE));
}

static void
ephy_favicon_action_init (EphyFaviconAction *action)
{
	action->priv = g_new0 (EphyFaviconActionPrivate, 1);
	action->priv->icon = NULL;
}

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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-favicon-action.h"
#include "ephy-window.h"
#include "ephy-tab.h"
#include "ephy-dnd.h"
#include "ephy-favicon-cache.h"
#include "ephy-shell.h"
#include "ephy-debug.h"

#include <gtk/gtktoolitem.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkeventbox.h>

static GtkTargetEntry url_drag_types [] =
{
        { EPHY_DND_URI_LIST_TYPE,   0, 0 },
        { EPHY_DND_TEXT_TYPE,       0, 1 },
        { EPHY_DND_URL_TYPE,        0, 2 }
};
static int n_url_drag_types = G_N_ELEMENTS (url_drag_types);

#define EPHY_FAVICON_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FAVICON_ACTION, EphyFaviconActionPrivate))

struct EphyFaviconActionPrivate
{
	EphyWindow *window;
	char *icon;
	EphyFaviconCache *cache;
};

enum
{
	PROP_0,
	PROP_WINDOW,
	PROP_ICON
};

static void ephy_favicon_action_init       (EphyFaviconAction *action);
static void ephy_favicon_action_class_init (EphyFaviconActionClass *class);
static void ephy_favicon_action_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

GType
ephy_favicon_action_get_type (void)
{
	static GType type = 0;

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

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyFaviconAction",
					       &type_info, 0);
	}
	return type;
}

static void
each_url_get_data_binder (EphyDragEachSelectedItemDataGet iteratee,
			  gpointer iterator_context, gpointer data)
{
	const char *title;
	char *location;
	EphyTab *tab;
	EphyEmbed *embed;
	EphyWindow *window = EPHY_WINDOW(iterator_context);

	tab = ephy_window_get_active_tab (window);
	embed = ephy_tab_get_embed (tab);
	location = ephy_embed_get_location (embed, TRUE);
	title = ephy_tab_get_title (tab);

	iteratee (location, title, data);

	g_free (location);
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
                time, window, each_url_get_data_binder);
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *image;
	GtkWidget *ebox;
	GtkWidget *item;

	item = GTK_WIDGET (gtk_tool_item_new ());

	ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
	image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (ebox), image);
	gtk_container_set_border_width (GTK_CONTAINER (ebox), 2);
	gtk_container_add (GTK_CONTAINER (item), ebox);
	gtk_widget_show (image);
	gtk_widget_show (ebox);

	g_object_set_data (G_OBJECT (item), "image", image);

	gtk_drag_source_set (ebox,
                             GDK_BUTTON1_MASK,
                             url_drag_types,
                             n_url_drag_types,
                             GDK_ACTION_COPY);
	g_signal_connect (ebox,
			  "drag_data_get",
			  G_CALLBACK (favicon_drag_data_get_cb),
			  EPHY_FAVICON_ACTION (action)->priv->window);

	return item;
}

static void
ephy_favicon_action_sync_icon (GtkAction *action, GParamSpec *pspec,
			       GtkWidget *proxy)
{
	EphyFaviconAction *fav_action = EPHY_FAVICON_ACTION (action);
	char *url;
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;

	url = fav_action->priv->icon;
	image = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "image"));

	if (url)
	{
		pixbuf = ephy_favicon_cache_get (fav_action->priv->cache, url);
	}

	if (pixbuf)
	{
		gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
		g_object_unref (pixbuf);
	}
	else
	{
		gtk_image_set_from_stock (GTK_IMAGE (image),
					  GTK_STOCK_JUMP_TO,
					  GTK_ICON_SIZE_MENU);
	}
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	ephy_favicon_action_sync_icon (action, NULL, proxy);
	g_signal_connect_object (action, "notify::icon",
				 G_CALLBACK (ephy_favicon_action_sync_icon),
				 proxy, 0);

	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
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
			fav->priv->icon = g_value_dup_string (value);
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
			g_value_set_string (value, fav->priv->icon);
			break;
	}
}

static void
ephy_favicon_action_class_init (EphyFaviconActionClass *class)
{
	GtkActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->set_property = ephy_favicon_action_set_property;
	object_class->get_property = ephy_favicon_action_get_property;
        object_class->finalize = ephy_favicon_action_finalize;

	parent_class = g_type_class_peek_parent (class);
	action_class = GTK_ACTION_CLASS (class);

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
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

	g_type_class_add_private (object_class, sizeof(EphyFaviconActionPrivate));
}

static void
ephy_favicon_action_init (EphyFaviconAction *action)
{
	action->priv = EPHY_FAVICON_ACTION_GET_PRIVATE (action);

	action->priv->icon = NULL;

	action->priv->cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache
		(EPHY_EMBED_SHELL (ephy_shell)));
	g_object_ref (action->priv->cache);
}

static void
ephy_favicon_action_finalize (GObject *object)
{
        EphyFaviconAction *action = EPHY_FAVICON_ACTION (object);

	g_free (action->priv->icon);

	g_object_unref (action->priv->cache);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

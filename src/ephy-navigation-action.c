/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2008 Jan Alonzo
 *  Copyright © 2009 Igalia S.L.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-navigation-action.h"

#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-link.h"
#include "ephy-shell.h"
#include "ephy-type-builtins.h"
#include "ephy-window.h"

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define EPHY_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NAVIGATION_ACTION, EphyNavigationActionPrivate))

struct _EphyNavigationActionPrivate
{
	EphyWindow *window;
	char *arrow_tooltip;
	guint statusbar_cid;
};

enum
{
	PROP_0,
	PROP_ARROW_TOOLTIP,
	PROP_WINDOW
};

static void ephy_navigation_action_init       (EphyNavigationAction *action);
static void ephy_navigation_action_class_init (EphyNavigationActionClass *class);

G_DEFINE_TYPE (EphyNavigationAction, ephy_navigation_action, EPHY_TYPE_LINK_ACTION)


#define MAX_LABEL_LENGTH 48

static GtkWidget *
build_dropdown_menu (EphyNavigationAction *action)
{
	EphyNavigationActionClass *class = EPHY_NAVIGATION_ACTION_GET_CLASS (action);

	return class->build_dropdown_menu (action);
}

static void
menu_activated_cb (GtkMenuToolButton *button,
		   EphyNavigationAction *action)
{
	GtkWidget *menu = NULL;

	LOG ("menu_activated_cb");

	menu = build_dropdown_menu (action);
	gtk_menu_tool_button_set_menu (button, menu);
}

static void
connect_proxy (GtkAction *gaction,
	       GtkWidget *proxy)
{
	LOG ("Connect navigation action proxy");

	if (GTK_IS_MENU_TOOL_BUTTON (proxy))
	{
		EphyNavigationAction *action = EPHY_NAVIGATION_ACTION (gaction);
		EphyNavigationActionPrivate *priv = action->priv;
		GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (proxy);
		GtkWidget *menu;

		/* set dummy menu so the arrow gets sensitive */
		menu = gtk_menu_new ();
		gtk_menu_tool_button_set_menu (button, menu);
		gtk_menu_tool_button_set_arrow_tooltip_text (button, priv->arrow_tooltip);

		g_signal_connect (proxy, "show-menu",
				  G_CALLBACK (menu_activated_cb), gaction);
	}

	GTK_ACTION_CLASS (ephy_navigation_action_parent_class)->connect_proxy (gaction, proxy);
}

static void
ephy_navigation_action_init (EphyNavigationAction *action)
{
	action->priv = EPHY_NAVIGATION_ACTION_GET_PRIVATE (action);
}

static void
ephy_navigation_action_finalize (GObject *object)
{
	EphyNavigationAction *action = EPHY_NAVIGATION_ACTION (object);

	g_free (action->priv->arrow_tooltip);

	G_OBJECT_CLASS (ephy_navigation_action_parent_class)->finalize (object);
}

static void
ephy_navigation_action_set_property (GObject *object,
				     guint prop_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	EphyNavigationAction *nav = EPHY_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			nav->priv->arrow_tooltip = g_value_dup_string (value);
			g_object_notify (object, "tooltip");
			break;
		case PROP_WINDOW:
			nav->priv->window = EPHY_WINDOW (g_value_get_object (value));
			break;
	}
}

static void
ephy_navigation_action_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	EphyNavigationAction *nav = EPHY_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			g_value_set_string (value, nav->priv->arrow_tooltip);
			break;
		case PROP_WINDOW:
			g_value_set_object (value, nav->priv->window);
			break;
	}
}

static void
ephy_navigation_action_class_init (EphyNavigationActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->finalize = ephy_navigation_action_finalize;
	object_class->set_property = ephy_navigation_action_set_property;
	object_class->get_property = ephy_navigation_action_get_property;

	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
	action_class->connect_proxy = connect_proxy;

	class->build_dropdown_menu = NULL;

	g_object_class_install_property (object_class,
					 PROP_ARROW_TOOLTIP,
					 g_param_spec_string ("arrow-tooltip", NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window", NULL, NULL,
							      G_TYPE_OBJECT,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyNavigationActionPrivate));
}

EphyWindow *
_ephy_navigation_action_get_window (EphyNavigationAction *action)
{
	  g_return_val_if_fail (EPHY_IS_NAVIGATION_ACTION (action), NULL);

	  return action->priv->window;
}

guint
_ephy_navigation_action_get_statusbar_context_id (EphyNavigationAction *action)
{
	  g_return_val_if_fail (EPHY_IS_NAVIGATION_ACTION (action), 0);

	  return action->priv->statusbar_cid;
}

GtkWidget *
_ephy_navigation_action_new_history_menu_item (const char *origtext,
					       const char *address)
{
	EphyFaviconCache *cache;
	EphyHistory *history;
	GtkWidget *item, *image;
	GdkPixbuf *icon = NULL;
	GtkLabel *label;
	const char *icon_address;

	g_return_val_if_fail (address != NULL && origtext != NULL, NULL);

	item = gtk_image_menu_item_new_with_label (origtext);

	label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (item)));
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MAX_LABEL_LENGTH);

	history = EPHY_HISTORY
		(ephy_embed_shell_get_global_history (embed_shell));
	icon_address = ephy_history_get_icon (history, address);

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (embed_shell));
	icon = ephy_favicon_cache_get (cache, icon_address);

	if (icon != NULL)
	{
		image = gtk_image_new_from_pixbuf (icon);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		gtk_widget_show (image);
		g_object_unref (icon);
	}

	gtk_widget_show (item);

	return item;
}

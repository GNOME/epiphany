/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2008 Jan Alonzo
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
#include "ephy-type-builtins.h"
#include "ephy-window.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-link.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#define HISTORY_ITEM_DATA_KEY	  "HistoryItem"
#define URL_DATA_KEY	          "GoURL"
#define WEBKIT_BACK_FORWARD_LIMIT 100

#define EPHY_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NAVIGATION_ACTION, EphyNavigationActionPrivate))

typedef enum
{
	WEBKIT_HISTORY_BACKWARD,
	WEBKIT_HISTORY_FORWARD
} WebKitHistoryType;

struct _EphyNavigationActionPrivate
{
	EphyWindow *window;
	EphyNavigationDirection direction;
	char *arrow_tooltip;
	guint statusbar_cid;
};

enum
{
	PROP_0,
	PROP_ARROW_TOOLTIP,
	PROP_DIRECTION,
	PROP_WINDOW
};

static void ephy_navigation_action_init       (EphyNavigationAction *action);
static void ephy_navigation_action_class_init (EphyNavigationActionClass *class);

G_DEFINE_TYPE (EphyNavigationAction, ephy_navigation_action, EPHY_TYPE_LINK_ACTION)

#define MAX_LABEL_LENGTH 48

static GtkWidget *
new_history_menu_item (const char *origtext,
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

	label = GTK_LABEL (GTK_BIN (item)->child);
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

static void
activate_back_or_forward_menu_item_cb (GtkWidget *menuitem,
				       EphyNavigationAction *action)
{
	WebKitWebHistoryItem *item;
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (action->priv->window));
	g_return_if_fail (embed != NULL);

	if (ephy_gui_is_middle_click ())
	{
		embed = ephy_link_open (EPHY_LINK (action), "about:blank", NULL,
				        EPHY_LINK_NEW_TAB);
		g_return_if_fail (embed != NULL);
	}

	item = (WebKitWebHistoryItem*)g_object_get_data (G_OBJECT (menuitem), HISTORY_ITEM_DATA_KEY);
	g_return_if_fail (item != NULL);

	webkit_web_view_go_to_back_forward_item (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), item);
}

static void
select_menu_item_cb (GtkWidget *menuitem,
	             EphyNavigationAction *action)
{
	const char *url;
	GtkWidget *statusbar;
	WebKitWebHistoryItem *item;

	item = (WebKitWebHistoryItem*)g_object_get_data (G_OBJECT (menuitem), HISTORY_ITEM_DATA_KEY);
	if (item)
	{
		url = webkit_web_history_item_get_uri (item);
	}
	else
	{
		url = g_object_get_data (G_OBJECT (menuitem), URL_DATA_KEY);
		g_return_if_fail (url != NULL);
	}

	statusbar = ephy_window_get_statusbar (action->priv->window);

	gtk_statusbar_push (GTK_STATUSBAR (statusbar), action->priv->statusbar_cid, url);
}

static void
deselect_menu_item_cb (GtkWidget *menuitem,
	               EphyNavigationAction *action)
{
	GtkWidget *statusbar;

	statusbar = ephy_window_get_statusbar (action->priv->window);

	gtk_statusbar_pop (GTK_STATUSBAR (statusbar), action->priv->statusbar_cid);
}

static void
activate_up_menu_item_cb (GtkWidget *menuitem,
			  EphyNavigationAction *action)
{
	EphyEmbed *embed;
	char *url;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (action->priv->window));
	g_return_if_fail (embed != NULL);

	url = g_object_get_data (G_OBJECT (menuitem), URL_DATA_KEY);
	g_return_if_fail (url != NULL);

	ephy_link_open (EPHY_LINK (action), url, NULL,
			ephy_gui_is_middle_click () ? EPHY_LINK_NEW_TAB : 0);
}

static GList*
webkit_construct_history_list (WebKitWebView *web_view, WebKitHistoryType hist_type) 
{
	WebKitWebBackForwardList *web_back_forward_list;
	GList *webkit_items;

	web_back_forward_list = webkit_web_view_get_back_forward_list (web_view);

	if (hist_type == WEBKIT_HISTORY_FORWARD)
		webkit_items = webkit_web_back_forward_list_get_forward_list_with_limit (web_back_forward_list,
											 WEBKIT_BACK_FORWARD_LIMIT);
	else
		webkit_items = webkit_web_back_forward_list_get_back_list_with_limit (web_back_forward_list,
										      WEBKIT_BACK_FORWARD_LIMIT);

	return webkit_items;
}

static GtkWidget *
build_back_or_forward_menu (EphyNavigationAction *action)
{
	EphyWindow *window = action->priv->window;
	GtkMenuShell *menu;
	EphyEmbed *embed;
	GList *list, *l;
	WebKitWebView *web_view;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_val_if_fail (embed != NULL, NULL);

	web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
	g_return_val_if_fail (web_view != NULL, NULL);

	if (action->priv->direction == EPHY_NAVIGATION_DIRECTION_BACK)
		list = webkit_construct_history_list (web_view,
						      WEBKIT_HISTORY_BACKWARD);
	else
		list = webkit_construct_history_list (web_view,
						      WEBKIT_HISTORY_FORWARD);

	menu = GTK_MENU_SHELL (gtk_menu_new ());

	l = list;

	for (l = list; l != NULL; l = l->next)
	{
		GtkWidget *item;
		WebKitWebHistoryItem *hitem;
		const char *title, *url;

		hitem = (WebKitWebHistoryItem*)l->data;
		url = webkit_web_history_item_get_uri (hitem);
		title = webkit_web_history_item_get_title (hitem);

		item = new_history_menu_item (title ? title : url, url);

		g_object_set_data_full (G_OBJECT (item), HISTORY_ITEM_DATA_KEY, hitem,
					(GDestroyNotify) g_object_unref);

		g_signal_connect (item, "activate",
				  G_CALLBACK (activate_back_or_forward_menu_item_cb),
				  action);
		g_signal_connect (item, "select",
				  G_CALLBACK (select_menu_item_cb),
				  action);
		g_signal_connect (item, "deselect",
				  G_CALLBACK (deselect_menu_item_cb),
				  action);

		gtk_menu_shell_append (menu, item);
		gtk_widget_show_all (item);
	}
	
	g_list_free (list);

	return GTK_WIDGET (menu);
}

static GtkWidget *
build_up_menu (EphyNavigationAction *action)
{
	EphyWindow *window = action->priv->window;
	EphyEmbed *embed;
	EphyHistory *history;
	GtkMenuShell *menu;
	GtkWidget *item;
	GSList *list, *l;
	char *url;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_val_if_fail (embed != NULL, NULL);

	menu = GTK_MENU_SHELL (gtk_menu_new ());

    	history = EPHY_HISTORY
		(ephy_embed_shell_get_global_history (embed_shell));

	list = ephy_embed_get_go_up_list (embed);

	for (l = list; l != NULL; l = l->next)
	{
		EphyNode *node;
		const char *title = NULL;

		url = l->data;

		if (url == NULL) continue;

		node = ephy_history_get_page (history, url);
		if (node != NULL)
		{
			title = ephy_node_get_property_string (node, EPHY_NODE_PAGE_PROP_TITLE);
		}

		item = new_history_menu_item (title ? title : url, url);

		g_object_set_data_full (G_OBJECT (item), URL_DATA_KEY, url,
					(GDestroyNotify) g_free);
		g_signal_connect (item, "activate",
				  G_CALLBACK (activate_up_menu_item_cb), action);
		g_signal_connect (item, "select",
				  G_CALLBACK (select_menu_item_cb), action);
		g_signal_connect (item, "deselect",
				  G_CALLBACK (deselect_menu_item_cb), action);

		gtk_menu_shell_append (menu, item);
		gtk_widget_show (item);
	}

	/* the list data has been consumed */
	g_slist_free (list);

	return GTK_WIDGET (menu);
}

static void
menu_activated_cb (GtkMenuToolButton *button,
		   EphyNavigationAction *action)
{
	GtkWidget *menu = NULL;

	LOG ("menu_activated_cb dir %d", action->priv->direction);

	switch (action->priv->direction)
	{
		case EPHY_NAVIGATION_DIRECTION_UP:
			menu = build_up_menu (action);
			break;
		case EPHY_NAVIGATION_DIRECTION_FORWARD:
		case EPHY_NAVIGATION_DIRECTION_BACK:
			menu = build_back_or_forward_menu (action);
			break;
		default:
			g_assert_not_reached ();
			break;
	}

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
ephy_navigation_action_activate (GtkAction *gtk_action)
{
	EphyNavigationAction *action = EPHY_NAVIGATION_ACTION (gtk_action);
	EphyWindow *window = action->priv->window;
	EphyEmbed *embed;
	WebKitWebView *web_view;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

	if (action->priv->direction == EPHY_NAVIGATION_DIRECTION_BACK)
	{
		if (ephy_gui_is_middle_click ())
		{
			embed = ephy_link_open (EPHY_LINK (action),
						"about:blank",
						NULL,
						EPHY_LINK_NEW_TAB);
		}
		webkit_web_view_go_back (web_view);
	}
	else if (action->priv->direction == EPHY_NAVIGATION_DIRECTION_FORWARD)
	{
		if (ephy_gui_is_middle_click ())
		{
			embed = ephy_link_open (EPHY_LINK (action),
						"about:blank",
						NULL,
						EPHY_LINK_NEW_TAB);
		}
		webkit_web_view_go_forward (web_view);
	}
	else if (action->priv->direction == EPHY_NAVIGATION_DIRECTION_UP)
	{
		GSList *up_list;
		
		up_list = ephy_embed_get_go_up_list (embed);
		ephy_link_open (EPHY_LINK (action),
				up_list->data,
				NULL,
				ephy_gui_is_middle_click () ? EPHY_LINK_NEW_TAB : 0);
		g_slist_free (up_list);
	}
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
		case PROP_DIRECTION:
			nav->priv->direction = g_value_get_int (value);
			break;
		case PROP_WINDOW:
			{
				GtkWidget *statusbar;

				nav->priv->window = EPHY_WINDOW (g_value_get_object (value));

				/* statusbar context to display current selected item */
				statusbar = ephy_window_get_statusbar (nav->priv->window);

				nav->priv->statusbar_cid = gtk_statusbar_get_context_id (
								GTK_STATUSBAR (statusbar), 
								"navigation_message");
			}
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
		case PROP_DIRECTION:
			g_value_set_int (value, nav->priv->direction);
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
	action_class->activate = ephy_navigation_action_activate;

	g_object_class_install_property (object_class,
					 PROP_ARROW_TOOLTIP,
					 g_param_spec_string ("arrow-tooltip", NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_DIRECTION,
					 g_param_spec_int ("direction", NULL, NULL,
							   0,
							   G_MAXINT,
							   0,
							   G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window", NULL, NULL,
							      G_TYPE_OBJECT,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyNavigationActionPrivate));
}

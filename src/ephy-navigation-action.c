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

#include "config.h"

#include "ephy-navigation-action.h"
#include "ephy-type-builtins.h"
#include "ephy-window.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-embed-shell.h"
#include "ephy-link.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenutoolbutton.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtkstatusbar.h>

#define NTH_DATA_KEY	"GoNTh"
#define URL_DATA_KEY	"GoURL"

#define EPHY_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NAVIGATION_ACTION, EphyNavigationActionPrivate))

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

static GObjectClass *parent_class = NULL;

GType
ephy_navigation_action_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo type_info =
		{
			sizeof (EphyNavigationActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ephy_navigation_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (EphyNavigationAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_navigation_action_init,
		};

		type = g_type_register_static (EPHY_TYPE_LINK_ACTION,
					       "EphyNavigationAction",
					       &type_info, 0);
	}

	return type;
}

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
	EphyEmbed *embed;
	int go_nth;
	char *url;

	embed = ephy_window_get_active_embed (action->priv->window);
	g_return_if_fail (embed != NULL);

	go_nth = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem), NTH_DATA_KEY));

	url = g_object_get_data (G_OBJECT (menuitem), URL_DATA_KEY);
	g_return_if_fail (url != NULL);

	if (ephy_gui_is_middle_click ())
	{
		EphyEmbed *dest;
		EphyTab *newTab;

		newTab = ephy_link_open (EPHY_LINK (action), "about:blank", NULL,
					 EPHY_LINK_NEW_TAB);
		g_return_if_fail (newTab != NULL);

		dest = ephy_tab_get_embed (newTab);
		g_return_if_fail (dest != NULL);

		ephy_embed_shistory_copy (embed, dest, TRUE, TRUE, FALSE);
		embed = dest;
	}
	ephy_embed_shistory_go_nth (embed, go_nth);
}

static void
select_menu_item_cb (GtkWidget *menuitem,
	             EphyNavigationAction *action)
{
	char *url;
	GtkWidget *statusbar;

	url = g_object_get_data (G_OBJECT (menuitem), URL_DATA_KEY);
	g_return_if_fail (url != NULL);

	statusbar = ephy_window_get_statusbar (action->priv->window);

	gtk_statusbar_push (GTK_STATUSBAR (statusbar), action->priv->statusbar_cid, url);
}

static void
deselect_menu_item_cb (GtkWidget *menuitem,
	               EphyNavigationAction *action)
{
	char *url;
	GtkWidget *statusbar;

	url = g_object_get_data (G_OBJECT (menuitem), URL_DATA_KEY);
	g_return_if_fail (url != NULL);

	statusbar = ephy_window_get_statusbar (action->priv->window);

	gtk_statusbar_pop (GTK_STATUSBAR (statusbar), action->priv->statusbar_cid);
}

static void
activate_up_menu_item_cb (GtkWidget *menuitem,
			  EphyNavigationAction *action)
{
	EphyEmbed *embed;
	char *url;

	embed = ephy_window_get_active_embed (action->priv->window);
	g_return_if_fail (embed != NULL);

	url = g_object_get_data (G_OBJECT (menuitem), URL_DATA_KEY);
	g_return_if_fail (url != NULL);

	ephy_link_open (EPHY_LINK (action), url, NULL,
			ephy_gui_is_middle_click () ? EPHY_LINK_NEW_TAB : 0);
}

static GtkWidget *
build_back_or_forward_menu (EphyNavigationAction *action)
{
	EphyWindow *window = action->priv->window;
	GtkMenuShell *menu;
	EphyEmbed *embed;
	int pos, count;
	int start, end;

	embed = ephy_window_get_active_embed (window);
	g_return_val_if_fail (embed != NULL, NULL);

	pos = ephy_embed_shistory_get_pos (embed);
	count = ephy_embed_shistory_n_items (embed);

	if (count == 0) return NULL;

	if (action->priv->direction == EPHY_NAVIGATION_DIRECTION_BACK)
	{
		start = pos - 1;
		end = -1;
	}
	else
	{
		start = pos + 1;
		end = count;
	}

	menu = GTK_MENU_SHELL (gtk_menu_new ());

	while (start != end)
	{
		GtkWidget *item;
		char *title = NULL, *url = NULL;

		ephy_embed_shistory_get_nth (embed, start, FALSE, &url, &title);

		if (url == NULL) continue;

		item = new_history_menu_item (title ? title : url, url);

		g_object_set_data (G_OBJECT (item), NTH_DATA_KEY,
				   GINT_TO_POINTER (start));
		g_object_set_data_full (G_OBJECT (item), URL_DATA_KEY, url, 
					(GDestroyNotify) g_free);

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

		g_free (title);

		if (start < end)
		{
			start++;
		}
		else
		{
			start--;
		}
	}

	return GTK_WIDGET (menu);
}

static GtkWidget *
build_up_menu (EphyNavigationAction *action)
{
	EphyWindow *window = action->priv->window;
	EphyEmbed *embed;
	GtkMenuShell *menu;
	GtkWidget *item;
	GSList *list, *l;
	char *url;

	embed = ephy_window_get_active_embed (window);
	g_return_val_if_fail (embed != NULL, NULL);

	menu = GTK_MENU_SHELL (gtk_menu_new ());

	list = ephy_embed_get_go_up_list (embed);

	for (l = list; l != NULL; l = l->next)
	{
		url = l->data;

		if (url == NULL) continue;

		item = new_history_menu_item (url, url);

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

static gboolean
set_tooltip_cb (GtkMenuToolButton *proxy,
		GtkTooltips *tooltips,
		const char *tip,
		const char *tip_private,
		EphyNavigationAction *action)
{
	gtk_menu_tool_button_set_arrow_tooltip (proxy, tooltips,
						action->priv->arrow_tooltip,
						NULL);

	/* don't stop emission */
	return FALSE;
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	LOG ("Connect navigation action proxy");

	if (GTK_IS_MENU_TOOL_BUTTON (proxy))
	{
		GtkWidget *menu;

		/* set dummy menu so the arrow gets sensitive */
		menu = gtk_menu_new ();
		gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (proxy), menu);

		g_signal_connect (proxy, "show-menu",
				  G_CALLBACK (menu_activated_cb), action);

		g_signal_connect (proxy, "set-tooltip",
				  G_CALLBACK (set_tooltip_cb), action);
	}

	GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);
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

	parent_class->finalize (object);
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

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
	action_class->connect_proxy = connect_proxy;

	g_object_class_install_property (object_class,
					 PROP_ARROW_TOOLTIP,
					 g_param_spec_string ("arrow-tooltip",
							      "Arrow Tooltip",
							      "Arrow Tooltip",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_DIRECTION,
					 g_param_spec_int ("direction",
							   "Direction",
							   "Direction",
							   0,
							   G_MAXINT,
							   0,
							   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "The navigation window",
							      G_TYPE_OBJECT,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EphyNavigationActionPrivate));
}

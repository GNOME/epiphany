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

#include "ephy-navigation-action.h"
#include "ephy-arrow-toolbutton.h"
#include "ephy-window.h"
#include "ephy-string.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-embed-shell.h"
#include "ephy-debug.h"

#include <gtk/gtkimage.h>
#include <gtk/gtkimagemenuitem.h>

static void ephy_navigation_action_init       (EphyNavigationAction *action);
static void ephy_navigation_action_class_init (EphyNavigationActionClass *class);

static GObjectClass *parent_class = NULL;

#define EPHY_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NAVIGATION_ACTION, EphyNavigationActionPrivate))

struct EphyNavigationActionPrivate
{
	EphyWindow *window;
	EphyNavigationDirection direction;
};

enum
{
	PROP_0,
	PROP_DIRECTION,
	PROP_WINDOW
};

GType
ephy_navigation_action_get_type (void)
{
	static GType type = 0;

	if (type == 0)
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

		type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyNavigationAction",
					       &type_info, 0);
	}

	return type;
}

#define MAX_LENGTH 60

static GtkWidget *
new_history_menu_item (const char *origtext,
		       const char *address)
{
	GtkWidget *item, *image;
	GdkPixbuf *icon = NULL;
	char *short_text;

	if (address != NULL)
	{
		EphyFaviconCache *cache;
		EphyHistory *history;
		const char *icon_address;

		history = EPHY_HISTORY
			(ephy_embed_shell_get_global_history (embed_shell));
		icon_address = ephy_history_get_icon (history, address);

		cache = EPHY_FAVICON_CACHE
			(ephy_embed_shell_get_favicon_cache (embed_shell));
		icon = ephy_favicon_cache_get (cache, icon_address);
	}

	short_text = ephy_string_shorten (origtext, MAX_LENGTH);
	item = gtk_image_menu_item_new_with_label (short_text);
	g_free (short_text);

	image = gtk_image_new_from_pixbuf (icon);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_show (image);
	gtk_widget_show (item);

	if (icon != NULL)
	{
		g_object_unref (icon);
	}

	return item;
}

static void
activate_back_or_forward_menu_item_cb (GtkWidget *menu, EphyWindow *window)
{
	EphyEmbed *embed;
	int go_nth;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	go_nth = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(menu), "go_nth"));

	ephy_embed_shistory_go_nth (embed, go_nth);
}

static void
activate_up_menu_item_cb (GtkWidget *menu, EphyWindow *window)
{
	EphyEmbed *embed;
	int go_nth;
	GSList *l;
	gchar *url;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	go_nth = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(menu), "go_nth"));

	l = ephy_embed_get_go_up_list (embed);

	url = g_slist_nth_data (l, go_nth);
	if (url)
	{
		ephy_window_load_url (window, url);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

static void
setup_back_or_forward_menu (EphyWindow *window, GtkMenuShell *ms, EphyNavigationDirection dir)
{
	int pos, count;
	EphyEmbed *embed;
	int start, end;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	pos = ephy_embed_shistory_get_pos (embed);
	count = ephy_embed_shistory_n_items (embed);

	if (count == 0) return;

	if (dir == EPHY_NAVIGATION_DIRECTION_BACK)
	{
		start = pos - 1;
		end = -1;
	}
	else
	{
		start = pos + 1;
		end = count;
	}

	while (start != end)
	{
		char *title, *url;
		GtkWidget *item;
		ephy_embed_shistory_get_nth (embed, start, FALSE, &url, &title);
		item = new_history_menu_item (title ? title : url, url);
		gtk_menu_shell_append (ms, item);
		g_object_set_data (G_OBJECT (item), "go_nth", GINT_TO_POINTER (start));
		g_signal_connect (item, "activate",
                                  G_CALLBACK (activate_back_or_forward_menu_item_cb), window);
		gtk_widget_show_all (item);

		g_free (url);
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
}

static void
setup_up_menu (EphyWindow *window, GtkMenuShell *ms)
{
	EphyEmbed *embed;
	GSList *l;
	GSList *li;
	int count = 0;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	l = ephy_embed_get_go_up_list (embed);

	for (li = l; li; li = li->next)
	{
		char *url = li->data;
		GtkWidget *item;

		item = new_history_menu_item (url, url);
		gtk_menu_shell_append (ms, item);
		g_object_set_data (G_OBJECT(item), "go_nth", GINT_TO_POINTER (count));
		g_signal_connect (item, "activate",
                                  G_CALLBACK (activate_up_menu_item_cb), window);
		gtk_widget_show_all (item);
		count ++;
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

static void
menu_activated_cb (EphyArrowToolButton *w, EphyNavigationAction *b)
{
	EphyNavigationActionPrivate *p = b->priv;
	GtkMenuShell *ms = ephy_arrow_toolbutton_get_menu (w);
	EphyWindow *win = b->priv->window;
	GList *children;
	GList *li;

	LOG ("Show navigation menu")

	children = gtk_container_get_children (GTK_CONTAINER (ms));
	for (li = children; li; li = li->next)
	{
		gtk_container_remove (GTK_CONTAINER (ms), li->data);
	}
	g_list_free (children);

	switch (p->direction)
	{
	case EPHY_NAVIGATION_DIRECTION_UP:
		setup_up_menu (win, ms);
		break;
	case EPHY_NAVIGATION_DIRECTION_FORWARD:
	case EPHY_NAVIGATION_DIRECTION_BACK:
		setup_back_or_forward_menu (win, ms, p->direction);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	LOG ("Connect navigation action proxy")

	if (EPHY_IS_ARROW_TOOLBUTTON (proxy))
	{
		g_signal_connect (proxy, "menu-activated",
				  G_CALLBACK (menu_activated_cb), action);
	}

	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
}

static void
ephy_navigation_action_set_property (GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	EphyNavigationAction *nav;

	nav = EPHY_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_DIRECTION:
			nav->priv->direction = g_value_get_int (value);
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
	EphyNavigationAction *nav;

	nav = EPHY_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
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

	object_class->set_property = ephy_navigation_action_set_property;
	object_class->get_property = ephy_navigation_action_get_property;

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = EPHY_TYPE_ARROW_TOOLBUTTON;
	action_class->connect_proxy = connect_proxy;

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

	g_type_class_add_private (object_class, sizeof(EphyNavigationActionPrivate));
}

static void
ephy_navigation_action_init (EphyNavigationAction *action)
{
        action->priv = EPHY_NAVIGATION_ACTION_GET_PRIVATE (action);
}



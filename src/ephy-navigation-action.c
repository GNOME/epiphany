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

#include "ephy-navigation-action.h"
#include "ephy-arrow-toolbutton.h"
#include "ephy-window.h"
#include "ephy-debug.h"

static void ephy_navigation_action_init       (EphyNavigationAction *action);
static void ephy_navigation_action_class_init (EphyNavigationActionClass *class);

static GObjectClass *parent_class = NULL;

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
	static GtkType type = 0;

	if (!type)
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

		type = g_type_register_static (EGG_TYPE_ACTION,
					       "EphyNavigationAction",
					       &type_info, 0);
	}
	return type;
}

static GtkWidget *
new_history_menu_item (gchar *origtext,
                       const GdkPixbuf *ico)
{
        GtkWidget *item = gtk_image_menu_item_new ();
        GtkWidget *hb = gtk_hbox_new (FALSE, 0);
        GtkWidget *label = gtk_label_new (origtext);

        gtk_box_pack_start (GTK_BOX (hb), label, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (item), hb);

        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                       gtk_image_new_from_pixbuf ((GdkPixbuf *) ico));

        gtk_widget_show_all (item);

        return item;
}

static void
activate_back_or_forward_menu_item_cb (GtkWidget *menu, EphyWindow *window)
{
	EphyEmbed *embed;
	int go_nth;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	go_nth = (int)g_object_get_data (G_OBJECT(menu), "go_nth");

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

	go_nth = (int)g_object_get_data (G_OBJECT(menu), "go_nth");

	ephy_embed_get_go_up_list (embed, &l);

	url = g_slist_nth_data (l, go_nth);
	if (url)
	{
		ephy_embed_load_url (embed, url);
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

	ephy_embed_shistory_get_pos (embed, &pos);
	ephy_embed_shistory_count (embed, &count);

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
		item = new_history_menu_item (title ? title : url, NULL);
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

	ephy_embed_get_go_up_list (embed, &l);

	for (li = l; li; li = li->next)
	{
		char *url = li->data;
		GtkWidget *item;

		item = new_history_menu_item (url, NULL);
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
connect_proxy (EggAction *action, GtkWidget *proxy)
{
	LOG ("Connect navigation action proxy")

	g_signal_connect (proxy, "menu-activated",
			  G_CALLBACK (menu_activated_cb), action);

	(* EGG_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
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
	EggActionClass *action_class;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->set_property = ephy_navigation_action_set_property;
	object_class->get_property = ephy_navigation_action_get_property;

	parent_class = g_type_class_peek_parent (class);
	action_class = EGG_ACTION_CLASS (class);

	action_class->toolbar_item_type = EPHY_ARROW_TOOLBUTTON_TYPE;
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

}

static void
ephy_navigation_action_init (EphyNavigationAction *action)
{
        action->priv = g_new0 (EphyNavigationActionPrivate, 1);
}



/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#include "ephy-zoom-action.h"
#include "ephy-zoom-control.h"
#include "ephy-zoom.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define EPHY_ZOOM_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ZOOM_ACTION, EphyZoomActionPrivate))

struct _EphyZoomActionPrivate
{
	float zoom;
};

enum
{
	PROP_0,
	PROP_ZOOM
};


static void ephy_zoom_action_init       (EphyZoomAction *action);
static void ephy_zoom_action_class_init (EphyZoomActionClass *class);

enum
{
	ZOOM_TO_LEVEL_SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
ephy_zoom_action_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0))
        {
                const GTypeInfo our_info =
			{
				sizeof (EphyZoomActionClass),
				NULL, /* base_init */
				NULL, /* base_finalize */
				(GClassInitFunc) ephy_zoom_action_class_init,
				NULL,
				NULL, /* class_data */
				sizeof (EphyZoomAction),
				0, /* n_preallocs */
				(GInstanceInitFunc) ephy_zoom_action_init,
			};

                type = g_type_register_static (GTK_TYPE_ACTION,
					       "EphyZoomAction",
					       &our_info, 0);
        }

        return type;
}

static void
zoom_to_level_cb (EphyZoomControl *control,
		  float zoom,
		  EphyZoomAction *action)
{
	g_signal_emit (action, signals[ZOOM_TO_LEVEL_SIGNAL], 0, zoom);
}

static void
sync_zoom_cb (GtkAction *action, GParamSpec *pspec, GtkWidget *proxy)
{
	EphyZoomAction *zoom_action = EPHY_ZOOM_ACTION (action);

	g_object_set (G_OBJECT (proxy), "zoom", zoom_action->priv->zoom, NULL);
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	if (EPHY_IS_ZOOM_CONTROL (proxy))
	{
		/* Ensure the sync is done when the item is added to
		   the toolbar */
		sync_zoom_cb (action, NULL, proxy);

		g_signal_connect_object (action, "notify::zoom",
					 G_CALLBACK (sync_zoom_cb), proxy, 0);
	
		g_signal_connect (proxy, "zoom_to_level",
				  G_CALLBACK (zoom_to_level_cb), action);
	}

	GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);
}

static void
proxy_menu_activate_cb (GtkMenuItem *menu_item, EphyZoomAction *action)
{
	gint index;
	float zoom;

	/* menu item was toggled OFF */
	if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item))) return;

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "zoom-level"));
	zoom = zoom_levels[index].level;

	if (zoom != action->priv->zoom)
	{
		g_signal_emit (action, signals[ZOOM_TO_LEVEL_SIGNAL], 0, zoom);
	}
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
	EphyZoomActionPrivate *p = EPHY_ZOOM_ACTION (action)->priv;
	GtkWidget *menu, *menu_item;
	GSList *group = NULL;
	int i;

	menu = gtk_menu_new ();

	for (i = 0; i < n_zoom_levels; i++)
	{
		menu_item = gtk_radio_menu_item_new_with_label (group, _(zoom_levels[i].name));
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));

		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
						p->zoom == zoom_levels[i].level);

		gtk_widget_show (menu_item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

		g_object_set_data (G_OBJECT (menu_item), "zoom-level", GINT_TO_POINTER (i));
		g_signal_connect_object (G_OBJECT (menu_item), "activate",
					 G_CALLBACK (proxy_menu_activate_cb), action, 0);
	}

	gtk_widget_show (menu);

        menu_item = GTK_ACTION_CLASS (parent_class)->create_menu_item (action);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);

	gtk_widget_show (menu_item);

	return menu_item;
}

static void
ephy_zoom_action_set_property (GObject *object,
			       guint prop_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	EphyZoomAction *action;

	action = EPHY_ZOOM_ACTION (object);

	switch (prop_id)
	{
		case PROP_ZOOM:
			action->priv->zoom = g_value_get_float (value);
			break;
	}
}

static void
ephy_zoom_action_get_property (GObject *object,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	EphyZoomAction *action;

	action = EPHY_ZOOM_ACTION (object);

	switch (prop_id)
	{
		case PROP_ZOOM:
			g_value_set_float (value, action->priv->zoom);
			break;
	}
}

static void
ephy_zoom_action_class_init (EphyZoomActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->set_property = ephy_zoom_action_set_property;
	object_class->get_property = ephy_zoom_action_get_property;

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = EPHY_TYPE_ZOOM_CONTROL;
	action_class->connect_proxy = connect_proxy;
	action_class->create_menu_item = create_menu_item;

	g_object_class_install_property (object_class,
					 PROP_ZOOM,
					 g_param_spec_float ("zoom", NULL, NULL,
							     ZOOM_MINIMAL,
							     ZOOM_MAXIMAL,
							     1.0,
							     G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	signals[ZOOM_TO_LEVEL_SIGNAL] =
		g_signal_new ("zoom-to-level",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyZoomActionClass, zoom_to_level),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__FLOAT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_FLOAT);

	g_type_class_add_private (object_class, sizeof (EphyZoomActionPrivate));
}

static void
ephy_zoom_action_init (EphyZoomAction *action)
{
	action->priv = EPHY_ZOOM_ACTION_GET_PRIVATE (action);

	action->priv->zoom = 1.0;
}

void
ephy_zoom_action_set_zoom_level (EphyZoomAction *action, float zoom)
{
	g_return_if_fail (EPHY_IS_ZOOM_ACTION (action));
	
	if (zoom < ZOOM_MINIMAL || zoom > ZOOM_MAXIMAL) return;

	action->priv->zoom = zoom;
	g_object_notify (G_OBJECT (action), "zoom");
}

float
ephy_zoom_action_get_zoom_level (EphyZoomAction *action)
{
	g_return_val_if_fail (EPHY_IS_ZOOM_ACTION (action), 1.0);
	
	return action->priv->zoom;
}

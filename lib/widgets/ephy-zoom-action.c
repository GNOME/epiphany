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

#include "ephy-zoom.h"

#include <glib/gi18n.h>

/**
 * SECTION:ephy-zoom-action
 * @short_description: A #GtkAction implementing a zoom control
 *
 * #EphyZoomAction implements a #GtkAction able to control the zoom level.
 */

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

G_DEFINE_TYPE (EphyZoomAction, ephy_zoom_action, GTK_TYPE_ACTION)

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

        menu_item = GTK_ACTION_CLASS (ephy_zoom_action_parent_class)->create_menu_item (action);

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

	action_class->create_menu_item = create_menu_item;

	/**
	* EphyZoomAction:zoom:
	*
	* The current value of #EphyZoomAction, as a float.
	*/
	g_object_class_install_property (object_class,
					 PROP_ZOOM,
					 g_param_spec_float ("zoom", NULL, NULL,
							     ZOOM_MINIMAL,
							     ZOOM_MAXIMAL,
							     1.0,
							     G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyZoomAction::zoom-to-level:
	* @action: the object on which the signal is emitted
	* @level: new zoom level
	*
	* Emitted when the user changes the value of the #EphyZoomAction.
	*/
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

/**
 * ephy_zoom_action_set_zoom_level:
 * @action: an #EphyZoomAction
 * @zoom: the new value for the zoom level
 *
 * Sets the zoom level of @action.
 **/
void
ephy_zoom_action_set_zoom_level (EphyZoomAction *action, float zoom)
{
	g_return_if_fail (EPHY_IS_ZOOM_ACTION (action));
	
	if (zoom < ZOOM_MINIMAL || zoom > ZOOM_MAXIMAL) return;

	action->priv->zoom = zoom;
	g_object_notify (G_OBJECT (action), "zoom");
}

/**
 * ephy_zoom_action_get_zoom_level:
 * @action: an #EphyZoomControl
 *
 * Get the current zoom level of @action.
 *
 * Returns: the zoom level as a float
 **/
float
ephy_zoom_action_get_zoom_level (EphyZoomAction *action)
{
	g_return_val_if_fail (EPHY_IS_ZOOM_ACTION (action), 1.0);
	
	return action->priv->zoom;
}

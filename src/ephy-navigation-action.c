/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2008 Jan Alonzo
 *  Copyright © 2009, 2011 Igalia S.L.
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

#include "ephy-middle-clickable-tool-button.h"
#include "ephy-window.h"

#include <gtk/gtk.h>

#define EPHY_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NAVIGATION_ACTION, EphyNavigationActionPrivate))

struct _EphyNavigationActionPrivate
{
	EphyWindow *window;
};

enum
{
	PROP_0,
	PROP_WINDOW
};

static void ephy_navigation_action_init       (EphyNavigationAction *action);
static void ephy_navigation_action_class_init (EphyNavigationActionClass *class);

G_DEFINE_TYPE (EphyNavigationAction, ephy_navigation_action, EPHY_TYPE_LINK_ACTION)

static void
ephy_navigation_action_init (EphyNavigationAction *action)
{
	action->priv = EPHY_NAVIGATION_ACTION_GET_PRIVATE (action);
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

	action_class->toolbar_item_type = EPHY_TYPE_MIDDLE_CLICKABLE_TOOL_BUTTON;

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

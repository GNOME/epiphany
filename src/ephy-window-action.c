/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-window-action.h"

#include <gtk/gtk.h>

#define EPHY_WINDOW_ACTION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_WINDOW_ACTION, EphyWindowActionPrivate))

struct _EphyWindowActionPrivate {
    EphyWindow *window;
};

enum {
    PROP_0,
    PROP_WINDOW
};

G_DEFINE_TYPE (EphyWindowAction, ephy_window_action, GTK_TYPE_ACTION)

static void
ephy_window_action_init (EphyWindowAction *action)
{
    action->priv = EPHY_WINDOW_ACTION_GET_PRIVATE (action);
}

static void
ephy_window_action_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    EphyWindowAction *action = EPHY_WINDOW_ACTION (object);
    
    switch (property_id) {
    case PROP_WINDOW:
        action->priv->window = EPHY_WINDOW (g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ephy_window_action_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    EphyWindowAction *action = EPHY_WINDOW_ACTION (object);
    
    switch (property_id) {
    case PROP_WINDOW:
        g_value_set_object (value, action->priv->window);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ephy_window_action_class_init (EphyWindowActionClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);
    
    object_class->set_property = ephy_window_action_set_property;
    object_class->get_property = ephy_window_action_get_property;
    
    g_object_class_install_property (object_class,
                                     PROP_WINDOW,
                                     g_param_spec_object ("window", NULL, NULL,
                                                          G_TYPE_OBJECT,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT));
    
    g_type_class_add_private (object_class, sizeof (EphyWindowActionPrivate));
}

EphyWindow *
ephy_window_action_get_window (EphyWindowAction *action)
{
    g_return_val_if_fail (EPHY_IS_WINDOW_ACTION (action), NULL);

    return action->priv->window;
}

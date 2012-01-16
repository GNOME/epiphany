/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2011 Igalia S.L.
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
#include "ephy-combined-stop-reload-action.h"

#include "window-commands.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (EphyCombinedStopReloadAction, ephy_combined_stop_reload_action, EPHY_TYPE_WINDOW_ACTION)

#define COMBINED_STOP_RELOAD_ACTION_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION, EphyCombinedStopReloadActionPrivate))

struct _EphyCombinedStopReloadActionPrivate
{
  gboolean loading;
  gulong action_handler_id;
};

GtkActionEntry combined_stop_reload_action_entries [] = {
  { NULL, "process-stop-symbolic", N_("Stop"), NULL,
    N_("Stop current data transfer"),
    G_CALLBACK (window_cmd_view_stop) },
  { NULL, "view-refresh-symbolic", N_("_Reload"), NULL,
    N_("Display the latest content of the current page"),
    G_CALLBACK (window_cmd_view_reload) }
};

typedef enum {
  EPHY_COMBINED_STOP_RELOAD_ACTION_STOP,
  EPHY_COMBINED_STOP_RELOAD_ACTION_REFRESH
} EphyCombinedStopReloadActionEnum;

enum {
  PROP_0,
  PROP_LOADING
};

void
ephy_combined_stop_reload_action_set_loading (EphyCombinedStopReloadAction *action,
                                              gboolean loading)
{
  EphyCombinedStopReloadActionEnum action_enum;
  EphyCombinedStopReloadActionPrivate *priv;

  g_return_if_fail (EPHY_IS_COMBINED_STOP_RELOAD_ACTION (action));

  priv = action->priv;

  if (priv->loading == loading && priv->action_handler_id)
    return;

  action_enum = loading ?
    EPHY_COMBINED_STOP_RELOAD_ACTION_STOP : EPHY_COMBINED_STOP_RELOAD_ACTION_REFRESH;

  g_object_set (action,
                "icon-name", combined_stop_reload_action_entries[action_enum].stock_id,
                "tooltip", combined_stop_reload_action_entries[action_enum].tooltip,
                NULL);

  if (priv->action_handler_id)
    g_signal_handler_disconnect (action, priv->action_handler_id);

  priv->action_handler_id = g_signal_connect (action, "activate",
                                              combined_stop_reload_action_entries[action_enum].callback,
                                              ephy_window_action_get_window (EPHY_WINDOW_ACTION (action)));

  priv->loading = loading;
}

static void
ephy_combined_stop_reload_action_get_property (GObject    *object,
                                               guint       property_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  EphyCombinedStopReloadAction *action = EPHY_COMBINED_STOP_RELOAD_ACTION (object);

  switch (property_id)
    {
    case PROP_LOADING:
      g_value_set_boolean (value, action->priv->loading);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ephy_combined_stop_reload_action_set_property (GObject      *object,
                                               guint         property_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  EphyCombinedStopReloadAction *action = EPHY_COMBINED_STOP_RELOAD_ACTION (object);

  switch (property_id)
  {
  case PROP_LOADING:
    ephy_combined_stop_reload_action_set_loading (action,
                                      g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_combined_stop_reload_action_class_init (EphyCombinedStopReloadActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EphyCombinedStopReloadActionPrivate));

  object_class->get_property = ephy_combined_stop_reload_action_get_property;
  object_class->set_property = ephy_combined_stop_reload_action_set_property;

  g_object_class_install_property (object_class,
                                   PROP_LOADING,
                                   g_param_spec_boolean ("loading", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
ephy_combined_stop_reload_action_init (EphyCombinedStopReloadAction *self)
{
  self->priv = COMBINED_STOP_RELOAD_ACTION_PRIVATE (self);
}

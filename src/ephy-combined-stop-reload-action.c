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

#include "ephy-window.h"
#include "window-commands.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (EphyCombinedStopReloadAction, ephy_combined_stop_reload_action, GTK_TYPE_ACTION)

#define COMBINED_STOP_RELOAD_ACTION_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_COMBINED_STOP_RELOAD_ACTION, EphyCombinedStopReloadActionPrivate))

struct _EphyCombinedStopReloadActionPrivate
{
  gboolean loading;
  EphyWindow *window;
  gulong action_handler_id;
};

GtkActionEntry combined_stop_reload_action_entries [] = {
  { NULL, GTK_STOCK_STOP, N_("Stop"), NULL,
    N_("Stop current data transfer"),
    G_CALLBACK (window_cmd_view_stop) },
  { NULL, GTK_STOCK_REFRESH, N_("_Reload"), NULL,
    N_("Display the latest content of the current page"),
    G_CALLBACK (window_cmd_view_reload) }
};

typedef enum {
  EPHY_COMBINED_STOP_RELOAD_ACTION_STOP,
  EPHY_COMBINED_STOP_RELOAD_ACTION_REFRESH
} EphyCombinedStopReloadActionEnum;

enum {
  PROP_0,
  PROP_LOADING,
  PROP_WINDOW
};

void
ephy_combined_stop_reload_action_set_loading (EphyCombinedStopReloadAction *action,
                                              gboolean loading)
{
  EphyCombinedStopReloadActionEnum action_enum;

  if (action->priv->loading == loading)
    return;

  action_enum = loading ?
    EPHY_COMBINED_STOP_RELOAD_ACTION_STOP : EPHY_COMBINED_STOP_RELOAD_ACTION_REFRESH;

  g_object_set (action,
                "label", combined_stop_reload_action_entries[action_enum].label,
                "stock-id", combined_stop_reload_action_entries[action_enum].stock_id,
                "tooltip", combined_stop_reload_action_entries[action_enum].tooltip,
                NULL);

  if (action->priv->action_handler_id)
    g_signal_handler_disconnect (action, action->priv->action_handler_id);

  action->priv->action_handler_id = g_signal_connect (action, "activate",
                                                      combined_stop_reload_action_entries[action_enum].callback,
                                                      action->priv->window);

  action->priv->loading = loading;
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
    case PROP_WINDOW:
      g_value_set_object (value, action->priv->window);
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
  case PROP_WINDOW:
    action->priv->window = EPHY_WINDOW (g_value_get_object (value));
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

  g_object_class_install_property (object_class,
                                   PROP_WINDOW,
                                   g_param_spec_object ("window", NULL, NULL,
                                                        EPHY_TYPE_WINDOW,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

}

static void
ephy_combined_stop_reload_action_init (EphyCombinedStopReloadAction *self)
{
  self->priv = COMBINED_STOP_RELOAD_ACTION_PRIVATE (self);
}

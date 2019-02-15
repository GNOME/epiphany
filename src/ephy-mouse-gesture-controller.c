/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Jan-Michael Brummer
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-embed.h"
#include "ephy-mouse-gesture-controller.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-window.h"

#include <math.h>
#include <gtk/gtk.h>

#define NUM_SEQUENCES 2

typedef enum {
  MOUSE_DIRECTION_UNKNOWN = 0,
  MOUSE_DIRECTION_RIGHT,
  MOUSE_DIRECTION_LEFT,
  MOUSE_DIRECTION_DOWN,
  MOUSE_DIRECTION_UP,
} MouseDirection;

struct _EphyMouseGestureController {
  GObject parent_instance;

  GtkEventController *controller;
  EphyWindow *window;
  WebKitWebView *web_view;

  MouseDirection sequence[NUM_SEQUENCES];
  MouseDirection direction;
  gint sequence_pos;
  gdouble last_x;
  gdouble last_y;
  gboolean gesture_active;
};

enum {
  PROP_0,
  PROP_WINDOW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE (EphyMouseGestureController, ephy_mouse_gesture_controller, G_TYPE_OBJECT)

static void
ephy_mouse_gesture_controller_motion_cb (GtkEventControllerMotion *controller,
                                         gdouble                   x,
                                         gdouble                   y,
                                         gpointer                  user_data)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (user_data);
  MouseDirection direction;
  gdouble offset_x, offset_y;

  if (!self->gesture_active || self->sequence_pos == NUM_SEQUENCES)
    return;

  if (isnan (self->last_x) || isnan (self->last_y)) {
    self->last_x = x;
    self->last_y = y;
    return;
  }

  offset_x = x - self->last_x;
  offset_y = y - self->last_y;

  /* Try to guess direction */
  if (fabs (offset_x) > fabs (offset_y) * 2) {
    if (offset_x > 0)
      direction = MOUSE_DIRECTION_RIGHT;
    else
      direction = MOUSE_DIRECTION_LEFT;
  } else if (fabs (offset_y) > fabs (offset_x) * 2) {
    if (offset_y > 0)
      direction = MOUSE_DIRECTION_DOWN;
    else
      direction = MOUSE_DIRECTION_UP;
  } else {
    return;
  }

  self->last_x = x;
  self->last_y = y;

  if (self->direction == direction)
    return;

  self->sequence[self->sequence_pos++] = direction;

  self->direction = direction;
}

static void
ephy_mouse_gesture_controller_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (object);

  switch (prop_id) {
    case PROP_WINDOW:
      self->window = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_mouse_gesture_controller_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (object);

  switch (prop_id) {
    case PROP_WINDOW:
      g_value_set_object (value, self->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_mouse_gesture_controller_reset (EphyMouseGestureController *self)
{
  self->direction = MOUSE_DIRECTION_UNKNOWN;
  self->sequence_pos = 0;
  self->last_x = NAN;
  self->last_y = NAN;
  self->gesture_active = FALSE;
}

static gboolean
ephy_mouse_gesture_controller_button_press_cb (GtkWidget *widget,
                                               GdkEvent  *event,
                                               gpointer   user_data)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (user_data);
  GdkEventButton *button_event = (GdkEventButton *)event;

  if (button_event->button == GDK_BUTTON_MIDDLE)
    self->gesture_active = TRUE;
  else
    self->gesture_active = FALSE;

  return FALSE;
}

static void
handle_gesture (gpointer user_data)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (user_data);
  GActionGroup *action_group_toolbar = gtk_widget_get_action_group (GTK_WIDGET (self->window), "toolbar");
  GActionGroup *action_group_win = gtk_widget_get_action_group (GTK_WIDGET (self->window), "win");
  GActionGroup *action_group_tab = gtk_widget_get_action_group (GTK_WIDGET (self->window), "tab");
  GAction *action;

  switch (self->sequence_pos) {
    case 1:
      if (self->sequence[0] == MOUSE_DIRECTION_LEFT) {
        /* Nav back */
        action = g_action_map_lookup_action (G_ACTION_MAP (action_group_toolbar), "navigation-back");
        g_action_activate (action, NULL);
      } else if (self->sequence[0] == MOUSE_DIRECTION_RIGHT) {
        /* Nav forward */
        action = g_action_map_lookup_action (G_ACTION_MAP (action_group_toolbar), "navigation-forward");
        g_action_activate (action, NULL);
      } else if (self->sequence[0] == MOUSE_DIRECTION_DOWN) {
        /* New tab */
        action = g_action_map_lookup_action (G_ACTION_MAP (action_group_win), "new-tab");
        g_action_activate (action, NULL);
      }
      break;
    case 2:
      if (self->sequence[0] == MOUSE_DIRECTION_DOWN && self->sequence[1] == MOUSE_DIRECTION_RIGHT) {
        /* Close tab */
        action = g_action_map_lookup_action (G_ACTION_MAP (action_group_tab), "close");
        g_action_activate (action, NULL);
      } else if (self->sequence[0] == MOUSE_DIRECTION_UP && self->sequence[1] == MOUSE_DIRECTION_DOWN) {
        /* Reload tab */
        action = g_action_map_lookup_action (G_ACTION_MAP (action_group_toolbar), "reload");
        g_action_activate (action, NULL);
      }
      break;
    default:
      break;
  }

  ephy_mouse_gesture_controller_reset (self);
}

static gboolean
ephy_mouse_gesture_controller_button_release_cb (GtkWidget *widget,
                                                 GdkEvent  *event,
                                                 gpointer   user_data)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (user_data);
  GdkEventButton *button_event = (GdkEventButton *)event;

  if (button_event->button == GDK_BUTTON_MIDDLE) {
    if (self->gesture_active && g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_MOUSE_GESTURES))
      handle_gesture (user_data);

    self->gesture_active = FALSE;
  }

  return FALSE;
}

static void
ephy_mouse_gesture_controller_init (EphyMouseGestureController *self)
{
}

void
ephy_mouse_gesture_controller_unset_web_view (EphyMouseGestureController *self)
{
  if (self->web_view) {
    g_signal_handlers_disconnect_by_func (self->web_view,
                                          G_CALLBACK (ephy_mouse_gesture_controller_button_press_cb),
                                          self);
    g_signal_handlers_disconnect_by_func (self->web_view,
                                          G_CALLBACK (ephy_mouse_gesture_controller_button_release_cb),
                                          self);
    g_clear_object (&self->web_view);
  }
}

static void
ephy_mouse_gesture_controller_dispose (GObject *object)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (object);

  g_clear_object (&self->controller);
  ephy_mouse_gesture_controller_unset_web_view (self);

  G_OBJECT_CLASS (ephy_mouse_gesture_controller_parent_class)->dispose (object);
}

static void
ephy_mouse_gesture_controller_constructed (GObject *object)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (object);

  ephy_mouse_gesture_controller_reset (self);

  self->controller = gtk_event_controller_motion_new (GTK_WIDGET (self->window));
  g_signal_connect (self->controller, "motion", G_CALLBACK (ephy_mouse_gesture_controller_motion_cb), self);
}

static void
ephy_mouse_gesture_controller_class_init (EphyMouseGestureControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_mouse_gesture_controller_dispose;
  object_class->constructed = ephy_mouse_gesture_controller_constructed;

  /* class creation */
  object_class->set_property = ephy_mouse_gesture_controller_set_property;
  object_class->get_property = ephy_mouse_gesture_controller_get_property;

  obj_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "window",
                         "window",
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyMouseGestureController *
ephy_mouse_gesture_controller_new (EphyWindow *window)
{
  return g_object_new (EPHY_TYPE_MOUSE_GESTURE_CONTROLLER,
                       "window", window,
                       NULL);
}

void
ephy_mouse_gesture_controller_set_web_view (EphyMouseGestureController *self,
                                            WebKitWebView              *web_view)
{
  ephy_mouse_gesture_controller_unset_web_view (self);

  g_signal_connect_object (web_view, "button-press-event", G_CALLBACK (ephy_mouse_gesture_controller_button_press_cb), self, 0);
  g_signal_connect_object (web_view, "button-release-event", G_CALLBACK (ephy_mouse_gesture_controller_button_release_cb), self, 0);

  self->web_view = g_object_ref (web_view);
}

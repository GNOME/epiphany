/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Jan-Michael Brummer
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
#include "ephy-touchpad-gesture-controller.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-window.h"

#include <math.h>
#include <gtk/gtk.h>

#define NUM_SEQUENCES 2

typedef enum {
  TOUCHPAD_DIRECTION_UNKNOWN = 0,
  TOUCHPAD_DIRECTION_RIGHT,
  TOUCHPAD_DIRECTION_LEFT,
  TOUCHPAD_DIRECTION_DOWN,
  TOUCHPAD_DIRECTION_UP,
} TouchpadDirection;

struct _EphyTouchpadGestureController {
  GObject parent_instance;

  GtkGesture *gesture;
  EphyWindow *window;

  TouchpadDirection direction;
};

enum {
  PROP_0,
  PROP_WINDOW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE (EphyTouchpadGestureController, ephy_touchpad_gesture_controller, G_TYPE_OBJECT)

static void
ephy_touchpad_gesture_controller_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  EphyTouchpadGestureController *self = EPHY_TOUCHPAD_GESTURE_CONTROLLER (object);

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
ephy_touchpad_gesture_controller_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  EphyTouchpadGestureController *self = EPHY_TOUCHPAD_GESTURE_CONTROLLER (object);

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
ephy_touchpad_gesture_controller_pan (GtkGesturePan  *gesture,
                                      GtkPanDirection direction,
                                      gdouble         offset,
                                      gpointer        user_data)
{
  EphyTouchpadGestureController *self = EPHY_TOUCHPAD_GESTURE_CONTROLLER (user_data);

  if (direction == GTK_PAN_DIRECTION_LEFT) {
    self->direction = TOUCHPAD_DIRECTION_LEFT;
  } else if (direction == GTK_PAN_DIRECTION_RIGHT) {
    self->direction = TOUCHPAD_DIRECTION_RIGHT;
  }
}

static void
ephy_touchpad_gesture_controller_drag_end (GtkGestureDrag *drag,
                                           gdouble         x,
                                           gdouble         y,
                                           gpointer        user_data)
{
  EphyTouchpadGestureController *self = EPHY_TOUCHPAD_GESTURE_CONTROLLER (user_data);
  GActionGroup *action_group_toolbar = gtk_widget_get_action_group (GTK_WIDGET (self->window), "toolbar");
  GAction *action;

  if (self->direction == TOUCHPAD_DIRECTION_RIGHT) {
    /* Nav forward */
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group_toolbar), "navigation-forward");
    g_action_activate (action, NULL);
  } else if (self->direction == TOUCHPAD_DIRECTION_LEFT) {
    /* Nav back */
    action = g_action_map_lookup_action (G_ACTION_MAP (action_group_toolbar), "navigation-back");
    g_action_activate (action, NULL);
  }

  self->direction = TOUCHPAD_DIRECTION_UNKNOWN;
}


static void
ephy_touchpad_gesture_controller_init (EphyTouchpadGestureController *self)
{
}

static void
ephy_touchpad_gesture_controller_dispose (GObject *object)
{
  EphyTouchpadGestureController *self = EPHY_TOUCHPAD_GESTURE_CONTROLLER (object);

  if (self->gesture != NULL && self->window != NULL) {
    g_signal_handlers_disconnect_by_func (self->gesture,
                                          G_CALLBACK (ephy_touchpad_gesture_controller_pan),
                                          self);
    g_signal_handlers_disconnect_by_func (self->gesture,
                                          G_CALLBACK (ephy_touchpad_gesture_controller_drag_end),
                                          self);
  }

  g_clear_object (&self->gesture);

  G_OBJECT_CLASS (ephy_touchpad_gesture_controller_parent_class)->dispose (object);
}

static void
ephy_touchpad_gesture_controller_constructed (GObject *object)
{
  EphyTouchpadGestureController *self = EPHY_TOUCHPAD_GESTURE_CONTROLLER (object);

  gtk_widget_add_events (GTK_WIDGET (self->window), GDK_TOUCHPAD_GESTURE_MASK);
  self->direction = TOUCHPAD_DIRECTION_UNKNOWN;
  self->gesture = g_object_new (GTK_TYPE_GESTURE_PAN,
                                "widget", GTK_WIDGET (self->window),
                                "orientation", GTK_ORIENTATION_HORIZONTAL,
                                "n-points", 3,
                                NULL);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->gesture),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect (self->gesture, "pan", G_CALLBACK (ephy_touchpad_gesture_controller_pan), self);
  g_signal_connect (self->gesture, "drag-end", G_CALLBACK (ephy_touchpad_gesture_controller_drag_end), self);
}

static void
ephy_touchpad_gesture_controller_class_init (EphyTouchpadGestureControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_touchpad_gesture_controller_dispose;
  object_class->constructed = ephy_touchpad_gesture_controller_constructed;

  /* class creation */
  object_class->set_property = ephy_touchpad_gesture_controller_set_property;
  object_class->get_property = ephy_touchpad_gesture_controller_get_property;

  obj_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "window",
                         "window",
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyTouchpadGestureController *
ephy_touchpad_gesture_controller_new (EphyWindow *window)
{
  return g_object_new (EPHY_TYPE_TOUCHPAD_GESTURE_CONTROLLER,
                       "window", window,
                       NULL);
}

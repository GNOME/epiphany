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

  GtkGesture *gesture;
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

G_DEFINE_FINAL_TYPE (EphyMouseGestureController, ephy_mouse_gesture_controller, G_TYPE_OBJECT)

static void
ephy_mouse_gesture_controller_reset (EphyMouseGestureController *self)
{
  self->direction = MOUSE_DIRECTION_UNKNOWN;
  self->sequence_pos = 0;
  self->last_x = 0;
  self->last_y = 0;
  self->gesture_active = FALSE;
}

static void
drag_begin_cb (GtkGestureDrag             *gesture,
               double                      start_x,
               double                      start_y,
               EphyMouseGestureController *self)
{
  GtkWidget *picked_widget;

  if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_MOUSE_GESTURES)) {
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  picked_widget = gtk_widget_pick (GTK_WIDGET (self->window),
                                   start_x,
                                   start_y,
                                   GTK_PICK_DEFAULT);

  if (picked_widget != GTK_WIDGET (self->web_view)) {
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    return;
  }
}

static void
drag_update_cb (GtkGestureDrag             *gesture,
                double                      offset_x,
                double                      offset_y,
                EphyMouseGestureController *self)
{
  MouseDirection direction;
  double delta_x = offset_x - self->last_x;
  double delta_y = offset_y - self->last_y;

  self->last_x = offset_x;
  self->last_y = offset_y;

  if (!self->gesture_active &&
      gtk_drag_check_threshold (GTK_WIDGET (self->window),
                                0, 0, offset_x, offset_y)) {
    self->gesture_active = TRUE;
  }

  if (!self->gesture_active || self->sequence_pos == NUM_SEQUENCES)
    return;

  /* Try to guess direction */
  if (fabs (delta_x) > fabs (delta_y) * 2) {
    if (delta_x > 0)
      direction = MOUSE_DIRECTION_RIGHT;
    else
      direction = MOUSE_DIRECTION_LEFT;
  } else if (fabs (delta_y) > fabs (delta_x) * 2) {
    if (delta_y > 0)
      direction = MOUSE_DIRECTION_DOWN;
    else
      direction = MOUSE_DIRECTION_UP;
  } else {
    return;
  }

  if (self->direction == direction)
    return;

  self->sequence[self->sequence_pos++] = direction;

  self->direction = direction;
}

static void
drag_end_cb (GtkGestureDrag             *gesture,
             double                      offset_x,
             double                      offset_y,
             EphyMouseGestureController *self)
{
  GActionGroup *action_group_toolbar = ephy_window_get_action_group (self->window, "toolbar");
  GActionGroup *action_group_win = ephy_window_get_action_group (self->window, "win");
  GActionGroup *action_group_tab = ephy_window_get_action_group (self->window, "tab");
  GAction *action;

  if (!self->gesture_active)
    return;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

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

static void
cancel_cb (GtkGesture                 *gesture,
           GdkEventSequence           *sequence,
           EphyMouseGestureController *self)
{
  ephy_mouse_gesture_controller_reset (self);
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
ephy_mouse_gesture_controller_init (EphyMouseGestureController *self)
{
}

static void
ephy_mouse_gesture_controller_dispose (GObject *object)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (object);

  if (self->gesture) {
    gtk_widget_remove_controller (GTK_WIDGET (self->window),
                                  GTK_EVENT_CONTROLLER (self->gesture));
    self->gesture = NULL;
  }

  ephy_mouse_gesture_controller_unset_web_view (self);

  G_OBJECT_CLASS (ephy_mouse_gesture_controller_parent_class)->dispose (object);
}

static void
ephy_mouse_gesture_controller_constructed (GObject *object)
{
  EphyMouseGestureController *self = EPHY_MOUSE_GESTURE_CONTROLLER (object);

  ephy_mouse_gesture_controller_reset (self);

  self->gesture = gtk_gesture_drag_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->gesture),
                                              GTK_PHASE_CAPTURE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->gesture),
                                 GDK_BUTTON_MIDDLE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (self->gesture), TRUE);

  g_signal_connect (self->gesture, "drag-begin",
                    G_CALLBACK (drag_begin_cb), self);
  g_signal_connect (self->gesture, "drag-update",
                    G_CALLBACK (drag_update_cb), self);
  g_signal_connect (self->gesture, "drag-end",
                    G_CALLBACK (drag_end_cb), self);
  g_signal_connect (self->gesture, "cancel",
                    G_CALLBACK (cancel_cb), self);

  gtk_widget_add_controller (GTK_WIDGET (self->window),
                             GTK_EVENT_CONTROLLER (self->gesture));
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
                         NULL, NULL,
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

  self->web_view = g_object_ref (web_view);
}

void
ephy_mouse_gesture_controller_unset_web_view (EphyMouseGestureController *self)
{
  g_clear_object (&self->web_view);
}

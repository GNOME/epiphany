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
#include "ephy-mouse-gesture.h"
#include "ephy-window.h"

#include <math.h>
#include <gtk/gtk.h>

struct _EphyMouseGesture {
  GObject parent_instance;

  GtkEventController *controller;
  EphyWindow *window;
};

enum {
  PROP_0,
  PROP_WINDOW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE (EphyMouseGesture, ephy_mouse_gesture, G_TYPE_OBJECT)

#define NUM_SEQUENCES 2

typedef enum {
  MOUSE_DIRECTION_UNKNOWN = 0,
  MOUSE_DIRECTION_RIGHT,
  MOUSE_DIRECTION_LEFT,
  MOUSE_DIRECTION_DOWN,
  MOUSE_DIRECTION_UP,
} MouseDirection;

static MouseDirection sequence[NUM_SEQUENCES];
static gint sequence_pos = 0;
static gdouble last_x = NAN;
static gdouble last_y = NAN;
static gboolean gesture_active = FALSE;

static void
ephy_mouse_gesture_motion_cb (GtkEventControllerMotion *controller,
                              gdouble                   x,
                              gdouble                   y,
                              gpointer                  user_data)
{
  static MouseDirection last_direction = MOUSE_DIRECTION_UNKNOWN;
  MouseDirection direction;
  gdouble offset_x, offset_y;

  if (!gesture_active || sequence_pos == NUM_SEQUENCES)
    return;

  if (isnan (last_x) || isnan (last_y)) {
    last_x = x;
    last_y = y;
    return;
  }

  offset_x = x - last_x;
  offset_y = y - last_y;

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

  last_x = x;
  last_y = y;

  if (last_direction == direction)
    return;

  sequence[sequence_pos++] = direction;

  last_direction = direction;
}

static void
ephy_mouse_gesture_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  EphyMouseGesture *self = EPHY_MOUSE_GESTURE (object);

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
ephy_mouse_gesture_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  EphyMouseGesture *self = EPHY_MOUSE_GESTURE (object);

  switch (prop_id) {
    case PROP_WINDOW:
      g_value_set_object (value, self->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
ephy_mouse_gesture_button_press_cb (GtkWidget *widget,
                                    GdkEvent  *event,
                                    gpointer   user_data)
{
  GdkEventButton *button_event = (GdkEventButton *)event;

  if (button_event->button == GDK_BUTTON_MIDDLE)
    gesture_active = TRUE;
  else
    gesture_active = FALSE;

  return FALSE;
}

static void
handle_gesture (gpointer user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  GActionGroup *action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "toolbar");
  GAction *action;

  if (sequence_pos == 2) {
    if (sequence[0] == MOUSE_DIRECTION_DOWN && sequence[1] == MOUSE_DIRECTION_RIGHT) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "navigation-forward");
      g_action_activate (action, NULL);

      g_print ("Go Right\n");
    } else if (sequence[0] == MOUSE_DIRECTION_DOWN && sequence[1] == MOUSE_DIRECTION_LEFT) {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "navigation-back");
      g_action_activate (action, NULL);

      g_print ("Go Left\n");
    }
  }

  sequence_pos = 0;
  last_x = NAN;
  last_y = NAN;
}

static gboolean
ephy_mouse_gesture_button_release_cb (GtkWidget *widget,
                                      GdkEvent  *event,
                                      gpointer   user_data)
{
  GdkEventButton *button_event = (GdkEventButton *)event;

  if (button_event->button == GDK_BUTTON_MIDDLE) {
    if (gesture_active)
      handle_gesture (user_data);

    gesture_active = FALSE;
  }

  return FALSE;
}

static void
ephy_mouse_gesture_init (EphyMouseGesture *self)
{
}

static void
ephy_mouse_gesture_dispose (GObject *object)
{
  EphyMouseGesture *self = EPHY_MOUSE_GESTURE (object);

  if (self->controller != NULL && self->window != NULL)
    g_signal_handlers_disconnect_by_func (self->controller,
                                          G_CALLBACK (ephy_mouse_gesture_motion_cb),
                                          self->window);
  g_clear_object (&self->controller);

  G_OBJECT_CLASS (ephy_mouse_gesture_parent_class)->dispose (object);
}

static void
ephy_mouse_gesture_constructed (GObject *object)
{
  EphyMouseGesture *self = EPHY_MOUSE_GESTURE (object);

  self->controller = gtk_event_controller_motion_new (GTK_WIDGET (self->window));
  g_signal_connect (self->controller, "motion", G_CALLBACK (ephy_mouse_gesture_motion_cb), self->window);
}

static void
ephy_mouse_gesture_class_init (EphyMouseGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_mouse_gesture_dispose;
  object_class->constructed = ephy_mouse_gesture_constructed;

  /* class creation */
  object_class->set_property = ephy_mouse_gesture_set_property;
  object_class->get_property = ephy_mouse_gesture_get_property;

  obj_properties[PROP_WINDOW] =
    g_param_spec_object ("window",
                         "window",
                         "window",
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyMouseGesture *
ephy_mouse_gesture_new (EphyWindow *window)
{
  return g_object_new (EPHY_TYPE_MOUSE_GESTURE,
                       "window", window,
                       NULL);
}

void
ephy_mouse_gesture_set_web_view (EphyMouseGesture *self,
                                 WebKitWebView    *web_view)
{
  g_signal_connect (web_view, "button-press-event", G_CALLBACK (ephy_mouse_gesture_button_press_cb), self->window);
  g_signal_connect (web_view, "button-release-event", G_CALLBACK (ephy_mouse_gesture_button_release_cb), self->window);
}

void
ephy_mouse_gesture_unset_web_view (EphyMouseGesture *self,
                                   WebKitWebView    *web_view)
{
  g_signal_handlers_disconnect_by_func (web_view,
                                        G_CALLBACK (ephy_mouse_gesture_button_press_cb),
                                        self->window);
  g_signal_handlers_disconnect_by_func (web_view,
                                        G_CALLBACK (ephy_mouse_gesture_button_release_cb),
                                        self->window);
}

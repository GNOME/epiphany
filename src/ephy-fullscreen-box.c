/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2021 Purism SPC
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
#include "ephy-fullscreen-box.h"

#include <handy.h>

#define FULLSCREEN_HIDE_DELAY 300
#define SHOW_HEADERBAR_DISTANCE_PX 5

struct _EphyFullscreenBox {
  GtkEventBox parent_instance;

  HdyFlap *flap;
  GtkEventController *controller;
  GtkGesture *gesture;

  gboolean fullscreen;
  gboolean autohide;

  guint timeout_id;

  GtkWidget *last_focus;
  gdouble last_y;
  gboolean is_touch;
};

static void ephy_fullscreen_box_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyFullscreenBox, ephy_fullscreen_box, GTK_TYPE_EVENT_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                ephy_fullscreen_box_buildable_init))

enum {
  PROP_0,
  PROP_FULLSCREEN,
  PROP_AUTOHIDE,
  PROP_TITLEBAR,
  PROP_CONTENT,
  PROP_REVEALED,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];

static void
show_ui (EphyFullscreenBox *self)
{
  g_clear_handle_id (&self->timeout_id, g_source_remove);

  hdy_flap_set_reveal_flap (self->flap, TRUE);
}

static void
hide_ui (EphyFullscreenBox *self)
{
  g_clear_handle_id (&self->timeout_id, g_source_remove);

  if (!self->fullscreen)
    return;

  hdy_flap_set_reveal_flap (self->flap, FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (self->flap));
}

static gboolean
hide_timeout_cb (EphyFullscreenBox *self)
{
  self->timeout_id = 0;

  hide_ui (self);

  return G_SOURCE_REMOVE;
}

static void
start_hide_timeout (EphyFullscreenBox *self)
{
  if (!hdy_flap_get_reveal_flap (self->flap))
    return;

  if (self->timeout_id)
    return;

  self->timeout_id = g_timeout_add (FULLSCREEN_HIDE_DELAY,
                                    (GSourceFunc)hide_timeout_cb,
                                    self);
}

static gboolean
is_descendant_of (GtkWidget *widget,
                  GtkWidget *target)
{
  GtkWidget *parent;

  if (!widget)
    return FALSE;

  if (widget == target)
    return TRUE;

  parent = widget;

  while (parent && parent != target && !GTK_IS_POPOVER (parent))
    parent = gtk_widget_get_parent (parent);

  if (GTK_IS_POPOVER (parent))
    return is_descendant_of (gtk_popover_get_relative_to (GTK_POPOVER (parent)), target);

  return parent == target;
}

static double
get_titlebar_area_height (EphyFullscreenBox *self)
{
  gdouble height;

  height = gtk_widget_get_allocated_height (hdy_flap_get_flap (self->flap));
  height *= hdy_flap_get_reveal_progress (self->flap);
  height = MAX (height, SHOW_HEADERBAR_DISTANCE_PX);

  return height;
}

static void
update (EphyFullscreenBox *self,
        gboolean           hide_immediately)
{
  if (!self->autohide || !self->fullscreen)
    return;

  if (!self->is_touch &&
      self->last_y <= get_titlebar_area_height (self)) {
    show_ui (self);
    return;
  }

  if (self->last_focus && is_descendant_of (self->last_focus,
                                            hdy_flap_get_flap (self->flap)))
    show_ui (self);
  else if (hide_immediately)
    hide_ui (self);
  else
    start_hide_timeout (self);
}

static void
motion_cb (EphyFullscreenBox *self,
           double             x,
           double             y)
{
  self->is_touch = FALSE;
  self->last_y = y;

  update (self, TRUE);
}

static void
enter_cb (EphyFullscreenBox *self,
          double             x,
          double             y)
{
  g_autoptr (GdkEvent) event = gtk_get_current_event ();

  if (event->crossing.window != gtk_widget_get_window (GTK_WIDGET (self)) ||
      event->crossing.detail == GDK_NOTIFY_INFERIOR)
    return;

  motion_cb (self, x, y);
}

static void
press_cb (EphyFullscreenBox *self,
          int                n_press,
          double             x,
          double             y)
{
  gtk_gesture_set_state (self->gesture, GTK_EVENT_SEQUENCE_DENIED);

  self->is_touch = TRUE;

  if (y > get_titlebar_area_height (self))
    update (self, TRUE);
}

static void
set_focus_cb (EphyFullscreenBox *self,
              GtkWidget         *widget)
{
  self->last_focus = widget;

  update (self, TRUE);
}

static void
notify_reveal_cb (EphyFullscreenBox *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REVEALED]);
}

static void
ephy_fullscreen_box_hierarchy_changed (GtkWidget *widget,
                                       GtkWidget *previous_toplevel)
{
  EphyFullscreenBox *self = EPHY_FULLSCREEN_BOX (widget);
  GtkWidget *toplevel;

  if (previous_toplevel && GTK_IS_WINDOW (previous_toplevel))
    g_signal_handlers_disconnect_by_func (previous_toplevel, set_focus_cb, widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (toplevel && GTK_IS_WINDOW (toplevel)) {
    g_signal_connect_object (toplevel, "set-focus",
                             G_CALLBACK (set_focus_cb), widget,
                             G_CONNECT_SWAPPED);

    set_focus_cb (self, gtk_window_get_focus (GTK_WINDOW (toplevel)));
  } else {
    set_focus_cb (self, NULL);
  }
}

static void
ephy_fullscreen_box_add (GtkContainer *container,
                         GtkWidget    *widget)
{
  EphyFullscreenBox *self = EPHY_FULLSCREEN_BOX (container);

  if (!self->flap)
    GTK_CONTAINER_CLASS (ephy_fullscreen_box_parent_class)->add (container, widget);
  else
    gtk_container_add (GTK_CONTAINER (self->flap), widget);
}

static void
ephy_fullscreen_box_remove (GtkContainer *container,
                            GtkWidget    *widget)
{
  EphyFullscreenBox *self = EPHY_FULLSCREEN_BOX (container);

  if (widget == GTK_WIDGET (self->flap)) {
    GTK_CONTAINER_CLASS (ephy_fullscreen_box_parent_class)->remove (container, widget);
    self->flap = NULL;
  } else {
    gtk_container_remove (GTK_CONTAINER (self->flap), widget);
  }
}

static void
ephy_fullscreen_box_forall (GtkContainer *container,
                            gboolean      include_internals,
                            GtkCallback   callback,
                            gpointer      callback_data)
{
  EphyFullscreenBox *self = EPHY_FULLSCREEN_BOX (container);

  if (include_internals) {
    GTK_CONTAINER_CLASS (ephy_fullscreen_box_parent_class)->forall (container,
                                                                    include_internals,
                                                                    callback,
                                                                    callback_data);
  } else {
    gtk_container_foreach (GTK_CONTAINER (self->flap),
                           callback,
                           callback_data);
  }
}

static void
ephy_fullscreen_box_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EphyFullscreenBox *self = EPHY_FULLSCREEN_BOX (object);

  switch (prop_id) {
    case PROP_FULLSCREEN:
      g_value_set_boolean (value, ephy_fullscreen_box_get_fullscreen (self));
      break;

    case PROP_AUTOHIDE:
      g_value_set_boolean (value, ephy_fullscreen_box_get_autohide (self));
      break;

    case PROP_TITLEBAR:
      g_value_set_object (value, ephy_fullscreen_box_get_titlebar (self));
      break;

    case PROP_CONTENT:
      g_value_set_object (value, ephy_fullscreen_box_get_content (self));
      break;

    case PROP_REVEALED:
      g_value_set_boolean (value, hdy_flap_get_reveal_flap (self->flap));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_fullscreen_box_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EphyFullscreenBox *self = EPHY_FULLSCREEN_BOX (object);

  switch (prop_id) {
    case PROP_FULLSCREEN:
      ephy_fullscreen_box_set_fullscreen (self, g_value_get_boolean (value));
      break;

    case PROP_AUTOHIDE:
      ephy_fullscreen_box_set_autohide (self, g_value_get_boolean (value));
      break;

    case PROP_TITLEBAR:
      ephy_fullscreen_box_set_titlebar (self, g_value_get_object (value));
      break;

    case PROP_CONTENT:
      ephy_fullscreen_box_set_content (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_fullscreen_box_dispose (GObject *object)
{
  EphyFullscreenBox *self = EPHY_FULLSCREEN_BOX (object);

  g_clear_object (&self->controller);
  g_clear_object (&self->gesture);

  G_OBJECT_CLASS (ephy_fullscreen_box_parent_class)->dispose (object);
}

static void
ephy_fullscreen_box_class_init (EphyFullscreenBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = ephy_fullscreen_box_get_property;
  object_class->set_property = ephy_fullscreen_box_set_property;
  object_class->dispose = ephy_fullscreen_box_dispose;

  widget_class->hierarchy_changed = ephy_fullscreen_box_hierarchy_changed;

  container_class->add = ephy_fullscreen_box_add;
  container_class->remove = ephy_fullscreen_box_remove;
  container_class->forall = ephy_fullscreen_box_forall;

  props[PROP_FULLSCREEN] =
    g_param_spec_boolean ("fullscreen",
                          "Fullscreen",
                          "Fullscreen",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_AUTOHIDE] =
    g_param_spec_boolean ("autohide",
                          "Autohide",
                          "Autohide",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_TITLEBAR] =
    g_param_spec_object ("titlebar",
                         "Titlebar",
                         "Titlebar",
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_CONTENT] =
    g_param_spec_object ("content",
                         "Content",
                         "Content",
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_REVEALED] =
    g_param_spec_boolean ("revealed",
                          "Revealed",
                          "Revealed",
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, "fullscreenbox");
}

static void
ephy_fullscreen_box_init (EphyFullscreenBox *self)
{
  HdyFlap *flap;

  self->autohide = TRUE;

  gtk_widget_add_events (GTK_WIDGET (self), GDK_ALL_EVENTS_MASK);

  flap = HDY_FLAP (hdy_flap_new ());
  gtk_orientable_set_orientation (GTK_ORIENTABLE (flap), GTK_ORIENTATION_VERTICAL);
  hdy_flap_set_flap_position (flap, GTK_PACK_START);
  hdy_flap_set_fold_policy (flap, HDY_FLAP_FOLD_POLICY_NEVER);
  hdy_flap_set_locked (flap, TRUE);
  hdy_flap_set_modal (flap, FALSE);
  hdy_flap_set_swipe_to_open (flap, FALSE);
  hdy_flap_set_swipe_to_close (flap, FALSE);
  hdy_flap_set_transition_type (flap, HDY_FLAP_TRANSITION_TYPE_OVER);
  gtk_widget_show (GTK_WIDGET (flap));

  g_signal_connect_object (flap, "notify::reveal-flap",
                           G_CALLBACK (notify_reveal_cb), self, G_CONNECT_SWAPPED);

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (flap));
  self->flap = flap;

  self->controller = gtk_event_controller_motion_new (GTK_WIDGET (self));
  gtk_event_controller_set_propagation_phase (self->controller, GTK_PHASE_CAPTURE);
  g_signal_connect_object (self->controller, "enter",
                           G_CALLBACK (enter_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->controller, "motion",
                           G_CALLBACK (motion_cb), self, G_CONNECT_SWAPPED);

  self->gesture = gtk_gesture_multi_press_new (GTK_WIDGET (self));
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->gesture),
                                              GTK_PHASE_CAPTURE);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->gesture), TRUE);
  g_signal_connect_object (self->gesture, "pressed",
                           G_CALLBACK (press_cb), self, G_CONNECT_SWAPPED);
}

static void
ephy_fullscreen_box_buildable_add_child (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *type)
{
  EphyFullscreenBox *self = EPHY_FULLSCREEN_BOX (buildable);

  if (!g_strcmp0 (type, "titlebar"))
    ephy_fullscreen_box_set_titlebar (self, GTK_WIDGET (child));
  else
    ephy_fullscreen_box_set_content (self, GTK_WIDGET (child));
}

static void
ephy_fullscreen_box_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = ephy_fullscreen_box_buildable_add_child;
}

EphyFullscreenBox *
ephy_fullscreen_box_new (void)
{
  return g_object_new (EPHY_TYPE_FULLSCREEN_BOX, NULL);
}

gboolean
ephy_fullscreen_box_get_fullscreen (EphyFullscreenBox *self)
{
  g_return_val_if_fail (EPHY_IS_FULLSCREEN_BOX (self), FALSE);

  return self->fullscreen;
}

void
ephy_fullscreen_box_set_fullscreen (EphyFullscreenBox *self,
                                    gboolean           fullscreen)
{
  g_return_if_fail (EPHY_IS_FULLSCREEN_BOX (self));

  fullscreen = !!fullscreen;

  if (fullscreen == self->fullscreen)
    return;

  self->fullscreen = fullscreen;

  if (!self->autohide)
    return;

  if (fullscreen) {
    hdy_flap_set_fold_policy (self->flap, HDY_FLAP_FOLD_POLICY_ALWAYS);
    update (self, FALSE);
  } else {
    hdy_flap_set_fold_policy (self->flap, HDY_FLAP_FOLD_POLICY_NEVER);
    show_ui (self);
  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FULLSCREEN]);
}

gboolean
ephy_fullscreen_box_get_autohide (EphyFullscreenBox *self)
{
  g_return_val_if_fail (EPHY_IS_FULLSCREEN_BOX (self), FALSE);

  return self->autohide;
}

void
ephy_fullscreen_box_set_autohide (EphyFullscreenBox *self,
                                  gboolean           autohide)
{
  g_return_if_fail (EPHY_IS_FULLSCREEN_BOX (self));

  autohide = !!autohide;

  if (autohide == self->autohide)
    return;

  self->autohide = autohide;

  if (!self->fullscreen)
    return;

  if (autohide)
    hide_ui (self);
  else
    show_ui (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_AUTOHIDE]);
}

GtkWidget *
ephy_fullscreen_box_get_titlebar (EphyFullscreenBox *self)
{
  g_return_val_if_fail (EPHY_IS_FULLSCREEN_BOX (self), NULL);

  return hdy_flap_get_flap (self->flap);
}

void
ephy_fullscreen_box_set_titlebar (EphyFullscreenBox *self,
                                  GtkWidget         *titlebar)
{
  g_return_if_fail (EPHY_IS_FULLSCREEN_BOX (self));
  g_return_if_fail (GTK_IS_WIDGET (titlebar) || titlebar == NULL);

  if (hdy_flap_get_flap (self->flap) == titlebar)
    return;

  hdy_flap_set_flap (self->flap, titlebar);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLEBAR]);
}

GtkWidget *
ephy_fullscreen_box_get_content (EphyFullscreenBox *self)
{
  g_return_val_if_fail (EPHY_IS_FULLSCREEN_BOX (self), NULL);

  return hdy_flap_get_content (self->flap);
}

void
ephy_fullscreen_box_set_content (EphyFullscreenBox *self,
                                 GtkWidget         *content)
{
  g_return_if_fail (EPHY_IS_FULLSCREEN_BOX (self));
  g_return_if_fail (GTK_IS_WIDGET (content) || content == NULL);

  if (hdy_flap_get_content (self->flap) == content)
    return;

  hdy_flap_set_content (self->flap, content);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONTENT]);
}

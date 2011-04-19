/*
 *
 * Copyright © 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "ephy-overlay-escaping-child.h"

#define EPHY_OVERLAY_ESCAPING_CHILD_DEFAULT_DISTANCE 20

G_DEFINE_TYPE (EphyOverlayEscapingChild, ephy_overlay_escaping_child, GEDIT_TYPE_OVERLAY_CHILD);

/* properties */
enum
{
  PROP_0,
  PROP_ESCAPING_DISTANCE
};

struct _EphyOverlayEscapingChildPrivate
{
  guint escaping_distance;
  GtkAllocation initial_allocation;
  GdkRectangle escaping_area;
};

/* If the pointer leaves the window, restore the widget position */
static gboolean
parent_leave_notify_event (GtkWidget *widget,
                           GdkEventMotion *event,
                           GtkWidget *parent)
{
  EphyOverlayEscapingChildPrivate *priv = EPHY_OVERLAY_ESCAPING_CHILD (widget)->priv;
  GtkAllocation alloc;

  gtk_widget_get_allocation (widget, &alloc);
  alloc.y = priv->initial_allocation.y;
  gtk_widget_size_allocate (widget, &alloc);

  return FALSE;
}

/* this should be in Gdk...really */
static gboolean
is_point_in_rectangle (int point_x,
                       int point_y,
                       GdkRectangle rectangle)
{
  int rectangle_x_higher_bound = rectangle.x + rectangle.width;
  int rectangle_y_higher_bound = rectangle.y + rectangle.height;

  return point_x >= rectangle.x && point_x < rectangle_x_higher_bound
    && point_y >= rectangle.y && point_y < rectangle_y_higher_bound;
}

/* Keep the widget-pointer distance at at least
 * EphyOverlayEscapingChildPrivate::escaping_distance by sliding the widget
 * away if needed.
 */
static gboolean
parent_motion_notify_event (GtkWidget *widget,
                            GdkEventMotion *event,
                            GtkWidget *parent)
{
  EphyOverlayEscapingChildPrivate *priv = EPHY_OVERLAY_ESCAPING_CHILD (widget)->priv;
  int distance_x, distance_y;
  GtkAllocation alloc;

  gtk_widget_get_allocation (widget, &alloc);

  if (is_point_in_rectangle (event->x, event->y, priv->escaping_area)) {
    gtk_widget_get_pointer (widget, &distance_x, &distance_y);
    alloc.y += priv->escaping_distance + distance_y;
  }
  else {
    /* Put the widget at its original position if we are out of the escaping
     * zone. Do nothing if it is already there.
     */
    if (alloc.y == priv->initial_allocation.y)
      return FALSE;
    alloc.y = priv->initial_allocation.y;
  }

  gtk_widget_size_allocate (widget, &alloc);

  return FALSE;
}

/* When the parent overlay is resized, the child relative position is modified.
 * So we update our initial_allocation to this new value and redefine our
 * escaping area.
 */
static void
parent_size_allocate (GtkWidget    *widget,
                      GdkRectangle *allocation,
                      GtkWidget      *parent)
{
  EphyOverlayEscapingChildPrivate *priv = EPHY_OVERLAY_ESCAPING_CHILD (widget)->priv;
  GtkAllocation initial_allocation;

  gtk_widget_get_allocation (widget, &initial_allocation);
  priv->escaping_area = priv->initial_allocation = initial_allocation;

  /* Define an escaping area around the widget.
   * Current implementation only handle horizontal lowerside widgets
   */
  priv->escaping_area.height += priv->escaping_distance;
  /* escape on both right and left */
  priv->escaping_area.width += 2 * priv->escaping_distance;
  priv->escaping_area.x -= priv->escaping_distance;
  priv->escaping_area.y -= priv->escaping_distance;
}

/* Install listeners on our overlay parents to locate the pointer
 * and our relative position.
 */
static void
ephy_overlay_escaping_child_parent_set (GtkWidget *widget,
                                        GtkWidget *previous_parent)
{
  GtkWidget *parent;

  if (previous_parent != NULL) {
    g_signal_handlers_disconnect_by_func (previous_parent,
                                          G_CALLBACK (parent_motion_notify_event),
                                          widget);
    g_signal_handlers_disconnect_by_func (previous_parent,
                                          G_CALLBACK (parent_leave_notify_event),
                                          widget);
    g_signal_handlers_disconnect_by_func (previous_parent,
                                          G_CALLBACK (parent_size_allocate),
                                          widget);
  }

  parent = gtk_widget_get_parent (widget);
  if (parent == NULL)
    return;

  g_signal_connect_swapped (parent,
                            "motion-notify-event",
                            G_CALLBACK (parent_motion_notify_event),
                            widget);
  g_signal_connect_swapped (parent,
                            "leave-notify-event",
                            G_CALLBACK (parent_leave_notify_event),
                            widget);
  g_signal_connect_swapped (parent,
                            "size-allocate",
                            G_CALLBACK (parent_size_allocate),
                            widget);

}

/* When the mouse is over us, translate the event coords and slide the widget
 * accordingly
 */
static gboolean
ephy_overlay_escaping_child_motion_notify_event (GtkWidget *widget,
                                                 GdkEventMotion *event)
{
  EphyOverlayEscapingChildPrivate *priv = EPHY_OVERLAY_ESCAPING_CHILD (widget)->priv;

  event->x += priv->initial_allocation.x;
  event->y += priv->initial_allocation.y;
  return parent_motion_notify_event (widget, event, gtk_widget_get_parent (widget));
}

/* Make our event window propagate mouse motion events, so we can slide the widget,
 * when hovered.
 */
static void
ephy_overlay_escaping_child_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (ephy_overlay_escaping_child_parent_class)->realize (widget);
  GdkWindow *window = gtk_widget_get_window (widget);
  GdkEventMask events = gdk_window_get_events (window);
  events |= GDK_POINTER_MOTION_MASK;
  gdk_window_set_events (window, events);
}

static void
ephy_overlay_escaping_child_get_property (GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
  EphyOverlayEscapingChild *self = EPHY_OVERLAY_ESCAPING_CHILD (object);
  EphyOverlayEscapingChildPrivate *priv = self->priv;

  switch (property_id) {
  case PROP_ESCAPING_DISTANCE:
    g_value_set_uint (value, priv->escaping_distance);
  break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  break;
  }
}

static void
ephy_overlay_escaping_child_set_property (GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
  EphyOverlayEscapingChild *self = EPHY_OVERLAY_ESCAPING_CHILD (object);
  EphyOverlayEscapingChildPrivate *priv = self->priv;

  switch (property_id)
  {
  case PROP_ESCAPING_DISTANCE:
    priv->escaping_distance = g_value_get_uint (value);
  break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  break;
  }
}

static void
ephy_overlay_escaping_child_class_init (EphyOverlayEscapingChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof(EphyOverlayEscapingChildPrivate));

  gobject_class->get_property = ephy_overlay_escaping_child_get_property;
  gobject_class->set_property = ephy_overlay_escaping_child_set_property;

  widget_class->parent_set = ephy_overlay_escaping_child_parent_set;
  widget_class->motion_notify_event = ephy_overlay_escaping_child_motion_notify_event;
  widget_class->realize = ephy_overlay_escaping_child_realize;

  g_object_class_install_property (gobject_class,
                                   PROP_ESCAPING_DISTANCE,
                                   g_param_spec_uint ("escaping-distance",
                                                      "Escaping distance",
                                                      "Maximum distance between the mouse pointer and the widget",
                                                      0,
                                                      G_MAXUINT,
                                                      EPHY_OVERLAY_ESCAPING_CHILD_DEFAULT_DISTANCE,
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));
}

static void
ephy_overlay_escaping_child_init (EphyOverlayEscapingChild *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            EPHY_TYPE_OVERLAY_ESCAPING_CHILD,
                                            EphyOverlayEscapingChildPrivate);
}

/**
 * ephy_overlay_escaping_child_new:
 * @widget: the wrapped #GtkWidget
 * @escaping_distance: the distance from which the widget escapes the mouse
 * pointer
 *
 * Creates a new #EphyOverlayEscapingChild object wrapping the provided
 * widget. The widget will stay at a minimal distance of @escaping_distance pixel
 * from the mouse pointer.
 *
 * Returns: a new #EphyOverlayEscapingChild object
 */
EphyOverlayEscapingChild *
ephy_overlay_escaping_child_new_with_distance (GtkWidget *widget,
                                               guint escaping_distance)
{
  return g_object_new (EPHY_TYPE_OVERLAY_ESCAPING_CHILD,
                       "widget", widget,
                       "escaping-distance", escaping_distance,
                       NULL);
}

/**
 * ephy_overlay_escaping_child_new:
 * @widget: the wrapped #GtkWidget
 *
 * Creates a new #EphyOverlayEscapingChild object wrapping the provided
 * widget. The widget will stay at a minimal distance of 20 pixels from
 * the mouse pointer.
 *
 * Returns: a new #EphyOverlayEscapingChild object
 */
EphyOverlayEscapingChild *
ephy_overlay_escaping_child_new (GtkWidget *widget)
{
  return g_object_new (EPHY_TYPE_OVERLAY_ESCAPING_CHILD,
                       "widget", widget,
                       NULL);
}

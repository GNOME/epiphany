/* Nautilus - Floating status bar.
 *
 * Copyright (C) 2011 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include <string.h>

#include "nautilus-floating-bar.h"

#define HOVER_HIDE_TIMEOUT_INTERVAL 100

struct _NautilusFloatingBar {
  GtkBox parent_instance;

  gchar *primary_label;
  GtkWidget *primary_label_widget;

  guint hover_timeout_id;
};

enum {
  PROP_PRIMARY_LABEL = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NautilusFloatingBar, nautilus_floating_bar,
               GTK_TYPE_BOX);

static void
nautilus_floating_bar_remove_hover_timeout (NautilusFloatingBar *self)
{
  if (self->hover_timeout_id != 0) {
    g_source_remove (self->hover_timeout_id);
    self->hover_timeout_id = 0;
  }
}

typedef struct {
  GtkWidget *overlay;
  GtkWidget *floating_bar;
  GdkDevice *device;
  gint y_down_limit;
  gint y_upper_limit;
} CheckPointerData;

static void
check_pointer_data_free (gpointer data)
{
  g_slice_free (CheckPointerData, data);
}

static gboolean
check_pointer_timeout (gpointer user_data)
{
  CheckPointerData *data = user_data;
  gint pointer_y = -1;

  gdk_window_get_device_position (gtk_widget_get_window (data->overlay), data->device,
                                  NULL, &pointer_y, NULL);

  if (pointer_y == -1 || pointer_y < data->y_down_limit || pointer_y > data->y_upper_limit) {
    gtk_widget_show (data->floating_bar);
    NAUTILUS_FLOATING_BAR (data->floating_bar)->hover_timeout_id = 0;
    return G_SOURCE_REMOVE;
  } else {
    gtk_widget_hide (data->floating_bar);
  }

  return G_SOURCE_CONTINUE;
}

static gboolean
overlay_enter_notify_cb (GtkWidget        *parent,
                         GdkEventCrossing *event,
                         gpointer          user_data)
{
  GtkWidget *widget = user_data;
  CheckPointerData *data;
  gint y_pos;

  NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);

  if (self->hover_timeout_id != 0) {
    g_source_remove (self->hover_timeout_id);
  }

  if (event->window != gtk_widget_get_window (widget)) {
    return GDK_EVENT_PROPAGATE;
  }

  gdk_window_get_position (gtk_widget_get_window (widget), NULL, &y_pos);

  data = g_slice_new (CheckPointerData);
  data->overlay = parent;
  data->floating_bar = widget;
  data->device = gdk_event_get_device ((GdkEvent *)event);
  data->y_down_limit = y_pos;
  data->y_upper_limit = y_pos + gtk_widget_get_allocated_height (widget);

  self->hover_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, HOVER_HIDE_TIMEOUT_INTERVAL,
                                               check_pointer_timeout, data,
                                               check_pointer_data_free);

  g_source_set_name_by_id (self->hover_timeout_id, "[nautilus-floating-bar] overlay_enter_notify_cb");

  return GDK_EVENT_STOP;
}

static void
nautilus_floating_bar_parent_set (GtkWidget *widget,
                                  GtkWidget *old_parent)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (widget);

  if (old_parent != NULL) {
    g_signal_handlers_disconnect_by_func (old_parent,
                                          overlay_enter_notify_cb, widget);
  }

  if (parent != NULL) {
    g_signal_connect (parent, "enter-notify-event",
                      G_CALLBACK (overlay_enter_notify_cb), widget);
  }
}

static void
get_padding_and_border (GtkWidget *widget,
                        GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
  border->left += tmp.left;
}

static void
nautilus_floating_bar_get_preferred_width (GtkWidget *widget,
                                           gint      *minimum_size,
                                           gint      *natural_size)
{
  GtkBorder border;

  get_padding_and_border (widget, &border);

  GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->get_preferred_width (widget,
                                                                              minimum_size,
                                                                              natural_size);

  *minimum_size += border.left + border.right;
  *natural_size += border.left + border.right;
}

static void
nautilus_floating_bar_get_preferred_width_for_height (GtkWidget *widget,
                                                      gint       height,
                                                      gint      *minimum_size,
                                                      gint      *natural_size)
{
  GtkBorder border;

  get_padding_and_border (widget, &border);

  GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->get_preferred_width_for_height (widget,
                                                                                         height,
                                                                                         minimum_size,
                                                                                         natural_size);

  *minimum_size += border.left + border.right;
  *natural_size += border.left + border.right;
}

static void
nautilus_floating_bar_get_preferred_height (GtkWidget *widget,
                                            gint      *minimum_size,
                                            gint      *natural_size)
{
  GtkBorder border;

  get_padding_and_border (widget, &border);

  GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->get_preferred_height (widget,
                                                                               minimum_size,
                                                                               natural_size);

  *minimum_size += border.top + border.bottom;
  *natural_size += border.top + border.bottom;
}

static void
nautilus_floating_bar_get_preferred_height_for_width (GtkWidget *widget,
                                                      gint       width,
                                                      gint      *minimum_size,
                                                      gint      *natural_size)
{
  GtkBorder border;

  get_padding_and_border (widget, &border);

  GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->get_preferred_height_for_width (widget,
                                                                                         width,
                                                                                         minimum_size,
                                                                                         natural_size);

  *minimum_size += border.top + border.bottom;
  *natural_size += border.top + border.bottom;
}

static void
nautilus_floating_bar_constructed (GObject *obj)
{
  NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);
  GtkWidget *w;

  G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->constructed (obj);

  w = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
  gtk_container_add (GTK_CONTAINER (self), w);
  self->primary_label_widget = w;
  gtk_widget_show (w);

  g_object_set (w,
                "margin-top", 2,
                "margin-bottom", 2,
                "margin-start", 12,
                "margin-end", 12,
                NULL);
}

static void
nautilus_floating_bar_finalize (GObject *obj)
{
  NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);

  nautilus_floating_bar_remove_hover_timeout (self);
  g_free (self->primary_label);

  G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->finalize (obj);
}

static void
nautilus_floating_bar_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

  switch (property_id) {
    case PROP_PRIMARY_LABEL:
      g_value_set_string (value, self->primary_label);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
nautilus_floating_bar_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

  switch (property_id) {
    case PROP_PRIMARY_LABEL:
      nautilus_floating_bar_set_primary_label (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
nautilus_floating_bar_init (NautilusFloatingBar *self)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "floating-bar");
}

static void
nautilus_floating_bar_class_init (NautilusFloatingBarClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  oclass->constructed = nautilus_floating_bar_constructed;
  oclass->set_property = nautilus_floating_bar_set_property;
  oclass->get_property = nautilus_floating_bar_get_property;
  oclass->finalize = nautilus_floating_bar_finalize;

  wclass->get_preferred_width = nautilus_floating_bar_get_preferred_width;
  wclass->get_preferred_width_for_height = nautilus_floating_bar_get_preferred_width_for_height;
  wclass->get_preferred_height = nautilus_floating_bar_get_preferred_height;
  wclass->get_preferred_height_for_width = nautilus_floating_bar_get_preferred_height_for_width;
  wclass->parent_set = nautilus_floating_bar_parent_set;

  properties[PROP_PRIMARY_LABEL] =
    g_param_spec_string ("primary-label",
                         "Bar's primary label",
                         "Primary label displayed by the bar",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

void
nautilus_floating_bar_set_primary_label (NautilusFloatingBar *self,
                                         const gchar         *label)
{
  if (g_strcmp0 (self->primary_label, label) != 0) {
    g_free (self->primary_label);
    self->primary_label = g_strdup (label);

    gtk_label_set_label (GTK_LABEL (self->primary_label_widget), label);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_LABEL]);
  }
}

GtkWidget *
nautilus_floating_bar_new (void)
{
  return g_object_new (NAUTILUS_TYPE_FLOATING_BAR, NULL);
}

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
};

enum {
  PROP_PRIMARY_LABEL = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NautilusFloatingBar, nautilus_floating_bar,
               GTK_TYPE_BOX);

static void
nautilus_floating_bar_finalize (GObject *obj)
{
  NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);

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
  GtkWidget *w = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_MIDDLE);
  gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
  gtk_box_append (GTK_BOX (self), w);
  self->primary_label_widget = w;

  gtk_widget_set_margin_top (w, 2);
  gtk_widget_set_margin_bottom (w, 2);
  gtk_widget_set_margin_start (w, 12);
  gtk_widget_set_margin_end (w, 12);

  gtk_widget_set_can_target (GTK_WIDGET (self), FALSE);
  gtk_widget_add_css_class (GTK_WIDGET (self), "floating-bar");
}

static void
nautilus_floating_bar_class_init (NautilusFloatingBarClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = nautilus_floating_bar_set_property;
  oclass->get_property = nautilus_floating_bar_get_property;
  oclass->finalize = nautilus_floating_bar_finalize;

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

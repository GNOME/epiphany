/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Arnaud Bonatti
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
#include "ephy-encoding-row.h"

#include "ephy-encoding.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

struct _EphyEncodingRow {
  GtkBox parent_instance;

  EphyEncoding *encoding;

  /* from the UI file */
  GtkLabel *encoding_label;
  GtkImage *selected_image;
};

enum {
  PROP_0,
  PROP_ENCODING,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_FINAL_TYPE (EphyEncodingRow, ephy_encoding_row, GTK_TYPE_BOX)

void
ephy_encoding_row_set_selected (EphyEncodingRow *row,
                                gboolean         selected)
{
  g_assert (EPHY_IS_ENCODING_ROW (row));

  gtk_widget_set_visible (GTK_WIDGET (row->selected_image), selected);
}

static void
ephy_encoding_row_init (EphyEncodingRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ephy_encoding_row_set_encoding (EphyEncodingRow *self,
                                EphyEncoding    *encoding)
{
  g_assert (EPHY_IS_ENCODING (encoding));

  self->encoding = encoding;
  gtk_label_set_text (self->encoding_label,
                      ephy_encoding_get_title_elided (encoding));
}

EphyEncoding *
ephy_encoding_row_get_encoding (EphyEncodingRow *row)
{
  return row->encoding;
}

static void
ephy_encoding_row_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (prop_id) {
    case PROP_ENCODING:
      ephy_encoding_row_set_encoding (EPHY_ENCODING_ROW (object),
                                      g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_encoding_row_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  switch (prop_id) {
    case PROP_ENCODING:
      g_value_set_object (value, EPHY_ENCODING_ROW (object)->encoding);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_encoding_row_class_init (EphyEncodingRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  /* class creation */
  object_class->set_property = ephy_encoding_row_set_property;
  object_class->get_property = ephy_encoding_row_get_property;

  obj_properties[PROP_ENCODING] =
    g_param_spec_object ("encoding",
                         NULL, NULL,
                         EPHY_TYPE_ENCODING,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  /* load from UI file */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/encoding-row.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyEncodingRow, encoding_label);
  gtk_widget_class_bind_template_child (widget_class, EphyEncodingRow, selected_image);
}

EphyEncodingRow *
ephy_encoding_row_new (EphyEncoding *encoding)
{
  return g_object_new (EPHY_TYPE_ENCODING_ROW,
                       "encoding", encoding,
                       NULL);
}

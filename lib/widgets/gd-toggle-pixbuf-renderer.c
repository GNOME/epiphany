/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "gd-toggle-pixbuf-renderer.h"

G_DEFINE_TYPE (GdTogglePixbufRenderer, gd_toggle_pixbuf_renderer, GTK_TYPE_CELL_RENDERER_PIXBUF);

enum {
  PROP_ACTIVE = 1,
  PROP_TOGGLE_VISIBLE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _GdTogglePixbufRendererPrivate {
  gboolean active;
  gboolean toggle_visible;
};

static void
gd_toggle_pixbuf_renderer_render (GtkCellRenderer      *cell,
                                  cairo_t              *cr,
                                  GtkWidget            *widget,
                                  const GdkRectangle   *background_area,
                                  const GdkRectangle   *cell_area,
                                  GtkCellRendererState  flags)
{
  gint icon_size = -1;
  gint check_x, check_y, x_offset, xpad, ypad;
  GtkStyleContext *context;
  GdTogglePixbufRenderer *self = GD_TOGGLE_PIXBUF_RENDERER (cell);
  GtkTextDirection direction;

  GTK_CELL_RENDERER_CLASS (gd_toggle_pixbuf_renderer_parent_class)->render
    (cell, cr, widget,
     background_area, cell_area, flags);

  if (!self->priv->toggle_visible)
    return;

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
  direction = gtk_widget_get_direction (widget);
  gtk_widget_style_get (widget,
                        "check-icon-size", &icon_size,
                        NULL);

  if (icon_size == -1)
    icon_size = 40;

  if (direction == GTK_TEXT_DIR_RTL)
    x_offset = xpad;
  else
    x_offset = cell_area->width - icon_size - xpad;

  check_x = cell_area->x + x_offset;
  check_y = cell_area->y + cell_area->height - icon_size - ypad;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_CHECK);

  if (self->priv->active)
    gtk_style_context_set_state (context, GTK_STATE_FLAG_ACTIVE);

  gtk_render_check (context, cr,
                    check_x, check_y,
                    icon_size, icon_size);

  gtk_style_context_restore (context);
}

static void
gd_toggle_pixbuf_renderer_get_size (GtkCellRenderer *cell,
                                    GtkWidget       *widget,
                                    const GdkRectangle *cell_area,
                                    gint *x_offset,
                                    gint *y_offset,
                                    gint *width,
                                    gint *height)
{
  gint icon_size;

  gtk_widget_style_get (widget,
                        "check-icon-size", &icon_size,
                        NULL);

  GTK_CELL_RENDERER_CLASS (gd_toggle_pixbuf_renderer_parent_class)->get_size
    (cell, widget, cell_area,
     x_offset, y_offset, width, height);

  *width += icon_size / 4;
}

static void
gd_toggle_pixbuf_renderer_get_property (GObject    *object,
                                        guint       property_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GdTogglePixbufRenderer *self = GD_TOGGLE_PIXBUF_RENDERER (object);

  switch (property_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->priv->active);
      break;
    case PROP_TOGGLE_VISIBLE:
      g_value_set_boolean (value, self->priv->toggle_visible);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gd_toggle_pixbuf_renderer_set_property (GObject    *object,
                                        guint       property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  GdTogglePixbufRenderer *self = GD_TOGGLE_PIXBUF_RENDERER (object);

  switch (property_id)
    {
    case PROP_ACTIVE:
      self->priv->active = g_value_get_boolean (value);
      break;
    case PROP_TOGGLE_VISIBLE:
      self->priv->toggle_visible = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gd_toggle_pixbuf_renderer_class_init (GdTogglePixbufRendererClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *crclass = GTK_CELL_RENDERER_CLASS (klass);

  crclass->render = gd_toggle_pixbuf_renderer_render;
  crclass->get_size = gd_toggle_pixbuf_renderer_get_size;
  oclass->get_property = gd_toggle_pixbuf_renderer_get_property;
  oclass->set_property = gd_toggle_pixbuf_renderer_set_property;

  properties[PROP_ACTIVE] = 
    g_param_spec_boolean ("active",
                          "Active",
                          "Whether the cell renderer is active",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);
  properties[PROP_TOGGLE_VISIBLE] =
    g_param_spec_boolean ("toggle-visible",
                          "Toggle visible",
                          "Whether to draw the toggle indicator",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  g_type_class_add_private (klass, sizeof (GdTogglePixbufRendererPrivate));
  g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
gd_toggle_pixbuf_renderer_init (GdTogglePixbufRenderer *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GD_TYPE_TOGGLE_PIXBUF_RENDERER,
                                            GdTogglePixbufRendererPrivate);
}

GtkCellRenderer *
gd_toggle_pixbuf_renderer_new (void)
{
  return g_object_new (GD_TYPE_TOGGLE_PIXBUF_RENDERER, NULL);
}

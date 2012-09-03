/*
 * Copyright (c) 2011 Red Hat, Inc.
 * Copyright (c) 2012 Igalia, S.L.
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
 * Based on gd-toggle-pixbuf-renderer by Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "config.h"

#include "ephy-removable-pixbuf-renderer.h"
#include "ephy-widgets-type-builtins.h"

G_DEFINE_TYPE (EphyRemovablePixbufRenderer, ephy_removable_pixbuf_renderer, GD_TYPE_TOGGLE_PIXBUF_RENDERER);

enum {
  DELETE_CLICKED,
  LAST_SIGNAL
};

enum {
  PROP_0 = 0,
  PROP_RENDER_POLICY
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _EphyRemovablePixbufRendererPrivate {
  EphyRemovablePixbufRenderPolicy policy;
  GdkPixbuf *close_icon;
};

static void
get_icon_rectangle (GtkWidget *widget,
		    GtkCellRenderer *cell,
		    const GdkRectangle *cell_area,
		    GdkPixbuf *icon,
		    GdkRectangle *rectangle)
{
  GtkTextDirection direction;
  gint x_offset, y_offset, xpad, ypad;
  gint icon_size;
  gint w = 0, h = 0;
  GdkPixbuf *pixbuf;

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
  direction = gtk_widget_get_direction (widget);
  icon_size = gdk_pixbuf_get_width (icon);

  g_object_get (cell, "pixbuf", &pixbuf, NULL);
  if (pixbuf) {
    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);
    g_object_unref (pixbuf);
  }

  x_offset = (cell_area->width - w)/2 + 10;
  y_offset = (cell_area->height - h)/2 + 9;

  if (direction == GTK_TEXT_DIR_RTL)
    x_offset += xpad;
  else
    x_offset = cell_area->width - icon_size - xpad - x_offset;

  rectangle->x = cell_area->x + x_offset;
  rectangle->y = cell_area->y + ypad + y_offset;
  rectangle->width = rectangle->height = icon_size;
}

static void
ephy_removable_pixbuf_renderer_render (GtkCellRenderer      *cell,
				       cairo_t              *cr,
				       GtkWidget            *widget,
				       const GdkRectangle   *background_area,
				       const GdkRectangle   *cell_area,
				       GtkCellRendererState  flags)
{
  GtkStyleContext *context;
  EphyRemovablePixbufRenderer *self = EPHY_REMOVABLE_PIXBUF_RENDERER (cell);
  GdkRectangle icon_area;

  GTK_CELL_RENDERER_CLASS (ephy_removable_pixbuf_renderer_parent_class)->render
    (cell, cr, widget,
     background_area, cell_area, flags);

  if (self->priv->policy ==  EPHY_REMOVABLE_PIXBUF_RENDER_NEVER ||
      (self->priv->policy == EPHY_REMOVABLE_PIXBUF_RENDER_PRELIT && !(flags & GTK_CELL_RENDERER_PRELIT)))
    return;

  get_icon_rectangle (widget, cell, cell_area, self->priv->close_icon, &icon_area);
  context = gtk_widget_get_style_context (widget);
  gtk_render_icon (context, cr, self->priv->close_icon, icon_area.x, icon_area.y);
}

static gboolean
ephy_removable_pixbuf_renderer_activate (GtkCellRenderer      *cell,
					 GdkEvent             *event,
					 GtkWidget            *widget,
					 const gchar          *path,
					 const GdkRectangle   *background_area,
					 const GdkRectangle   *cell_area,
					 GtkCellRendererState  flags)
{
  GdkRectangle icon_area;
  GdkEventButton *ev = (GdkEventButton *) gtk_get_current_event();
  EphyRemovablePixbufRendererPrivate *priv = EPHY_REMOVABLE_PIXBUF_RENDERER (cell)->priv;

  if (priv->policy ==  EPHY_REMOVABLE_PIXBUF_RENDER_NEVER)
    return FALSE;

  get_icon_rectangle (widget, cell, cell_area, priv->close_icon, &icon_area);
  if (icon_area.x <= ev->x && ev->x <= icon_area.x + icon_area.width &&
      icon_area.y <= ev->y && ev->y <= icon_area.y + icon_area.height) {
    g_signal_emit (cell, signals [DELETE_CLICKED], 0, path);
    return TRUE;
  }

  return FALSE;
}

static void
ephy_removable_pixbuf_renderer_get_property (GObject    *object,
					     guint       property_id,
					     GValue     *value,
					     GParamSpec *pspec)
{
  EphyRemovablePixbufRenderer *self = EPHY_REMOVABLE_PIXBUF_RENDERER (object);

  switch (property_id)
    {
    case PROP_RENDER_POLICY:
      g_value_set_enum (value, self->priv->policy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ephy_removable_pixbuf_renderer_set_property (GObject    *object,
					     guint       property_id,
					     const GValue *value,
					     GParamSpec *pspec)
{
  EphyRemovablePixbufRenderer *self = EPHY_REMOVABLE_PIXBUF_RENDERER (object);

  switch (property_id)
    {
    case PROP_RENDER_POLICY:
      self->priv->policy = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
ephy_removable_pixbuf_renderer_dispose (GObject     *object)
{
  EphyRemovablePixbufRendererPrivate *priv = EPHY_REMOVABLE_PIXBUF_RENDERER (object)->priv;

  if (priv->close_icon)
    g_clear_object (&priv->close_icon);

  G_OBJECT_CLASS (ephy_removable_pixbuf_renderer_parent_class)->dispose (object);
}

static void
ephy_removable_pixbuf_renderer_class_init (EphyRemovablePixbufRendererClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *crclass = GTK_CELL_RENDERER_CLASS (klass);

  crclass->render = ephy_removable_pixbuf_renderer_render;
  crclass->activate = ephy_removable_pixbuf_renderer_activate;
  oclass->get_property = ephy_removable_pixbuf_renderer_get_property;
  oclass->set_property = ephy_removable_pixbuf_renderer_set_property;
  oclass->dispose = ephy_removable_pixbuf_renderer_dispose;

  g_object_class_install_property (oclass,
				   PROP_RENDER_POLICY,
				   g_param_spec_enum ("render-policy",
						      "Render policy",
						      "The rendering policy for the close icon in the renderer",
						      EPHY_TYPE_REMOVABLE_PIXBUF_RENDER_POLICY,
						      EPHY_REMOVABLE_PIXBUF_RENDER_PRELIT,
						      G_PARAM_CONSTRUCT |
						      G_PARAM_READWRITE |
						      G_PARAM_STATIC_STRINGS));

  signals[DELETE_CLICKED] =
    g_signal_new ("delete-clicked",
		  G_OBJECT_CLASS_TYPE (oclass),
		  G_SIGNAL_RUN_LAST,
		  0, NULL, NULL, NULL,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);

  g_type_class_add_private (klass, sizeof (EphyRemovablePixbufRendererPrivate));
}

static void
ephy_removable_pixbuf_renderer_init (EphyRemovablePixbufRenderer *self)
{
  GtkIconTheme *icon_theme;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPHY_TYPE_REMOVABLE_PIXBUF_RENDERER,
                                            EphyRemovablePixbufRendererPrivate);
  g_object_set (self, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
  icon_theme = gtk_icon_theme_get_default ();
  self->priv->close_icon = gtk_icon_theme_load_icon (icon_theme,
						     "window-close-symbolic",
						     24, 0, NULL);
}

GtkCellRenderer *
ephy_removable_pixbuf_renderer_new (void)
{
  return g_object_new (EPHY_TYPE_REMOVABLE_PIXBUF_RENDERER, NULL);
}

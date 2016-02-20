/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "ephy-downloads-progress-icon.h"

#include "ephy-downloads-manager.h"
#include "ephy-embed-shell.h"

struct _EphyDownloadsProgressIcon
{
  GtkDrawingArea parent_instance;

  GtkWidget *downloads_box;
};

G_DEFINE_TYPE (EphyDownloadsProgressIcon, ephy_downloads_progress_icon, GTK_TYPE_DRAWING_AREA)

static gboolean
ephy_downloads_progress_icon_draw (GtkWidget *widget,
                                   cairo_t   *cr)
{
  gint width, height;
  EphyDownloadsManager *manager;
  GtkStyleContext *style_context;
  GdkRGBA color;
  gdouble progress;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());
  progress = ephy_downloads_manager_get_estimated_progress (manager);

  style_context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (style_context, gtk_widget_get_state_flags (widget), &color);
  color.alpha *= progress == 1 ? 1 : 0.2;

  gdk_cairo_set_source_rgba (cr, &color);
  cairo_move_to (cr, width / 4., 0);
  cairo_line_to (cr, width - (width / 4.), 0);
  cairo_line_to (cr, width - (width / 4.), height / 2.);
  cairo_line_to (cr, width, height / 2.);
  cairo_line_to (cr, width / 2., height);
  cairo_line_to (cr, 0, height / 2.);
  cairo_line_to (cr, width / 4., height / 2.);
  cairo_line_to (cr, width / 4., 0);
  cairo_fill_preserve (cr);

  if (progress > 0 && progress < 1) {
    cairo_clip (cr);
    color.alpha = 1;
    gdk_cairo_set_source_rgba (cr, &color);
    cairo_rectangle (cr, 0, 0, width, height * progress);
    cairo_fill (cr);
  }

  return TRUE;
}

static void
ephy_downloads_progress_icon_class_init (EphyDownloadsProgressIconClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->draw = ephy_downloads_progress_icon_draw;
}

static void
ephy_downloads_progress_icon_init (EphyDownloadsProgressIcon *icon)
{
  g_object_set (icon, "width-request", 16, "height-request", 16, NULL);
  gtk_widget_set_valign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);
  gtk_widget_set_halign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);
}

GtkWidget *ephy_downloads_progress_icon_new (void)
{
  return GTK_WIDGET (g_object_new (EPHY_TYPE_DOWNLOADS_PROGRESS_ICON, NULL));
}

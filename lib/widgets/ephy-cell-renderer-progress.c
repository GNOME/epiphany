/* gtkcellrendererprogress.c
 * Copyright (C) 2002 Naba Kumar <kh_naba@users.sourceforge.net>
 * heavily modified by Jörgen Scheibengruber <mfcn@gmx.de>
 * heavily modified by Marco Pesenti Gritti <marco@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* KEEP IN SYNC with the original in gtk+ (HEAD branch) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "ephy-cell-renderer-progress.h"
#include <glib/gi18n.h>

#define EPHY_CELL_RENDERER_PROGRESS_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),                        \
                                                                                     EPHY_TYPE_CELL_RENDERER_PROGRESS, \
                                                                                     EphyCellRendererProgressPrivate))

enum
{
	PROP_0,
	PROP_VALUE,
	PROP_TEXT
}; 

struct _EphyCellRendererProgressPrivate
{
  gint value;
  gchar *text;
  gchar *label;
  gint min_h;
  gint min_w;
};

static void ephy_cell_renderer_progress_finalize     (GObject                 *object);
static void ephy_cell_renderer_progress_get_property (GObject                 *object,
						     guint                    param_id,
						     GValue                  *value,
						     GParamSpec              *pspec);
static void ephy_cell_renderer_progress_set_property (GObject                 *object,
						     guint                    param_id,
						     const GValue            *value,
						     GParamSpec              *pspec);
static void ephy_cell_renderer_progress_set_value    (EphyCellRendererProgress *cellprogress,
						     gint                     value);
static void ephy_cell_renderer_progress_set_text     (EphyCellRendererProgress *cellprogress,
						     const gchar              *text);
static void compute_dimensions                      (GtkCellRenderer         *cell,
                                                     GtkWidget               *widget,
						     const gchar             *text,
						     gint                    *width,
						     gint                    *height);
static void ephy_cell_renderer_progress_get_size     (GtkCellRenderer         *cell,
						     GtkWidget               *widget,
						     GdkRectangle            *cell_area,
						     gint                    *x_offset,
						     gint                    *y_offset,
						     gint                    *width,
						     gint                    *height);
static void ephy_cell_renderer_progress_render       (GtkCellRenderer         *cell,
						     GdkWindow               *window,
						     GtkWidget               *widget,
						     GdkRectangle            *background_area,
						     GdkRectangle            *cell_area,
						     GdkRectangle            *expose_area,
						     GtkCellRendererState     flags);

     
G_DEFINE_TYPE (EphyCellRendererProgress, ephy_cell_renderer_progress, GTK_TYPE_CELL_RENDERER);

static void
ephy_cell_renderer_progress_class_init (EphyCellRendererProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);
  
  object_class->finalize = ephy_cell_renderer_progress_finalize;
  object_class->get_property = ephy_cell_renderer_progress_get_property;
  object_class->set_property = ephy_cell_renderer_progress_set_property;
  
  cell_class->get_size = ephy_cell_renderer_progress_get_size;
  cell_class->render = ephy_cell_renderer_progress_render;
  
  /**
   * EphyCellRendererProgress:value:
   * 
   * The "value" property determines the percentage to which the
   * progress bar will be "filled in". 
   *
   * Since: 2.6
   **/
  g_object_class_install_property (object_class,
				   PROP_VALUE,
				   g_param_spec_int ("value",
						     "Value",
						     "Value of the progress bar",
						     -2, 100, 0,
						     G_PARAM_READWRITE));

  /**
   * EphyCellRendererProgress:text:
   * 
   * The "text" property determines the label which will be drawn
   * over the progress bar. Setting this property to %NULL causes the default 
   * label to be displayed. Setting this property to an empty string causes 
   * no label to be displayed.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (object_class,
				   PROP_TEXT,
				   g_param_spec_string ("text",
							"Text",
							"Text on the progress bar",
							NULL,
							G_PARAM_READWRITE));

  g_type_class_add_private (object_class, 
			    sizeof (EphyCellRendererProgressPrivate));
}

static void
ephy_cell_renderer_progress_init (EphyCellRendererProgress *cellprogress)
{
  cellprogress->priv = EPHY_CELL_RENDERER_PROGRESS_GET_PRIVATE (cellprogress);
  cellprogress->priv->value = 0;
  cellprogress->priv->text = NULL;
  cellprogress->priv->label = NULL;
  cellprogress->priv->min_w = -1;

  GTK_CELL_RENDERER (cellprogress)->xpad = 4;
  GTK_CELL_RENDERER (cellprogress)->ypad = 8;
}


/**
 * ephy_cell_renderer_progress_new:
 * 
 * Creates a new #EphyCellRendererProgress. 
 *
 * Return value: the new cell renderer
 *
 * Since: 2.6
 **/
GtkCellRenderer*
ephy_cell_renderer_progress_new (void)
{
  return GTK_CELL_RENDERER (g_object_new (EPHY_TYPE_CELL_RENDERER_PROGRESS, NULL));
}

static void
ephy_cell_renderer_progress_finalize (GObject *object)
{
  EphyCellRendererProgress *cellprogress = EPHY_CELL_RENDERER_PROGRESS (object);
  
  g_free (cellprogress->priv->text);
  g_free (cellprogress->priv->label);
  
  G_OBJECT_CLASS (ephy_cell_renderer_progress_parent_class)->finalize (object);
}

static void
ephy_cell_renderer_progress_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec)
{
  EphyCellRendererProgress *cellprogress = EPHY_CELL_RENDERER_PROGRESS (object);
  
  switch (param_id)
    {
    case PROP_VALUE:
      g_value_set_int (value, cellprogress->priv->value);
      break;
    case PROP_TEXT:
      g_value_set_string (value, cellprogress->priv->text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
ephy_cell_renderer_progress_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
  EphyCellRendererProgress *cellprogress = EPHY_CELL_RENDERER_PROGRESS (object);
  
  switch (param_id)
    {
    case PROP_VALUE:
      ephy_cell_renderer_progress_set_value (cellprogress, 
					    g_value_get_int (value));
      break;
    case PROP_TEXT:
      ephy_cell_renderer_progress_set_text (cellprogress,
					   g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
ephy_cell_renderer_progress_set_value (EphyCellRendererProgress *cellprogress, 
				      gint                     value)
{
  gchar *text;
  
  cellprogress->priv->value = value;

  if (cellprogress->priv->text)
    text = g_strdup (cellprogress->priv->text);
  else if (cellprogress->priv->value == EPHY_PROGRESS_CELL_FAILED)
    /* Translator hint: this is a label on progress bars inside a tree view. 
     */
    text = g_strdup (_("Failed"));
  else if (cellprogress->priv->value == EPHY_PROGRESS_CELL_UNKNOWN)
    /* Translator hint: this is a label on progress bars inside a tree view. 
     */
    text = g_strdup (_("Unknown"));
  else
    /* Translator hint: this is the default label on progress bars
     * inside a tree view. %d will be replaced by the percentage 
     */
    text = g_strdup_printf (_("%d %%"), cellprogress->priv->value);
  
  g_free (cellprogress->priv->label);
  cellprogress->priv->label = text;
}

static void
ephy_cell_renderer_progress_set_text (EphyCellRendererProgress *cellprogress, 
				     const gchar               *text)
{
  gchar *new_text;

  new_text = g_strdup (text);
  g_free (cellprogress->priv->text);
  cellprogress->priv->text = new_text;

  /* Update the label */
  ephy_cell_renderer_progress_set_value (cellprogress, cellprogress->priv->value);
}

static void
compute_dimensions (GtkCellRenderer *cell,
		    GtkWidget       *widget, 
		    const gchar     *text, 
		    gint            *width, 
		    gint            *height)
{
  PangoRectangle logical_rect;
  PangoLayout *layout;
  
  layout = gtk_widget_create_pango_layout (widget, text);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  
  if (width)
    *width = logical_rect.width + cell->xpad * 2 + widget->style->xthickness * 2;
  
  if (height)
    *height = logical_rect.height + cell->ypad * 2 + widget->style->ythickness * 2;
  
  g_object_unref (G_OBJECT (layout));
}

static void
ephy_cell_renderer_progress_get_size (GtkCellRenderer *cell,
				     GtkWidget       *widget,
				     GdkRectangle    *cell_area,
				     gint            *x_offset,
				     gint            *y_offset,
				     gint            *width,
				     gint            *height)
{
  EphyCellRendererProgress *cellprogress = EPHY_CELL_RENDERER_PROGRESS (cell);
  gint w, h;
  
  if (cellprogress->priv->min_w < 0)
    compute_dimensions (cell, widget, _("Unknown"),
			&cellprogress->priv->min_w,
			&cellprogress->priv->min_h);
  
  compute_dimensions (cell, widget, cellprogress->priv->label, &w, &h);
  
  if (width)
      *width = MAX (cellprogress->priv->min_w, w);
  
  if (height)
    *height = MIN (cellprogress->priv->min_h, h);
}

static void
ephy_cell_renderer_progress_render (GtkCellRenderer *cell,
				   GdkWindow       *window,
				   GtkWidget       *widget,
				   GdkRectangle    *background_area,
				   GdkRectangle    *cell_area,
				   GdkRectangle    *expose_area,
				   GtkCellRendererState flags)
{
  EphyCellRendererProgress *cellprogress = EPHY_CELL_RENDERER_PROGRESS (cell);
  GdkGC *gc;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint x, y, w, h, perc_w, pos;
  GdkRectangle clip;
  gboolean is_rtl;

  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  
  gc = gdk_gc_new (window);
  
  x = cell_area->x + cell->xpad;
  y = cell_area->y + cell->ypad;
  
  w = cell_area->width - cell->xpad * 2;
  h = cell_area->height - cell->ypad * 2;
  
  gdk_gc_set_rgb_fg_color (gc, &widget->style->fg[GTK_STATE_NORMAL]);
  gdk_draw_rectangle (window, gc, TRUE, x, y, w, h);
  
  x += widget->style->xthickness;
  y += widget->style->ythickness;
  w -= widget->style->xthickness * 2;
  h -= widget->style->ythickness * 2;
  gdk_gc_set_rgb_fg_color (gc, &widget->style->bg[GTK_STATE_NORMAL]);
  gdk_draw_rectangle (window, gc, TRUE, x, y, w, h);
  
  gdk_gc_set_rgb_fg_color (gc, &widget->style->bg[GTK_STATE_SELECTED]);
  perc_w = w * MAX (0, cellprogress->priv->value) / 100;
  gdk_draw_rectangle (window, gc, TRUE, is_rtl ? (x + w - perc_w) : x, y, perc_w, h);
  
  layout = gtk_widget_create_pango_layout (widget, cellprogress->priv->label);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  
  pos = (w - logical_rect.width)/2;
  
  clip.x = x;
  clip.y = y;
  clip.width = is_rtl ? w - perc_w : perc_w;
  clip.height = h; 

  gtk_paint_layout (widget->style, window, 
		    is_rtl ? GTK_STATE_NORMAL : GTK_STATE_SELECTED,
		    FALSE, &clip, widget, "progressbar",
		    x + pos, y + (h - logical_rect.height)/2,
		    layout);

  clip.x = clip.x + clip.width;
  clip.width = w - clip.width;

  gtk_paint_layout (widget->style, window, 
		    is_rtl ?  GTK_STATE_SELECTED : GTK_STATE_NORMAL,
		    FALSE, &clip, widget, "progressbar",
		    x + pos, y + (h - logical_rect.height)/2,
		    layout);
  
  g_object_unref (G_OBJECT (layout));
  g_object_unref (G_OBJECT (gc));
}


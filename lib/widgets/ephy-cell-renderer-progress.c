/* gtkcellrenderer.c
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

#include <config.h>
#include <stdlib.h>
#include <bonobo/bonobo-i18n.h>

#include "ephy-cell-renderer-progress.h"

static void ephy_cell_renderer_progress_init       (EphyCellRendererProgress *celltext);
static void ephy_cell_renderer_progress_class_init (EphyCellRendererProgressClass *class);

#define EPHY_CELL_RENDERER_PROGRESS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_CELL_RENDERER_PROGRESS, EphyCellRendererProgressPrivate))

#define XPAD 4
#define YPAD 2
#define UNKNOWN_STRING _("Unknown")

enum
{
	PROP_0,
	PROP_VALUE
}; 

struct _EphyCellRendererProgressPrivate
{
	int value;
	char *text;
	int min_h;
	int min_w;
};

static gpointer parent_class;

GtkType
ephy_cell_renderer_progress_get_type (void)
{
	static GtkType cell_progress_type = 0;

	if (!cell_progress_type)
	{
		static const GTypeInfo cell_progress_info =
		{
			sizeof (EphyCellRendererProgressClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) ephy_cell_renderer_progress_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (EphyCellRendererProgress),
			0,              /* n_preallocs */
			(GInstanceInitFunc) ephy_cell_renderer_progress_init,
		};
		cell_progress_type = g_type_register_static (GTK_TYPE_CELL_RENDERER,
                                               "EphyCellRendererProgress",
                                               &cell_progress_info, 0);
	}

	return cell_progress_type;
}

static void
ephy_cell_renderer_progress_init (EphyCellRendererProgress *cellprogress)
{
	cellprogress->priv = EPHY_CELL_RENDERER_PROGRESS_GET_PRIVATE (cellprogress);
	cellprogress->priv->value = 0;
	cellprogress->priv->text = NULL;
	cellprogress->priv->min_w = -1;
}

static void
ephy_cell_renderer_progress_set_value (EphyCellRendererProgress *cellprogress, int value)
{
	char *text;

	cellprogress->priv->value = value;

	if (cellprogress->priv->value == EPHY_PROGRESS_CELL_FAILED)
	{
		text = g_strdup (_("Failed"));
	}
	else if (cellprogress->priv->value == EPHY_PROGRESS_CELL_UNKNOWN)
	{
		text = g_strdup (UNKNOWN_STRING);
	}
	else
	{
		text = g_strdup_printf ("%d %%", cellprogress->priv->value);
	}

	g_free (cellprogress->priv->text);
	cellprogress->priv->text = text;
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
			ephy_cell_renderer_progress_set_value
				(cellprogress, g_value_get_int (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
	g_object_notify (object, "value");
}

static void
compute_dimensions (GtkWidget *widget, const char *text, int *width, int *height)
{
	PangoRectangle logical_rect;
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout (widget, text);
	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

	if (width)
	{
		*width = logical_rect.width + XPAD * 2 +
			 widget->style->xthickness * 2;
	}

	if (height)
	{
		*height = logical_rect.height + YPAD * 2 +
			  widget->style->ythickness * 2;
	}

	g_object_unref (G_OBJECT (layout));
}

static void
ephy_cell_renderer_progress_get_size (GtkCellRenderer *cell,
				      GtkWidget *widget,
				      GdkRectangle *cell_area,
				      gint *x_offset,
				      gint *y_offset,
				      gint *width,
				      gint *height)
{
	EphyCellRendererProgress *cellprogress = EPHY_CELL_RENDERER_PROGRESS (cell);
	int w, h;

	if (cellprogress->priv->min_w < 0)
	{
		compute_dimensions (widget, UNKNOWN_STRING,
				    &cellprogress->priv->min_w,
				    &cellprogress->priv->min_h);
	}

	compute_dimensions (widget, cellprogress->priv->text, &w, &h);

	if (width)
	{
		*width = MAX (cellprogress->priv->min_w, w);
	}

	if (height)
	{
		*height = MIN (cellprogress->priv->min_h, h);
	}
}

GtkCellRenderer*
ephy_cell_renderer_progress_new (void)
{
	return GTK_CELL_RENDERER (g_object_new (ephy_cell_renderer_progress_get_type (), NULL));
}

static void
ephy_cell_renderer_progress_render (GtkCellRenderer *cell,
			            GdkWindow *window,
			            GtkWidget *widget,
			            GdkRectangle *background_area,
			            GdkRectangle *cell_area,
			            GdkRectangle *expose_area,
			            guint flags)
{
	EphyCellRendererProgress *cellprogress = (EphyCellRendererProgress *) cell;
	GdkGC *gc;
	PangoLayout *layout;
	PangoRectangle logical_rect;
	GtkStateType state;
	int x, y, w, h, perc_w, pos;

	gc = gdk_gc_new (window);

	x = cell_area->x + XPAD;
	y = cell_area->y + YPAD;

	w = cell_area->width - XPAD * 2;
	h = cell_area->height - YPAD * 2;

	gdk_gc_set_rgb_fg_color (gc, &widget->style->fg[GTK_STATE_NORMAL]);
	gdk_draw_rectangle (window, gc, TRUE, x, y, w, h);

	x += widget->style->xthickness;
	y += widget->style->ythickness;
	w -= widget->style->xthickness * 2;
	h -= widget->style->ythickness * 2;
	gdk_gc_set_rgb_fg_color (gc, &widget->style->bg[GTK_STATE_NORMAL]);
	gdk_draw_rectangle (window, gc, TRUE, x, y, w, h);

	gdk_gc_set_rgb_fg_color (gc, &widget->style->bg[GTK_STATE_SELECTED]);
	perc_w = w * cellprogress->priv->value / 100;
	gdk_draw_rectangle (window, gc, TRUE, x, y, perc_w, h);

	layout = gtk_widget_create_pango_layout (widget, cellprogress->priv->text);
	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

	pos = (w - logical_rect.width)/2;

	if (perc_w < pos + logical_rect.width/2)
	{
		state = GTK_STATE_NORMAL;
	}
	else
	{
		state = GTK_STATE_SELECTED;
	}

	gtk_paint_layout (widget->style, window, state,
			  FALSE, cell_area, widget, "progressbar",
                          x + pos, y + (h - logical_rect.height)/2,
                          layout);

	g_object_unref (G_OBJECT (layout));
	g_object_unref (G_OBJECT (gc));
}

static void
ephy_cell_renderer_progress_finalize (GObject *object)
{
        EphyCellRendererProgress *cellprogress = EPHY_CELL_RENDERER_PROGRESS (object);

	g_free (cellprogress->priv->text);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_cell_renderer_progress_class_init (EphyCellRendererProgressClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
  
        object_class->finalize = ephy_cell_renderer_progress_finalize;
	object_class->get_property = ephy_cell_renderer_progress_get_property;
	object_class->set_property = ephy_cell_renderer_progress_set_property;

	cell_class->get_size = ephy_cell_renderer_progress_get_size;
	cell_class->render = ephy_cell_renderer_progress_render;
  
	g_object_class_install_property (object_class,
                                         PROP_VALUE,
                                         g_param_spec_int ("value",
                                                           "Value",
                                                           "Value of the progress bar.",
                                                           -2, 100, 0,
                                                           G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EphyCellRendererProgressPrivate));
}

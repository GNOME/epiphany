/* 
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2002 Marco Pesenti Gritti
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * Ephy port by Marco Pesenti Gritti <marco@it.gnome.org>
 * 
 * This is the spinner (for busy feedback) for the location bar
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-spinner.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkicontheme.h>

#define spinner_DEFAULT_TIMEOUT 100	/* Milliseconds Per Frame */

#define EPHY_SPINNER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SPINNER, EphySpinnerDetails))

struct EphySpinnerDetails
{
	GList	*image_list;
	GdkPixbuf *quiescent_pixbuf;
	GtkIconTheme *icon_theme;

	int	max_frame;
	int	delay;
	int	current_frame;
	guint	timer_task;

	gboolean ready;
	gboolean small_mode;
};

static void ephy_spinner_class_init		(EphySpinnerClass *class);
static void ephy_spinner_init			(EphySpinner *spinner);
static void ephy_spinner_load_images		(EphySpinner *spinner);
static void ephy_spinner_unload_images		(EphySpinner *spinner);
static void ephy_spinner_remove_update_callback	(EphySpinner *spinner);
static void ephy_spinner_theme_changed		(GtkIconTheme *icon_theme,
						 EphySpinner *spinner);

static GObjectClass *parent_class = NULL;

GType
ephy_spinner_get_type (void)
{
        static GType type = 0;

        if (type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphySpinnerClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_spinner_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphySpinner),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_spinner_init
                };

                type = g_type_register_static (GTK_TYPE_EVENT_BOX,
                                               "EphySpinner",
                                               &our_info, 0);
        }

        return type;
}

/*
 * ephy_spinner_new:
 *
 * Create a new #EphySpinner. The spinner is a widget
 * that gives the user feedback about network status with
 * an animated image.
 *
 * Return Value: the spinner #GtkWidget
 **/
GtkWidget *
ephy_spinner_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_SPINNER, NULL));
}

static gboolean
is_throbbing (EphySpinner *spinner)
{
	return spinner->details->timer_task != 0;
}

/* loop through all the images taking their union to compute the width and height of the spinner */
static void
get_spinner_dimensions (EphySpinner *spinner, int *spinner_width, int* spinner_height)
{
	int current_width, current_height;
	int pixbuf_width, pixbuf_height;
	GList *image_list;
	GdkPixbuf *pixbuf;

	current_width = 0;
	current_height = 0;

	if (spinner->details->quiescent_pixbuf != NULL)
	{
		/* start with the quiescent image */
		current_width = gdk_pixbuf_get_width (spinner->details->quiescent_pixbuf);
		current_height = gdk_pixbuf_get_height (spinner->details->quiescent_pixbuf);
	}

	/* loop through all the installed images, taking the union */
	image_list = spinner->details->image_list;
	while (image_list != NULL)
	{
		pixbuf = GDK_PIXBUF (image_list->data);
		pixbuf_width = gdk_pixbuf_get_width (pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (pixbuf);

		if (pixbuf_width > current_width)
		{
			current_width = pixbuf_width;
		}

		if (pixbuf_height > current_height)
		{
			current_height = pixbuf_height;
		}

		image_list = image_list->next;
	}

	/* return the result */
	*spinner_width = current_width;
	*spinner_height = current_height;
}

static void
ephy_spinner_init (EphySpinner *spinner)
{
	GtkWidget *widget = GTK_WIDGET (spinner);

	GTK_WIDGET_UNSET_FLAGS (spinner, GTK_NO_WINDOW);

	gtk_widget_set_events (widget,
			       gtk_widget_get_events (widget)
			       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

	spinner->details = EPHY_SPINNER_GET_PRIVATE (spinner);

	spinner->details->delay = spinner_DEFAULT_TIMEOUT;
	/* FIXME: icon theme is per-screen, not global */
	spinner->details->icon_theme = gtk_icon_theme_get_default ();
	g_signal_connect_object (spinner->details->icon_theme,
				 "changed",
				 G_CALLBACK (ephy_spinner_theme_changed),
				 spinner, 0);

	spinner->details->quiescent_pixbuf = NULL;
	spinner->details->image_list = NULL;
	spinner->details->max_frame = 0;
	spinner->details->current_frame = 0;
	spinner->details->timer_task = 0;

	ephy_spinner_load_images (spinner);
	gtk_widget_show (widget);
}

/* handler for handling theme changes */
static void
ephy_spinner_theme_changed (GtkIconTheme *icon_theme, EphySpinner *spinner)
{
	gtk_widget_hide (GTK_WIDGET (spinner));
	ephy_spinner_load_images (spinner);
	gtk_widget_show (GTK_WIDGET (spinner));
	gtk_widget_queue_resize ( GTK_WIDGET (spinner));
}

/* here's the routine that selects the image to draw, based on the spinner's state */

static GdkPixbuf *
select_spinner_image (EphySpinner *spinner)
{
	GList *element;

	if (spinner->details->timer_task == 0)
	{
		if (spinner->details->quiescent_pixbuf)
		{
			return g_object_ref (spinner->details->quiescent_pixbuf);
		}
		else
		{
			return NULL;
		}
	}

	if (spinner->details->image_list == NULL)
	{
		return NULL;
	}

	element = g_list_nth (spinner->details->image_list, spinner->details->current_frame);
	g_return_val_if_fail (element != NULL, NULL);

	return g_object_ref (element->data);
}

/* handle expose events */

static int
ephy_spinner_expose (GtkWidget *widget, GdkEventExpose *event)
{
	EphySpinner *spinner;
	GdkPixbuf *pixbuf;
	GdkGC *gc;
	int x_offset, y_offset, width, height;
	GdkRectangle pix_area, dest;

	g_return_val_if_fail (EPHY_IS_SPINNER (widget), FALSE);

	spinner = EPHY_SPINNER (widget);

	if (!GTK_WIDGET_DRAWABLE (spinner)) return TRUE;

	pixbuf = select_spinner_image (spinner);
	if (pixbuf == NULL)
	{
		return FALSE;
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* Compute the offsets for the image centered on our allocation */
	x_offset = (widget->allocation.width - width) / 2;
	y_offset = (widget->allocation.height - height) / 2;

	pix_area.x = x_offset;
	pix_area.y = y_offset;
	pix_area.width = width;
	pix_area.height = height;

	if (!gdk_rectangle_intersect (&event->area, &pix_area, &dest))
	{
		g_object_unref (pixbuf);
		return FALSE;
	}

	gc = gdk_gc_new (widget->window);
	gdk_draw_pixbuf (widget->window, gc, pixbuf,
			 dest.x - x_offset, dest.y - y_offset,
			 dest.x, dest.y,
			 dest.width, dest.height,
			 GDK_RGB_DITHER_MAX, 0, 0);
	g_object_unref (gc);

	g_object_unref (pixbuf);

	return FALSE;
}

/* here's the actual timeout task to bump the frame and schedule a redraw */

static gboolean
bump_spinner_frame (gpointer callback_data)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (callback_data);

	if (!GTK_WIDGET_DRAWABLE (spinner))
	{
		return TRUE;
	}

	spinner->details->current_frame += 1;
	if (spinner->details->current_frame > spinner->details->max_frame - 1)
	{
		spinner->details->current_frame = 0;
	}

	gtk_widget_queue_draw (GTK_WIDGET (spinner));
	return TRUE;
}

/**
 * ephy_spinner_start:
 * @spinner: a #EphySpinner
 *
 * Start the spinner animation.
 **/
void
ephy_spinner_start (EphySpinner *spinner)
{
	if (is_throbbing (spinner))
	{
		return;
	}

	if (spinner->details->timer_task != 0)
	{
		g_source_remove (spinner->details->timer_task);
	}

	/* reset the frame count */
	spinner->details->current_frame = 0;
	spinner->details->timer_task = g_timeout_add (spinner->details->delay,
						      bump_spinner_frame,
						      spinner);
}

static void
ephy_spinner_remove_update_callback (EphySpinner *spinner)
{
	if (spinner->details->timer_task != 0)
	{
		g_source_remove (spinner->details->timer_task);
	}

	spinner->details->timer_task = 0;
}

/**
 * ephy_spinner_stop:
 * @spinner: a #EphySpinner
 *
 * Stop the spinner animation.
 **/
void
ephy_spinner_stop (EphySpinner *spinner)
{
	if (!is_throbbing (spinner))
	{
		return;
	}

	ephy_spinner_remove_update_callback (spinner);
	gtk_widget_queue_draw (GTK_WIDGET (spinner));

}

/* routines to load the images used to draw the spinner */

/* unload all the images, and the list itself */

static void
ephy_spinner_unload_images (EphySpinner *spinner)
{
	GList *current_entry;

	if (spinner->details->quiescent_pixbuf != NULL)
	{
		g_object_unref (spinner->details->quiescent_pixbuf);
		spinner->details->quiescent_pixbuf = NULL;
	}

	/* unref all the images in the list, and then let go of the list itself */
	current_entry = spinner->details->image_list;
	while (current_entry != NULL)
	{
		g_object_unref (current_entry->data);
		current_entry = current_entry->next;
	}

	g_list_free (spinner->details->image_list);
	spinner->details->image_list = NULL;

	spinner->details->current_frame = 0;
}

static GdkPixbuf *
scale_to_real_size (EphySpinner *spinner, GdkPixbuf *pixbuf)
{
	GdkPixbuf *result;
	int size;

	size = gdk_pixbuf_get_height (pixbuf);

	if (spinner->details->small_mode)
	{
		result = gdk_pixbuf_scale_simple (pixbuf,
						  size * 2 / 3,
						  size * 2 / 3,
						  GDK_INTERP_BILINEAR);
	}
	else
	{
		result = g_object_ref (pixbuf);
	}

	return result;
}

static GdkPixbuf *
extract_frame (EphySpinner *spinner, GdkPixbuf *grid_pixbuf, int x, int y, int size)
{
	GdkPixbuf *pixbuf, *result;

	if (x + size > gdk_pixbuf_get_width (grid_pixbuf) ||
	    y + size > gdk_pixbuf_get_height (grid_pixbuf))
	{
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_subpixbuf (grid_pixbuf,
					   x, y,
					   size, size);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	result = scale_to_real_size (spinner, pixbuf);
	g_object_unref (pixbuf);

	return result;
}

/* load all of the images of the spinner sequentially */
static void
ephy_spinner_load_images (EphySpinner *spinner)
{
	int grid_width, grid_height, x, y, size;
	const char *icon;
	GdkPixbuf *icon_pixbuf, *pixbuf;
	GList *image_list;
	GtkIconInfo *icon_info;

	ephy_spinner_unload_images (spinner);

	/* Load the animation */
	icon_info = gtk_icon_theme_lookup_icon (spinner->details->icon_theme,
					        "gnome-spinner", -1, 0);
	if (icon_info == NULL)
	{
		g_warning ("Throbber animation not found");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	grid_width = gdk_pixbuf_get_width (icon_pixbuf);
	grid_height = gdk_pixbuf_get_height (icon_pixbuf);

	image_list = NULL;
	for (y = 0; y < grid_height; y += size)
	{
		for (x = 0; x < grid_width ; x += size)
		{
			pixbuf = extract_frame (spinner, icon_pixbuf, x, y, size);

			if (pixbuf)
			{
				image_list = g_list_prepend (image_list, pixbuf);
			}
			else
			{
				g_warning ("Cannot extract frame from the grid");
			}
		}
	}
	spinner->details->image_list = g_list_reverse (image_list);
	spinner->details->max_frame = g_list_length (spinner->details->image_list);

	gtk_icon_info_free (icon_info);
	g_object_unref (icon_pixbuf);

	/* Load the rest icon */
	icon_info = gtk_icon_theme_lookup_icon (spinner->details->icon_theme,
					        "gnome-spinner-rest", -1, 0);
	if (icon_info == NULL)
	{
		g_warning ("Throbber rest icon not found");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	spinner->details->quiescent_pixbuf = scale_to_real_size (spinner, icon_pixbuf);

	g_object_unref (icon_pixbuf);
	gtk_icon_info_free (icon_info);
}

/*
 * ephy_spinner_set_small_mode:
 * @spinner: a #EphySpinner
 * @new_mode: pass true to enable the small mode, false to disable
 *
 * Set the size mode of the spinner. We need a small mode to deal
 * with only icons toolbars.
 **/
void
ephy_spinner_set_small_mode (EphySpinner *spinner, gboolean new_mode)
{
	if (new_mode != spinner->details->small_mode)
	{
		spinner->details->small_mode = new_mode;
		ephy_spinner_load_images (spinner);

		gtk_widget_queue_resize (GTK_WIDGET (spinner));
	}
}

/* handle setting the size */

static void
ephy_spinner_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	int spinner_width, spinner_height;
	EphySpinner *spinner = EPHY_SPINNER (widget);

	get_spinner_dimensions (spinner, &spinner_width, &spinner_height);

	/* allocate some extra margin so we don't butt up against toolbar edges */
	requisition->width = spinner_width + 4;
	requisition->height = spinner_height + 4;
}

static void
ephy_spinner_finalize (GObject *object)
{
	EphySpinner *spinner = EPHY_SPINNER (object);

	ephy_spinner_remove_update_callback (spinner);
	ephy_spinner_unload_images (spinner);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_spinner_class_init (EphySpinnerClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = ephy_spinner_finalize;

	widget_class->expose_event = ephy_spinner_expose;
	widget_class->size_request = ephy_spinner_size_request;

	g_type_class_add_private (object_class, sizeof (EphySpinnerDetails));
}

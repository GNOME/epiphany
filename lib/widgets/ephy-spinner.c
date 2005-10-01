/* 
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2002-2004 Marco Pesenti Gritti
 * Copyright (C) 2004 Christian Persch
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
 * $Id$
 */

#include "config.h"

#include "ephy-spinner.h"
#include "ephy-debug.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtksettings.h>

/* Spinner cache implementation */

#define EPHY_TYPE_SPINNER_CACHE			(ephy_spinner_cache_get_type())
#define EPHY_SPINNER_CACHE(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_SPINNER_CACHE, EphySpinnerCache))
#define EPHY_SPINNER_CACHE_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_SPINNER_CACHE, EphySpinnerCacheClass))
#define EPHY_IS_SPINNER_CACHE(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_SPINNER_CACHE))
#define EPHY_IS_SPINNER_CACHE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_SPINNER_CACHE))
#define EPHY_SPINNER_CACHE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_SPINNER_CACHE, EphySpinnerCacheClass))

typedef struct _EphySpinnerCache	EphySpinnerCache;
typedef struct _EphySpinnerCacheClass	EphySpinnerCacheClass;
typedef struct _EphySpinnerCachePrivate	EphySpinnerCachePrivate;
typedef struct _EphySpinnerImages	EphySpinnerImages;

struct _EphySpinnerImages
{
	GtkIconSize size;
	int width;
	int height;
	GdkPixbuf *quiescent_pixbuf;
	GList *images;
};

struct _EphySpinnerCacheClass
{
	GObjectClass parent_class;
};

struct _EphySpinnerCache
{
	GObject parent_object;

	/*< private >*/
	EphySpinnerCachePrivate *priv;
};

#define EPHY_SPINNER_CACHE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SPINNER_CACHE, EphySpinnerCachePrivate))

struct _EphySpinnerCachePrivate
{
	GtkIconTheme *icon_theme;
	EphySpinnerImages *originals;
	/* List of EphySpinnerImages scaled to different sizes */
	GList *images;
};

static void ephy_spinner_cache_class_init (EphySpinnerCacheClass *klass);
static void ephy_spinner_cache_init	  (EphySpinnerCache *cache);

static GObjectClass *cache_parent_class = NULL;

static GType
ephy_spinner_cache_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphySpinnerCacheClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_spinner_cache_class_init,
			NULL,
			NULL,
			sizeof (EphySpinnerCache),
			0,
			(GInstanceInitFunc) ephy_spinner_cache_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphySpinnerCache",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_spinner_images_free (EphySpinnerImages *images)
{
	if (images != NULL)
	{
		g_list_foreach (images->images, (GFunc) g_object_unref, NULL);
		g_object_unref (images->quiescent_pixbuf);

		g_free (images);
	}
}

static EphySpinnerImages *
ephy_spinner_images_copy (EphySpinnerImages *images)
{
	EphySpinnerImages *copy = g_new (EphySpinnerImages, 1);

	copy->size = images->size;
	copy->width = images->width;
	copy->height = images->height;

	copy->quiescent_pixbuf = g_object_ref (images->quiescent_pixbuf);
	copy->images = g_list_copy (images->images);
	g_list_foreach (copy->images, (GFunc) g_object_ref, NULL);

	return copy;
}

static void
ephy_spinner_cache_unload (EphySpinnerCache *cache)
{
	g_list_foreach (cache->priv->images, (GFunc) ephy_spinner_images_free, NULL);
	cache->priv->images = NULL;
	cache->priv->originals = NULL;
}

static GdkPixbuf *
extract_frame (GdkPixbuf *grid_pixbuf,
	       int x,
	       int y,
	       int size)
{
	GdkPixbuf *pixbuf;

	if (x + size > gdk_pixbuf_get_width (grid_pixbuf) ||
	    y + size > gdk_pixbuf_get_height (grid_pixbuf))
	{
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_subpixbuf (grid_pixbuf,
					   x, y,
					   size, size);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	return pixbuf;
}

static void
ephy_spinner_cache_load (EphySpinnerCache *cache)
{
	EphySpinnerImages *images;
	GdkPixbuf *icon_pixbuf, *pixbuf;
	GtkIconInfo *icon_info;
	int grid_width, grid_height, x, y, size, h, w;
	const char *icon;

	LOG ("EphySpinnerCache loading");

	ephy_spinner_cache_unload (cache);

	START_PROFILER ("loading spinner animation")

	/* Load the animation */
	icon_info = gtk_icon_theme_lookup_icon (cache->priv->icon_theme,
						"gnome-spinner", -1, 0);
	if (icon_info == NULL)
	{
		STOP_PROFILER ("loading spinner animation")

		g_warning ("Throbber animation not found\n");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	if (icon_pixbuf == NULL)
	{
		STOP_PROFILER ("loading spinner animation")

		g_warning ("Could not load the spinner file\n");
		gtk_icon_info_free (icon_info);
		return;
	}

	grid_width = gdk_pixbuf_get_width (icon_pixbuf);
	grid_height = gdk_pixbuf_get_height (icon_pixbuf);

	images = g_new (EphySpinnerImages, 1);
	cache->priv->images = g_list_prepend (NULL, images);
	cache->priv->originals = images;

	images->size = GTK_ICON_SIZE_INVALID;
	images->width = images->height = size;
	images->images = NULL;
	images->quiescent_pixbuf = NULL;

	for (y = 0; y < grid_height; y += size)
	{
		for (x = 0; x < grid_width ; x += size)
		{
			pixbuf = extract_frame (icon_pixbuf, x, y, size);

			if (pixbuf)
			{
				images->images =
					g_list_prepend (images->images, pixbuf);
			}
			else
			{
				g_warning ("Cannot extract frame from the grid\n");
			}
		}
	}
	images->images = g_list_reverse (images->images);

	gtk_icon_info_free (icon_info);
	g_object_unref (icon_pixbuf);

	/* Load the rest icon */
	icon_info = gtk_icon_theme_lookup_icon (cache->priv->icon_theme,
						"gnome-spinner-rest", -1, 0);
	if (icon_info == NULL)
	{
		STOP_PROFILER ("loading spinner animation")

		g_warning ("Throbber rest icon not found\n");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	gtk_icon_info_free (icon_info);

	if (icon_pixbuf == NULL)
	{
		STOP_PROFILER ("loading spinner animation")

		g_warning ("Could not load spinner rest icon\n");
		ephy_spinner_images_free (images);
		return;
	}

	images->quiescent_pixbuf = icon_pixbuf;

	w = gdk_pixbuf_get_width (icon_pixbuf);
	h = gdk_pixbuf_get_height (icon_pixbuf);
	images->width = MAX (images->width, w);
	images->height = MAX (images->height, h);

	STOP_PROFILER ("loading spinner animation")
}

static int
compare_size (gconstpointer images_ptr,
	      gconstpointer size_ptr)
{
	const EphySpinnerImages *images = (const EphySpinnerImages *) images_ptr;
	GtkIconSize size = GPOINTER_TO_INT (size_ptr);

	if (images->size == size)
	{
		return 0;
	}

	return -1;
}

static GdkPixbuf *
scale_to_size (GdkPixbuf *pixbuf,
	       int dw,
	       int dh)
{
	GdkPixbuf *result;
	int pw, ph;

	pw = gdk_pixbuf_get_width (pixbuf);
	ph = gdk_pixbuf_get_height (pixbuf);

	if (pw != dw || ph != dh)
	{
		result = gdk_pixbuf_scale_simple (pixbuf, dw, dh,
						  GDK_INTERP_BILINEAR);
	}
	else
	{
		result = g_object_ref (pixbuf);
	}

	return result;
}

static EphySpinnerImages *
ephy_spinner_cache_get_images (EphySpinnerCache *cache,
			       GtkIconSize size)
{
	EphySpinnerImages *images;
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	GList *element, *l;
	int h, w;

	LOG ("Getting animation images at size %d", size);

	if (cache->priv->images == NULL || cache->priv->originals == NULL)
	{
		return NULL;
	}

	element = g_list_find_custom (cache->priv->images,
				      GINT_TO_POINTER (size),
				      (GCompareFunc) compare_size);
	if (element != NULL)
	{
		return ephy_spinner_images_copy ((EphySpinnerImages *) element->data);
	}

	if (!gtk_icon_size_lookup_for_settings (gtk_settings_get_default (), size, &w, &h))
	{
		g_warning ("Failed to lookup icon size\n");
		return NULL;
	}

	images = g_new (EphySpinnerImages, 1);
	images->size = size;
	images->width = w;
	images->height = h;
	images->images = NULL;

	START_PROFILER ("scaling spinner animation")

	for (l = cache->priv->originals->images; l != NULL; l = l->next)
	{
		pixbuf = (GdkPixbuf *) l->data;
		scaled_pixbuf = scale_to_size (pixbuf, w, h);

		images->images = g_list_prepend (images->images, scaled_pixbuf);
	}
	images->images = g_list_reverse (images->images);

	images->quiescent_pixbuf =
		scale_to_size (cache->priv->originals->quiescent_pixbuf, w, h);

	/* store in cache */
	cache->priv->images = g_list_prepend (cache->priv->images, images);

	STOP_PROFILER ("scaling spinner animation")

	return ephy_spinner_images_copy (images);
}

static void
ephy_spinner_cache_init (EphySpinnerCache *cache)
{
	cache->priv = EPHY_SPINNER_CACHE_GET_PRIVATE (cache);

	LOG ("EphySpinnerCache initialising");

	/* FIXME: icon theme is per-screen, not global */
	cache->priv->icon_theme = gtk_icon_theme_get_default ();
	g_signal_connect_swapped (cache->priv->icon_theme, "changed",
				  G_CALLBACK (ephy_spinner_cache_load), cache);

	ephy_spinner_cache_load (cache);
}

static void
ephy_spinner_cache_finalize (GObject *object)
{
	EphySpinnerCache *cache = EPHY_SPINNER_CACHE (object); 

	LOG ("EphySpinnerCache finalising");

	g_signal_handlers_disconnect_by_func
		(cache->priv->icon_theme, G_CALLBACK(ephy_spinner_cache_load), cache);

	ephy_spinner_cache_unload (cache);

	G_OBJECT_CLASS (cache_parent_class)->finalize (object);
}

static void
ephy_spinner_cache_class_init (EphySpinnerCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	cache_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_spinner_cache_finalize;

	g_type_class_add_private (object_class, sizeof (EphySpinnerCachePrivate));
}

static EphySpinnerCache *spinner_cache = NULL;

static EphySpinnerCache *
ephy_spinner_cache_ref (void)
{
	if (spinner_cache == NULL)
	{
		EphySpinnerCache **cache_ptr;

		spinner_cache = g_object_new (EPHY_TYPE_SPINNER_CACHE, NULL);
		cache_ptr = &spinner_cache;
		g_object_add_weak_pointer (G_OBJECT (spinner_cache),
					   (gpointer *) cache_ptr);

		return spinner_cache;
	}
	else
	{
		return g_object_ref (spinner_cache);
	}
}

/* Spinner implementation */

#define SPINNER_TIMEOUT 125 /* ms */

#define EPHY_SPINNER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SPINNER, EphySpinnerDetails))

struct _EphySpinnerDetails
{
	GtkIconTheme *icon_theme;
	EphySpinnerCache *cache;
	GtkIconSize size;
	EphySpinnerImages *images;
	GList *current_image;
	guint timer_task;
	guint spinning : 1;
};

static void ephy_spinner_class_init (EphySpinnerClass *class);
static void ephy_spinner_init	    (EphySpinner *spinner);

static GObjectClass *parent_class = NULL;

GType
ephy_spinner_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
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

static gboolean
ephy_spinner_load_images (EphySpinner *spinner)
{
	EphySpinnerDetails *details = spinner->details;

	if (details->images == NULL)
	{
		START_PROFILER ("ephy_spinner_load_images")

		details->images =
			ephy_spinner_cache_get_images (details->cache, details->size);

		if (details->images != NULL)
		{
			details->current_image = details->images->images;
		}

		STOP_PROFILER ("ephy_spinner_load_images")
	}

	return details->images != NULL;
}

static void
ephy_spinner_unload_images (EphySpinner *spinner)
{
	ephy_spinner_images_free (spinner->details->images);
	spinner->details->images = NULL;
	spinner->details->current_image = NULL;
}

static void
icon_theme_changed_cb (GtkIconTheme *icon_theme,
		       EphySpinner *spinner)
{
	ephy_spinner_unload_images (spinner);
	gtk_widget_queue_resize (GTK_WIDGET (spinner));
}

static void
ephy_spinner_init (EphySpinner *spinner)
{
	GtkWidget *widget = GTK_WIDGET (spinner);

	gtk_widget_set_events (widget,
			       gtk_widget_get_events (widget)
			       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

	spinner->details = EPHY_SPINNER_GET_PRIVATE (spinner);

	spinner->details->cache = ephy_spinner_cache_ref ();
	spinner->details->size = GTK_ICON_SIZE_INVALID;
	spinner->details->spinning = FALSE;

	/* FIXME: multihead */
	spinner->details->icon_theme = gtk_icon_theme_get_default ();
	g_signal_connect (spinner->details->icon_theme, "changed",
			  G_CALLBACK (icon_theme_changed_cb), spinner);

}

static GdkPixbuf *
select_spinner_image (EphySpinner *spinner)
{
	EphySpinnerDetails *details = spinner->details;
	EphySpinnerImages *images = details->images;

	g_return_val_if_fail (images != NULL, NULL);

	if (spinner->details->timer_task == 0)
	{
		if (images->quiescent_pixbuf != NULL)
		{
			return g_object_ref (details->images->quiescent_pixbuf);
		}

		return NULL;
	}

	g_return_val_if_fail (details->current_image != NULL, NULL);

	return g_object_ref (details->current_image->data);
}

static int
ephy_spinner_expose (GtkWidget *widget,
		     GdkEventExpose *event)
{
	EphySpinner *spinner = EPHY_SPINNER (widget);
	GdkPixbuf *pixbuf;
	GdkGC *gc;
	int x_offset, y_offset, width, height;
	GdkRectangle pix_area, dest;

	if (!GTK_WIDGET_DRAWABLE (spinner))
	{
		return TRUE;
	}

	if (!ephy_spinner_load_images (spinner))
	{
		return TRUE;
	}

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

	pix_area.x = x_offset + widget->allocation.x;
	pix_area.y = y_offset + widget->allocation.y;
	pix_area.width = width;
	pix_area.height = height;

	if (!gdk_rectangle_intersect (&event->area, &pix_area, &dest))
	{
		g_object_unref (pixbuf);
		return FALSE;
	}

	gc = gdk_gc_new (widget->window);
	gdk_draw_pixbuf (widget->window, gc, pixbuf,
			 dest.x - x_offset - widget->allocation.x,
			 dest.y - y_offset - widget->allocation.y,
			 dest.x, dest.y,
			 dest.width, dest.height,
			 GDK_RGB_DITHER_MAX, 0, 0);
	g_object_unref (gc);

	g_object_unref (pixbuf);

	return FALSE;
}

static gboolean
bump_spinner_frame_cb (EphySpinner *spinner)
{
	GList *frame;

	if (!GTK_WIDGET_DRAWABLE (spinner))
	{
		return TRUE;
	}

	frame = spinner->details->current_image;

	if (g_list_next (frame) != NULL)
	{
		frame = frame->next;
	}
	else
	{
		frame = g_list_first (frame);
	}

	spinner->details->current_image = frame;

	gtk_widget_queue_draw (GTK_WIDGET (spinner));

	/* run again */
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
	EphySpinnerDetails *details = spinner->details;

	details->spinning = TRUE;

	if (GTK_WIDGET_MAPPED (GTK_WIDGET (spinner)) &&
	    details->timer_task == 0)
	{

		if (spinner->details->images != NULL)
		{
			/* reset to first frame */
			spinner->details->current_image =
				spinner->details->images->images;
		}

		spinner->details->timer_task =
			g_timeout_add (SPINNER_TIMEOUT,
				       (GSourceFunc) bump_spinner_frame_cb,
				       spinner);
	}
}

static void
ephy_spinner_remove_update_callback (EphySpinner *spinner)
{
	if (spinner->details->timer_task != 0)
	{
		g_source_remove (spinner->details->timer_task);
		spinner->details->timer_task = 0;
	}
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
	EphySpinnerDetails *details = spinner->details;

	details->spinning = FALSE;

	if (spinner->details->timer_task != 0)
	{
		ephy_spinner_remove_update_callback (spinner);

		if (GTK_WIDGET_MAPPED (GTK_WIDGET (spinner)))
		{
			gtk_widget_queue_draw (GTK_WIDGET (spinner));
		}
	}
}

/*
 * ephy_spinner_set_size:
 * @spinner: a #EphySpinner
 * @size: the size of type %GtkIconSize
 *
 * Set the size of the spinner. Use %GTK_ICON_SIZE_INVALID to use the
 * native size of the icons.
 **/
void
ephy_spinner_set_size (EphySpinner *spinner,
		       GtkIconSize size)
{
	if (size != spinner->details->size)
	{
		ephy_spinner_unload_images (spinner);

		spinner->details->size = size;

		gtk_widget_queue_resize (GTK_WIDGET (spinner));
	}
}

static void
ephy_spinner_size_request (GtkWidget *widget,
			   GtkRequisition *requisition)
{
	EphySpinner *spinner = EPHY_SPINNER (widget);

	if (!ephy_spinner_load_images (spinner))
	{
		requisition->width = requisition->height = 0;
		gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (widget),
						   spinner->details->size,
						   &requisition->width,
					           &requisition->height);
		return;
	}

	requisition->width = spinner->details->images->width;
	requisition->height = spinner->details->images->height;

	/* allocate some extra margin so we don't butt up against toolbar edges */
	if (spinner->details->size != GTK_ICON_SIZE_MENU)
	{
		requisition->width += 4;
		requisition->height += 4;
	}
}

static void
ephy_spinner_map (GtkWidget *widget)
{
	EphySpinner *spinner = EPHY_SPINNER (widget);
	EphySpinnerDetails *details = spinner->details;

	GTK_WIDGET_CLASS (parent_class)->map (widget);

	if (details->spinning)
	{
		ephy_spinner_start (spinner);
	}
}

static void
ephy_spinner_unmap (GtkWidget *widget)
{
	EphySpinner *spinner = EPHY_SPINNER (widget);

	ephy_spinner_remove_update_callback (spinner);

	GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
ephy_spinner_finalize (GObject *object)
{
	EphySpinner *spinner = EPHY_SPINNER (object);

	g_signal_handlers_disconnect_by_func
		(spinner->details->icon_theme,
		 G_CALLBACK (icon_theme_changed_cb), spinner);

	ephy_spinner_remove_update_callback (spinner);
	ephy_spinner_unload_images (spinner);

	g_object_unref (spinner->details->cache);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_spinner_class_init (EphySpinnerClass *class)
{
	GObjectClass *object_class =  G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = ephy_spinner_finalize;

	widget_class->expose_event = ephy_spinner_expose;
	widget_class->size_request = ephy_spinner_size_request;
	widget_class->map = ephy_spinner_map;
	widget_class->unmap = ephy_spinner_unmap;

	g_type_class_add_private (object_class, sizeof (EphySpinnerDetails));
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
	return GTK_WIDGET (g_object_new (EPHY_TYPE_SPINNER,
					 "visible-window", FALSE,
					 NULL));
}

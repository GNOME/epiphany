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

#include "config.h"
#include "ephy-spinner.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-string.h"
#include "ephy-file-helpers.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-util.h>
#include <math.h>
#include <libgnomevfs/gnome-vfs.h>

#define spinner_DEFAULT_TIMEOUT 100	/* Milliseconds Per Frame */

struct EphySpinnerDetails {
	GList	*image_list;

	GdkPixbuf *quiescent_pixbuf;

	int	max_frame;
	int	delay;
	int	current_frame;
	guint	timer_task;

	gboolean ready;
	gboolean small_mode;

	gboolean button_in;
	gboolean button_down;

	gint theme_notif;
};

static void ephy_spinner_class_init (EphySpinnerClass *class);
static void ephy_spinner_init (EphySpinner *spinner);
static void ephy_spinner_load_images            (EphySpinner *spinner);
static void ephy_spinner_unload_images          (EphySpinner *spinner);
static void ephy_spinner_remove_update_callback (EphySpinner *spinner);


static GList *spinner_directories = NULL;

static void
ephy_spinner_init_directory_list (void);
static void
ephy_spinner_search_directory (const gchar *base, GList **spinner_list);
static EphySpinnerInfo *
ephy_spinner_get_theme_info (const gchar *base, const gchar *theme_name);
static gchar *
ephy_spinner_get_theme_path (const gchar *theme_name);


static GObjectClass *parent_class = NULL;

GType
ephy_spinner_get_type (void)
{
        static GType ephy_spinner_type = 0;

        if (ephy_spinner_type == 0)
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

                ephy_spinner_type = g_type_register_static (GTK_TYPE_EVENT_BOX,
                                                              "EphySpinner",
                                                              &our_info, 0);

		ephy_spinner_init_directory_list ();
        }

        return ephy_spinner_type;

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
        GtkWidget *s;

        s = GTK_WIDGET (g_object_new (EPHY_SPINNER_TYPE, NULL));

        return s;
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
	GList *current_entry;
	GdkPixbuf *pixbuf;

	/* start with the quiescent image */
	current_width = gdk_pixbuf_get_width (spinner->details->quiescent_pixbuf);
	current_height = gdk_pixbuf_get_height (spinner->details->quiescent_pixbuf);

	/* loop through all the installed images, taking the union */
	current_entry = spinner->details->image_list;
	while (current_entry != NULL) {
		pixbuf = GDK_PIXBUF (current_entry->data);
		pixbuf_width = gdk_pixbuf_get_width (pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (pixbuf);

		if (pixbuf_width > current_width) {
			current_width = pixbuf_width;
		}

		if (pixbuf_height > current_height) {
			current_height = pixbuf_height;
		}

		current_entry = current_entry->next;
	}

	/* return the result */
	*spinner_width = current_width;
	*spinner_height = current_height;
}

/* handler for handling theme changes */
static void
ephy_spinner_theme_changed (GConfClient *client,
                              guint cnxn_id,
                              GConfEntry *entry,
			      gpointer user_data)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (user_data);
	gtk_widget_hide (GTK_WIDGET (spinner));
	ephy_spinner_load_images (spinner);
	gtk_widget_show (GTK_WIDGET (spinner));
	gtk_widget_queue_resize ( GTK_WIDGET (spinner));
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

	spinner->details = g_new0 (EphySpinnerDetails, 1);

	spinner->details->delay = spinner_DEFAULT_TIMEOUT;

	ephy_spinner_load_images (spinner);
	gtk_widget_show (widget);

	spinner->details->theme_notif =
		eel_gconf_notification_add (CONF_TOOLBAR_SPINNER_THEME,
					    (GConfClientNotifyFunc)
					     ephy_spinner_theme_changed,
					     spinner);
}

/* here's the routine that selects the image to draw, based on the spinner's state */

static GdkPixbuf *
select_spinner_image (EphySpinner *spinner)
{
	GList *element;

	if (spinner->details->timer_task == 0) {
		return g_object_ref (spinner->details->quiescent_pixbuf);
	}

	if (spinner->details->image_list == NULL) {
		return NULL;
	}

	element = g_list_nth (spinner->details->image_list, spinner->details->current_frame);

	return g_object_ref (element->data);
}

static guchar
lighten_component (guchar cur_value)
{
        int new_value = cur_value;
        new_value += 24 + (new_value >> 3);
        if (new_value > 255) {
                new_value = 255;
        }
        return (guchar) new_value;
}

static GdkPixbuf *
create_new_pixbuf (GdkPixbuf *src)
{
        g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
        g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
                               && gdk_pixbuf_get_n_channels (src) == 3)
                              || (gdk_pixbuf_get_has_alpha (src)
                                  && gdk_pixbuf_get_n_channels (src) == 4), NULL);

        return gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
                               gdk_pixbuf_get_has_alpha (src),
                               gdk_pixbuf_get_bits_per_sample (src),
                               gdk_pixbuf_get_width (src),
                               gdk_pixbuf_get_height (src));
}

static GdkPixbuf *
eel_create_darkened_pixbuf (GdkPixbuf *src, int saturation, int darken)
{
        gint i, j;
        gint width, height, src_row_stride, dest_row_stride;
        gboolean has_alpha;
        guchar *target_pixels, *original_pixels;
        guchar *pixsrc, *pixdest;
        guchar intensity;
        guchar alpha;
        guchar negalpha;
        guchar r, g, b;
        GdkPixbuf *dest;

        g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
        g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
                               && gdk_pixbuf_get_n_channels (src) == 3)
                              || (gdk_pixbuf_get_has_alpha (src)
                                  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
        g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

        dest = create_new_pixbuf (src);

        has_alpha = gdk_pixbuf_get_has_alpha (src);
        width = gdk_pixbuf_get_width (src);
        height = gdk_pixbuf_get_height (src);
        dest_row_stride = gdk_pixbuf_get_rowstride (dest);
        src_row_stride = gdk_pixbuf_get_rowstride (src);
        target_pixels = gdk_pixbuf_get_pixels (dest);
        original_pixels = gdk_pixbuf_get_pixels (src);

        for (i = 0; i < height; i++) {
                pixdest = target_pixels + i * dest_row_stride;
                pixsrc = original_pixels + i * src_row_stride;
                for (j = 0; j < width; j++) {
                        r = *pixsrc++;
                        g = *pixsrc++;
                        b = *pixsrc++;
                        intensity = (r * 77 + g * 150 + b * 28) >> 8;
                        negalpha = ((255 - saturation) * darken) >> 8;
                        alpha = (saturation * darken) >> 8;
                        *pixdest++ = (negalpha * intensity + alpha * r) >> 8;
                        *pixdest++ = (negalpha * intensity + alpha * g) >> 8;
                        *pixdest++ = (negalpha * intensity + alpha * b) >> 8;
                        if (has_alpha) {
                                *pixdest++ = *pixsrc++;
                        }
                }
        }
        return dest;
}

static GdkPixbuf *
eel_create_spotlight_pixbuf (GdkPixbuf* src)
{
        GdkPixbuf *dest;
        int i, j;
        int width, height, has_alpha, src_row_stride, dst_row_stride;
        guchar *target_pixels, *original_pixels;
        guchar *pixsrc, *pixdest;

        g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
        g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
                               && gdk_pixbuf_get_n_channels (src) == 3)
                              || (gdk_pixbuf_get_has_alpha (src)
                                  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
        g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

        dest = create_new_pixbuf (src);

        has_alpha = gdk_pixbuf_get_has_alpha (src);
        width = gdk_pixbuf_get_width (src);
        height = gdk_pixbuf_get_height (src);
        dst_row_stride = gdk_pixbuf_get_rowstride (dest);
        src_row_stride = gdk_pixbuf_get_rowstride (src);
        target_pixels = gdk_pixbuf_get_pixels (dest);
        original_pixels = gdk_pixbuf_get_pixels (src);

        for (i = 0; i < height; i++) {
                pixdest = target_pixels + i * dst_row_stride;
                pixsrc = original_pixels + i * src_row_stride;
                for (j = 0; j < width; j++) {
                        *pixdest++ = lighten_component (*pixsrc++);
                        *pixdest++ = lighten_component (*pixsrc++);
                        *pixdest++ = lighten_component (*pixsrc++);
                        if (has_alpha) {
                                *pixdest++ = *pixsrc++;
                        }
                }
        }
        return dest;
}

/* handle expose events */

static int
ephy_spinner_expose (GtkWidget *widget, GdkEventExpose *event)
{
	EphySpinner *spinner;
	GdkPixbuf *pixbuf, *massaged_pixbuf;
	int x_offset, y_offset, width, height;
	GdkRectangle pix_area, dest;

	g_return_val_if_fail (IS_EPHY_SPINNER (widget), FALSE);

	spinner = EPHY_SPINNER (widget);
	if (!spinner->details->ready) {
		return FALSE;
	}

	pixbuf = select_spinner_image (spinner);
	if (pixbuf == NULL) {
		return FALSE;
	}

	/* Get the right tint on the image */

	if (spinner->details->button_in) {
		if (spinner->details->button_down) {
			massaged_pixbuf = eel_create_darkened_pixbuf (pixbuf, 0.8 * 255, 0.8 * 255);
		} else {
			massaged_pixbuf = eel_create_spotlight_pixbuf (pixbuf);
		}
		g_object_unref (pixbuf);
		pixbuf = massaged_pixbuf;
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

	if (!gdk_rectangle_intersect (&event->area, &pix_area, &dest)) {
		g_object_unref (pixbuf);
		return FALSE;
	}

	gdk_pixbuf_render_to_drawable_alpha (
		pixbuf, widget->window,
		dest.x - x_offset, dest.y - y_offset,
		dest.x, dest.y,
		dest.width, dest.height,
		GDK_PIXBUF_ALPHA_BILEVEL, 128,
		GDK_RGB_DITHER_MAX,
		0, 0);

	g_object_unref (pixbuf);

	return FALSE;
}

static void
ephy_spinner_map (GtkWidget *widget)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (widget);

	GTK_WIDGET_CLASS (parent_class)->map (widget);

	spinner->details->ready = TRUE;
}

/* here's the actual timeout task to bump the frame and schedule a redraw */

static gboolean
bump_spinner_frame (gpointer callback_data)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (callback_data);
	if (!spinner->details->ready) {
		return TRUE;
	}

	spinner->details->current_frame += 1;
	if (spinner->details->current_frame > spinner->details->max_frame - 1) {
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
	if (is_throbbing (spinner)) {
		return;
	}

	if (spinner->details->timer_task != 0) {
		gtk_timeout_remove (spinner->details->timer_task);
	}

	/* reset the frame count */
	spinner->details->current_frame = 0;
	spinner->details->timer_task = gtk_timeout_add (spinner->details->delay,
							 bump_spinner_frame,
							 spinner);
}

static void
ephy_spinner_remove_update_callback (EphySpinner *spinner)
{
	if (spinner->details->timer_task != 0) {
		gtk_timeout_remove (spinner->details->timer_task);
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
	if (!is_throbbing (spinner)) {
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

	if (spinner->details->quiescent_pixbuf != NULL) {
		g_object_unref (spinner->details->quiescent_pixbuf);
		spinner->details->quiescent_pixbuf = NULL;
	}

	/* unref all the images in the list, and then let go of the list itself */
	current_entry = spinner->details->image_list;
	while (current_entry != NULL) {
		g_object_unref (current_entry->data);
		current_entry = current_entry->next;
	}

	g_list_free (spinner->details->image_list);
	spinner->details->image_list = NULL;
}

static GdkPixbuf*
load_themed_image (const char *path, const char *file_name,
		   gboolean small_mode)
{
	GdkPixbuf *pixbuf, *temp_pixbuf;
	char *image_path;

	image_path = g_build_filename (path, file_name, NULL);

	if (!g_file_test(image_path, G_FILE_TEST_EXISTS))
        {
		g_free (image_path);
		return NULL;
	}

	if (image_path) {
		pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);

		if (small_mode && pixbuf) {
			temp_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
							       gdk_pixbuf_get_width (pixbuf) * 2 / 3,
							       gdk_pixbuf_get_height (pixbuf) * 2 / 3,
							       GDK_INTERP_BILINEAR);
			g_object_unref (pixbuf);
			pixbuf = temp_pixbuf;
		}

		g_free (image_path);

		return pixbuf;
	}
	return NULL;
}

/* utility to make the spinner frame name from the index */

static char *
make_spinner_frame_name (int index)
{
	return g_strdup_printf ("%03d.png", index);
}

/* load all of the images of the spinner sequentially */
static void
ephy_spinner_load_images (EphySpinner *spinner)
{
	int index;
	char *spinner_frame_name;
	GdkPixbuf *pixbuf, *qpixbuf;
	GList *image_list;
	char *image_theme;
	char *path;

	ephy_spinner_unload_images (spinner);

	image_theme = eel_gconf_get_string (CONF_TOOLBAR_SPINNER_THEME);

	path = ephy_spinner_get_theme_path (image_theme);
	g_return_if_fail (path != NULL);

	qpixbuf = load_themed_image (path, "rest.png",
			spinner->details->small_mode);

	g_return_if_fail (qpixbuf != NULL);
	spinner->details->quiescent_pixbuf = qpixbuf;

	spinner->details->max_frame = 50;

	image_list = NULL;
	for (index = 1; index <= spinner->details->max_frame; index++) {
		spinner_frame_name = make_spinner_frame_name (index);
		pixbuf = load_themed_image (path, spinner_frame_name,
					    spinner->details->small_mode);
		g_free (spinner_frame_name);
		if (pixbuf == NULL) {
			spinner->details->max_frame = index - 1;
			break;
		}
		image_list = g_list_prepend (image_list, pixbuf);
	}
	spinner->details->image_list = g_list_reverse (image_list);

	g_free (image_theme);
}

static gboolean
ephy_spinner_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (widget);

	if (!spinner->details->button_in) {
		spinner->details->button_in = TRUE;
		gtk_widget_queue_draw (widget);
	}

	return FALSE;
}

static gboolean
ephy_spinner_leave_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (widget);

	if (spinner->details->button_in) {
		spinner->details->button_in = FALSE;
		gtk_widget_queue_draw (widget);
	}

	return FALSE;
}

/* handle button presses by posting a change on the "location" property */

static gboolean
ephy_spinner_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (widget);

	if (event->button == 1) {
		spinner->details->button_down = TRUE;
		spinner->details->button_in = TRUE;
		gtk_widget_queue_draw (widget);
		return TRUE;
	}

	return FALSE;
}

static void
ephy_spinner_set_location (EphySpinner *spinner)
{
}

static gboolean
ephy_spinner_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (widget);

	if (event->button == 1) {
		if (spinner->details->button_in) {
			ephy_spinner_set_location (spinner);
		}
		spinner->details->button_down = FALSE;
		gtk_widget_queue_draw (widget);
		return TRUE;
	}

	return FALSE;
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
	if (new_mode != spinner->details->small_mode) {
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
	requisition->width = spinner_width + 8;
	requisition->height = spinner_height;
}

static void
ephy_spinner_finalize (GObject *object)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (object);

	ephy_spinner_remove_update_callback (spinner);
	ephy_spinner_unload_images (spinner);

	eel_gconf_notification_remove (spinner->details->theme_notif);

	g_free (spinner->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_spinner_class_init (EphySpinnerClass *class)
{
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	widget_class = GTK_WIDGET_CLASS (class);

	G_OBJECT_CLASS (class)->finalize = ephy_spinner_finalize;

	widget_class->expose_event = ephy_spinner_expose;
	widget_class->button_press_event = ephy_spinner_button_press_event;
	widget_class->button_release_event = ephy_spinner_button_release_event;
	widget_class->enter_notify_event = ephy_spinner_enter_notify_event;
	widget_class->leave_notify_event = ephy_spinner_leave_notify_event;
	widget_class->size_request = ephy_spinner_size_request;
	widget_class->map = ephy_spinner_map;
}

static void
ephy_spinner_search_directory  (const gchar *base, GList **spinner_list)
{
	GnomeVFSResult rc;
	GList *list, *node;

	rc = gnome_vfs_directory_list_load
		(&list, base, (GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
			       GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE |
			       GNOME_VFS_FILE_INFO_FOLLOW_LINKS));
	if (rc != GNOME_VFS_OK) return;

	for (node = list; node != NULL; node = g_list_next (node))
	{
		GnomeVFSFileInfo *file_info = node->data;
		EphySpinnerInfo *info;

		if (file_info->name[0] == '.')
			continue;
		if (file_info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
			continue;

		info = ephy_spinner_get_theme_info (base, file_info->name);
		if (info != NULL)
		{
			*spinner_list = g_list_append (*spinner_list, info);
		}
	}

	gnome_vfs_file_info_list_free (list);
}

static EphySpinnerInfo *
ephy_spinner_get_theme_info (const gchar *base, const gchar *theme_name)
{
	EphySpinnerInfo *info;
	gchar *path;
	gchar *icon;

	path = g_build_filename (base, theme_name, NULL);
	icon = g_build_filename (path, "rest.png", NULL);

	if (!g_file_test (icon, G_FILE_TEST_EXISTS))
	{
		g_free (path);
		g_free (icon);

		/* handle nautilus throbbers as well */

		path = g_build_filename (base, theme_name, "throbber", NULL);
		icon = g_build_filename (path, "rest.png", NULL);
	}

	if (!g_file_test (icon, G_FILE_TEST_EXISTS))
	{
		g_free (path);
		g_free (icon);

		return NULL;
	}

	info = g_new(EphySpinnerInfo, 1);
	info->name	= g_strdup (theme_name);
	info->directory	= path;
	info->filename	= icon;

	return info;
}

static void
ephy_spinner_init_directory_list (void)
{
	gchar *path;

	path = g_build_filename (g_get_home_dir (), ephy_dot_dir (), "spinners", NULL);
	spinner_directories = g_list_append (spinner_directories, path);

	path = g_build_filename (SHARE_DIR, "spinners", NULL);
	spinner_directories = g_list_append (spinner_directories, path);

	path = g_build_filename (SHARE_DIR, "..", "pixmaps", "nautilus", NULL);
	spinner_directories = g_list_append (spinner_directories, path);

#ifdef NAUTILUS_PREFIX
	path = g_build_filename (NAUTILUS_PREFIX, "share", "pixmaps", "nautilus", NULL);
	spinner_directories = g_list_append (spinner_directories, path);
#endif
}

GList *
ephy_spinner_list_spinners (void)
{
	GList *spinner_list = NULL;
	GList *tmp;

	for (tmp = spinner_directories; tmp != NULL; tmp = g_list_next (tmp))
	{
		gchar *path = tmp->data;
		ephy_spinner_search_directory (path, &spinner_list);
	}

	return spinner_list;
}

static gchar *
ephy_spinner_get_theme_path (const gchar *theme_name)
{
	EphySpinnerInfo *info;
	GList *tmp;

	for (tmp = spinner_directories; tmp != NULL; tmp = g_list_next (tmp))
	{
		gchar *path = tmp->data;

		info = ephy_spinner_get_theme_info (path, theme_name);
		if (info != NULL)
		{
			path = g_strdup (info->directory);
			ephy_spinner_info_free (info);
			return path;
		}
	}

	return NULL;
}

void
ephy_spinner_info_free (EphySpinnerInfo *info)
{
	g_free (info->name);
	g_free (info->directory);
	g_free (info->filename);
	g_free (info);
}

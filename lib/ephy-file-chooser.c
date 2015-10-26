/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-debug.h"
#include "ephy-settings.h"
#include "ephy-string.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

static void ephy_file_chooser_class_init	(EphyFileChooserClass *klass);
static void ephy_file_chooser_init		(EphyFileChooser *dialog);
static void ephy_file_chooser_image_preview	(GtkFileChooser *file_chooser, 
				 		 gpointer user_data);

#define PREVIEW_WIDTH 150
#define PREVIEW_HEIGHT 150

G_DEFINE_TYPE (EphyFileChooser, ephy_file_chooser, GTK_TYPE_FILE_CHOOSER_DIALOG)

static void
ephy_file_chooser_init (EphyFileChooser *dialog)
{
}

static GObject *
ephy_file_chooser_constructor (GType type,
			       guint n_construct_properties,
			       GObjectConstructParam *construct_params)

{
	GObject *object;
	char *downloads_dir;

	object = G_OBJECT_CLASS (ephy_file_chooser_parent_class)->constructor (type, n_construct_properties,
									       construct_params);

	downloads_dir = ephy_file_get_downloads_dir ();
	gtk_file_chooser_add_shortcut_folder
		(GTK_FILE_CHOOSER (object), downloads_dir, NULL);
	g_free (downloads_dir);

	return object;
}

GtkFileFilter *
ephy_file_chooser_add_pattern_filter (EphyFileChooser *dialog,
				      const char *title,
				      const char *first_pattern,
				      ...)
{
	GtkFileFilter *filth;
	va_list args;
	const char *pattern;

	filth = gtk_file_filter_new ();

	va_start (args, first_pattern);

	pattern = first_pattern;
	while (pattern != NULL)
	{
		gtk_file_filter_add_pattern (filth, pattern);
		pattern = va_arg (args, const char *);
	}
	va_end (args);

	gtk_file_filter_set_name (filth, title);

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filth);

	return filth;
}

GtkFileFilter *
ephy_file_chooser_add_mime_filter (EphyFileChooser *dialog,
				   const char *title,
				   const char *first_mimetype,
				   ...)
{
	GtkFileFilter *filth;
	va_list args;
	const char *mimetype;

	filth = gtk_file_filter_new ();

	va_start (args, first_mimetype);

	mimetype = first_mimetype;
	while (mimetype != NULL)
	{
		gtk_file_filter_add_mime_type (filth, mimetype);
		mimetype = va_arg (args, const char *);
	}
	va_end (args);

	gtk_file_filter_set_name (filth, title);

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filth);

	return filth;
}

static void
ephy_file_chooser_class_init (EphyFileChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructor = ephy_file_chooser_constructor;
}

static void
ephy_file_chooser_image_preview (GtkFileChooser *file_chooser, 
				 gpointer user_data)
{
	char *filename;
	GtkWidget *preview;
	GdkPixbuf *pixbuf;
	gboolean have_preview;
	
	pixbuf = NULL;
	preview = GTK_WIDGET (user_data);
	filename = gtk_file_chooser_get_preview_filename (file_chooser);
	
	if (filename)
		pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 
					PREVIEW_WIDTH, PREVIEW_HEIGHT, NULL);
	g_free (filename);
	
	have_preview = (pixbuf != NULL);
	gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
	
	if (pixbuf)
		g_object_unref (pixbuf);
		
	gtk_file_chooser_set_preview_widget_active (file_chooser, have_preview);
	
}

EphyFileChooser	*
ephy_file_chooser_new (const char *title,
		       GtkWidget *parent,
		       GtkFileChooserAction action,
		       EphyFileFilterDefault default_filter)
{
	EphyFileChooser *dialog;
	GtkFileFilter *filter[EPHY_FILE_FILTER_LAST];
	GtkWidget *preview;

	g_return_val_if_fail (default_filter >= 0 && default_filter <= EPHY_FILE_FILTER_LAST, NULL);

	dialog = EPHY_FILE_CHOOSER (g_object_new (EPHY_TYPE_FILE_CHOOSER,
						  "title", title,
						  "action", action,
						  NULL));

	if (action == GTK_FILE_CHOOSER_ACTION_OPEN	    ||
	    action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
	    action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	{
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("_Cancel"), GTK_RESPONSE_CANCEL,
					_("_Open"), GTK_RESPONSE_ACCEPT,
					NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_ACCEPT);
	}
	else if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("_Cancel"), GTK_RESPONSE_CANCEL,
					_("_Save"), GTK_RESPONSE_ACCEPT,
					NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_ACCEPT);
	}

	preview = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog), preview);
	gtk_file_chooser_set_preview_widget_active (GTK_FILE_CHOOSER (dialog), FALSE);
	g_signal_connect (dialog, "update-preview", G_CALLBACK (ephy_file_chooser_image_preview), preview);
    
	if (default_filter != EPHY_FILE_FILTER_NONE)
	{
		filter[EPHY_FILE_FILTER_ALL_SUPPORTED] =
			ephy_file_chooser_add_mime_filter
				(dialog,
				 _("All supported types"),
				 "text/html",
				 "application/xhtml+xml",
				 "text/xml",
				 "message/rfc822",            /* MHTML */
				 "multipart/related",         /* MHTML */
				 "application/x-mimearchive", /* MHTML */
				 "image/png",
				 "image/jpeg",
				 "image/gif",
				 NULL);

		filter[EPHY_FILE_FILTER_WEBPAGES] =
			ephy_file_chooser_add_mime_filter
				(dialog, _("Web pages"),
				 "text/html",
				 "application/xhtml+xml",
				 "text/xml",
				 "message/rfc822",            /* MHTML */
				 "multipart/related",         /* MHTML */
				 "application/x-mimearchive", /* MHTML */
				 NULL);

		filter[EPHY_FILE_FILTER_IMAGES] =
			ephy_file_chooser_add_mime_filter
				(dialog, _("Images"),
				 "image/png",
				 "image/jpeg",
				 "image/gif",
				 NULL);

		filter[EPHY_FILE_FILTER_ALL] =
			ephy_file_chooser_add_pattern_filter
				(dialog, _("All files"), "*", NULL);

		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog),
					     filter[default_filter]);
	}

	if (parent != NULL)
	{
		gtk_window_set_transient_for (GTK_WINDOW (dialog),
					      GTK_WINDOW (parent));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (parent)),
					     GTK_WINDOW (dialog));
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	}

	return dialog;
}

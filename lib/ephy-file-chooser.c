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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "eel-gconf-extensions.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "ephy-debug.h"
#include "ephy-stock-icons.h"

#include <gtk/gtkstock.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib/gi18n.h>

#define EPHY_FILE_CHOOSER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FILE_CHOOSER, EphyFileChooserPrivate))

struct _EphyFileChooserPrivate
{
	char *persist_key;
};

static void ephy_file_chooser_class_init	(EphyFileChooserClass *klass);
static void ephy_file_chooser_init		(EphyFileChooser *dialog);

enum
{
	PROP_0,
	PROP_PERSIST_KEY
};

static GObjectClass *parent_class = NULL;

GType
ephy_file_chooser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyFileChooserClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_file_chooser_class_init,
			NULL,
			NULL,
			sizeof (EphyFileChooser),
			0,
			(GInstanceInitFunc) ephy_file_chooser_init
		};

		type = g_type_register_static (GTK_TYPE_FILE_CHOOSER_DIALOG,
					       "EphyFileChooser",
					       &our_info, 0);
	}

	return type;
}

static void
current_folder_changed_cb (GtkFileChooser *chooser, EphyFileChooser *dialog)
{
	if (dialog->priv->persist_key != NULL)
	{
		char *dir;

		dir = gtk_file_chooser_get_current_folder (chooser);

		eel_gconf_set_path (dialog->priv->persist_key, dir);

		g_free (dir);
	}
}

static void
ephy_file_chooser_init (EphyFileChooser *dialog)
{
	dialog->priv = EPHY_FILE_CHOOSER_GET_PRIVATE (dialog);
}

static GObject *
ephy_file_chooser_constructor (GType type,
			       guint n_construct_properties,
			       GObjectConstructParam *construct_params)

{
	GObject *object;
	char *downloads_dir;

	object = parent_class->constructor (type, n_construct_properties,
					    construct_params);

	downloads_dir = ephy_file_get_downloads_dir ();
	gtk_file_chooser_add_shortcut_folder
		(GTK_FILE_CHOOSER (object), downloads_dir, NULL);
	g_free (downloads_dir);

	gtk_window_set_icon_name (GTK_WINDOW (object), EPHY_STOCK_EPHY);

	return object;
}

static void
ephy_file_chooser_finalize (GObject *object)
{
	EphyFileChooser *dialog = EPHY_FILE_CHOOSER (object);

	g_free (dialog->priv->persist_key);

	LOG ("EphyFileChooser finalised");

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
ephy_file_chooser_set_persist_key (EphyFileChooser *dialog, const char *key)
{
	char *dir, *expanded, *converted;

	g_return_if_fail (key != NULL && key[0] != '\0');

	dialog->priv->persist_key = g_strdup (key);

	dir = eel_gconf_get_string (key);
	if (dir != NULL)
	{
		converted = g_filename_from_utf8
			(dir, -1, NULL, NULL, NULL);

		if (converted != NULL)
		{
			expanded = gnome_vfs_expand_initial_tilde (converted);

			gtk_file_chooser_set_current_folder
				(GTK_FILE_CHOOSER (dialog), expanded);

			g_free (expanded);
			g_free (converted);
		}

		g_free (dir);
	}

	g_signal_connect (dialog, "current-folder-changed",
			  G_CALLBACK (current_folder_changed_cb), dialog);
}

const char *
ephy_file_chooser_get_persist_key (EphyFileChooser *dialog)
{
	g_return_val_if_fail (EPHY_IS_FILE_CHOOSER (dialog), NULL);

	return dialog->priv->persist_key;
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
ephy_file_chooser_set_property (GObject *object,
				guint prop_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EphyFileChooser *dialog = EPHY_FILE_CHOOSER (object);
	
	switch (prop_id)
	{
		case PROP_PERSIST_KEY:
			ephy_file_chooser_set_persist_key (dialog, g_value_get_string (value));
			break;
	}
}

static void
ephy_file_chooser_get_property (GObject *object,
				guint prop_id,
				GValue *value,
				GParamSpec *pspec)
{
	EphyFileChooser *dialog = EPHY_FILE_CHOOSER (object);

	switch (prop_id)
	{
		case PROP_PERSIST_KEY:
			g_value_set_string (value, ephy_file_chooser_get_persist_key (dialog));
			break;
	}
}

static void
ephy_file_chooser_class_init (EphyFileChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->constructor = ephy_file_chooser_constructor;
	object_class->finalize = ephy_file_chooser_finalize;
	object_class->get_property = ephy_file_chooser_get_property;
	object_class->set_property = ephy_file_chooser_set_property;

	g_object_class_install_property (object_class,
					 PROP_PERSIST_KEY,
					 g_param_spec_string ("persist-key",
							      "Persist Key",
							      "The gconf key to which to persist the selected directory",
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphyFileChooserPrivate));
}

EphyFileChooser	*
ephy_file_chooser_new (const char *title,
		       GtkWidget *parent,
		       GtkFileChooserAction action,
		       const char *persist_key,
		       EphyFileFilterDefault default_filter)
{
	EphyFileChooser *dialog;
	GtkFileFilter *filter[EPHY_FILE_FILTER_LAST];

	g_return_val_if_fail (default_filter >= 0 && default_filter <= EPHY_FILE_FILTER_LAST, NULL);

	dialog = EPHY_FILE_CHOOSER (g_object_new (EPHY_TYPE_FILE_CHOOSER,
						  "title", title,
						  "action", action,
						  NULL));

	/* NOTE: We cannot set this property on object construction time.
	 * This is because GtkFileChooserDialog overrides the gobject
	 * constructor; the GtkFileChooser delegate will only be set
	 * _after_ our instance_init and construct-param setters will have
	 * run.
	 */
	if (persist_key != NULL)
	{
		ephy_file_chooser_set_persist_key (dialog, persist_key);
	}

	if (action == GTK_FILE_CHOOSER_ACTION_OPEN	    ||
	    action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
	    action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
	{
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_ACCEPT);
	}
	else if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_ACCEPT);
	}

	if (default_filter != EPHY_FILE_FILTER_NONE)
	{
		filter[EPHY_FILE_FILTER_ALL_SUPPORTED] =
			ephy_file_chooser_add_mime_filter
				(dialog,
				 _("All supported types"),
				 "text/html",
				 "application/xhtml+xml",
				 "text/xml",
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
		gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (parent)),
					     GTK_WINDOW (dialog));
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	}

	return dialog;
}

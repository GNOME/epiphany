/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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
 *  $Id$
 */

#include "config.h"

#include "popup-commands.h"
#include "ephy-shell.h"
#include "ephy-embed-factory.h"
#include "ephy-embed-persist.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"
#include "ephy-bookmarks-ui.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtkclipboard.h>
#include <gtk/gtkmain.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

void
popup_cmd_link_in_new_window (GtkAction *action,
		              EphyWindow *window)
{
	EphyEmbedEvent *event;
	EphyTab *tab;
	const GValue *value;

	tab = ephy_window_get_active_tab (window);

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	value = ephy_embed_event_get_property (event, "link");

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void
popup_cmd_link_in_new_tab (GtkAction *action,
		           EphyWindow *window)
{
	EphyEmbedEvent *event;
	EphyTab *tab;
	const GValue *value;

	tab = ephy_window_get_active_tab (window);

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	value = ephy_embed_event_get_property (event, "link");

	ephy_shell_new_tab (ephy_shell, window, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
}

void
popup_cmd_bookmark_link (GtkAction *action,
			 EphyWindow *window)
{
	EphyEmbedEvent *event;
	const GValue *link_title;
	const GValue *link_rel;
	const GValue *link;
	const GValue *link_is_smart;
	const GValue *linktext;
	const char *title;
	const char *location;
	const char *rel;
	gboolean is_smart;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	link_is_smart = ephy_embed_event_get_property (event, "link_is_smart");
	link = ephy_embed_event_get_property (event, "link");
	link_title = ephy_embed_event_get_property (event, "link_title");
	link_rel = ephy_embed_event_get_property (event, "link_rel");
	linktext = ephy_embed_event_get_property (event, "linktext");

	location = g_value_get_string (link);
	g_return_if_fail (location);

	rel = g_value_get_string (link_rel);
	is_smart = g_value_get_int (link_is_smart);

	title = g_value_get_string (link_title);

	if (title == NULL || title[0] == '\0')
	{
		title = g_value_get_string (linktext);
	}

	if (title == NULL || title[0] == '\0')
	{
		title = location;
	}
	
	if (is_smart)
	{
		location = rel;
	}

	ephy_bookmarks_ui_add_bookmark (GTK_WINDOW (window), location, title);
}

static void
popup_cmd_copy_to_clipboard (EphyWindow *window, const char *text)
{
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE),
				text, -1);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				text, -1);
}

void
popup_cmd_copy_link_address (GtkAction *action,
			     EphyWindow *window)
{
	EphyEmbedEvent *event;
	EphyEmbedEventContext context;
	const char *address;
	const GValue *value;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	context = ephy_embed_event_get_context (event);

	if (context & EPHY_EMBED_CONTEXT_EMAIL_LINK)
	{
		value = ephy_embed_event_get_property (event, "email");
		address = g_value_get_string (value);
		popup_cmd_copy_to_clipboard (window, address);
	}
	else if (context & EPHY_EMBED_CONTEXT_LINK)
	{
		value = ephy_embed_event_get_property (event, "link");
		address = g_value_get_string (value);
		popup_cmd_copy_to_clipboard (window, address);
	}
}

static void
save_property_url_completed_cb (EphyEmbedPersist *persist)
{
	if (!(ephy_embed_persist_get_flags (persist) & 
				EPHY_EMBED_PERSIST_ASK_DESTINATION))
	{
		const char *dest;
		char *desktop_dir; 
		guint32 user_time;
		GnomeVFSURI *parent, *uri, *desktop;

		user_time = ephy_embed_persist_get_user_time (persist);
		dest = ephy_embed_persist_get_dest (persist);
		g_return_if_fail (dest != NULL);

		desktop_dir = ephy_file_desktop_dir ();

		/* Where are we going to save */
		uri = gnome_vfs_uri_new (dest); 
		desktop = gnome_vfs_uri_new (desktop_dir);

		g_return_if_fail (uri != NULL);

		parent = gnome_vfs_uri_get_parent (uri);

		/* If save location is the desktop, don't open it */
		if (!gnome_vfs_uri_equal (desktop, parent))
			ephy_file_browse_to (dest, user_time);

		g_free (desktop_dir);
		gnome_vfs_uri_unref (uri);
		gnome_vfs_uri_unref (parent);
		gnome_vfs_uri_unref (desktop);
	}
}

static void
save_property_url (GtkAction *action,
		   const char *title,
		   EphyWindow *window,
		   gboolean ask_dest,
		   const char *property)
{
	EphyEmbedEvent *event;
	const char *location;
	const GValue *value;
	EphyEmbedPersist *persist;
	EphyEmbed *embed;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	value = ephy_embed_event_get_property (event, property);
	location = g_value_get_string (value);

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	ephy_embed_persist_set_fc_title (persist, title);
	ephy_embed_persist_set_fc_parent (persist, GTK_WINDOW (window));
	ephy_embed_persist_set_flags
		(persist, EPHY_EMBED_PERSIST_FROM_CACHE |
			  (ask_dest ? EPHY_EMBED_PERSIST_ASK_DESTINATION : 0));
	ephy_embed_persist_set_persist_key
		(persist, CONF_STATE_SAVE_DIR);
	ephy_embed_persist_set_source (persist, location);

	g_signal_connect (persist, "completed",
			  G_CALLBACK (save_property_url_completed_cb), NULL);

	ephy_embed_persist_save (persist);

	g_object_unref (G_OBJECT(persist));
}

void
popup_cmd_open_link (GtkAction *action,
		     EphyWindow *window)
{
	EphyEmbedEvent *event;
	const char *location;
	const GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	event = ephy_window_get_context_event (window);
	value = ephy_embed_event_get_property (event, "link");
	location = g_value_get_string (value);

	ephy_embed_load_url (embed, location);
}

void
popup_cmd_download_link (GtkAction *action,
			 EphyWindow *window)
{
	save_property_url (action, _("Download Link"), window, 
			   FALSE, "link");
}

void
popup_cmd_download_link_as (GtkAction *action,
			    EphyWindow *window)
{
	save_property_url (action, _("Save Link As"), window, 
			   TRUE, "link");
}
void
popup_cmd_save_image_as (GtkAction *action,
			 EphyWindow *window)
{
	save_property_url (action, _("Save Image As"),
			   window, TRUE, "image");
}

#define GNOME_BACKGROUND_PREFERENCES "gnome-background-properties"

static void
background_download_completed (EphyEmbedPersist *persist)
{
	const char *bg;
	guint32 user_time;

	user_time = ephy_embed_persist_get_user_time (persist);

	bg = ephy_embed_persist_get_dest (persist);

	g_object_unref (persist);

	/* open the "Background Properties" capplet */
	if (!ephy_file_launch_desktop_file ("background.desktop", bg, user_time))
	{
		/* If the above try didn't work, then we try the Fedora name.
		 * This is a fix for #387206, but is actually a workaround for
		 * bugzilla.redhat.com #201867 */
		ephy_file_launch_desktop_file ("gnome-background.desktop", bg, user_time);
	}
}

void
popup_cmd_set_image_as_background (GtkAction *action,
				   EphyWindow *window)
{
	EphyEmbedEvent *event;
	const char *location;
	char *dest, *base, *base_converted;
	const GValue *value;
	EphyEmbedPersist *persist;
	EphyEmbed *embed;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	value = ephy_embed_event_get_property (event, "image");
	location = g_value_get_string (value);

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	base = g_path_get_basename (location);
	base_converted = g_filename_from_utf8 (base, -1, NULL, NULL, NULL);
	dest = g_build_filename (ephy_dot_dir (), base_converted, NULL);

	ephy_embed_persist_set_dest (persist, dest);
	ephy_embed_persist_set_flags (persist, EPHY_EMBED_PERSIST_NO_VIEW |
				     	       EPHY_EMBED_PERSIST_FROM_CACHE);
	ephy_embed_persist_set_source (persist, location);

	g_signal_connect (persist, "completed",
			  G_CALLBACK (background_download_completed),
			  NULL);

	ephy_embed_persist_save (persist);

	g_free (dest);
	g_free (base);
	g_free (base_converted);
}

void
popup_cmd_copy_image_location (GtkAction *action,
			       EphyWindow *window)
{
	EphyEmbedEvent *event;
	const char *location;
	const GValue *value;

	event = ephy_window_get_context_event (window);
	value = ephy_embed_event_get_property (event, "image");
	location = g_value_get_string (value);
	popup_cmd_copy_to_clipboard (window, location);
}

void
popup_cmd_open_frame (GtkAction *action,
		      EphyWindow *window)
{
	char *location;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	location = ephy_embed_get_location (embed, FALSE);

	ephy_embed_load_url (embed, location);

	g_free (location);
}

/* Opens an image URI using its associated handler. Or, if that
 * doesn't work, fallback to open the URI in a new browser window.
 */
static void
image_open_uri (const char *remote_address,
                const char *local_address,
		guint32 user_time)
{
	gboolean success;

	success = ephy_file_launch_handler (NULL, local_address, user_time);

	if (!success)
	{
		ephy_shell_new_tab (ephy_shell, NULL, NULL, remote_address,
				    EPHY_NEW_TAB_OPEN_PAGE |
				    EPHY_NEW_TAB_IN_NEW_WINDOW);
	}

	if (strcmp (remote_address, local_address) != 0)
	{
		if (success)
			ephy_file_delete_on_exit (local_address);
		else
			gnome_vfs_unlink (local_address);
	}
}

static void
save_source_completed_cb (EphyEmbedPersist *persist)
{
	const char *dest;
	const char *source;
	guint32 user_time;

	user_time = ephy_embed_persist_get_user_time (persist);
	dest = ephy_embed_persist_get_dest (persist);
	source = ephy_embed_persist_get_source (persist);
	g_return_if_fail (dest != NULL);

	image_open_uri (source, dest, user_time);
}

static void
save_temp_source (const char *address)
{
	char *tmp, *base;
	EphyEmbedPersist *persist;
	const char *static_temp_dir;

	static_temp_dir = ephy_file_tmp_dir ();
	if (static_temp_dir == NULL)
	{
		return;
	}

	base = g_build_filename (static_temp_dir, "viewimageXXXXXX", NULL);
	tmp = ephy_file_tmp_filename (base, "tmp"); /* FIXME */
	g_free (base);
	if (tmp == NULL)
	{
		return;
	}

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	ephy_embed_persist_set_source (persist, address);
	ephy_embed_persist_set_flags (persist, EPHY_EMBED_PERSIST_FROM_CACHE |
			 		       EPHY_EMBED_PERSIST_NO_VIEW);
	ephy_embed_persist_set_dest (persist, tmp);

	g_signal_connect (persist, "completed",
			  G_CALLBACK (save_source_completed_cb), NULL);

	ephy_embed_persist_save (persist);
	g_object_unref (persist);

	g_free (tmp);
}

void
popup_cmd_open_image (GtkAction *action,
		      EphyWindow *window)
{
	EphyEmbedEvent *event;
	const char *address;
	char *scheme;
	const GValue *value;
	EphyEmbed *embed;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	value = ephy_embed_event_get_property (event, "image");
	address = g_value_get_string (value);

	scheme = gnome_vfs_get_uri_scheme (address);
	if (scheme == NULL) return;

	if (strcmp (scheme, "file") == 0)
	{
		image_open_uri (address, address,
				gtk_get_current_event_time ());
	}
	else
	{
		save_temp_source (address);
	}

	g_free (scheme);
}

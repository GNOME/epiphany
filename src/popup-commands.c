/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#include "popup-commands.h"
#include "ephy-shell.h"
#include "ephy-embed-factory.h"
#include "ephy-embed-persist.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"

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

	ephy_embed_event_get_property (event, "link", &value);

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

	ephy_embed_event_get_property (event, "link", &value);

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

	ephy_embed_event_get_property (event, "link_is_smart", &link_is_smart);
	ephy_embed_event_get_property (event, "link", &link);
	ephy_embed_event_get_property (event, "link_title", &link_title);
	ephy_embed_event_get_property (event, "link_rel", &link_rel);
	ephy_embed_event_get_property (event, "linktext", &linktext);

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

	ephy_bookmarks_ui_add_bookmark (location, title, GTK_WINDOW (window));
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
		ephy_embed_event_get_property (event, "email", &value);
		address = g_value_get_string (value);
		popup_cmd_copy_to_clipboard (window, address);
	}
	else if (context & EPHY_EMBED_CONTEXT_LINK)
	{
		ephy_embed_event_get_property (event, "link", &value);
		address = g_value_get_string (value);
		popup_cmd_copy_to_clipboard (window, address);
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

	ephy_embed_event_get_property (event, property, &value);
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
	ephy_embed_event_get_property (event, "link", &value);
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
	char *type;
	guint32 user_time;

	user_time = ephy_embed_persist_get_user_time (persist);

	bg = ephy_embed_persist_get_dest (persist);
	eel_gconf_set_string (CONF_DESKTOP_BG_PICTURE, bg);

	type = eel_gconf_get_string (CONF_DESKTOP_BG_TYPE);
	if (type == NULL || strcmp (type, "none") == 0)
	{
		eel_gconf_set_string (CONF_DESKTOP_BG_TYPE, "wallpaper");
	}
	g_free (type);

	g_object_unref (persist);

	/* open the "Background Properties" capplet */
	ephy_file_launch_desktop_file ("background.desktop", user_time);
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

	ephy_embed_event_get_property (event, "image", &value);
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
	ephy_embed_event_get_property (event, "image", &value);
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

static void
image_open_uri (const char *address,
		gboolean delete,
		guint32 user_time)
{
	gboolean success;

	success = ephy_file_launch_handler (NULL, address, user_time);

	if (delete && success)
	{
		ephy_file_delete_on_exit (address);
	}
	else if (delete)
	{
		gnome_vfs_unlink (address);
	}
}

static void
save_source_completed_cb (EphyEmbedPersist *persist)
{
	const char *dest;
	guint32 user_time;

	user_time = ephy_embed_persist_get_user_time (persist);
	dest = ephy_embed_persist_get_dest (persist);
	g_return_if_fail (dest != NULL);

	image_open_uri (dest, TRUE, user_time);
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

	ephy_embed_event_get_property (event, "image", &value);
	address = g_value_get_string (value);

	scheme = gnome_vfs_get_uri_scheme (address);

	if (strcmp (scheme, "file") == 0)
	{
		image_open_uri (address, FALSE,
				gtk_get_current_event_time ());
	}
	else
	{
		save_temp_source (address);
	}

	g_free (scheme);
}

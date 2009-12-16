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
 */

#include "config.h"

#include "popup-commands.h"
#include "ephy-shell.h"
#include "ephy-embed-container.h"
#include "ephy-embed-persist.h"
#include "ephy-embed-utils.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-web-view.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

void
popup_cmd_link_in_new_window (GtkAction *action,
		              EphyWindow *window)
{
	EphyEmbedEvent *event;
	EphyEmbed *embed;
	GValue value = { 0, };

	embed = ephy_embed_container_get_active_child 
		(EPHY_EMBED_CONTAINER (window));

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	ephy_embed_event_get_property (event, "link-uri", &value);

	ephy_shell_new_tab (ephy_shell, NULL, embed,
			    g_value_get_string (&value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
	g_value_unset (&value);
}

void
popup_cmd_link_in_new_tab (GtkAction *action,
		           EphyWindow *window)
{
	EphyEmbedEvent *event;
	EphyEmbed *embed;
	GValue value = { 0, };

	embed = ephy_embed_container_get_active_child
		(EPHY_EMBED_CONTAINER (window));

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	ephy_embed_event_get_property (event, "link-uri", &value);

	ephy_shell_new_tab (ephy_shell, window, embed,
			    g_value_get_string (&value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	g_value_unset (&value);
}

void
popup_cmd_bookmark_link (GtkAction *action,
			 EphyWindow *window)
{
	EphyEmbedEvent *event;
	GValue link_title = { 0, };
	GValue link_rel = { 0, };
	GValue link = { 0, };
	GValue link_is_smart = { 0, };
	GValue linktext = { 0, };
	const char *title;
	const char *location;
	const char *rel;
	gboolean is_smart;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	/* FIXME: this is pretty much broken */
	ephy_embed_event_get_property (event, "link_is_smart", &link_is_smart);
	ephy_embed_event_get_property (event, "link-uri", &link);
	ephy_embed_event_get_property (event, "link_title", &link_title);
	ephy_embed_event_get_property (event, "link_rel", &link_rel);
	ephy_embed_event_get_property (event, "linktext", &linktext);

	location = g_value_get_string (&link);
	g_return_if_fail (location);

	rel = g_value_get_string (&link_rel);
	is_smart = g_value_get_int (&link_is_smart);

	title = g_value_get_string (&link_title);

	if (title == NULL || title[0] == '\0')
	{
		title = g_value_get_string (&linktext);
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
	g_value_unset (&link);
	g_value_unset (&link_rel);
	g_value_unset (&linktext);
	g_value_unset (&link_title);
	g_value_unset (&link_is_smart);
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
	guint context;
	const char *address;
	GValue value = { 0, };

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	context = ephy_embed_event_get_context (event);

#if 0
	if (context & EPHY_EMBED_CONTEXT_EMAIL_LINK)
	{
		value = ephy_embed_event_get_property (event, "email");
		address = g_value_get_string (&value);
		popup_cmd_copy_to_clipboard (window, address);
	}
#endif

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
	{
		ephy_embed_event_get_property (event, "link-uri", &value);
		address = g_value_get_string (&value);
		popup_cmd_copy_to_clipboard (window, address);
		g_value_unset (&value);
	}
}

static void
save_property_url_completed_cb (EphyEmbedPersist *persist)
{
	if (!(ephy_embed_persist_get_flags (persist) & 
				EPHY_EMBED_PERSIST_ASK_DESTINATION))
	{
		const char *dest;
		GFile *dest_file;
		guint32 user_time;

		user_time = ephy_embed_persist_get_user_time (persist);
		dest = ephy_embed_persist_get_dest (persist);

		g_return_if_fail (dest != NULL);

		dest_file = g_file_new_for_path (dest);
		
		g_return_if_fail (dest_file != NULL);
		/* If save location is the desktop, nautilus will not open */
		ephy_file_browse_to (dest_file, user_time);
		
		g_object_unref (dest_file);
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
	GValue value = { 0, };
	EphyEmbedPersist *persist;
	EphyEmbed *embed;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	ephy_embed_event_get_property (event, property, &value);
	location = g_value_get_string (&value);

	persist = EPHY_EMBED_PERSIST
		(g_object_new (EPHY_TYPE_EMBED_PERSIST, NULL));

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

	g_object_unref (G_OBJECT (persist));
	g_value_unset (&value);
}

void
popup_cmd_open_link (GtkAction *action,
		     EphyWindow *window)
{
	EphyEmbedEvent *event;
	const char *location;
	GValue value = { 0, };
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
		(EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	event = ephy_window_get_context_event (window);
	ephy_embed_event_get_property (event, "link-uri", &value);
	location = g_value_get_string (&value);
	ephy_web_view_load_url (EPHY_WEB_VIEW (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed)), location);
	g_value_unset (&value);
}

void
popup_cmd_download_link (GtkAction *action,
			 EphyWindow *window)
{
	save_property_url (action, _("Download Link"), window, 
			   FALSE, "link-uri");
}

void
popup_cmd_download_link_as (GtkAction *action,
			    EphyWindow *window)
{
	save_property_url (action, _("Save Link As"), window, 
			   TRUE, "link-uri");
}
void
popup_cmd_save_image_as (GtkAction *action,
			 EphyWindow *window)
{
	save_property_url (action, _("Save Image As"),
			   window, TRUE, "image-uri");
}

#define GNOME_APPEARANCE_PROPERTIES  "gnome-appearance-properties.desktop"

static void
background_download_completed (EphyEmbedPersist *persist,
			       GtkWidget *window)
{
	const char *bg;
	guint32 user_time;

	user_time = ephy_embed_persist_get_user_time (persist);

	bg = ephy_embed_persist_get_dest (persist);

	/* open the Appearance Properties capplet on the Background tab */
	if (!ephy_file_launch_desktop_file (GNOME_APPEARANCE_PROPERTIES, bg, user_time, window))
	{
		/* Fallback for <= 2.18 desktop: try to open the "Background Properties" capplet */
		if (!ephy_file_launch_desktop_file ("background.desktop", bg, user_time, window))
		{
			/* If the above try didn't work, then we try the Fedora name.
			 * This is a fix for #387206, but is actually a workaround for
			 * bugzilla.redhat.com #201867 */
			ephy_file_launch_desktop_file ("gnome-background.desktop", bg, user_time, window);
		}
	}

	g_object_unref (persist);
}

void
popup_cmd_set_image_as_background (GtkAction *action,
				   EphyWindow *window)
{
	EphyEmbedEvent *event;
	const char *location;
	char *dest, *base, *base_converted;
	GValue value = { 0, };
	EphyEmbedPersist *persist;
	EphyEmbed *embed;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	ephy_embed_event_get_property (event, "image-uri", &value);
	location = g_value_get_string (&value);

	persist = EPHY_EMBED_PERSIST
		(g_object_new (EPHY_TYPE_EMBED_PERSIST, NULL));

	base = g_path_get_basename (location);
	base_converted = g_filename_from_utf8 (base, -1, NULL, NULL, NULL);
	dest = g_build_filename (ephy_dot_dir (), base_converted, NULL);

	ephy_embed_persist_set_dest (persist, dest);
	ephy_embed_persist_set_flags (persist, EPHY_EMBED_PERSIST_NO_VIEW |
				     	       EPHY_EMBED_PERSIST_FROM_CACHE);
	ephy_embed_persist_set_source (persist, location);

	g_signal_connect (persist, "completed",
			  G_CALLBACK (background_download_completed),
			  window);

	ephy_embed_persist_save (persist);

	g_value_unset (&value);
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
	GValue value = { 0, };

	event = ephy_window_get_context_event (window);
	ephy_embed_event_get_property (event, "image-uri", &value);
	location = g_value_get_string (&value);
	popup_cmd_copy_to_clipboard (window, location);
	g_value_unset (&value);
}

void
popup_cmd_open_frame (GtkAction *action,
		      EphyWindow *window)
{
	char *location;
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
		(EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	location = ephy_web_view_get_location (ephy_embed_get_web_view (embed), FALSE);
	ephy_web_view_load_url (EPHY_WEB_VIEW (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed)), location);

	g_free (location);
}

/* Opens an image URI using its associated handler. Or, if that
 * doesn't work, fallback to open the URI in a new browser window.
 */
static void
image_open_uri (GFile *file,
                const char *remote_address,
		guint32 user_time)
{
	gboolean success;

	success = ephy_file_launch_handler (NULL, file, user_time);

	if (!success)
	{
		ephy_shell_new_tab (ephy_shell, NULL, NULL, remote_address,
				    EPHY_NEW_TAB_OPEN_PAGE |
				    EPHY_NEW_TAB_IN_NEW_WINDOW);
	}

	if (strcmp (remote_address, g_file_get_uri (file)) != 0)
	{
		if (success)
			ephy_file_delete_on_exit (file);
		else
			g_file_delete (file, NULL, NULL);
	}
}

static void
save_source_completed_cb (EphyEmbedPersist *persist)
{
	const char *dest;
	const char *source;
	guint32 user_time;
	GFile *file;

	user_time = ephy_embed_persist_get_user_time (persist);
	dest = ephy_embed_persist_get_dest (persist);
	source = ephy_embed_persist_get_source (persist);
	g_return_if_fail (dest != NULL);
	
	file = g_file_new_for_path (dest);

	image_open_uri (file, source, user_time);
	g_object_unref (file);
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
		(g_object_new (EPHY_TYPE_EMBED_PERSIST, NULL));

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
	char *scheme = NULL;
	GValue value = { 0, };
	EphyEmbed *embed;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	embed = ephy_embed_container_get_active_child 
		(EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	ephy_embed_event_get_property (event, "image-uri", &value);
	address = g_value_get_string (&value);

	scheme = g_uri_parse_scheme (address);
	if (scheme == NULL) goto out;

	if (strcmp (scheme, "file") == 0)
	{
		GFile *file;
		
		file = g_file_new_for_uri (address);
		image_open_uri (file, address,
				gtk_get_current_event_time ());
		g_object_unref (file);
	}
	else
	{
		save_temp_source (address);
	}

 out:
	g_value_unset (&value);
	g_free (scheme);
}

void
popup_cmd_inspect_element (GtkAction *action, EphyWindow *window)
{
	EphyEmbedEvent *event;
	EphyEmbed *embed;
	WebKitWebInspector *inspector;
	guint x, y;

	embed = ephy_embed_container_get_active_child
		(EPHY_EMBED_CONTAINER (window));

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	inspector = webkit_web_view_get_inspector
		(EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));

	ephy_embed_event_get_coords (event, &x, &y);
	webkit_web_inspector_inspect_coordinates (inspector, (gdouble)x, (gdouble)y);
}

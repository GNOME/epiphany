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
#include "ephy-download.h"
#include "ephy-shell.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-file-helpers.h"
#include "ephy-file-chooser.h"
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
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			    EPHY_NEW_TAB_APPEND_AFTER);
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
response_cb (GtkDialog *dialog,
	     int response_id,
	     EphyDownload *download)
{
	if (response_id == GTK_RESPONSE_ACCEPT)
	{
		char *uri;

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		ephy_download_set_destination_uri (download, uri);
		ephy_download_start (download);
		g_free (uri);
	}
	else
	{
		ephy_download_cancel (download);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
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
	EphyDownload *download;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	ephy_embed_event_get_property (event, property, &value);
	location = g_value_get_string (&value);

	download = ephy_download_new_for_uri (location);
	ephy_download_set_window (download, GTK_WIDGET (window));

	if (ask_dest)
	{
		EphyFileChooser *dialog;
		char *base;

		base = g_path_get_basename (location);
		dialog = ephy_file_chooser_new (title, GTK_WIDGET (window),
						GTK_FILE_CHOOSER_ACTION_SAVE,
						EPHY_PREFS_STATE_SAVE_DIR,
						EPHY_FILE_FILTER_ALL);

		gtk_file_chooser_set_do_overwrite_confirmation
				(GTK_FILE_CHOOSER (dialog), TRUE);
		gtk_file_chooser_set_current_name
				(GTK_FILE_CHOOSER (dialog), base);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (response_cb), download);
		gtk_widget_show (GTK_WIDGET (dialog));

		g_free (base);
	}
	else
	{
		ephy_download_set_auto_destination (download);
		ephy_download_start (download);
	}

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
	ephy_web_view_load_url (ephy_embed_get_web_view (embed), location);
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

static void
background_download_completed (EphyDownload *download,
			       GtkWidget *window)
{
	const char *uri;
	GSettings *settings;

	uri = ephy_download_get_destination_uri (download);
	settings = ephy_settings_get ("org.gnome.desktop.background");
	g_settings_set_string (settings, "picture-uri", uri);
}

void
popup_cmd_set_image_as_background (GtkAction *action,
				   EphyWindow *window)
{
	EphyEmbedEvent *event;
	const char *location;
	char *dest_uri, *dest, *base, *base_converted;
	GValue value = { 0, };
	EphyDownload *download;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	ephy_embed_event_get_property (event, "image-uri", &value);
	location = g_value_get_string (&value);

	download = ephy_download_new_for_uri (location);
	ephy_download_set_window (download, GTK_WIDGET (window));

	base = g_path_get_basename (location);
	base_converted = g_filename_from_utf8 (base, -1, NULL, NULL, NULL);
	dest = g_build_filename (g_get_home_dir (), "Pictures", base_converted, NULL);
	dest_uri = g_filename_to_uri (dest, NULL, NULL);

	ephy_download_set_destination_uri (download, dest_uri);

	g_signal_connect (download, "completed",
			  G_CALLBACK (background_download_completed), window);

	ephy_download_start (download);

	g_value_unset (&value);
	g_free (base);
	g_free (base_converted);
	g_free (dest);
	g_free (dest_uri);
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
	ephy_web_view_load_url (ephy_embed_get_web_view (embed), location);

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
save_source_completed_cb (EphyDownload *download)
{
	const char *dest;
	const char *source;
	guint32 user_time;
	GFile *file;

	user_time = ephy_download_get_start_time (download);
	dest = ephy_download_get_destination_uri (download);
	source = ephy_download_get_source_uri (download);
	g_return_if_fail (dest != NULL);
	
	file = g_file_new_for_uri (dest);

	image_open_uri (file, source, user_time);
	g_object_unref (file);
}

static void
save_temp_source (const char *address)
{
	EphyDownload *download;
	const char *static_temp_dir;
	char *base, *tmp_name, *tmp_path, *dest, *dest_uri;

	if (address == NULL) return;

	static_temp_dir = ephy_file_tmp_dir ();
	if (static_temp_dir == NULL) return;

	base = g_path_get_basename (address);
	tmp_name = g_strconcat (base, ".XXXXXX", NULL);
	g_free (base);

	tmp_path = g_build_filename (static_temp_dir, tmp_name, NULL);
	g_free (tmp_name);

	dest = ephy_file_tmp_filename (tmp_path, NULL);
	g_free (tmp_path);

	if (dest == NULL) return;

	dest_uri = g_filename_to_uri (dest, NULL, NULL);
	download = ephy_download_new_for_uri (address);
	ephy_download_set_destination_uri (download, dest_uri);

	g_signal_connect (download, "completed",
			  G_CALLBACK (save_source_completed_cb), NULL);

	ephy_download_start (download);

	g_free (dest);
	g_free (dest_uri);
}

void
popup_replace_spelling (GtkAction *action,
			EphyWindow *window)
{
	EphyEmbed *embed;
	WebKitWebView *view;
	WebKitWebFrame *frame;
	WebKitDOMDOMSelection *selection;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *default_view;

	embed = ephy_embed_container_get_active_child 
		(EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	view = WEBKIT_WEB_VIEW (ephy_embed_get_web_view (embed));
	g_return_if_fail (view != NULL);

	document = webkit_web_view_get_dom_document (view);
	default_view = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (default_view);
	webkit_dom_dom_selection_modify (selection, "move", "backward", "word");
	webkit_dom_dom_selection_modify (selection, "extend", "forward", "word");
	frame = webkit_web_view_get_focused_frame (view);
	webkit_web_frame_replace_selection (frame, gtk_action_get_label (action));
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

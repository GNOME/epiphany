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

#include "ephy-bookmarks-ui.h"
#include "ephy-download.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "ephy-private.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-web-view.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

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

	ephy_shell_new_tab (ephy_shell_get_default (),
			    NULL, embed,
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

	ephy_shell_new_tab (ephy_shell_get_default (),
			    window, embed,
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
	WebKitHitTestResult *result;
	const char *title;
	const char *location;

	event = ephy_window_get_context_event (window);

	result = ephy_embed_event_get_hit_test_result (event);
	if (!webkit_hit_test_result_context_is_link (result))
	{
		return;
	}

	location = webkit_hit_test_result_get_link_uri (result);
	title = webkit_hit_test_result_get_link_title (result);
	if (!title)
	{
		title = webkit_hit_test_result_get_link_label (result);
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
	guint context;
	const char *address;
	GValue value = { 0, };

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	context = ephy_embed_event_get_context (event);

	if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)
	{
		ephy_embed_event_get_property (event, "link-uri", &value);
		address = g_value_get_string (&value);

		if (g_str_has_prefix (address, "mailto:"))
			address = address + 7;

		popup_cmd_copy_to_clipboard (window, address);
		g_value_unset (&value);
	}
}

static void
save_property_url_to_destination (EphyWindow *window,
				  const char *location,
				  const char *destination)
{
	EphyDownload *download;

	download = ephy_download_new_for_uri (location, GTK_WINDOW (window));

	if (destination)
		ephy_download_set_destination_uri (download, destination);
}

static void
response_cb (GtkDialog *dialog,
	     int response_id,
	     char *location)
{
	if (response_id == GTK_RESPONSE_ACCEPT)
	{
		char *uri;
		GtkWindow *window;

		window = gtk_window_get_transient_for (GTK_WINDOW (dialog));

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		save_property_url_to_destination (EPHY_WINDOW (window), location, uri);
		g_free (uri);
	}

	g_free (location);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static char *
get_suggested_filename (EphyWebView *view)
{
	char *suggested_filename = NULL;
	const char *mimetype;
	WebKitURIResponse *response;
	WebKitWebResource *web_resource;

	web_resource = webkit_web_view_get_main_resource (WEBKIT_WEB_VIEW (view));
	response = webkit_web_resource_get_response (web_resource);
	mimetype = webkit_uri_response_get_mime_type (response);

	if ((g_ascii_strncasecmp (mimetype, "text/html", 9)) == 0)
	{
		/* Web Title will be used as suggested filename */
		suggested_filename = g_strconcat (ephy_web_view_get_title (view), ".mhtml", NULL);
	}
	else
	{
		suggested_filename = g_strdup (webkit_uri_response_get_suggested_filename (response));
		if (!suggested_filename)
		{
			SoupURI *soup_uri = soup_uri_new (webkit_web_resource_get_uri (web_resource));
			suggested_filename = g_path_get_basename (soup_uri->path);
			soup_uri_free (soup_uri);
		}
	}

	return suggested_filename;
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

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	ephy_embed_event_get_property (event, property, &value);
	location = g_value_get_string (&value);

	if (ask_dest)
	{
		EphyFileChooser *dialog;
		EphyEmbed *embed;
		EphyWebView *view;
		char *suggested_filename;

		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		view = ephy_embed_get_web_view (embed);

		dialog = ephy_file_chooser_new (title, GTK_WIDGET (window),
						GTK_FILE_CHOOSER_ACTION_SAVE,
						EPHY_FILE_FILTER_NONE);

		suggested_filename = ephy_sanitize_filename (get_suggested_filename (view));
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), suggested_filename);
		g_free (suggested_filename);

		gtk_file_chooser_set_do_overwrite_confirmation
				(GTK_FILE_CHOOSER (dialog), TRUE);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (response_cb), g_strdup (location));
		gtk_widget_show (GTK_WIDGET (dialog));
	}
	else
	{
		save_property_url_to_destination (window, location, NULL);
	}
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

	download = ephy_download_new_for_uri (location, GTK_WINDOW (window));

	base = g_path_get_basename (location);
	base_converted = g_filename_from_utf8 (base, -1, NULL, NULL, NULL);
	dest = g_build_filename (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES), base_converted, NULL);
	dest_uri = g_filename_to_uri (dest, NULL, NULL);

	ephy_download_set_destination_uri (download, dest_uri);
	ephy_download_set_action (download, EPHY_DOWNLOAD_ACTION_DO_NOTHING);

	g_signal_connect (download, "completed",
			  G_CALLBACK (background_download_completed), window);

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
popup_cmd_view_image_in_new_tab (GtkAction *action,
				 EphyWindow *window)
{
	EphyEmbedEvent *event;
	GValue value = { 0, };
	EphyEmbed *embed;

	event = ephy_window_get_context_event (window);
	g_return_if_fail (event != NULL);

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	ephy_embed_event_get_property (event, "image-uri", &value);

	ephy_shell_new_tab (ephy_shell_get_default (),
			    window, embed,
			    g_value_get_string (&value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			    EPHY_NEW_TAB_APPEND_AFTER);
	g_value_unset (&value);
}


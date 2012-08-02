/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2009 Collabora Ltd.
 *  Copyright © 2011 Igalia S.L.
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
#include "window-commands.h"

#include "ephy-bookmarks-editor.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-debug.h"
#include "ephy-dialog.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-find-toolbar.h"
#include "ephy-gui.h"
#include "ephy-history-window.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-notebook.h"
#include "ephy-prefs.h"
#include "ephy-private.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-string.h"
#include "ephy-web-app-utils.h"
#include "ephy-zoom.h"
#include "pdm-dialog.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>
#include <libsoup/soup.h>
#include <string.h>
#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

void
window_cmd_file_print (GtkAction *action,
		       EphyWindow *window)
{
	EphyEmbed *embed;
	EphyWebView *view;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (EPHY_IS_EMBED (embed));
	view = ephy_embed_get_web_view (embed);

	ephy_web_view_print (view);
}

void
window_cmd_file_send_to	(GtkAction *action,
			 EphyWindow *window)
{
	EphyEmbed *embed;
	char *command, *subject, *body;
	const char *location, *title;
	GdkScreen *screen;
	GError *error = NULL;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	location = ephy_web_view_get_address (ephy_embed_get_web_view (embed));
	title = ephy_web_view_get_title (ephy_embed_get_web_view (embed));

	subject = g_uri_escape_string (title, NULL, TRUE);
	body = g_uri_escape_string (location, NULL, TRUE);

	command = g_strconcat ("mailto:",
			       "?Subject=", subject,
			       "&Body=", body, NULL);

	g_free (subject);
	g_free (body);

	if (window)
	{
		screen = gtk_widget_get_screen (GTK_WIDGET (window));
	}
	else
	{
		screen = gdk_screen_get_default ();
	}
	
	if (!gtk_show_uri (screen, command, gtk_get_current_event_time(), &error)) 
	{
    		g_warning ("Unable to send link by email: %s\n", error->message);
    		g_error_free (error);
  	}

	g_free (command);
}

static gboolean
event_with_shift (void)
{
	GdkEvent *event;
	GdkEventType type = 0;
	guint state = 0;

	event = gtk_get_current_event ();
	if (event)
	{
		type = event->type;

		if (type == GDK_BUTTON_RELEASE)
		{
			state = event->button.state; 
		}
		else if (type == GDK_KEY_PRESS || type == GDK_KEY_RELEASE)
		{
			state = event->key.state;
		}

		gdk_event_free (event);
	}

	return (state & GDK_SHIFT_MASK) != 0;
}

void
window_cmd_go_location (GtkAction *action,
		        EphyWindow *window)
{
	ephy_window_activate_location (window);
}

void
window_cmd_view_stop (GtkAction *action,
		      EphyWindow *window)
{
	EphyEmbed *embed;
	
	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	webkit_web_view_stop_loading (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
}

void
window_cmd_view_reload (GtkAction *action,
		        EphyWindow *window)
{
	EphyEmbed *embed;
	WebKitWebView *view;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
	if (event_with_shift ())
		webkit_web_view_reload_bypass_cache (view);
	else
		webkit_web_view_reload (view);
}

void
window_cmd_file_bookmark_page (GtkAction *action,
			       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	ephy_bookmarks_ui_add_bookmark (GTK_WINDOW (window),
					ephy_web_view_get_address (ephy_embed_get_web_view (embed)),
					ephy_web_view_get_title (ephy_embed_get_web_view (embed)));
}

static void
open_response_cb (GtkDialog *dialog, int response, EphyWindow *window)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *uri, *converted;

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		if (uri != NULL)
		{
			converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);
	
			if (converted != NULL)
			{
				ephy_window_load_url (window, converted);
			}
	
			g_free (converted);
			g_free (uri);
	        }
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
save_response_cb (GtkDialog *dialog, int response, EphyEmbed *embed)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *uri, *converted;

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		if (uri != NULL)
		{
			converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

			if (converted != NULL)
			{
				EphyWebView *web_view = ephy_embed_get_web_view (embed);
				ephy_web_view_save (web_view, converted);
			}

			g_free (converted);
			g_free (uri);
	        }
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
window_cmd_file_open (GtkAction *action,
		      EphyWindow *window)
{
	EphyFileChooser *dialog;

	dialog = ephy_file_chooser_new (_("Open"),
					GTK_WIDGET (window),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					EPHY_FILE_FILTER_ALL_SUPPORTED);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (open_response_cb), window);

	gtk_widget_show (GTK_WIDGET (dialog));
}

static char *
get_suggested_filename (EphyWebView *view)
{
	char *suggested_filename;
	const char *mimetype;
#ifdef HAVE_WEBKIT2
	WebKitURIResponse *response;
#else
	WebKitWebFrame *frame;
	WebKitWebDataSource *data_source;
#endif
	WebKitWebResource *web_resource;

#ifdef HAVE_WEBKIT2
	web_resource = webkit_web_view_get_main_resource (WEBKIT_WEB_VIEW (view));
	response = webkit_web_resource_get_response (web_resource);
	mimetype = webkit_uri_response_get_mime_type (response);
#else
	frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view));
	data_source = webkit_web_frame_get_data_source (frame);
	web_resource = webkit_web_data_source_get_main_resource (data_source);
	mimetype = webkit_web_resource_get_mime_type (web_resource);
#endif

	if ((g_ascii_strncasecmp (mimetype, "text/html", 9)) == 0)
	{
		/* Web Title will be used as suggested filename*/
		suggested_filename = g_strconcat (ephy_web_view_get_title (view), ".html", NULL);
	}
	else
	{
		WebKitNetworkResponse *response = webkit_web_frame_get_network_response (frame);
		suggested_filename = g_strdup (webkit_network_response_get_suggested_filename (response));
	}

	return suggested_filename;
}

void
window_cmd_file_save_as (GtkAction *action,
			 EphyWindow *window)
{
	EphyEmbed *embed;
	EphyFileChooser *dialog;
	char *suggested_filename;
	EphyWebView *view;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	dialog = ephy_file_chooser_new (_("Save"),
					GTK_WIDGET (window),
					GTK_FILE_CHOOSER_ACTION_SAVE,
					EPHY_FILE_FILTER_NONE);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

	view = ephy_embed_get_web_view (embed);
	suggested_filename = get_suggested_filename (view);

	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), suggested_filename);
	g_free (suggested_filename);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (save_response_cb), embed);

	gtk_widget_show (GTK_WIDGET (dialog));
}

typedef struct {
	EphyWebView *view;
	GtkWidget *image;
	GtkWidget *entry;
	GtkWidget *spinner;
	GtkWidget *box;
	char *icon_href;
} EphyApplicationDialogData;

static void
ephy_application_dialog_data_free (EphyApplicationDialogData *data)
{
	g_free (data->icon_href);
	g_slice_free (EphyApplicationDialogData, data);
}

static void
take_page_snapshot_and_set_image (EphyApplicationDialogData *data)
{
	GdkPixbuf *snapshot;
	int x, y, w, h;

	x = y = 0;
	w = h = 128; /* GNOME hi-res icon size. */

	snapshot = ephy_web_view_get_snapshot (data->view, x, y, w, h);

	gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), snapshot);
	g_object_unref (snapshot);
}

#ifdef HAVE_WEBKIT2
static void
download_finished_cb (WebKitDownload *download,
		      EphyApplicationDialogData *data)
{
	char *filename;

	filename = g_filename_from_uri (webkit_download_get_destination (download), NULL, NULL);
	gtk_image_set_from_file (GTK_IMAGE (data->image), filename);
	g_free (filename);
}

static void
download_failed_cb (WebKitDownload *download,
		    GError *error,
		    EphyApplicationDialogData *data)
{
	g_signal_handlers_disconnect_by_func (download, download_finished_cb, data);
	/* Something happened, default to a page snapshot. */
	take_page_snapshot_and_set_image (data);
}
#else
static void
download_status_changed_cb (WebKitDownload *download,
			    GParamSpec *spec,
			    EphyApplicationDialogData *data)
{
	WebKitDownloadStatus status = webkit_download_get_status (download);
	char *filename;

	switch (status)
	{
	case WEBKIT_DOWNLOAD_STATUS_FINISHED:
		filename = g_filename_from_uri (webkit_download_get_destination_uri (download),
						   NULL, NULL);
		gtk_image_set_from_file (GTK_IMAGE (data->image), filename);
		g_free (filename);
		break;
	case WEBKIT_DOWNLOAD_STATUS_ERROR:
	case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
		/* Something happened, default to a page snapshot. */
		take_page_snapshot_and_set_image (data);
		break;
	default:
		break;
	}
}
#endif

static void
download_icon_and_set_image (EphyApplicationDialogData *data)
{
#ifndef HAVE_WEBKIT2
	WebKitNetworkRequest *request;
#endif
	WebKitDownload *download;
	char *destination, *destination_uri, *tmp_filename;

#ifdef HAVE_WEBKIT2
	download = webkit_web_context_download_uri (webkit_web_context_get_default (),
						    data->icon_href);
#else
	request = webkit_network_request_new (data->icon_href);
	download = webkit_download_new (request);
	g_object_unref (request);
#endif

	tmp_filename = ephy_file_tmp_filename ("ephy-download-XXXXXX", NULL);
	destination = g_build_filename (ephy_file_tmp_dir (), tmp_filename, NULL);
	destination_uri = g_filename_to_uri (destination, NULL, NULL);
#ifdef HAVE_WEBKIT2
	webkit_download_set_destination (download, destination_uri);
#else
	webkit_download_set_destination_uri (download, destination_uri);
#endif
	g_free (destination);
	g_free (destination_uri);
	g_free (tmp_filename);

#ifdef HAVE_WEBKIT2
	g_signal_connect (download, "finished",
			  G_CALLBACK (download_finished_cb), data);
	g_signal_connect (download, "failed",
			  G_CALLBACK (download_failed_cb), data);
#else
	g_signal_connect (download, "notify::status",
			  G_CALLBACK (download_status_changed_cb), data);

	webkit_download_start (download);
#endif
}

static void
fill_default_application_image (EphyApplicationDialogData *data)
{
#ifdef HAVE_WEBKIT2
	/* TODO: DOM Bindindgs */
#else
	WebKitDOMDocument *document;
	WebKitDOMNodeList *links;
	gulong length, i;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (data->view));
	links = webkit_dom_document_get_elements_by_tag_name (document, "link");
	length = webkit_dom_node_list_get_length (links);

	for (i = 0; i < length; i++)
	{
		char *rel;
		WebKitDOMNode *node = webkit_dom_node_list_item (links, i);
		rel = webkit_dom_html_link_element_get_rel (WEBKIT_DOM_HTML_LINK_ELEMENT (node));
		/* TODO: support more than one possible icon. */
		if (g_strcmp0 (rel, "apple-touch-icon") == 0 ||
		    g_strcmp0 (rel, "apple-touch-icon-precomposed") == 0)
		{
			data->icon_href = webkit_dom_html_link_element_get_href (WEBKIT_DOM_HTML_LINK_ELEMENT (node));
			download_icon_and_set_image (data);
			g_free (rel);
			return;
		}
	}
#endif
	/* If we make it here, no "apple-touch-icon" link was
	 * found. Take a snapshot of the page. */
	take_page_snapshot_and_set_image (data);
}

static void
fill_default_application_title (EphyApplicationDialogData *data)
{
	const char *title = ephy_web_view_get_title (data->view);
	gtk_entry_set_text (GTK_ENTRY (data->entry), title);
}

static void
notify_launch_cb (NotifyNotification *notification,
		  char *action,
		  gpointer user_data)
{
	char * desktop_file = user_data;
	/* A gross hack to be able to launch epiphany from within
	 * Epiphany. Might be a good idea to figure out a better
	 * solution... */
	g_unsetenv (EPHY_UUID_ENVVAR);
	ephy_file_launch_desktop_file (desktop_file, NULL, 0, NULL);
	g_free (desktop_file);
}

static gboolean
confirm_web_application_overwrite (GtkWindow *parent, const char *title)
{
  GtkResponseType response;
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent, 0,
				   GTK_MESSAGE_QUESTION,
				   GTK_BUTTONS_NONE,
				   _("A web application named '%s' already exists. Do you want to replace it?"),
				   title);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			  _("Cancel"),
			  GTK_RESPONSE_CANCEL,
			  _("Replace"),
			  GTK_RESPONSE_OK,
			  NULL);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("An application with the same name already exists. Replacing it will "
					      "overwrite it."));
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  return response == GTK_RESPONSE_OK;
}

static void
dialog_save_as_application_response_cb (GtkDialog *dialog,
					gint response,
					EphyApplicationDialogData *data)
{
	const char *app_name;
	char *desktop_file;
	char *message;
	NotifyNotification *notification;

	if (response == GTK_RESPONSE_OK) {
		app_name = gtk_entry_get_text (GTK_ENTRY (data->entry));

		if (ephy_web_application_exists (app_name))
		{
			if (confirm_web_application_overwrite (GTK_WINDOW (dialog), app_name))
				ephy_web_application_delete (app_name);
			else
				return;
		}

		/* Create Web Application, including a new profile and .desktop file. */
		desktop_file = ephy_web_application_create (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (data->view)),
							    app_name,
							    gtk_image_get_pixbuf (GTK_IMAGE (data->image)));
		if (desktop_file)
			message = g_strdup_printf (_("The application '%s' is ready to be used"),
						   app_name);
		else
			message = g_strdup_printf (_("The application '%s' could not be created"),
						   app_name);

		notification = notify_notification_new (message,
							NULL, NULL);
		g_free (message);

		if (desktop_file) {
			notify_notification_add_action (notification, "launch", _("Launch"),
							(NotifyActionCallback)notify_launch_cb,
							g_path_get_basename (desktop_file),
							NULL);
			notify_notification_set_icon_from_pixbuf (notification, gtk_image_get_pixbuf (GTK_IMAGE (data->image)));
			g_free (desktop_file);
		}

		notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
		notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
		notify_notification_set_hint (notification, "transient", g_variant_new_boolean (TRUE));
		notify_notification_show (notification, NULL);
	}

	ephy_application_dialog_data_free (data);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
window_cmd_file_save_as_application (GtkAction *action,
				     EphyWindow *window)
{
	EphyEmbed *embed;
	GtkWidget *dialog, *box, *image, *entry, *content_area;
	EphyWebView *view;
	EphyApplicationDialogData *data;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	view = EPHY_WEB_VIEW (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));

	/* Show dialog with icon, title. */
	dialog = gtk_dialog_new_with_buttons (_("Create Web Application"),
					      GTK_WINDOW (window),
					      0,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      _("C_reate"),
					      GTK_RESPONSE_OK,
					      NULL);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (content_area), 14); /* 14 + 2 * 5 = 24 */

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add (GTK_CONTAINER (content_area), box);

	image = gtk_image_new ();
	gtk_widget_set_size_request (image, 128, 128);
	gtk_container_add (GTK_CONTAINER (box), image);

	entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_box_pack_end (GTK_BOX (box), entry, FALSE, FALSE, 0);

	data = g_slice_new0 (EphyApplicationDialogData);
	data->view = view;
	data->image = image;
	data->entry = entry;

	fill_default_application_image (data);
	fill_default_application_title (data);

	gtk_widget_show_all (dialog);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (dialog_save_as_application_response_cb),
			  data);
	gtk_widget_show_all (dialog);
}

void
window_cmd_file_work_offline (GtkAction *action,
		              EphyWindow *window)
{
        /* TODO: WebKitGTK+ does not currently support offline status. */
#if 0
	EphyEmbedSingle *single;
	gboolean offline;

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	offline = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	ephy_embed_single_set_network_status (single, !offline);
#endif
}

void
window_cmd_file_close_window (GtkAction *action,
		              EphyWindow *window)
{
	GtkWidget *notebook;
	EphyEmbed *embed;

	notebook = ephy_window_get_notebook (window);

	if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
				    EPHY_PREFS_LOCKDOWN_QUIT) &&
	    gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) == 1)
	{
		return;
	}

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	g_signal_emit_by_name (notebook, "tab-close-request", embed);
}

void
window_cmd_edit_undo (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *widget;
	GtkWidget *embed;
	GtkWidget *location_entry;

	widget = gtk_window_get_focus (GTK_WINDOW (window));
	location_entry = gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY);
	
	if (location_entry)
	{
		ephy_location_entry_reset (EPHY_LOCATION_ENTRY (location_entry));
	}
	else 
	{
		embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);

		if (embed)
		{
#ifdef HAVE_WEBKIT2
			webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (EPHY_EMBED (embed)), "Undo");
#else
			webkit_web_view_undo (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (EPHY_EMBED (embed)));
#endif
		}
	}
}

void
window_cmd_edit_redo (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *widget;
	GtkWidget *embed;
	GtkWidget *location_entry;

	widget = gtk_window_get_focus (GTK_WINDOW (window));
	location_entry = gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY);
	
	if (location_entry)
	{
		ephy_location_entry_undo_reset (EPHY_LOCATION_ENTRY (location_entry));
	}
	else
	{
		embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);
		if (embed)
		{
#ifdef HAVE_WEBKIT2
			webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (EPHY_EMBED (embed)), "Redo");
#else
			webkit_web_view_redo (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (EPHY_EMBED (embed)));
#endif
		}
	}
}
void
window_cmd_edit_cut (GtkAction *action,
		     EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		EphyEmbed *embed;
		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

#ifdef HAVE_WEBKIT2
		webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_CUT);
#else
		webkit_web_view_cut_clipboard (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
#endif
	}
}

void
window_cmd_edit_copy (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);
#ifdef HAVE_WEBKIT2
		webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_COPY);
#else
		webkit_web_view_copy_clipboard (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
#endif
	}
}

void
window_cmd_edit_paste (GtkAction *action,
		       EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

#ifdef HAVE_WEBKIT2
		webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_PASTE);
#else
		webkit_web_view_paste_clipboard (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
#endif
	}
}

void
window_cmd_edit_delete (GtkAction *action,
			EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_delete_text (GTK_EDITABLE (widget), 0, -1);
	}
	else
	{
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

		/* FIXME: TODO */
#if 0
		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
						 "cmd_delete");
#endif
	}
}

void
window_cmd_edit_select_all (GtkAction *action,
			    EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
	}
	else
	{
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child 
                  (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

#ifdef HAVE_WEBKIT2
		webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), "SelectAll");
#else
		webkit_web_view_select_all (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
#endif
	}
}

void
window_cmd_edit_find (GtkAction *action,
		      EphyWindow *window)
{
	EphyFindToolbar *toolbar;
	
	toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_find_toolbar (window));
	ephy_find_toolbar_open (toolbar, FALSE, FALSE);
}

void
window_cmd_edit_find_next (GtkAction *action,
			   EphyWindow *window)
{
	EphyFindToolbar *toolbar;

	toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_find_toolbar (window));
	ephy_find_toolbar_find_next (toolbar);
}

void
window_cmd_edit_find_prev (GtkAction *action,
			   EphyWindow *window)
{
	EphyFindToolbar *toolbar;

	toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_find_toolbar (window));
	ephy_find_toolbar_find_previous (toolbar);
}

void
window_cmd_view_fullscreen (GtkAction *action,
			    EphyWindow *window)
{
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
		gtk_window_fullscreen (GTK_WINDOW (window));
	else
		gtk_window_unfullscreen (GTK_WINDOW (window));
}

void
window_cmd_view_zoom_in	(GtkAction *action,
			 EphyWindow *window)
{
	ephy_window_set_zoom (window, ZOOM_IN);
}

void
window_cmd_view_zoom_out (GtkAction *action,
			  EphyWindow *window)
{
	ephy_window_set_zoom (window, ZOOM_OUT);
}

void
window_cmd_view_zoom_normal (GtkAction *action,
			     EphyWindow *window)
{
	ephy_window_set_zoom (window, 1.0);
}

static void
view_source_embedded (const char *uri, EphyEmbed *embed)
{
	EphyEmbed *new_embed;

	new_embed = ephy_shell_new_tab
			(ephy_shell_get_default (),
			 EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
			 embed,
			 NULL,
			 EPHY_NEW_TAB_JUMP | EPHY_NEW_TAB_IN_EXISTING_WINDOW | EPHY_NEW_TAB_APPEND_AFTER);
#ifdef HAVE_WEBKIT2
	/* TODO: View Source */
#else
	webkit_web_view_set_view_source_mode
		(EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (new_embed), TRUE);
	webkit_web_view_load_uri
		(EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (new_embed), uri);
#endif
}


static void
save_temp_source_close_cb (GOutputStream *ostream, GAsyncResult *result, gpointer data)
{
	char *uri;
	GFile *file;
	GError *error = NULL;

	g_output_stream_close_finish (ostream, result, &error);
	if (error)
	{
		g_warning ("Unable to close file: %s", error->message);
		g_error_free (error);
		return;
	}

	uri = (char*)g_object_get_data (G_OBJECT (ostream), "ephy-save-temp-source-uri");

	file = g_file_new_for_uri (uri);

	if (!ephy_file_launch_handler ("text/plain", file, gtk_get_current_event_time ()))
	{
		/* Fallback to view the source inside the browser */
		const char *uri;
		EphyEmbed *embed;

		uri = (const char*) g_object_get_data (G_OBJECT (ostream),
						       "ephy-original-source-uri");
		embed = (EphyEmbed*)g_object_get_data (G_OBJECT (ostream),
						       "ephy-save-temp-source-embed");
		view_source_embedded (uri, embed);
	}
	g_object_unref (ostream);

	g_object_unref (file);
}

static void
save_temp_source_write_cb (GOutputStream *ostream, GAsyncResult *result, GString *data)
{
	GError *error = NULL;
	gssize written;

	written = g_output_stream_write_finish (ostream, result, &error);
	if (error)
	{
		g_string_free (data, TRUE);
		g_warning ("Unable to write to file: %s", error->message);
		g_error_free (error);

		g_output_stream_close_async (ostream, G_PRIORITY_DEFAULT, NULL,
					     (GAsyncReadyCallback)save_temp_source_close_cb,
					     NULL);

		return;
	}

	if (written == data->len)
	{
		g_string_free (data, TRUE);

		g_output_stream_close_async (ostream, G_PRIORITY_DEFAULT, NULL,
					     (GAsyncReadyCallback)save_temp_source_close_cb,
					     NULL);

		return;
	}

	data->len -= written;
	data->str += written;

	g_output_stream_write_async (ostream,
				     data->str, data->len,
				     G_PRIORITY_DEFAULT, NULL,
				     (GAsyncReadyCallback)save_temp_source_write_cb,
				     data);
}

#ifdef HAVE_WEBKIT2
static void
get_main_resource_data_cb (WebKitWebResource *resource, GAsyncResult *result, GOutputStream *ostream)
{
	guchar *data;
	gsize data_length;
	GString *data_str;
	GError *error = NULL;

	data = webkit_web_resource_get_data_finish (resource, result, &data_length, &error);
	if (error) {
		g_warning ("Unable to get main resource data: %s", error->message);
		g_error_free (error);
		return;
	}

	/* We create a new GString here because we need to make sure
	 * we keep writing in case of partial writes */
	data_str = g_string_new_len ((gchar *)data, data_length);
	g_free (data);

	g_output_stream_write_async (ostream,
				     data_str->str, data_str->len,
				     G_PRIORITY_DEFAULT, NULL,
				     (GAsyncReadyCallback)save_temp_source_write_cb,
				     data_str);
}
#endif

static void
save_temp_source_replace_cb (GFile *file, GAsyncResult *result, EphyEmbed *embed)
{
	EphyWebView *view;
#ifdef HAVE_WEBKIT2
	WebKitWebResource *resource;
#else
	WebKitWebFrame *frame;
	WebKitWebDataSource *data_source;
	GString *const_data;
	GString *data;
#endif
	GFileOutputStream *ostream;
	GError *error = NULL;

	ostream = g_file_replace_finish (file, result, &error);
	if (error)
	{
		g_warning ("Unable to replace file: %s", error->message);
		g_error_free (error);
		return;
	}

	g_object_set_data_full (G_OBJECT (ostream),
				"ephy-save-temp-source-uri",
				g_file_get_uri (file),
				g_free);

	view = ephy_embed_get_web_view (embed);

	g_object_set_data_full (G_OBJECT (ostream),
				"ephy-original-source-uri",
				g_strdup (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view))),
				g_free),

	g_object_set_data_full (G_OBJECT (ostream),
				"ephy-save-temp-source-embed",
				g_object_ref (embed),
				g_object_unref);

#ifdef HAVE_WEBKIT2
	resource = webkit_web_view_get_main_resource (WEBKIT_WEB_VIEW (view));
	webkit_web_resource_get_data (resource, NULL,
				      (GAsyncReadyCallback)get_main_resource_data_cb,
				      ostream);
#else
	frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view));
	data_source = webkit_web_frame_get_data_source (frame);
	const_data = webkit_web_data_source_get_data (data_source);

	/* We create a new GString here because we need to make sure
	 * we keep writing in case of partial writes */
	if (const_data)
		data = g_string_new_len (const_data->str, const_data->len);
	else
		data = g_string_new_len ("", 0);

	g_output_stream_write_async (G_OUTPUT_STREAM (ostream),
				     data->str, data->len,
				     G_PRIORITY_DEFAULT, NULL,
				     (GAsyncReadyCallback)save_temp_source_write_cb,
				     data);
#endif
}

static void
save_temp_source (EphyEmbed *embed,
		  guint32 user_time)
{
	GFile *file;
	char *tmp, *base;
	const char *static_temp_dir;

	static_temp_dir = ephy_file_tmp_dir ();
	if (static_temp_dir == NULL)
	{
		return;
	}

	base = g_build_filename (static_temp_dir, "viewsourceXXXXXX", NULL);
	tmp = ephy_file_tmp_filename (base, "html");
	g_free (base);
	if (tmp == NULL)
	{
		return;
	}

	file = g_file_new_for_path (tmp);
	g_file_replace_async (file, NULL, FALSE,
			      G_FILE_CREATE_REPLACE_DESTINATION|G_FILE_CREATE_PRIVATE,
			      G_PRIORITY_DEFAULT, NULL,
			      (GAsyncReadyCallback)save_temp_source_replace_cb,
			      embed);

	g_object_unref (file);
	g_free (tmp);
}

void
window_cmd_view_page_source (GtkAction *action,
			     EphyWindow *window)
{
	EphyEmbed *embed;
	const char *address;
	guint32 user_time;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

#ifdef HAVE_WEBKIT2
	/* TODO: View Source */
#else
	if (g_settings_get_boolean (EPHY_SETTINGS_MAIN,
				    EPHY_PREFS_INTERNAL_VIEW_SOURCE))
	{
		view_source_embedded (address, embed);
		return;
	}
#endif

	user_time = gtk_get_current_event_time ();

	if (g_str_has_prefix (address, "file://"))
	{
		GFile *file;
		
		file = g_file_new_for_uri (address);
		ephy_file_launch_handler ("text/plain", file, user_time);
		
		g_object_unref (file);
	}
	else
	{
		save_temp_source (embed, user_time);
	}
}

#define ABOUT_GROUP "About"

void
window_cmd_help_about (GtkAction *action,
		       GtkWidget *window)
{
	const char *licence_part[] = {
		N_("Web is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("The GNOME Web Browser is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with the GNOME Web Browser; if not, write to the Free Software Foundation, Inc., "
		   "51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA")
	};

	char *licence = NULL, *comments = NULL;
	GKeyFile *key_file;
	GError *error = NULL;
	char **list, **authors, **contributors, **past_authors, **artists, **documenters;
	gsize n_authors, n_contributors, n_past_authors, n_artists, n_documenters, i, j;

	key_file = g_key_file_new ();
	if (!g_key_file_load_from_file (key_file, DATADIR G_DIR_SEPARATOR_S "about.ini",
				        0, &error))
	{
		g_warning ("Couldn't load about data: %s\n", error->message);
		g_error_free (error);
		return;
	}

	list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Authors",
					   &n_authors, NULL);
	contributors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Contributors",
						   &n_contributors, NULL);
	past_authors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "PastAuthors",
						   &n_past_authors, NULL);

#define APPEND(_to,_from) \
	_to[i++] = g_strdup (_from);

#define APPEND_STRV_AND_FREE(_to,_from) \
	if (_from)\
	{\
		for (j = 0; _from[j] != NULL; ++j)\
		{\
			_to[i++] = _from[j];\
		}\
		g_free (_from);\
	}

	authors = g_new (char *, (list ? n_authors : 0) +
			 	 (contributors ? n_contributors : 0) +
				 (past_authors ? n_past_authors : 0) + 7 + 1);
	i = 0;
	APPEND_STRV_AND_FREE (authors, list);
	APPEND (authors, "");
	APPEND (authors, _("Contact us at:"));
	APPEND (authors, "<epiphany-list@gnome.org>");
	APPEND (authors, "");
	APPEND (authors, _("Contributors:"));
	APPEND_STRV_AND_FREE (authors, contributors);
	APPEND (authors, "");
	APPEND (authors, _("Past developers:"));
	APPEND_STRV_AND_FREE (authors, past_authors);
	authors[i++] = NULL;

	list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Artists", &n_artists, NULL);

	artists = g_new (char *, (list ? n_artists : 0) + 4 + 1);
	i = 0;
	APPEND_STRV_AND_FREE (artists, list);
	APPEND (artists, "");
	APPEND (artists, _("Contact us at:"));
	APPEND (artists, "<gnome-art-list@gnome.org>");
	APPEND (artists, "<gnome-themes-list@gnome.org>");
	artists[i++] = NULL;
	
	list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Documenters", &n_documenters, NULL);

	documenters = g_new (char *, (list ? n_documenters : 0) + 3 + 1);
	i = 0;
	APPEND_STRV_AND_FREE (documenters, list);
	APPEND (documenters, "");
	APPEND (documenters, _("Contact us at:"));
	APPEND (documenters, "<gnome-doc-list@gnome.org>");
	documenters[i++] = NULL;
	
#undef APPEND
#undef APPEND_STRV_AND_FREE

	g_key_file_free (key_file);

#ifdef HAVE_WEBKIT2
	comments = g_strdup_printf (_("A simple, clean, beautiful view of the web.\n"
				      "Powered by WebKit %d.%d.%d"),
				    webkit_get_major_version (),
				    webkit_get_minor_version (),
				    webkit_get_micro_version ());
#else
	comments = g_strdup_printf (_("A simple, clean, beautiful view of the web.\n"
	                              "Powered by WebKit %d.%d.%d"),
	                            webkit_major_version (),
	                            webkit_minor_version (),
	                            webkit_micro_version ());
#endif

	licence = g_strjoin ("\n\n",
			     _(licence_part[0]),
			     _(licence_part[1]),
			     _(licence_part[2]),
			    NULL);

	gtk_show_about_dialog (window ? GTK_WINDOW (window) : NULL,
			       "program-name", _("Web"),
			       "version", VERSION,
			       "copyright", "Copyright © 2002–2004 Marco Pesenti Gritti\n"
			                    "Copyright © 2003–2012 The Web Developers",
			       "artists", artists,
			       "authors", authors,
			       "comments", comments,
			       "documenters", documenters,
			       /* Translators: This is a special message that shouldn't be translated
			        * literally. It is used in the about box to give credits to
			        * the translators.
			        * Thus, you should translate it to your name and email address.
			        * You should also include other translators who have contributed to
			        * this translation; in that case, please write each of them on a separate
			        * line seperated by newlines (\n).
			        */
			       "translator-credits", _("translator-credits"),
			       "logo-icon-name", "web-browser",
			       "website", "http://www.gnome.org/projects/epiphany",
			       "website-label", _("Web Website"),
			       "license", licence,
			       "wrap-license", TRUE,
			       NULL);

	g_free (comments);
	g_free (licence);
	g_strfreev (artists);
	g_strfreev (authors);
	g_strfreev (documenters);
}

void
window_cmd_tabs_next (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *nb;

	nb = ephy_window_get_notebook (window);
	g_return_if_fail (nb != NULL);

	ephy_notebook_next_page (EPHY_NOTEBOOK (nb));
}

void
window_cmd_tabs_previous (GtkAction *action,
			  EphyWindow *window)
{
	GtkWidget *nb;

	nb = ephy_window_get_notebook (window);
	g_return_if_fail (nb != NULL);

	ephy_notebook_prev_page (EPHY_NOTEBOOK (nb));
}

void
window_cmd_tabs_move_left  (GtkAction *action,
			    EphyWindow *window)
{
	GtkWidget *child;
	GtkNotebook *notebook;
	int page;

	notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));
	page = gtk_notebook_get_current_page (notebook);
	if (page < 1) return;

	child = gtk_notebook_get_nth_page (notebook, page);
	gtk_notebook_reorder_child (notebook, child, page - 1);
}

void window_cmd_tabs_move_right (GtkAction *action,
				 EphyWindow *window)
{
	GtkWidget *child;
	GtkNotebook *notebook;
	int page, n_pages;

	notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));
	page = gtk_notebook_get_current_page (notebook);
	n_pages = gtk_notebook_get_n_pages (notebook) - 1;
	if (page > n_pages - 1) return;

	child = gtk_notebook_get_nth_page (notebook, page);
	gtk_notebook_reorder_child (notebook, child, page + 1);
}

void
window_cmd_tabs_detach  (GtkAction *action,
			 EphyWindow *window)
{
        EphyEmbed *embed;
        GtkNotebook *notebook;
        EphyWindow *new_window;

        notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));
        if (gtk_notebook_get_n_pages (notebook) <= 1)
                return;

        embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

        g_object_ref_sink (embed);
        gtk_notebook_remove_page (notebook, gtk_notebook_page_num (notebook, GTK_WIDGET (embed)));

        new_window = ephy_window_new ();
        ephy_embed_container_add_child (EPHY_EMBED_CONTAINER (new_window), embed, 0, FALSE);
        g_object_unref (embed);

        gtk_window_present (GTK_WINDOW (new_window));
}

void
window_cmd_load_location (GtkAction *action,
			  EphyWindow *window)
{
	const char *location;

	location = ephy_window_get_location (window);

	if (location)
	{
		EphyBookmarks *bookmarks;
		char *address;

		bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());

		address = ephy_bookmarks_resolve_address (bookmarks, location, NULL);
		g_return_if_fail (address != NULL);

		ephy_link_open (EPHY_LINK (window), address,
			        ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window)),
				ephy_link_flags_from_current_event ());
	}
}

void
window_cmd_browse_with_caret (GtkAction *action,
			      EphyWindow *window)
{
	gboolean active;
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
		(EPHY_EMBED_CONTAINER (window));
	
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	/* FIXME: perhaps a bit of a kludge; we check if there's an
	 * active embed because we don't want to show the dialog on
	 * startup when we sync the GtkAction with our GConf
	 * preference */
	if (active && embed)
	{
		GtkWidget *dialog;
		int response;

		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
						 _("Enable caret browsing mode?"));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("Pressing F7 turns caret browsing on or off. This feature "
							    "places a moveable cursor in web pages, allowing you to move "
							    "around with your keyboard. Do you want to enable caret browsing on?"));
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Enable"), GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

		response = gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_destroy (dialog);

		if (response == GTK_RESPONSE_CANCEL)
		{
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
			return;
		}
	}

	g_settings_set_boolean (EPHY_SETTINGS_MAIN,
				EPHY_PREFS_ENABLE_CARET_BROWSING, active);
}

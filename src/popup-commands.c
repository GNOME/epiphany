/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2017 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "popup-commands.h"

#include "ephy-downloads-manager.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-web-view.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

typedef enum {
  NEW_WINDOW,
  NEW_TAB
} LinkDestination;

static void
view_in_destination (EphyWindow     *window,
                     const char     *property_name,
                     LinkDestination destination)
{
  EphyEmbedEvent *event;
  GValue value = { 0, };
  EphyEmbed *embed;
  EphyEmbed *new_embed;
  EphyWebView *new_view;
  WebKitWebViewSessionState *session_state;
  EphyWindow *dest_window = window;
  EphyNewTabFlags flags = 0;

  event = ephy_window_get_context_event (window);
  g_assert (event != NULL);

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  ephy_embed_event_get_property (event, property_name, &value);
  switch (destination) {
    case NEW_WINDOW:
      dest_window = ephy_window_new ();
      break;
    case NEW_TAB:
      flags |= EPHY_NEW_TAB_APPEND_AFTER;
      break;
    default:
      g_assert_not_reached ();
  }

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  dest_window, embed, flags);

  new_view = ephy_embed_get_web_view (new_embed);
  session_state = webkit_web_view_get_session_state (WEBKIT_WEB_VIEW (ephy_embed_get_web_view (embed)));
  webkit_web_view_restore_session_state (WEBKIT_WEB_VIEW (new_view), session_state);
  webkit_web_view_session_state_unref (session_state);
  ephy_web_view_load_url (new_view, g_value_get_string (&value));
  g_value_unset (&value);
}

void
popup_cmd_link_in_new_window (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "link-uri", NEW_WINDOW);
}

void
popup_cmd_media_in_new_window (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "media-uri", NEW_WINDOW);
}

static void
popup_cmd_copy_to_clipboard (EphyWindow *window, const char *text)
{
  gtk_clipboard_set_text (gtk_clipboard_get_default (gdk_display_get_default ()),
                          text, -1);
}

void
popup_cmd_copy_link_address (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyEmbedEvent *event;
  guint context;
  const char *address;
  GValue value = { 0, };

  event = ephy_window_get_context_event (EPHY_WINDOW (user_data));
  g_assert (event != NULL);

  context = ephy_embed_event_get_context (event);

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    ephy_embed_event_get_property (event, "link-uri", &value);
    address = g_value_get_string (&value);

    if (g_str_has_prefix (address, "mailto:"))
      address = address + 7;

    popup_cmd_copy_to_clipboard (EPHY_WINDOW (user_data), address);
    g_value_unset (&value);
  }
}

static gboolean
cancel_download_idle_cb (EphyDownload *download)
{
  ephy_download_cancel (download);

  return FALSE;
}

typedef struct {
  char *title;
  EphyWindow *window;
} SavePropertyURLData;

static void
filename_suggested_cb (EphyDownload        *download,
                       const char          *suggested_filename,
                       SavePropertyURLData *data)
{
  GtkFileChooser *dialog;
  char *sanitized_filename;

  dialog = ephy_create_file_chooser (data->title,
                                     GTK_WIDGET (data->window),
                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                     EPHY_FILE_FILTER_NONE);
  gtk_file_chooser_set_do_overwrite_confirmation (dialog, TRUE);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY));

  sanitized_filename = ephy_sanitize_filename (g_strdup (suggested_filename));
  gtk_file_chooser_set_current_name (dialog, sanitized_filename);
  g_free (sanitized_filename);

  if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    char *uri;
    WebKitDownload *webkit_download;

    uri = gtk_file_chooser_get_uri (dialog);
    ephy_download_set_destination_uri (download, uri);
    g_free (uri);

    webkit_download = ephy_download_get_webkit_download (download);
    webkit_download_set_allow_overwrite (webkit_download, TRUE);

    ephy_downloads_manager_add_download (ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ()),
                                         download);
    g_settings_set_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY, gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog)));
  } else {
    g_idle_add_full (G_PRIORITY_DEFAULT,
                     (GSourceFunc)cancel_download_idle_cb,
                     g_object_ref (download),
                     g_object_unref);
  }

  g_object_unref (dialog);
  g_free (data->title);
  g_object_unref (data->window);
  g_free (data);

  g_object_unref (download);
}

static void
save_property_url (const char *title,
                   EphyWindow *window,
                   const char *property)
{
  EphyEmbedEvent *event;
  const char *location;
  EphyDownload *download;
  SavePropertyURLData *data;
  GValue value = G_VALUE_INIT;

  event = ephy_window_get_context_event (window);
  g_assert (event != NULL);

  ephy_embed_event_get_property (event, property, &value);
  location = g_value_get_string (&value);
  download = ephy_download_new_for_uri (location);
  data = g_new (SavePropertyURLData, 1);
  data->title = g_strdup (title);
  data->window = g_object_ref (window);
  g_signal_connect (download, "filename-suggested",
                    G_CALLBACK (filename_suggested_cb),
                    data);

  g_value_unset (&value);
}

void
popup_cmd_download_link_as (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  save_property_url (_("Save Link As"), EPHY_WINDOW (user_data), "link-uri");
}

void
popup_cmd_save_image_as (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  save_property_url (_("Save Image As"), EPHY_WINDOW (user_data), "image-uri");
}

void
popup_cmd_save_media_as (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  save_property_url (_("Save Media As"), EPHY_WINDOW (user_data), "media-uri");
}

static void
background_download_completed (EphyDownload *download,
                               GtkWidget    *window)
{
  const char *uri;
  GSettings *settings;

  uri = ephy_download_get_destination_uri (download);
  settings = ephy_settings_get ("org.gnome.desktop.background");
  g_settings_set_string (settings, "picture-uri", uri);
}

void
popup_cmd_set_image_as_background (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  EphyEmbedEvent *event;
  const char *location;
  char *dest_uri, *dest, *base, *base_converted;
  GValue value = { 0, };
  EphyDownload *download;

  event = ephy_window_get_context_event (EPHY_WINDOW (user_data));
  g_assert (event != NULL);

  ephy_embed_event_get_property (event, "image-uri", &value);
  location = g_value_get_string (&value);

  download = ephy_download_new_for_uri (location);

  base = g_path_get_basename (location);
  base_converted = g_filename_from_utf8 (base, -1, NULL, NULL, NULL);
  dest = g_build_filename (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES), base_converted, NULL);
  dest_uri = g_filename_to_uri (dest, NULL, NULL);

  ephy_download_set_destination_uri (download, dest_uri);
  ephy_downloads_manager_add_download (ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ()),
                                       download);
  g_object_unref (download);

  g_signal_connect (download, "completed",
                    G_CALLBACK (background_download_completed), user_data);

  g_value_unset (&value);
  g_free (base);
  g_free (base_converted);
  g_free (dest);
  g_free (dest_uri);
}

static void
popup_cmd_copy_location (EphyWindow *window,
                         const char *property_name)
{
  EphyEmbedEvent *event;
  const char *location;
  GValue value = { 0, };

  event = ephy_window_get_context_event (window);
  ephy_embed_event_get_property (event, property_name, &value);
  location = g_value_get_string (&value);
  popup_cmd_copy_to_clipboard (window, location);
  g_value_unset (&value);
}

void
popup_cmd_copy_image_location (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  popup_cmd_copy_location (EPHY_WINDOW (user_data), "image-uri");
}

void
popup_cmd_copy_media_location (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  popup_cmd_copy_location (EPHY_WINDOW (user_data), "media-uri");
}

void
popup_cmd_link_in_new_tab (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "link-uri", NEW_TAB);
}

void
popup_cmd_view_image_in_new_tab (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "image-uri", NEW_TAB);
}

void
popup_cmd_media_in_new_tab (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "media-uri", NEW_TAB);
}

void
popup_cmd_link_in_incognito_window (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  EphyEmbedEvent *event;
  GValue value = { 0, };

  event = ephy_window_get_context_event (EPHY_WINDOW (user_data));
  g_assert (event != NULL);

  ephy_embed_event_get_property (event, "link-uri", &value);
  ephy_open_incognito_window (g_value_get_string (&value));
  g_value_unset (&value);
}

void
popup_cmd_search_selection (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  EphyEmbed *embed, *new_embed;
  const char *search_term;
  char *search_url;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (user_data));
  g_assert (EPHY_IS_EMBED (embed));

  search_term = g_variant_get_string (parameter, NULL);
  search_url = ephy_embed_utils_autosearch_address (search_term);
  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (user_data), embed, EPHY_NEW_TAB_APPEND_AFTER | EPHY_NEW_TAB_JUMP);
  ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), search_url);
  g_free (search_url);
}

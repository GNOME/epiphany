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
#include "context-menu-commands.h"

#include "ephy-downloads-manager.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-web-view.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit/webkit.h>

typedef enum {
  NEW_WINDOW,
  NEW_TAB
} LinkDestination;

static void
view_in_destination (EphyWindow      *window,
                     const char      *property_name,
                     LinkDestination  destination)
{
  WebKitHitTestResult *hit_test_result;
  g_autofree char *value = NULL;
  EphyEmbed *embed;
  EphyEmbed *new_embed;
  EphyWebView *new_view;
  WebKitWebViewSessionState *session_state;
  EphyWindow *dest_window = window;
  EphyNewTabFlags flags = 0;

  hit_test_result = ephy_window_get_context_event (window);
  g_assert (hit_test_result != NULL);

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  g_object_get (hit_test_result, property_name, &value, NULL);
  switch (destination) {
    case NEW_WINDOW:
      dest_window = ephy_window_new ();
      break;
    case NEW_TAB:
      flags |= EPHY_NEW_TAB_APPEND_AFTER;
      if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))
        flags |= EPHY_NEW_TAB_JUMP;
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
  ephy_web_view_load_url (new_view, value);
}

void
context_cmd_link_in_new_window (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "link-uri", NEW_WINDOW);
}

void
context_cmd_media_in_new_window (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "media-uri", NEW_WINDOW);
}

static void
context_cmd_copy_to_clipboard (EphyWindow *window,
                               const char *text)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window));

  gdk_clipboard_set_text (clipboard, text);
}

void
context_cmd_copy_link_address (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  WebKitHitTestResult *hit_test_result;
  guint context;
  const char *address;

  hit_test_result = ephy_window_get_context_event (EPHY_WINDOW (user_data));
  g_assert (hit_test_result != NULL);

  context = webkit_hit_test_result_get_context (hit_test_result);

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    address = webkit_hit_test_result_get_link_uri (hit_test_result);

    if (g_str_has_prefix (address, "mailto:"))
      address = address + 7;

    context_cmd_copy_to_clipboard (EPHY_WINDOW (user_data), address);
  }
}

typedef struct {
  char *title;
  EphyWindow *window;
  EphyDownload *download;
} SavePropertyURLData;

static void
filename_confirmed_cb (GtkFileDialog       *dialog,
                       GAsyncResult        *result,
                       SavePropertyURLData *data)
{
  g_autoptr (GFile) file = NULL;

  file = gtk_file_dialog_save_finish (dialog, result, NULL);

  if (file) {
    g_autoptr (GFile) current_folder = NULL;
    WebKitDownload *webkit_download;

    ephy_download_set_destination (data->download, g_file_peek_path (file));

    webkit_download = ephy_download_get_webkit_download (data->download);
    webkit_download_set_allow_overwrite (webkit_download, TRUE);

    ephy_downloads_manager_add_download (ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ()),
                                         data->download);

    current_folder = g_file_get_parent (file);
    g_settings_set_string (EPHY_SETTINGS_WEB,
                           EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY,
                           g_file_peek_path (current_folder));
  } else {
    ephy_download_cancel (data->download);
  }

  g_free (data->title);
  g_object_unref (data->window);
  g_object_unref (data->download);
  g_free (data);
}

static gboolean
filename_suggested_cb (EphyDownload        *download,
                       const char          *suggested_filename,
                       SavePropertyURLData *data)
{
  GtkFileDialog *dialog;
  const char *last_directory_path;
  g_autofree char *sanitized_filename = NULL;

  dialog = gtk_file_dialog_new ();

  last_directory_path = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY);

  if (last_directory_path && last_directory_path[0]) {
    g_autoptr (GFile) last_directory = NULL;

    last_directory = g_file_new_for_path (last_directory_path);
    gtk_file_dialog_set_initial_folder (dialog, last_directory);
  }

  sanitized_filename = ephy_sanitize_filename (g_strdup (suggested_filename));
  gtk_file_dialog_set_initial_name (dialog, sanitized_filename);

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (data->window),
                        NULL,
                        (GAsyncReadyCallback)filename_confirmed_cb,
                        data);
  return TRUE;
}

static void
save_property_url (const char *title,
                   EphyWindow *window,
                   const char *property)
{
  WebKitHitTestResult *hit_test_result;
  g_autofree char *location = NULL;
  EphyDownload *download;
  SavePropertyURLData *data;

  hit_test_result = ephy_window_get_context_event (window);
  g_assert (hit_test_result != NULL);

  g_object_get (hit_test_result, property, &location, NULL);
  download = ephy_download_new_for_uri (location);
  data = g_new (SavePropertyURLData, 1);
  data->title = g_strdup (title);
  data->window = g_object_ref (window);
  data->download = download;
  g_signal_connect (download, "filename-suggested",
                    G_CALLBACK (filename_suggested_cb),
                    data);
}

void
context_cmd_download_link_as (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  save_property_url (_("Save Link As"), EPHY_WINDOW (user_data), "link-uri");
}

void
context_cmd_save_image_as (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  save_property_url (_("Save Image As"), EPHY_WINDOW (user_data), "image-uri");
}

void
context_cmd_save_media_as (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  save_property_url (_("Save Media As"), EPHY_WINDOW (user_data), "media-uri");
}

static void
background_download_completed (EphyDownload *download,
                               GtkWidget    *window)
{
  const char *path;
  g_autofree char *uri = NULL;
  g_autoptr (GError) error = NULL;

  path = ephy_download_get_destination (download);
  uri = g_filename_to_uri (path, NULL, &error);
  if (!uri)
    g_warning ("Failed to set desktop background: could not convert path %s to URI: %s", path, error->message);
  else
    g_settings_set_string (ephy_settings_get ("org.gnome.desktop.background"), "picture-uri", uri);
}

void
context_cmd_set_image_as_background (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  WebKitHitTestResult *hit_test_result;
  const char *location;
  g_autofree char *dest = NULL;
  g_autofree char *base = NULL;
  g_autofree char *base_converted = NULL;
  g_autoptr (EphyDownload) download = NULL;

  /* FIXME: Use wallpaper portal */
  if (ephy_is_running_inside_sandbox ())
    return;

  hit_test_result = ephy_window_get_context_event (EPHY_WINDOW (user_data));
  g_assert (hit_test_result != NULL);

  location = webkit_hit_test_result_get_image_uri (hit_test_result);

  download = ephy_download_new_for_uri (location);

  base = g_path_get_basename (location);
  base_converted = g_filename_from_utf8 (base, -1, NULL, NULL, NULL);
  dest = g_build_filename (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES), base_converted, NULL);

  ephy_download_set_destination (download, dest);
  ephy_downloads_manager_add_download (ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ()),
                                       download);

  g_signal_connect (download, "completed",
                    G_CALLBACK (background_download_completed), user_data);
}

static void
context_cmd_copy_location (EphyWindow *window,
                           const char *property_name)
{
  WebKitHitTestResult *hit_test_result;
  g_autofree char *location = NULL;

  hit_test_result = ephy_window_get_context_event (window);
  g_object_get (hit_test_result, property_name, &location, NULL);
  context_cmd_copy_to_clipboard (window, location);
}

void
context_cmd_copy_image_location (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  context_cmd_copy_location (EPHY_WINDOW (user_data), "image-uri");
}

void
context_cmd_copy_media_location (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  context_cmd_copy_location (EPHY_WINDOW (user_data), "media-uri");
}

void
context_cmd_link_in_new_tab (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "link-uri", NEW_TAB);
}

void
context_cmd_view_image_in_new_tab (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "image-uri", NEW_TAB);
}

void
context_cmd_media_in_new_tab (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  view_in_destination (EPHY_WINDOW (user_data), "media-uri", NEW_TAB);
}

void
context_cmd_link_in_incognito_window (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
  WebKitHitTestResult *hit_test_result;
  const char *link_uri;

  hit_test_result = ephy_window_get_context_event (EPHY_WINDOW (user_data));
  g_assert (hit_test_result != NULL);

  link_uri = webkit_hit_test_result_get_link_uri (hit_test_result);

  ephy_open_incognito_window (link_uri);
}

void
context_cmd_search_selection (GSimpleAction *action,
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

void
context_cmd_open_selection (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  EphyEmbed *embed;
  const char *open_term;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (user_data));
  g_assert (EPHY_IS_EMBED (embed));

  open_term = g_variant_get_string (parameter, NULL);
  ephy_web_view_load_url (ephy_embed_get_web_view (embed), open_term);
}

void
context_cmd_open_selection_in_new_tab (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  EphyEmbed *embed, *new_embed;
  const char *open_term;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (user_data));
  g_assert (EPHY_IS_EMBED (embed));

  open_term = g_variant_get_string (parameter, NULL);
  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (user_data), embed, EPHY_NEW_TAB_APPEND_AFTER | EPHY_NEW_TAB_JUMP);
  ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), open_term);
}

void
context_cmd_open_selection_in_new_window (GSimpleAction *action,
                                          GVariant      *parameter,
                                          gpointer       user_data)
{
  EphyEmbed *embed, *new_embed;
  const char *open_term;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (user_data));
  g_assert (EPHY_IS_EMBED (embed));

  open_term = g_variant_get_string (parameter, NULL);
  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  ephy_window_new (), embed, 0);
  ephy_web_view_load_url (ephy_embed_get_web_view (new_embed), open_term);
}

void
context_cmd_open_selection_in_incognito_window (GSimpleAction *action,
                                                GVariant      *parameter,
                                                gpointer       user_data)
{
  const char *open_term;

  open_term = g_variant_get_string (parameter, NULL);
  ephy_open_incognito_window (open_term);
}

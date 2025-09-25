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

#include "ephy-bookmark-properties.h"
#include "ephy-downloads-manager.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-web-view.h"
#include "window-commands.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libportal-gtk4/portal-gtk4.h>
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
  g_assert (hit_test_result);

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed);

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

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))
    gtk_widget_grab_focus (GTK_WIDGET (new_view));
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
  g_assert (hit_test_result);

  context = webkit_hit_test_result_get_context (hit_test_result);

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    address = webkit_hit_test_result_get_link_uri (hit_test_result);

    if (g_str_has_prefix (address, "mailto:"))
      address = address + 7;

    context_cmd_copy_to_clipboard (EPHY_WINDOW (user_data), address);
  }
}

static void
uri_launched_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GtkUriLauncher *launcher = GTK_URI_LAUNCHER (source);
  gboolean ret;
  g_autoptr (GError) error = NULL;

  ret = gtk_uri_launcher_launch_finish (launcher, result, &error);
  if (!ret)
    g_warning ("Failed to launch %s: %s", gtk_uri_launcher_get_uri (launcher), error->message);
}

void
context_cmd_send_via_email (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  EphyWindow *window = user_data;
  WebKitHitTestResult *hit_test_result;
  guint context;
  const char *label, *address;
  g_autofree char *subject = NULL;
  g_autofree char *body = NULL;
  g_autofree char *command = NULL;
  g_autoptr (GtkUriLauncher) launcher = NULL;

  hit_test_result = ephy_window_get_context_event (window);
  g_assert (hit_test_result);

  context = webkit_hit_test_result_get_context (hit_test_result);

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    label = webkit_hit_test_result_get_link_label (hit_test_result);
    address = webkit_hit_test_result_get_link_uri (hit_test_result);
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_assert (embed);

    label = ephy_embed_get_title (embed);
    address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));
  }

  subject = g_uri_escape_string (label, NULL, TRUE);
  body = g_uri_escape_string (address, NULL, TRUE);

  command = g_strconcat ("mailto:", "?Subject=", subject, "&Body=", body, NULL);

  launcher = gtk_uri_launcher_new (command);
  gtk_uri_launcher_launch (launcher, GTK_WINDOW (window), NULL, uri_launched_cb, NULL);
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
  g_assert (hit_test_result);

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
on_wallpaper_deleted (GObject      *source,
                      GAsyncResult *res,
                      gpointer      data)
{
  GFile *file = G_FILE (source);
  g_autoptr (GError) error = NULL;
  gboolean ret = g_file_delete_finish (file, res, &error);

  if (!ret) {
    g_warning ("Failed to delete downloaded wallpaper file: %s", error->message);
  }
}

static void
wallpaper_changed_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  XdpPortal *portal = XDP_PORTAL (source_object);
  g_autoptr (GError) error = NULL;
  g_autofree char *uri = user_data;
  g_autoptr (GFile) wallpaper_file = g_file_new_for_uri (uri);

  if (!xdp_portal_set_wallpaper_finish (portal, result, &error)) {
    g_warning ("Failed to set wallpaper: %s", error->message);
    g_clear_error (&error);
  }

  g_file_delete_async (wallpaper_file, G_PRIORITY_DEFAULT, NULL, on_wallpaper_deleted, NULL);
}

static void
background_download_completed (EphyDownload *download,
                               GtkWidget    *window)
{
  XdpPortal *portal = ephy_get_portal ();
  g_autoptr (XdpParent) parent_window = xdp_parent_new_gtk (GTK_WINDOW (window));
  const char *path;
  g_autofree char *uri = NULL;
  g_autoptr (GError) error = NULL;

  path = ephy_download_get_destination (download);
  uri = g_filename_to_uri (path, NULL, &error);
  if (!uri) {
    g_warning ("Could not convert filename `%s` to uri: %s", path, error->message);
    return;
  }
  xdp_portal_set_wallpaper (portal,
                            parent_window,
                            uri,
                            XDP_WALLPAPER_FLAG_BACKGROUND | XDP_WALLPAPER_FLAG_PREVIEW,
                            NULL,
                            wallpaper_changed_cb,
                            g_strdup (uri));
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

  hit_test_result = ephy_window_get_context_event (EPHY_WINDOW (user_data));
  g_assert (hit_test_result);

  location = webkit_hit_test_result_get_image_uri (hit_test_result);

  download = ephy_download_new_for_uri_internal (location);

  base = g_path_get_basename (location);
  base_converted = g_filename_from_utf8 (base, -1, NULL, NULL, NULL);
  dest = g_build_filename (g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD), base_converted, NULL);

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
  g_assert (hit_test_result);

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

  gtk_widget_grab_focus (GTK_WIDGET (ephy_embed_get_web_view (new_embed)));

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
  EphyWebView *web_view;
  const char *open_term;
  EphyNewTabFlags flags = EPHY_NEW_TAB_APPEND_AFTER;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (user_data));
  g_assert (EPHY_IS_EMBED (embed));

  open_term = g_variant_get_string (parameter, NULL);
  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))
    flags |= EPHY_NEW_TAB_JUMP;

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (user_data), embed, flags);
  web_view = ephy_embed_get_web_view (new_embed);
  ephy_web_view_load_url (web_view, open_term);

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB))
    gtk_widget_grab_focus (GTK_WIDGET (web_view));
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

void
context_cmd_add_link_to_bookmarks (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  WebKitHitTestResult *hit_test_result;
  guint context;
  const char *address;

  hit_test_result = ephy_window_get_context_event (EPHY_WINDOW (user_data));
  g_assert (hit_test_result);

  context = webkit_hit_test_result_get_context (hit_test_result);

  if (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) {
    EphyWindow *window = EPHY_WINDOW (user_data);
    GtkWidget *dialog;

    address = webkit_hit_test_result_get_link_uri (hit_test_result);

    dialog = ephy_bookmark_properties_new_for_link (window, address);
    adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
  }
}

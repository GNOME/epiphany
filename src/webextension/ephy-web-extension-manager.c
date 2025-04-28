/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019-2022 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include "ephy-browser-action.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-header-bar.h"
#include "ephy-location-entry.h"
#include "ephy-notification.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-web-extension.h"
#include "ephy-web-extension-manager.h"
#include "ephy-web-view.h"

#include "api/alarms.h"
#include "api/browseraction.h"
#include "api/commands.h"
#include "api/cookies.h"
#include "api/downloads.h"
#include "api/menus.h"
#include "api/notifications.h"
#include "api/pageaction.h"
#include "api/runtime.h"
#include "api/storage.h"
#include "api/tabs.h"
#include "api/windows.h"

#include <adwaita.h>
#include <archive.h>
#include <archive_entry.h>
#include <json-glib/json-glib.h>

static void handle_message_reply (EphyWebExtension *web_extension,
                                  JsonArray        *args);

static char *get_translation_contents (EphyWebExtension *web_extension);

struct _EphyWebExtensionManager {
  GObject parent_instance;

  GCancellable *cancellable;
  GPtrArray *web_extensions;
  GHashTable *page_action_map;

  GHashTable *browser_action_map;
  GListStore *browser_actions;

  GHashTable *user_agent_overrides;

  GHashTable *background_web_views;
  GHashTable *popup_web_views;

  GHashTable *pending_messages;
};

G_DEFINE_FINAL_TYPE (EphyWebExtensionManager, ephy_web_extension_manager, G_TYPE_OBJECT)

EphyWebExtensionApiHandler api_handlers[] = {
  {"alarms", ephy_web_extension_api_alarms_handler},
  {"browserAction", ephy_web_extension_api_browseraction_handler},
  {"commands", ephy_web_extension_api_commands_handler},
  {"cookies", ephy_web_extension_api_cookies_handler},
  {"downloads", ephy_web_extension_api_downloads_handler},
  {"menus", ephy_web_extension_api_menus_handler},
  {"notifications", ephy_web_extension_api_notifications_handler},
  {"pageAction", ephy_web_extension_api_pageaction_handler},
  {"runtime", ephy_web_extension_api_runtime_handler},
  {"storage", ephy_web_extension_api_storage_handler},
  {"tabs", ephy_web_extension_api_tabs_handler},
  {"windows", ephy_web_extension_api_windows_handler},
  {NULL, NULL},
};

enum {
  CHANGED,
  SHOW_BROWSER_ACTION,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GHashTable *
create_user_agent_overrides (void)
{
  GHashTable *overrides = g_hash_table_new (g_str_hash, g_str_equal);

/* We add Epiphany to the UA so proactive extensions can rely on it always. */
#define FIREFOX_OVERRIDE "Mozilla/5.0 (X11; Linux x86_64; rv:101.0) Gecko/20100101 Firefox/101.0 Epiphany/" EPHY_VERSION

  /* FIXME: These names are post translation. */

  /* Bitwarden has Safari specific hacks that cannot work on Epiphany such as calling out to their host mac app. */
  g_hash_table_insert (overrides, "Bitwarden - Free Password Manager", FIREFOX_OVERRIDE);

  return overrides;
}

static GVariant *
create_extension_data_variant (EphyWebExtension *extension)
{
  g_auto (GVariantDict) dict = G_VARIANT_DICT_INIT (NULL);
  g_autofree char *translations = get_translation_contents (extension);

  g_variant_dict_insert (&dict, "manifest", "s", ephy_web_extension_get_manifest (extension));
  g_variant_dict_insert (&dict, "translations", "s", translations);
  g_variant_dict_insert (&dict, "has-background-page", "b", ephy_web_extension_has_background_web_view (extension));

  return g_variant_dict_end (&dict);
}

static GVariant *
ephy_web_extension_manager_get_extension_initialization_data (EphyWebExtensionManager *self)
{
  g_auto (GVariantDict) dict = G_VARIANT_DICT_INIT (NULL);

  for (guint i = 0; i < self->web_extensions->len; i++) {
    EphyWebExtension *extension = g_ptr_array_index (self->web_extensions, i);

    g_variant_dict_insert_value (&dict,
                                 ephy_web_extension_get_guid (extension),
                                 create_extension_data_variant (extension));
  }

  return g_variant_dict_end (&dict);
}

static void
ephy_web_extension_manager_update_extension_initialization_data (EphyWebExtensionManager *self)
{
  EphyEmbedShell *embed = ephy_embed_shell_get_default ();
  ephy_embed_shell_set_web_extension_initialization_data (embed, ephy_web_extension_manager_get_extension_initialization_data (self));
}

static void
ephy_web_extension_manager_add_to_list (EphyWebExtensionManager *self,
                                        EphyWebExtension        *web_extension)
{
  g_ptr_array_add (self->web_extensions, g_object_ref (web_extension));
  ephy_web_extension_manager_update_extension_initialization_data (self);
  g_signal_emit (self, signals[CHANGED], 0);
}

static void
ephy_web_extension_manager_remove_from_list (EphyWebExtensionManager *self,
                                             EphyWebExtension        *web_extension)
{
  g_ptr_array_remove (self->web_extensions, web_extension);
  ephy_web_extension_manager_update_extension_initialization_data (self);
  g_signal_emit (self, signals[CHANGED], 0);
}

static EphyWebExtension *
ephy_web_extension_manager_get_extension_by_guid (EphyWebExtensionManager *self,
                                                  const char              *guid)
{
  for (guint i = 0; i < self->web_extensions->len; i++) {
    EphyWebExtension *web_extension = g_ptr_array_index (self->web_extensions, i);
    if (g_strcmp0 (guid, ephy_web_extension_get_guid (web_extension)) == 0)
      return web_extension;
  }

  return NULL;
}

static void
on_web_extension_loaded (GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GFile *target = G_FILE (source_object);
  g_autoptr (GError) error = NULL;
  g_autoptr (EphyWebExtension) web_extension = NULL;
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (user_data);

  web_extension = ephy_web_extension_load_finished (source_object, result, &error);
  if (!web_extension) {
    g_warning ("Failed to load extension %s: %s", g_file_peek_path (target), error->message);
    return;
  }

  ephy_web_extension_manager_add_to_list (self, web_extension);

  if (ephy_web_extension_manager_is_active (self, web_extension))
    ephy_web_extension_manager_set_active (self, web_extension, TRUE);
}

static void
scan_directory_ready_cb (GFile        *file,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  EphyWebExtensionManager *self = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFileEnumerator) enumerator = NULL;

  enumerator = g_file_enumerate_children_finish (file, result, &error);

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
      g_warning ("Failed to scan extensions directory: %s", error->message);
    return;
  }

  while (TRUE) {
    GFileInfo *info;
    GFile *child;

    if (!g_file_enumerator_iterate (enumerator, &info, &child, NULL, &error)) {
      g_warning ("Error enumerating extension directory: %s", error->message);
      break;
    }
    if (!info)
      break;

    ephy_web_extension_load_async (child, info, self->cancellable, on_web_extension_loaded, self);
  }
}

static void
ephy_web_extension_manager_scan_directory_async (EphyWebExtensionManager *self,
                                                 const char              *extension_dir_path)
{
  g_autoptr (GFile) extension_dir = g_file_new_for_path (extension_dir_path);

  g_file_enumerate_children_async (extension_dir,
                                   G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   G_PRIORITY_DEFAULT,
                                   self->cancellable,
                                   (GAsyncReadyCallback)scan_directory_ready_cb,
                                   self);
}

static void
ephy_webextension_scheme_cb (WebKitURISchemeRequest *request,
                             gpointer                user_data)
{
  EphyWebExtensionManager *self = ephy_web_extension_manager_get_default ();
  WebKitWebView *initiating_view;
  EphyWebExtension *web_extension;
  EphyWebExtension *target_web_extension;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GUri) uri = NULL;
  const char *initiating_uri_string;
  g_autoptr (GUri) initiating_uri = NULL;
  g_autoptr (GError) error = NULL;
  const char *initating_host;
  const unsigned char *data;
  gsize length;

  uri = g_uri_parse (webkit_uri_scheme_request_get_uri (request),
                     G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_SCHEME_NORMALIZE,
                     &error);
  if (!uri) {
    webkit_uri_scheme_request_finish_error (request, g_steal_pointer (&error));
    return;
  }

  target_web_extension = ephy_web_extension_manager_get_extension_by_guid (self, g_uri_get_host (uri));
  if (!target_web_extension) {
    error = g_error_new (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_HOST, "Could not find extension %s", g_uri_get_host (uri));
    webkit_uri_scheme_request_finish_error (request, g_steal_pointer (&error));
    return;
  }

  /* Figure out the source of this request. */
  initiating_view = webkit_uri_scheme_request_get_web_view (request);
  if (EPHY_IS_WEB_VIEW (initiating_view))
    initiating_uri_string = ephy_web_view_get_address (EPHY_WEB_VIEW (initiating_view));
  else
    initiating_uri_string = webkit_web_view_get_uri (initiating_view);
  if (!initiating_uri_string) {
    webkit_uri_scheme_request_finish_error (request, g_error_new (WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_HOST, _("Failed to determine initiating URI")));
    return;
  }

  initiating_uri = g_uri_parse (initiating_uri_string, G_URI_FLAGS_NON_DNS, &error);
  if (!initiating_uri) {
    webkit_uri_scheme_request_finish_error (request, g_steal_pointer (&error));
    return;
  }

  initating_host = g_uri_get_host (initiating_uri);
  web_extension = ephy_web_extension_manager_get_extension_by_guid (self, initating_host);

  /* If this is not originating from the same WebExtension view we must find it and filter it by web_accessible_resources. */
  if (web_extension != target_web_extension) {
    if (!ephy_web_extension_has_web_accessible_resource (target_web_extension, g_uri_get_path (uri) + 1)) {
      error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "'%s' is not a web_accessible_resource", g_uri_get_path (uri));
      webkit_uri_scheme_request_finish_error (request, g_steal_pointer (&error));
      return;
    }
  }

  data = ephy_web_extension_get_resource (target_web_extension, g_uri_get_path (uri) + 1, &length);
  if (!data) {
    error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "'%s' was not found", g_uri_get_path (uri));
    webkit_uri_scheme_request_finish_error (request, g_steal_pointer (&error));
    return;
  }

  stream = g_memory_input_stream_new_from_data (data, length, NULL);
  webkit_uri_scheme_request_finish (request, stream, length, NULL);
}

static void
ephy_web_extension_manager_constructed (GObject *object)
{
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (object);
  g_autofree char *dir = g_build_filename (ephy_default_profile_dir (), "web_extensions", NULL);

  self->background_web_views = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_object_unref);
  self->popup_web_views = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_ptr_array_free);
  self->page_action_map = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)g_hash_table_destroy);
  self->browser_action_map = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)g_object_unref);
  self->browser_actions = g_list_store_new (EPHY_TYPE_BROWSER_ACTION);
  self->pending_messages = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_hash_table_destroy);
  self->web_extensions = g_ptr_array_new_full (0, g_object_unref);
  self->user_agent_overrides = create_user_agent_overrides ();

  ephy_web_extension_manager_scan_directory_async (self, dir);
}

static void
ephy_web_extension_manager_dispose (GObject *object)
{
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (object);

  ephy_web_extension_api_downloads_dispose (self);

  g_list_store_remove_all (self->browser_actions);

  g_clear_pointer (&self->background_web_views, g_hash_table_destroy);
  g_clear_pointer (&self->popup_web_views, g_hash_table_destroy);
  g_clear_object (&self->browser_actions);
  g_clear_pointer (&self->browser_action_map, g_hash_table_destroy);
  g_clear_pointer (&self->page_action_map, g_hash_table_destroy);
  g_clear_pointer (&self->pending_messages, g_hash_table_destroy);
  g_clear_pointer (&self->web_extensions, g_ptr_array_unref);
  g_clear_pointer (&self->user_agent_overrides, g_hash_table_destroy);
}

static void
ephy_web_extension_manager_class_init (EphyWebExtensionManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_web_extension_manager_constructed;
  object_class->dispose = ephy_web_extension_manager_dispose;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[SHOW_BROWSER_ACTION] =
    g_signal_new ("show-browser-action",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1, EPHY_TYPE_BROWSER_ACTION);
}

static void
ephy_web_extension_manager_init (EphyWebExtensionManager *self)
{
  WebKitWebContext *web_context;

  web_context = ephy_embed_shell_get_web_context (ephy_embed_shell_get_default ());
  webkit_web_context_register_uri_scheme (web_context, "ephy-webextension", ephy_webextension_scheme_cb, NULL, NULL);
  webkit_security_manager_register_uri_scheme_as_secure (webkit_web_context_get_security_manager (web_context),
                                                         "ephy-webextension");

  ephy_web_extension_api_downloads_init (self);
}

EphyWebExtensionManager *
ephy_web_extension_manager_get_default (void)
{
  static EphyWebExtensionManager *manager = NULL;

  if (!manager)
    manager = g_object_new (EPHY_TYPE_WEB_EXTENSION_MANAGER, NULL);

  return manager;
}

GPtrArray *
ephy_web_extension_manager_get_web_extensions (EphyWebExtensionManager *self)
{
  return self->web_extensions;
}

void
ephy_web_extension_manager_open_inspector (EphyWebExtensionManager *self,
                                           EphyWebExtension        *web_extension)
{
  WebKitWebView *background_page = ephy_web_extension_manager_get_background_web_view (self, web_extension);

  if (!background_page)
    return;

  webkit_web_inspector_show (webkit_web_view_get_inspector (background_page));
}

/**
 * Installs/Adds all web_extensions to new EphyWindow.
 */
void
ephy_web_extension_manager_install_actions (EphyWebExtensionManager *self,
                                            EphyWindow              *window)
{
  for (guint i = 0; i < self->web_extensions->len; i++)
    ephy_web_extension_manager_add_web_extension_to_window (self, g_ptr_array_index (self->web_extensions, i), window);
}

void
on_new_web_extension_loaded (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  EphyWebExtension *web_extension;
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (user_data);

  web_extension = ephy_web_extension_load_finished (source_object, result, &error);
  if (!web_extension) {
    return;
  }

  ephy_web_extension_manager_add_to_list (self, web_extension);
}

static char *
decompress_xpi_finish (EphyWebExtensionManager  *self,
                       GAsyncResult             *result,
                       GError                  **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
on_extension_decompressed (GObject      *source,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (user_data);
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) target = NULL;
  GFileInfo *file_info;
  g_autofree char *path = decompress_xpi_finish (self, res, &error);

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not decompress WebExtension: %s", error->message);
    return;
  }

  target = g_file_new_for_path (path);
  file_info = g_file_query_info (target, G_FILE_ATTRIBUTE_STANDARD_TYPE, 0, NULL, &error);
  if (!file_info) {
    g_warning ("Failed to query file info: %s", error->message);
    return;
  }

  ephy_web_extension_load_async (g_steal_pointer (&target), file_info, self->cancellable, on_new_web_extension_loaded, self);
}

static int
copy_data (struct archive *ar,
           struct archive *aw)
{
  int r;
  const void *buff;
  size_t size;
  la_int64_t offset;

  for (;;) {
    r = archive_read_data_block (ar, &buff, &size, &offset);
    if (r == ARCHIVE_EOF)
      return (ARCHIVE_OK);
    if (r < ARCHIVE_OK)
      return (r);
    r = archive_write_data_block (aw, buff, size, offset);
    if (r < ARCHIVE_OK) {
      g_warning ("Failed to copy archive data: %s", archive_error_string (aw));
      return (r);
    }
  }
}

static void
decompress_xpi_thread (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  GFile *file = G_FILE (source_object);
  GFile *web_extensions_dir = task_data;
  struct archive *archive;
  struct archive *ext;
  int flags;
  int ret;
  const char *filename = g_file_get_path (file);
  g_autofree char *path = NULL;
  g_autofree char *basename = NULL;

  flags = ARCHIVE_EXTRACT_TIME;
  flags |= ARCHIVE_EXTRACT_PERM;
  flags |= ARCHIVE_EXTRACT_ACL;
  flags |= ARCHIVE_EXTRACT_FFLAGS;

  archive = archive_read_new ();
  archive_read_support_format_all (archive);
  archive_read_support_filter_all (archive);

  ext = archive_write_disk_new ();
  archive_write_disk_set_options (ext, flags);
  archive_write_disk_set_standard_lookup (ext);

  ret = archive_read_open_filename (archive, filename, 10240);
  if (ret) {
    g_warning ("Could not open archive: %s", filename);
    return;
  }

  basename = g_file_get_basename (file);
  path = g_build_filename (g_file_peek_path (web_extensions_dir), basename, NULL);

  while (1) {
    struct archive_entry *entry;
    g_autofree char *full_path = NULL;

    ret = archive_read_next_header (archive, &entry);
    if (ret == ARCHIVE_EOF)
      break;

    if (ret < ARCHIVE_OK)
      g_warning ("Error extracting archive: %s", archive_error_string (archive));

    if (ret < ARCHIVE_WARN)
      return;

    full_path = g_build_filename (path, archive_entry_pathname (entry), NULL);
    archive_entry_set_pathname (entry, full_path);
    ret = archive_write_header (ext, entry);
    if (ret < ARCHIVE_OK)
      g_warning ("Could not write archive: %s", archive_error_string (ext));
    else if (archive_entry_size (entry) > 0) {
      ret = copy_data (archive, ext);
      if (ret < ARCHIVE_OK)
        g_warning ("Could not copy archive data: %s", archive_error_string (ext));
      if (ret < ARCHIVE_WARN)
        return;
    }

    ret = archive_write_finish_entry (ext);
    if (ret < ARCHIVE_OK)
      g_warning ("Could not finish archive: %s", archive_error_string (ext));
    if (ret < ARCHIVE_WARN)
      return;
  }

  archive_read_close (archive);
  archive_read_free (archive);
  archive_write_close (ext);
  archive_write_free (ext);

  g_task_return_pointer (task, g_steal_pointer (&path), g_free);
}

static void
decompress_xpi (GFile               *extension,
                GFile               *web_extensions_dir,
                GCancellable        *cancellable,
                GAsyncReadyCallback  callback,
                gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (extension);
  g_assert (web_extensions_dir);

  task = g_task_new (extension, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (web_extensions_dir), g_object_unref);
  g_task_set_return_on_cancel (task, TRUE);

  g_task_run_in_thread (task, decompress_xpi_thread);
}

/**
 * Install a new web web_extension into the local web_extension directory.
 * File should only point to a manifest.json or a .xpi file
 */
void
ephy_web_extension_manager_install (EphyWebExtensionManager *self,
                                    GFile                   *file)
{
  g_autoptr (GFile) target = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  gboolean is_xpi = FALSE;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) web_extensions_dir = NULL;
  g_autofree char *basename = NULL;
  const char *path;

  web_extensions_dir = g_file_new_build_filename (ephy_default_profile_dir (), "web_extensions", NULL);
  path = g_file_peek_path (file);
  g_assert (path);
  is_xpi = g_str_has_suffix (path, ".xpi");

  /* FIXME: Make this async. */

  if (is_xpi) {
    decompress_xpi (file, web_extensions_dir, self->cancellable, on_extension_decompressed, self);
  } else {
    /* Otherwise we copy the parent directory. */
    g_autoptr (GFile) parent = g_file_get_parent (file);
    basename = g_file_get_basename (parent);
    target = g_file_get_child (web_extensions_dir, basename);

    ephy_copy_directory (g_file_peek_path (parent), g_file_peek_path (target));

    if (target) {
      file_info = g_file_query_info (target, G_FILE_ATTRIBUTE_STANDARD_TYPE, 0, self->cancellable, &error);
      if (!file_info) {
        g_warning ("Failed to query file info: %s", error->message);
        return;
      }

      ephy_web_extension_load_async (g_steal_pointer (&target), file_info, self->cancellable, on_new_web_extension_loaded, self);
    }
  }
}

void
ephy_web_extension_manager_uninstall (EphyWebExtensionManager *self,
                                      EphyWebExtension        *web_extension)
{
  if (ephy_web_extension_manager_is_active (self, web_extension))
    ephy_web_extension_manager_set_active (self, web_extension, FALSE);

  ephy_web_extension_remove (web_extension);
  ephy_web_extension_manager_remove_from_list (self, web_extension);
}

void
ephy_web_extension_manager_update_location_entry (EphyWebExtensionManager *self,
                                                  EphyWindow              *window)
{
  GtkWidget *title_widget;
  EphyLocationEntry *lentry;
  EphyTabView *tab_view = ephy_window_get_tab_view (EPHY_WINDOW (window));
  GtkWidget *page = ephy_tab_view_get_selected_page (tab_view);
  EphyWebView *web_view;

  if (!page)
    return;

  web_view = ephy_embed_get_web_view (EPHY_EMBED (page));
  title_widget = GTK_WIDGET (ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (ephy_window_get_header_bar (window))));
  if (!EPHY_IS_LOCATION_ENTRY (title_widget))
    return;

  lentry = EPHY_LOCATION_ENTRY (title_widget);

  ephy_location_entry_page_action_clear (lentry);

  for (guint i = 0; i < self->web_extensions->len; i++) {
    EphyWebExtension *web_extension = g_ptr_array_index (self->web_extensions, i);
    GtkWidget *action = ephy_web_extension_manager_get_page_action (self, web_extension, web_view);

    if (action)
      ephy_location_entry_page_action_add (lentry, action);
  }
}

WebKitWebView *
ephy_web_extension_manager_get_background_web_view (EphyWebExtensionManager *self,
                                                    EphyWebExtension        *web_extension)
{
  return g_hash_table_lookup (self->background_web_views, web_extension);
}

static void
ephy_web_extension_manager_set_background_web_view (EphyWebExtensionManager *self,
                                                    EphyWebExtension        *web_extension,
                                                    WebKitWebView           *web_view)
{
  g_hash_table_insert (self->background_web_views, web_extension, web_view);
}

void
ephy_web_extension_manager_append_context_menu (EphyWebExtensionManager *self,
                                                WebKitWebView           *web_view,
                                                WebKitContextMenu       *context_menu,
                                                WebKitHitTestResult     *hit_test_result,
                                                GdkModifierType          modifiers,
                                                gboolean                 is_audio,
                                                gboolean                 is_video)
{
  gboolean inserted_separator = FALSE;

  for (guint i = 0; i < self->web_extensions->len; i++) {
    EphyWebExtension *extension = g_ptr_array_index (self->web_extensions, i);
    WebKitContextMenuItem *item;

    item = ephy_web_extension_api_menus_create_context_menu (extension, web_view, context_menu,
                                                             hit_test_result, modifiers, is_audio, is_video);
    if (item) {
      if (!inserted_separator) {
        webkit_context_menu_append (context_menu, webkit_context_menu_item_new_separator ());
        inserted_separator = TRUE;
      }
      webkit_context_menu_append (context_menu, item);
    }
  }
}

static void
page_action_clicked (GtkButton *button,
                     gpointer   user_data)
{
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);
  EphyShell *shell = ephy_shell_get_default ();
  EphyWebView *view = EPHY_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  g_autofree char *json = NULL;
  g_autofree char *script = NULL;
  EphyWebExtensionManager *self = ephy_web_extension_manager_get_default ();
  WebKitWebView *web_view = ephy_web_extension_manager_get_background_web_view (self, web_extension);

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "url");
  json_builder_add_string_value (builder, ephy_web_view_get_address (view));
  json_builder_set_member_name (builder, "id");
  json_builder_add_int_value (builder, ephy_web_view_get_uid (view));
  json_builder_end_object (builder);

  root = json_builder_get_root (builder);

  json = json_to_string (root, FALSE);

  script = g_strdup_printf ("window.browser.pageAction.onClicked._emit(%s);", json);
  webkit_web_view_evaluate_javascript (web_view,
                                       script, -1,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);
}

static GtkWidget *
create_page_action_widget (EphyWebExtensionManager *self,
                           EphyWebExtension        *web_extension)
{
  GtkWidget *image;
  GtkWidget *button;

  button = gtk_button_new ();
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

  image = gtk_image_new ();
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_button_set_child (GTK_BUTTON (button), image);

  gtk_widget_add_css_class (button, "image-button");
  gtk_widget_add_css_class (button, "entry-icon");
  gtk_widget_add_css_class (button, "end");

  g_signal_connect_object (button, "clicked",
                           G_CALLBACK (page_action_clicked), web_extension, 0);

  return g_object_ref (button);
}

static void
respond_with_error (WebKitUserMessage *message,
                    const char        *error)
{
  WebKitUserMessage *reply = webkit_user_message_new ("error", g_variant_new_string (error));
  webkit_user_message_send_reply (message, reply);
}

typedef struct {
  EphyWebExtensionSender *sender;
  WebKitUserMessage *message;
  JsonNode *args;
} ApiHandlerData;

static void
api_handler_data_free (ApiHandlerData *data)
{
  g_object_unref (data->message);
  json_node_unref (data->args);
  g_free (data->sender);
  g_free (data);
}

static ApiHandlerData *
api_handler_data_new (EphyWebExtension  *extension,
                      WebKitWebView     *view,
                      guint64            frame_id,
                      WebKitUserMessage *message,
                      JsonNode          *args)
{
  ApiHandlerData *data = g_new (ApiHandlerData, 1);
  data->message = g_object_ref (message);
  data->args = json_node_ref (args);
  data->sender = g_new (EphyWebExtensionSender, 1);
  data->sender->extension = extension;
  data->sender->view = view;
  data->sender->frame_id = frame_id;
  return data;
}

static void
on_web_extension_api_handler_finish (EphyWebExtension *web_extension,
                                     GAsyncResult     *result,
                                     gpointer          user_data)
{
  g_autoptr (GError) error = NULL;
  GTask *task = G_TASK (result);
  ApiHandlerData *data = g_task_get_task_data (task);
  g_autofree char *json = g_task_propagate_pointer (task, &error);
  WebKitUserMessage *reply;

  if (error) {
    respond_with_error (data->message, error->message);
  } else {
    reply = webkit_user_message_new ("", g_variant_new_string (json ? json : ""));
    webkit_user_message_send_reply (data->message, reply);
  }

  g_object_unref (task);
}

static gboolean
extension_view_handle_user_message (WebKitWebView     *web_view,
                                    WebKitUserMessage *message,
                                    gpointer           user_data)
{
  EphyWebExtension *web_extension = user_data;
  const char *name = webkit_user_message_get_name (message);
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) split = NULL;
  const char *guid;
  const char *json_string;
  g_autoptr (JsonNode) json = NULL;
  JsonArray *json_args;
  guint64 frame_id;

  g_variant_get (webkit_user_message_get_parameters (message), "(&st&s)", &guid, &frame_id, &json_string);

  LOG ("%s(): Called for %s, function %s (%s)\n", __FUNCTION__, ephy_web_extension_get_name (web_extension), name, json_string);

  json = json_from_string (json_string, &error);
  if (!json || !JSON_NODE_HOLDS_ARRAY (json)) {
    g_warning ("Received invalid JSON: %s", error ? error->message : "JSON was not an array");
    respond_with_error (message, "Invalid function arguments");
    return TRUE;
  }

  json_args = json_node_get_array (json);
  json_array_seal (json_args);

  /* Private API for message replies handled by the manager. */
  if (strcmp (name, "runtime._sendMessageReply") == 0) {
    WebKitUserMessage *reply = webkit_user_message_new ("", g_variant_new_string (""));
    handle_message_reply (web_extension, json_args);
    webkit_user_message_send_reply (message, reply);
    return TRUE;
  }

  split = g_strsplit (name, ".", 2);
  if (g_strv_length (split) != 2) {
    respond_with_error (message, "Invalid function name");
    return TRUE;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (api_handlers); idx++) {
    EphyWebExtensionApiHandler handler = api_handlers[idx];

    if (g_strcmp0 (handler.name, split[0]) == 0) {
      /* TODO: Cancellable */
      GTask *task = g_task_new (web_extension, NULL, (GAsyncReadyCallback)on_web_extension_api_handler_finish, NULL);
      ApiHandlerData *data = api_handler_data_new (web_extension, web_view, frame_id, message, json);
      g_task_set_task_data (task, data, (GDestroyNotify)api_handler_data_free);

      handler.execute (data->sender, split[1], json_args, task);
      return TRUE;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);
  respond_with_error (message, "Not Implemented");
  return TRUE;
}

static gboolean
content_scripts_handle_user_message (WebKitWebView     *web_view,
                                     WebKitUserMessage *message,
                                     gpointer           user_data)
{
  EphyWebExtension *web_extension = user_data;
  g_autoptr (GError) error = NULL;
  const char *name = webkit_user_message_get_name (message);
  g_auto (GStrv) split = NULL;
  const char *json_string;
  g_autoptr (JsonNode) json = NULL;
  JsonArray *json_args;
  const char *extension_guid;
  GTask *task;
  guint64 frame_id;

  g_variant_get (webkit_user_message_get_parameters (message), "(&st&s)", &extension_guid, &frame_id, &json_string);

  /* Multiple extensions can send user-messages from the same web-view, so only the target one handles this. */
  if (strcmp (extension_guid, ephy_web_extension_get_guid (web_extension)) != 0)
    return FALSE;

  LOG ("%s(): Called for %s, function %s (%s)\n", __FUNCTION__, ephy_web_extension_get_name (web_extension), name, json_string);

  json = json_from_string (json_string, &error);
  if (!json || !JSON_NODE_HOLDS_ARRAY (json)) {
    g_warning ("Received invalid JSON: %s", error ? error->message : "JSON was not an array");
    respond_with_error (message, "Invalid function arguments");
    return TRUE;
  }

  json_args = json_node_get_array (json);
  json_array_seal (json_args);

  /* Private API for message replies handled by the manager. */
  if (strcmp (name, "runtime._sendMessageReply") == 0) {
    WebKitUserMessage *reply = webkit_user_message_new ("", g_variant_new_string (""));
    handle_message_reply (web_extension, json_args);
    webkit_user_message_send_reply (message, reply);
    return TRUE;
  }

  split = g_strsplit (name, ".", 2);
  if (g_strv_length (split) != 2) {
    respond_with_error (message, "Invalid function name");
    return TRUE;
  }

  /* Content Scripts are very limited in their API access compared to extension views so we handle them individually. */
  if (strcmp (split[0], "storage") == 0) {
    ApiHandlerData *data = api_handler_data_new (web_extension, web_view, frame_id, message, json);
    task = g_task_new (web_extension, NULL, (GAsyncReadyCallback)on_web_extension_api_handler_finish, NULL);
    g_task_set_task_data (task, data, (GDestroyNotify)api_handler_data_free);

    ephy_web_extension_api_storage_handler (data->sender, split[1], json_args, task);
    return TRUE;
  }

  if (strcmp (name, "runtime.sendMessage") == 0) {
    ApiHandlerData *data = api_handler_data_new (web_extension, web_view, frame_id, message, json);
    task = g_task_new (web_extension, NULL, (GAsyncReadyCallback)on_web_extension_api_handler_finish, NULL);

    g_task_set_task_data (task, data, (GDestroyNotify)api_handler_data_free);

    ephy_web_extension_api_runtime_handler (data->sender, split[1], json_args, task);
    return TRUE;
  }

  respond_with_error (message, "Permission Denied");
  return TRUE;
}

static void
add_content_scripts (EphyWebExtension *web_extension,
                     EphyWebView      *web_view)
{
  GList *content_scripts = ephy_web_extension_get_content_scripts (web_extension);
  WebKitUserContentManager *ucm;

  if (!content_scripts)
    return;

  ucm = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (web_view));
  /* NOTE: This will have to connect/disconnect script-message-recieved once we implement content-script APIs using this. */

  for (GList *list = content_scripts; list && list->data; list = list->next) {
    GList *js_list = ephy_web_extension_get_content_script_js (web_extension, list->data);

    for (GList *tmp_list = js_list; tmp_list && tmp_list->data; tmp_list = tmp_list->next) {
      webkit_user_content_manager_add_script (WEBKIT_USER_CONTENT_MANAGER (ucm), tmp_list->data);
    }
  }
}

static void
remove_content_scripts (EphyWebExtension *self,
                        EphyWebView      *web_view)
{
  GList *content_scripts = ephy_web_extension_get_content_scripts (self);
  WebKitUserContentManager *ucm;

  if (!content_scripts)
    return;

  ucm = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (web_view));

  for (GList *list = content_scripts; list && list->data; list = list->next) {
    GList *js_list = ephy_web_extension_get_content_script_js (self, list->data);

    for (GList *tmp_list = js_list; tmp_list && tmp_list->data; tmp_list = tmp_list->next)
      webkit_user_content_manager_remove_script (WEBKIT_USER_CONTENT_MANAGER (ucm), tmp_list->data);
  }
}

static void
remove_custom_css (EphyWebExtension *self,
                   EphyWebView      *web_view)
{
  GList *custom_css = ephy_web_extension_get_custom_css_list (self);
  GList *list;
  WebKitUserContentManager *ucm;

  if (!custom_css)
    return;

  ucm = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (web_view));

  for (list = custom_css; list && list->data; list = list->next)
    webkit_user_content_manager_remove_style_sheet (WEBKIT_USER_CONTENT_MANAGER (ucm), ephy_web_extension_custom_css_style (self, list->data));
}

static char *
get_translation_contents (EphyWebExtension *web_extension)
{
  /* FIXME: Use current locale and fallback to default web_extension locale if necessary. */
  g_autofree char *path = g_strdup_printf ("_locales/%s/messages.json", "en");
  g_autofree char *data = ephy_web_extension_get_resource_as_string (web_extension, path);

  return data ? g_steal_pointer (&data) : g_strdup ("{}");
}

static void
send_to_page_ready_cb (WebKitWebView    *web_view,
                       GAsyncResult     *result,
                       EphyWebExtension *web_extension)
{
  g_autoptr (WebKitUserMessage) response = webkit_web_view_send_message_to_page_finish (web_view, result, NULL);
  (void)response;

  add_content_scripts (web_extension, EPHY_WEB_VIEW (web_view));
}

static void
destroy_page_action (GtkWidget *action)
{
  EphyLocationEntry *entry;

  entry = EPHY_LOCATION_ENTRY (gtk_widget_get_ancestor (action, EPHY_TYPE_LOCATION_ENTRY));

  ephy_location_entry_page_action_remove (entry, action);
}

static void
ephy_web_extension_manager_add_web_extension_to_webview (EphyWebExtensionManager *self,
                                                         EphyWebExtension        *web_extension,
                                                         EphyWindow              *window,
                                                         EphyWebView             *web_view)
{
  GtkWidget *title_widget = GTK_WIDGET (ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (ephy_window_get_header_bar (window))));
  EphyLocationEntry *lentry = NULL;

  if (EPHY_IS_LOCATION_ENTRY (title_widget)) {
    lentry = EPHY_LOCATION_ENTRY (title_widget);

    if (lentry && ephy_web_extension_has_page_action (web_extension)) {
      GtkWidget *page_action = create_page_action_widget (self, web_extension);
      GHashTable *table;

      table = g_hash_table_lookup (self->page_action_map, web_extension);
      if (!table) {
        table = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)destroy_page_action);
        g_hash_table_insert (self->page_action_map, web_extension, table);
      }

      g_hash_table_insert (table, web_view, g_steal_pointer (&page_action));
    }
  }

  g_signal_connect (web_view, "user-message-received",
                    G_CALLBACK (content_scripts_handle_user_message),
                    web_extension);

  webkit_web_view_send_message_to_page (WEBKIT_WEB_VIEW (web_view),
                                        webkit_user_message_new ("WebExtension.Initialize",
                                                                 g_variant_new ("(sv)", ephy_web_extension_get_guid (web_extension), create_extension_data_variant (web_extension))),
                                        NULL, (GAsyncReadyCallback)send_to_page_ready_cb, web_extension);
}

static void
page_attached_cb (AdwTabView *tab_view,
                  AdwTabPage *page,
                  gint        position,
                  gpointer    user_data)
{
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);
  GtkWidget *child = adw_tab_page_get_child (page);
  EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (child));
  EphyWindow *window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (tab_view)));
  EphyWebExtensionManager *self = ephy_web_extension_manager_get_default ();

  ephy_web_extension_manager_add_web_extension_to_webview (self, web_extension, window, web_view);
  ephy_web_extension_manager_update_location_entry (self, window);
}

static void
init_web_extension_api (WebKitWebContext *web_context,
                        EphyWebExtension *web_extension)
{
  g_autoptr (GVariant) user_data = NULL;
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autofree char *translations = get_translation_contents (web_extension);

#if DEVELOPER_MODE
  webkit_web_context_set_web_process_extensions_directory (web_context, BUILD_ROOT "/embed/web-process-extension");
#else
  webkit_web_context_set_web_process_extensions_directory (web_context, EPHY_WEB_PROCESS_EXTENSIONS_DIR);
#endif

  user_data = g_variant_new ("(smsbv)",
                             ephy_web_extension_get_guid (web_extension),
                             ephy_profile_dir_is_default () ? NULL : ephy_profile_dir (),
                             FALSE /* should_remember_passwords */,
                             ephy_web_extension_manager_get_extension_initialization_data (manager));
  webkit_web_context_set_web_process_extensions_initialization_user_data (web_context, g_steal_pointer (&user_data));
}

static gboolean
decide_policy_cb (WebKitWebView            *web_view,
                  WebKitPolicyDecision     *decision,
                  WebKitPolicyDecisionType  decision_type,
                  EphyWebExtension         *web_extension)
{
  WebKitNavigationPolicyDecision *navigation_decision;
  WebKitNavigationAction *navigation_action;
  WebKitURIRequest *request;
  const char *request_uri;
  const char *request_scheme;
  EphyEmbed *embed;
  EphyWebView *new_view;

  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
      decision_type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
    return FALSE;

  navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
  navigation_action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
  request = webkit_navigation_action_get_request (navigation_action);
  request_uri = webkit_uri_request_get_uri (request);

  if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
    g_autofree char *allowed_prefix = g_strdup_printf ("ephy-webextension://%s/", ephy_web_extension_get_guid (web_extension));
    if (g_str_has_prefix (request_uri, allowed_prefix))
      webkit_policy_decision_use (decision);
    else {
      g_warning ("Extension '%s' tried to navigate to %s", ephy_web_extension_get_name (web_extension), request_uri);
      webkit_policy_decision_ignore (decision);
    }
    return TRUE;
  }

  request_scheme = g_uri_peek_scheme (request_uri);
  if (g_strcmp0 (request_scheme, "https") == 0 || g_strcmp0 (request_scheme, "http") == 0) {
    embed = ephy_shell_new_tab (ephy_shell_get_default (), NULL, NULL, 0);
    new_view = ephy_embed_get_web_view (embed);
    ephy_web_view_load_url (new_view, request_uri);
  }

  webkit_policy_decision_ignore (decision);
  return TRUE;
}

GtkWidget *
ephy_web_extensions_manager_create_web_extensions_webview (EphyWebExtension *web_extension)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autoptr (WebKitSettings) settings = NULL;
  WebKitWebContext *web_context = NULL;
  GtkWidget *web_view;
  WebKitWebView *background_view;
  const char *custom_user_agent;

  settings = webkit_settings_new_with_settings ("enable-write-console-messages-to-stdout", TRUE,
                                                "enable-developer-extras", TRUE,
                                                "enable-fullscreen", FALSE,
                                                "javascript-can-access-clipboard", ephy_web_extension_has_permission (web_extension, "clipboardWrite"),
                                                "hardware-acceleration-policy", WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER, /* Reduces memory usage. */
                                                NULL);
  custom_user_agent = g_hash_table_lookup (manager->user_agent_overrides,
                                           ephy_web_extension_get_name (web_extension));
  if (custom_user_agent)
    webkit_settings_set_user_agent (settings, custom_user_agent);
  else
    webkit_settings_set_user_agent_with_application_details (settings, "Epiphany", EPHY_VERSION);

  /* If there is a background view the web context is shared with the related-view property. */
  background_view = ephy_web_extension_manager_get_background_web_view (manager, web_extension);
  if (!background_view) {
    web_context = webkit_web_context_new ();
    webkit_web_context_register_uri_scheme (web_context, "ephy-webextension", ephy_webextension_scheme_cb, NULL, NULL);
    webkit_security_manager_register_uri_scheme_as_secure (webkit_web_context_get_security_manager (web_context),
                                                           "ephy-webextension");
    g_signal_connect_object (web_context, "initialize-web-process-extensions", G_CALLBACK (init_web_extension_api), web_extension, 0);
  }

  web_view = g_object_new (WEBKIT_TYPE_WEB_VIEW,
                           "web-context", web_context,
                           "settings", settings,
                           "related-view", background_view,
                           "default-content-security-policy", ephy_web_extension_get_content_security_policy (web_extension),
                           "web-extension-mode", WEBKIT_WEB_EXTENSION_MODE_MANIFESTV2,
                           NULL);

  webkit_web_view_set_cors_allowlist (WEBKIT_WEB_VIEW (web_view), ephy_web_extension_get_host_permissions (web_extension));
  g_signal_connect (web_view, "user-message-received", G_CALLBACK (extension_view_handle_user_message), web_extension);
  g_signal_connect (web_view, "decide-policy", G_CALLBACK (decide_policy_cb), web_extension);

  return web_view;
}

static void
on_popup_load_changed (WebKitWebView   *web_view,
                       WebKitLoadEvent  load_event,
                       gpointer         user_data)
{
  /* Delay showing so the popover grows to the proper size. */
  /* TODO: Other browsers resize on DOM changes.
   *       We also need to limit this to a max size of 800x600 like Firefox. */
  if (load_event == WEBKIT_LOAD_FINISHED)
    gtk_widget_set_visible (GTK_WIDGET (web_view), TRUE);
}

static void
on_popup_view_destroyed (GtkWidget *widget,
                         gpointer   user_data)
{
  EphyWebExtension *web_extension = user_data;
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  GPtrArray *popup_views = g_hash_table_lookup (manager->popup_web_views, web_extension);

  g_assert (g_ptr_array_remove_fast (popup_views, widget));
}

static void
ephy_web_extension_manager_register_popup_view (EphyWebExtensionManager *manager,
                                                EphyWebExtension        *web_extension,
                                                GtkWidget               *web_view)
{
  GPtrArray *popup_views = g_hash_table_lookup (manager->popup_web_views, web_extension);

  if (!popup_views) {
    popup_views = g_ptr_array_new ();
    g_hash_table_insert (manager->popup_web_views, web_extension, popup_views);
  }

  g_ptr_array_add (popup_views, web_view);
  g_signal_connect (web_view, "destroy", G_CALLBACK (on_popup_view_destroyed), web_extension);
}

GtkWidget *
ephy_web_extension_manager_create_browser_popup (EphyWebExtensionManager *self,
                                                 EphyWebExtension        *web_extension)
{
  GtkWidget *web_view;
  g_autofree char *popup_uri = NULL;
  const char *popup;

  web_view = ephy_web_extensions_manager_create_web_extensions_webview (web_extension);
  gtk_widget_set_hexpand (web_view, TRUE);
  gtk_widget_set_vexpand (web_view, TRUE);
  gtk_widget_set_visible (web_view, FALSE); /* Shown in on_popup_load_changed. */

  ephy_web_extension_manager_register_popup_view (self, web_extension, web_view);

  popup = ephy_web_extension_get_browser_popup (web_extension);
  popup_uri = g_strdup_printf ("ephy-webextension://%s/%s", ephy_web_extension_get_guid (web_extension), popup);
  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (web_view), popup_uri);
  g_signal_connect (web_view, "load-changed", G_CALLBACK (on_popup_load_changed), NULL);

  return web_view;
}

void
ephy_web_extension_manager_browseraction_set_badge_text (EphyWebExtensionManager *self,
                                                         EphyWebExtension        *web_extension,
                                                         const char              *text)
{
  EphyBrowserAction *action = EPHY_BROWSER_ACTION (g_hash_table_lookup (self->browser_action_map, web_extension));

  if (!action)
    return;

  ephy_browser_action_set_badge_text (action, text);
}

void
ephy_web_extension_manager_browseraction_set_badge_background_color (EphyWebExtensionManager *self,
                                                                     EphyWebExtension        *web_extension,
                                                                     GdkRGBA                 *color)
{
  EphyBrowserAction *action = EPHY_BROWSER_ACTION (g_hash_table_lookup (self->browser_action_map, web_extension));

  if (!action)
    return;

  ephy_browser_action_set_badge_background_color (action, color);
}

void
ephy_web_extension_manager_add_web_extension_to_window (EphyWebExtensionManager *self,
                                                        EphyWebExtension        *web_extension,
                                                        EphyWindow              *window)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (EPHY_WINDOW (window));
  AdwTabView *view = ephy_tab_view_get_tab_view (tab_view);

  if (!ephy_web_extension_manager_is_active (self, web_extension))
    return;

  /* Add page actions and add content script */
  for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
    GtkWidget *page = ephy_tab_view_get_nth_page (tab_view, i);
    EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (page));

    ephy_web_extension_manager_add_web_extension_to_webview (self, web_extension, window, web_view);
  }

  ephy_web_extension_manager_update_location_entry (self, window);
  g_signal_connect_object (view, "page-attached", G_CALLBACK (page_attached_cb), web_extension, 0);
}

static gboolean
remove_page_action (gpointer key,
                    gpointer value,
                    gpointer user_data)
{
  return key == user_data;
}

void
ephy_web_extension_manager_remove_web_extension_from_webview (EphyWebExtensionManager *self,
                                                              EphyWebExtension        *web_extension,
                                                              EphyWindow              *window,
                                                              EphyWebView             *web_view)
{
  GtkWidget *title_widget = GTK_WIDGET (ephy_header_bar_get_title_widget (EPHY_HEADER_BAR (ephy_window_get_header_bar (window))));
  EphyLocationEntry *lentry = NULL;
  GHashTableIter iter;
  gpointer key;
  GHashTable *table;

  if (EPHY_IS_LOCATION_ENTRY (title_widget))
    lentry = EPHY_LOCATION_ENTRY (title_widget);

  g_hash_table_iter_init (&iter, self->page_action_map);
  while (g_hash_table_iter_next (&iter, &key, (gpointer) & table)) {
    if (key != web_extension)
      continue;

    g_hash_table_foreach_remove (table, remove_page_action, web_view);
  }

  if (lentry)
    ephy_location_entry_page_action_clear (lentry);

  g_signal_handlers_disconnect_by_func (web_view, content_scripts_handle_user_message, web_extension);

  remove_content_scripts (web_extension, web_view);
  remove_custom_css (web_extension, web_view);
}

void
ephy_web_extension_manager_remove_web_extension_from_window (EphyWebExtensionManager *self,
                                                             EphyWebExtension        *web_extension,
                                                             EphyWindow              *window)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (EPHY_WINDOW (window));
  AdwTabView *view = ephy_tab_view_get_tab_view (tab_view);

  if (ephy_web_extension_manager_is_active (self, web_extension))
    return;

  for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
    GtkWidget *page = ephy_tab_view_get_nth_page (tab_view, i);
    EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (page));

    ephy_web_extension_manager_remove_web_extension_from_webview (self, web_extension, window, web_view);
  }

  ephy_web_extension_manager_update_location_entry (self, window);

  g_signal_handlers_disconnect_by_data (view, web_extension);
}

gboolean
ephy_web_extension_manager_is_active (EphyWebExtensionManager *self,
                                      EphyWebExtension        *web_extension)
{
  g_auto (GStrv) web_extensions_active = g_settings_get_strv (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_WEBEXTENSIONS_ACTIVE);

  return g_strv_contains ((const char * const *)web_extensions_active, ephy_web_extension_get_name (web_extension));
}

static void
run_background_script (EphyWebExtensionManager *self,
                       EphyWebExtension        *web_extension)
{
  GtkWidget *background;
  const char *page;

  if (!ephy_web_extension_has_background_web_view (web_extension) || ephy_web_extension_manager_get_background_web_view (self, web_extension))
    return;

  page = ephy_web_extension_background_web_view_get_page (web_extension);

  /* Create new background web_view */
  background = ephy_web_extensions_manager_create_web_extensions_webview (web_extension);
  ephy_web_extension_manager_set_background_web_view (self, web_extension, WEBKIT_WEB_VIEW (background));

  if (page) {
    g_autofree char *page_uri = g_strdup_printf ("ephy-webextension://%s/%s", ephy_web_extension_get_guid (web_extension), page);
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (background), page_uri);
    return;
  }
}

static GPtrArray *
strv_to_ptr_array (char **strv)
{
  GPtrArray *array = g_ptr_array_new ();

  for (char **str = strv; *str; ++str) {
    g_ptr_array_add (array, g_strdup (*str));
  }

  return array;
}

static gboolean
extension_equal (gconstpointer a,
                 gconstpointer b)
{
  return g_strcmp0 (a, b) == 0;
}

typedef struct {
  EphyWebExtension *web_extension;
  guint64 window_id;
} WindowAddedCallbackData;

static void
on_page_attached (AdwTabView *self,
                  AdwTabPage *page,
                  gint        position,
                  gpointer    user_data)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  EphyWebExtension *web_extension = user_data;
  g_autoptr (JsonNode) json = NULL;
  GtkWidget *embed = adw_tab_page_get_child (page);
  EphyWebView *view;

  view = ephy_embed_get_web_view (EPHY_EMBED (embed));
  json = ephy_web_extension_api_tabs_create_tab_object (web_extension, view);
  ephy_web_extension_manager_emit_in_extension_views (manager, web_extension, "tabs.onCreated", json_to_string (json, FALSE));
}

static void
on_page_detached (AdwTabView *self,
                  AdwTabPage *page,
                  gint        position,
                  gpointer    user_data)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  EphyWebExtension *web_extension = user_data;
  g_autofree char *tab_json = NULL;
  GtkWidget *embed = adw_tab_page_get_child (page);
  EphyWebView *view;

  view = ephy_embed_get_web_view (EPHY_EMBED (embed));
  tab_json = g_strdup_printf ("%ld", ephy_web_view_get_uid (view));
  ephy_web_extension_manager_emit_in_extension_views (manager, web_extension, "tabs.onRemoved", tab_json);
}

static gboolean
application_window_added_timeout_cb (gpointer user_data)
{
  WindowAddedCallbackData *data = user_data;
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  EphyWindow *window = ephy_web_extension_api_windows_get_window_for_id (data->window_id);
  EphyTabView *tab_view;
  AdwTabView *adw_tab_view;
  g_autofree char *window_json = NULL;

  /* It is possible the window was removed before this timeout. */
  if (!window)
    return G_SOURCE_REMOVE;

  window_json = ephy_web_extension_api_windows_create_window_json (data->web_extension, window);
  ephy_web_extension_manager_emit_in_extension_views (manager, data->web_extension, "windows.onCreated", window_json);

  tab_view = ephy_window_get_tab_view (window);
  adw_tab_view = ephy_tab_view_get_tab_view (tab_view);
  g_signal_connect (G_OBJECT (adw_tab_view), "page-attached", G_CALLBACK (on_page_attached), data->web_extension);
  g_signal_connect (G_OBJECT (adw_tab_view), "page-detached", G_CALLBACK (on_page_detached), data->web_extension);

  return G_SOURCE_REMOVE;
}

void
ephy_web_extension_manager_foreach_extension (EphyWebExtensionManager     *self,
                                              EphyWebExtensionForeachFunc  func,
                                              gpointer                     user_data)
{
  g_ptr_array_foreach (self->web_extensions, (GFunc)func, user_data);
}

static void
application_window_added_cb (EphyShell        *shell,
                             EphyWindow       *window,
                             EphyWebExtension *web_extension)
{
  /* At this point the EphyWindow has no EphyEmbed child so we can't get information from it.
   * We also can't really know ahead of time if multiple tabs are opening so simply adding a delay
   * tends to give accurate results. */
  WindowAddedCallbackData *data = g_new (WindowAddedCallbackData, 1);
  data->window_id = ephy_window_get_uid (window);
  data->web_extension = web_extension;
  g_timeout_add_seconds_full (G_PRIORITY_DEFAULT_IDLE, 1, application_window_added_timeout_cb, data, g_free);
}

static void
application_window_removed_cb (EphyShell        *shell,
                               EphyWindow       *window,
                               EphyWebExtension *web_extension)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autofree char *window_json = g_strdup_printf ("%ld", ephy_window_get_uid (window));
  EphyTabView *tab_view;
  AdwTabView *adw_tab_view;

  ephy_web_extension_manager_emit_in_extension_views (manager, web_extension, "windows.onRemoved", window_json);

  tab_view = ephy_window_get_tab_view (window);
  adw_tab_view = ephy_tab_view_get_tab_view (tab_view);
  g_signal_handlers_disconnect_by_func (G_OBJECT (adw_tab_view), G_CALLBACK (on_page_attached), web_extension);
  g_signal_handlers_disconnect_by_func (G_OBJECT (adw_tab_view), G_CALLBACK (on_page_detached), web_extension);
}

static void
remove_browser_action (EphyWebExtensionManager *self,
                       EphyWebExtension        *web_extension)
{
  EphyBrowserAction *action;
  guint position;

  action = g_hash_table_lookup (self->browser_action_map, web_extension);
  if (!action)
    return;

  g_assert (g_list_store_find (self->browser_actions, action, &position));
  g_list_store_remove (self->browser_actions, position);

  g_hash_table_remove (self->browser_action_map, web_extension);
}

void
ephy_web_extension_manager_set_active (EphyWebExtensionManager *self,
                                       EphyWebExtension        *web_extension,
                                       gboolean                 active)
{
  g_auto (GStrv) web_extensions_active = g_settings_get_strv (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_WEBEXTENSIONS_ACTIVE);
  EphyShell *shell = ephy_shell_get_default ();
  GList *windows = gtk_application_get_windows (GTK_APPLICATION (shell));
  GList *list;
  g_autoptr (GPtrArray) array = strv_to_ptr_array (web_extensions_active);
  const char *name = ephy_web_extension_get_name (web_extension);
  gboolean found;
  guint idx;

  /* Update settings */
  found = g_ptr_array_find_with_equal_func (array, name, extension_equal, &idx);
  if (active) {
    if (!found)
      g_ptr_array_add (array, (gpointer)name);
  } else {
    if (found)
      g_ptr_array_remove_index (array, idx);
  }

  g_ptr_array_add (array, NULL);

  g_settings_set_strv (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_WEBEXTENSIONS_ACTIVE, (const gchar * const *)array->pdata);

  /* Update window web_extension state */
  for (list = windows; list && list->data; list = list->next) {
    EphyWindow *window = EPHY_WINDOW (list->data);

    if (active) {
      ephy_web_extension_manager_add_web_extension_to_window (self, web_extension, window);
      application_window_added_cb (shell, window, web_extension);
    } else
      ephy_web_extension_manager_remove_web_extension_from_window (self, web_extension, window);
  }

  if (active) {
    g_signal_connect (shell, "window-added", G_CALLBACK (application_window_added_cb), web_extension);
    g_signal_connect (shell, "window-removed", G_CALLBACK (application_window_removed_cb), web_extension);

    if (ephy_web_extension_has_background_web_view (web_extension))
      run_background_script (self, web_extension);

    if (ephy_web_extension_has_browser_action (web_extension)) {
      EphyBrowserAction *action = ephy_browser_action_new (web_extension);
      g_list_store_append (self->browser_actions, action);
      g_hash_table_insert (self->browser_action_map, web_extension, g_steal_pointer (&action));
    }

    ephy_web_extension_api_commands_init (web_extension);
  } else {
    g_signal_handlers_disconnect_by_data (shell, web_extension);

    remove_browser_action (self, web_extension);
    g_hash_table_remove (self->background_web_views, web_extension);
    g_object_set_data (G_OBJECT (web_extension), "alarms", NULL); /* Set in alarms.c */
    ephy_web_extension_api_commands_dispose (web_extension);
  }
}

void
ephy_web_extension_manager_show_browser_action (EphyWebExtensionManager *self,
                                                EphyWebExtension        *web_extension)
{
  EphyBrowserAction *action = EPHY_BROWSER_ACTION (g_hash_table_lookup (self->browser_action_map, web_extension));

  if (!action || ephy_browser_action_activate (action))
    return;

  g_signal_emit (self, signals[SHOW_BROWSER_ACTION], 0, action);
}

GtkWidget *
ephy_web_extension_manager_get_page_action (EphyWebExtensionManager *self,
                                            EphyWebExtension        *web_extension,
                                            EphyWebView             *web_view)
{
  GHashTable *table;
  GtkWidget *ret = NULL;

  table = g_hash_table_lookup (self->page_action_map, web_extension);
  if (table)
    ret = g_hash_table_lookup (table, web_view);

  return ret;
}

GListStore *
ephy_web_extension_manager_get_browser_actions (EphyWebExtensionManager *self)
{
  return self->browser_actions;
}

void
ephy_web_extension_manager_handle_notifications_action (EphyWebExtensionManager *self,
                                                        GVariant                *params)
{
  EphyWebExtension *web_extension;
  g_autofree char *json = NULL;
  const char *extension_guid;
  const char *notification_id;
  int index;

  g_variant_get (params, "(&s&si)", &extension_guid, &notification_id, &index);
  web_extension = ephy_web_extension_manager_get_extension_by_guid (self, extension_guid);
  if (!web_extension)
    return;

  if (index == -1) {
    json = g_strdup_printf ("\"%s\"", notification_id);
    ephy_web_extension_manager_emit_in_extension_views (self, web_extension, "notifications.onClicked", json);
  } else {
    json = g_strdup_printf ("\"%s\", %d", notification_id, index);
    ephy_web_extension_manager_emit_in_extension_views (self, web_extension, "notifications.onButtonClicked", json);
  }
}

void
ephy_web_extension_manager_handle_context_menu_action (EphyWebExtensionManager *self,
                                                       GVariant                *params)
{
  EphyWebExtension *web_extension;
  const char *extension_guid;
  const char *onclickdata;
  const char *tabdata;
  g_autofree char *json = NULL;

  g_variant_get (params, "(&s&s&s)", &extension_guid, &onclickdata, &tabdata);

  web_extension = ephy_web_extension_manager_get_extension_by_guid (self, extension_guid);
  if (!web_extension)
    return;

  json = g_strconcat (onclickdata, ", ", tabdata, NULL);
  ephy_web_extension_manager_emit_in_extension_views (self, web_extension, "menus.onClicked", json);
}

static void
handle_message_reply (EphyWebExtension *web_extension,
                      JsonArray        *args)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  GHashTable *pending_messages = g_hash_table_lookup (manager->pending_messages, web_extension);
  GTask *pending_task;
  const char *message_guid;
  JsonNode *reply;

  message_guid = ephy_json_array_get_string (args, 0);
  if (!message_guid) {
    g_debug ("Received invalid message reply");
    return;
  }

  pending_task = g_hash_table_lookup (pending_messages, message_guid);
  if (!pending_task) {
    g_debug ("Received message not found in pending replies");
    return;
  }
  g_hash_table_steal (pending_messages, message_guid);

  reply = ephy_json_array_get_element (args, 1);
  g_task_return_pointer (pending_task, reply ? json_to_string (reply, FALSE) : NULL, g_free);
}

typedef struct {
  EphyWebExtension *web_extension;
  char *message_guid; /* Owned by manager->pending_messages. */
  guint pending_view_responses;
  gboolean handled;
} PendingMessageReplyTracker;

static void
tab_emit_ready_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  PendingMessageReplyTracker *tracker = user_data;
  GHashTable *pending_messages;
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) value = NULL;
  GTask *pending_task;

  value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source),
                                                      result,
                                                      &error);

  /* If it returned true it will be asynchronously handled later. Otherwise we
   * complete it now with undefined. */
  if (error || !jsc_value_to_boolean (value)) {
    pending_messages = g_hash_table_lookup (manager->pending_messages, tracker->web_extension);
    pending_task = g_hash_table_lookup (pending_messages, tracker->message_guid);
    if (pending_task) {
      g_assert (g_hash_table_steal (pending_messages, tracker->message_guid));
      g_clear_pointer (&tracker->message_guid, g_free);

      g_task_return_pointer (pending_task, NULL, NULL);
    }
  }

  if (error)
    g_warning ("Emitting in tab errored: %s", error->message);

  g_free (tracker);
}

void
ephy_web_extension_manager_emit_in_tab_with_reply (EphyWebExtensionManager *self,
                                                   EphyWebExtension        *web_extension,
                                                   const char              *name,
                                                   const char              *message_json,
                                                   WebKitWebView           *target_web_view,
                                                   const char              *sender_json,
                                                   GTask                   *reply_task)
{
  g_autofree char *script = NULL;
  PendingMessageReplyTracker *tracker = NULL;
  GHashTable *pending_messages;
  g_autofree char *message_guid = NULL;

  g_assert (reply_task);
  g_assert (target_web_view);

  /* This is similar to ephy_web_extension_manager_emit_in_extension_views_internal()
   * except it only emits in a single view in a private script world. */

  message_guid = g_dbus_generate_guid ();
  script = g_strdup_printf ("window.browser.%s._emit_with_reply(%s, %s, '%s');", name, message_json, sender_json, message_guid);

  tracker = g_new0 (PendingMessageReplyTracker, 1);
  tracker->web_extension = web_extension;
  tracker->message_guid = message_guid;
  webkit_web_view_evaluate_javascript (target_web_view,
                                       script, -1,
                                       ephy_web_extension_get_guid (web_extension),
                                       NULL,
                                       NULL,
                                       tab_emit_ready_cb,
                                       tracker);

  pending_messages = g_hash_table_lookup (self->pending_messages, web_extension);
  if (!pending_messages) {
    pending_messages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_object_unref);
    g_hash_table_insert (self->pending_messages, web_extension, pending_messages);
  }

  if (!g_hash_table_replace (pending_messages, g_steal_pointer (&message_guid), reply_task))
    g_warning ("Duplicate message GUID");
}

static void
on_extension_emit_ready (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  PendingMessageReplyTracker *tracker = user_data;
  GHashTable *pending_messages;
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) value = NULL;

  value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source),
                                                      result,
                                                      &error);

  if (!error && jsc_value_to_boolean (value))
    tracker->handled = TRUE;

  /* Once all views have been notified it will either be handled by one of them, in which case
   * handle_message_reply() finishes the task, or we finish it here with an empty response. */
  /* FIXME: A race condition is possible where a view is destroyed before it responds. */
  tracker->pending_view_responses--;
  if (tracker->pending_view_responses == 0) {
    if (!tracker->handled) {
      GTask *pending_task;

      pending_messages = g_hash_table_lookup (manager->pending_messages, tracker->web_extension);
      pending_task = g_hash_table_lookup (pending_messages, tracker->message_guid);
      /* It is possible another view responded and removed the pending_task already. */
      if (pending_task) {
        g_assert (g_hash_table_steal (pending_messages, tracker->message_guid));
        g_clear_pointer (&tracker->message_guid, g_free);

        g_task_return_pointer (pending_task, NULL, NULL);
      }
    }
    g_free (tracker);
  }

  if (error)
    g_warning ("Emitting in view errored: %s", error->message);
}

void
ephy_web_extension_manager_emit_in_background_view (EphyWebExtensionManager *self,
                                                    EphyWebExtension        *web_extension,
                                                    const char              *name,
                                                    const char              *json)
{
  g_autofree char *script = NULL;
  WebKitWebView *web_view = ephy_web_extension_manager_get_background_web_view (self, web_extension);

  if (!web_view)
    return;

  script = g_strdup_printf ("window.browser.%s._emit(%s);", name, json);

  webkit_web_view_evaluate_javascript (web_view,
                                       script, -1,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);
}

static void
ephy_web_extension_manager_emit_in_extension_views_internal (EphyWebExtensionManager *self,
                                                             EphyWebExtension        *web_extension,
                                                             EphyWebExtensionSender  *sender,
                                                             const char              *name,
                                                             const char              *message_json,
                                                             GTask                   *reply_task)
{
  WebKitWebView *background_view = ephy_web_extension_manager_get_background_web_view (self, web_extension);
  GPtrArray *popup_views = g_hash_table_lookup (self->popup_web_views, web_extension);
  g_autofree char *script = NULL;
  PendingMessageReplyTracker *tracker = NULL;
  guint pending_views = 0;
  GHashTable *pending_messages;
  g_autofree char *message_guid = NULL;

  /* The `runtime.sendMessage()` API emits `runtime.onMessage` and waits for a reply.
   * The way this is implemented is:
   *  - All API handlers can be async: Returning a Promise backed by GTask (@reply_task).
   *  - Instead of completing the GTask we store it for each message waiting on replies.
   *  - We then call every extension view and track if any of them handled it (see webextensions-common.js).
   *  - If none handled it we complete with an empty message.
   *  - Otherwise we wait for our private `runtime._sendMessageReply` API call.
   *  - The first `runtime._sendMessageReply` call wins and completes the GTask with its data.
   */
  if (reply_task) {
    g_autofree char *sender_json = ephy_web_extension_create_sender_object (sender);
    message_guid = g_dbus_generate_guid ();
    tracker = g_new0 (PendingMessageReplyTracker, 1);
    script = g_strdup_printf ("window.browser.%s._emit_with_reply(%s, %s, '%s');", name, message_json, sender_json, message_guid);
  } else
    script = g_strdup_printf ("window.browser.%s._emit(%s);", name, message_json);

  if (background_view) {
    if (!sender || (sender->view != background_view)) {
      webkit_web_view_evaluate_javascript (background_view,
                                           script, -1,
                                           NULL, NULL, NULL,
                                           reply_task ? on_extension_emit_ready : NULL,
                                           tracker);
      pending_views++;
    }
  }

  if (popup_views) {
    for (guint i = 0; i < popup_views->len; i++) {
      WebKitWebView *popup_view = g_ptr_array_index (popup_views, i);
      if (!sender || (sender->view == popup_view))
        continue;

      webkit_web_view_evaluate_javascript (popup_view,
                                           script, -1,
                                           NULL, NULL, NULL,
                                           reply_task ? on_extension_emit_ready : NULL,
                                           tracker);
      pending_views++;
    }
  }

  if (!reply_task)
    return;

  if (!pending_views) {
    g_task_return_pointer (reply_task, NULL, NULL);
    g_free (tracker);
    return;
  }

  tracker->web_extension = web_extension;
  tracker->pending_view_responses = pending_views;
  tracker->message_guid = message_guid;

  pending_messages = g_hash_table_lookup (self->pending_messages, web_extension);
  if (!pending_messages) {
    pending_messages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_object_unref);
    g_hash_table_insert (self->pending_messages, web_extension, pending_messages);
  }

  if (!g_hash_table_replace (pending_messages, g_steal_pointer (&message_guid), reply_task))
    g_warning ("Duplicate message GUID");
}

void
ephy_web_extension_manager_emit_in_extension_views (EphyWebExtensionManager *self,
                                                    EphyWebExtension        *web_extension,
                                                    const char              *name,
                                                    const char              *json)
{
  ephy_web_extension_manager_emit_in_extension_views_internal (self, web_extension, NULL, name, json, NULL);
}

void
ephy_web_extension_manager_emit_in_extension_views_with_reply (EphyWebExtensionManager *self,
                                                               EphyWebExtension        *web_extension,
                                                               EphyWebExtensionSender  *sender,
                                                               const char              *name,
                                                               const char              *json,
                                                               GTask                   *reply_task)
{
  g_assert (reply_task);
  g_assert (sender);
  ephy_web_extension_manager_emit_in_extension_views_internal (self, web_extension, sender, name, json, reply_task);
}

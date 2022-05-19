/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019-2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include "api/notifications.h"
#include "api/pageaction.h"
#include "api/runtime.h"
#include "api/tabs.h"

#include <json-glib/json-glib.h>

struct _EphyWebExtensionManager {
  GObject parent_instance;

  GCancellable *cancellable;
  GList *web_extensions;
  GHashTable *page_action_map;
  GHashTable *browser_action_map;
  GHashTable *background_web_views;
};

G_DEFINE_TYPE (EphyWebExtensionManager, ephy_web_extension_manager, G_TYPE_OBJECT)

EphyWebExtensionApiHandler api_handlers[] = {
  {"notifications", ephy_web_extension_api_notifications_handler},
  {"pageAction", ephy_web_extension_api_pageaction_handler},
  {"runtime", ephy_web_extension_api_runtime_handler},
  {"tabs", ephy_web_extension_api_tabs_handler},
  {NULL, NULL},
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
ephy_web_extension_manager_add_to_list (EphyWebExtensionManager *self,
                                        EphyWebExtension        *web_extension)
{
  self->web_extensions = g_list_append (self->web_extensions, g_object_ref (web_extension));

  g_signal_emit (self, signals[CHANGED], 0);
}

static void
ephy_web_extension_manager_remove_from_list (EphyWebExtensionManager *self,
                                             EphyWebExtension        *web_extension)
{
  self->web_extensions = g_list_remove (self->web_extensions, web_extension);
  g_object_unref (web_extension);

  g_signal_emit (self, signals[CHANGED], 0);
}

void
on_web_extension_loaded (GObject      *source_object,
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
  g_object_unref (web_extension);

  if (ephy_web_extension_manager_is_active (self, web_extension))
    ephy_web_extension_manager_set_active (self, web_extension, TRUE);
}

static void
ephy_web_extension_manager_scan_directory (EphyWebExtensionManager *self,
                                           const char              *extension_dir)
{
  g_autoptr (GDir) dir = NULL;
  g_autoptr (GError) error = NULL;
  const char *directory;

  if (g_mkdir_with_parents (extension_dir, 0700) != 0)
    g_warning ("Failed to create %s: %s", extension_dir, g_strerror (errno));

  if (!g_file_test (extension_dir, G_FILE_TEST_EXISTS))
    g_mkdir_with_parents (extension_dir, 0700);

  dir = g_dir_open (extension_dir, 0, &error);
  if (!dir) {
    g_warning ("Could not open %s: %s", extension_dir, error->message);
    return;
  }

  errno = 0;
  while ((directory = g_dir_read_name (dir))) {
    g_autofree char *filename = NULL;
    g_autoptr (GFile) file = NULL;

    if (errno != 0) {
      g_warning ("Problem reading %s: %s", extension_dir, g_strerror (errno));
      break;
    }

    filename = g_build_filename (extension_dir, directory, NULL);
    file = g_file_new_for_path (filename);

    ephy_web_extension_load_async (file, self->cancellable, on_web_extension_loaded, self);

    errno = 0;
  }
}

static void
ephy_web_extension_manager_constructed (GObject *object)
{
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (object);
  g_autofree char *dir = g_build_filename (ephy_default_profile_dir (), "web_extensions", NULL);

  self->background_web_views = g_hash_table_new (NULL, NULL);
  self->page_action_map = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)g_hash_table_destroy);
  self->browser_action_map = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)gtk_widget_destroy);
  self->web_extensions = NULL;

  ephy_web_extension_manager_scan_directory (self, dir);
}

static void
ephy_web_extension_manager_dispose (GObject *object)
{
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (object);

  g_clear_pointer (&self->background_web_views, g_hash_table_destroy);
  g_clear_pointer (&self->page_action_map, g_hash_table_destroy);
  g_list_free_full (g_steal_pointer (&self->web_extensions), g_object_unref);
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
}

static void
ephy_web_extension_manager_init (EphyWebExtensionManager *self)
{
}

EphyWebExtensionManager *
ephy_web_extension_manager_new (void)
{
  return g_object_new (EPHY_TYPE_WEB_EXTENSION_MANAGER, NULL);
}

GList *
ephy_web_extension_manager_get_web_extensions (EphyWebExtensionManager *self)
{
  return self->web_extensions;
}

/**
 * Installs/Adds all web_extensions to new EphyWindow.
 */
void
ephy_web_extension_manager_install_actions (EphyWebExtensionManager *self,
                                            EphyWindow              *window)
{
  for (GList *list = self->web_extensions; list && list->data; list = list->next)
    ephy_web_extension_manager_add_web_extension_to_window (self, list->data, window);
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
/**
 * Install a new web web_extension into the local web_extension directory.
 * File should only point to a manifest.json or a .xpi file
 */
void
ephy_web_extension_manager_install (EphyWebExtensionManager *self,
                                    GFile                   *file)
{
  g_autoptr (GFile) target = NULL;
  g_autofree char *basename = NULL;
  gboolean is_xpi = FALSE;

  basename = g_file_get_basename (file);
  is_xpi = g_str_has_suffix (basename, ".xpi");

  if (!is_xpi) {
    g_autoptr (GFile) source = NULL;

    /* Get parent directory */
    source = g_file_get_parent (file);
    target = g_file_new_build_filename (ephy_default_profile_dir (), "web_extensions", g_file_get_basename (source), NULL);

    ephy_copy_directory (g_file_get_path (source), g_file_get_path (target));
  } else {
    g_autoptr (GError) error = NULL;
    target = g_file_new_build_filename (ephy_default_profile_dir (), "web_extensions", g_file_get_basename (file), NULL);

    if (!g_file_copy (file, target, G_FILE_COPY_NONE, NULL, NULL, NULL, &error)) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
        g_warning ("Could not copy file for web_extensions: %s", error->message);
        return;
      }
    }
  }

  if (target)
    ephy_web_extension_load_async (g_steal_pointer (&target), self->cancellable, on_new_web_extension_loaded, self);
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

  for (GList *list = ephy_web_extension_manager_get_web_extensions (self); list && list->data; list = list->next) {
    EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (list->data);
    GtkWidget *action = ephy_web_extension_manager_get_page_action (self, web_extension, web_view);

    if (action)
      ephy_location_entry_page_action_add (lentry, action);
  }
}

EphyWebView *
ephy_web_extension_manager_get_background_web_view (EphyWebExtensionManager *self,
                                                    EphyWebExtension        *web_extension)
{
  return g_hash_table_lookup (self->background_web_views, web_extension);
}

static void
ephy_web_extension_manager_set_background_web_view (EphyWebExtensionManager *self,
                                                    EphyWebExtension        *web_extension,
                                                    EphyWebView             *web_view)
{
  g_hash_table_insert (self->background_web_views, web_extension, web_view);
}

static gboolean
page_action_clicked (GtkWidget      *event_box,
                     GdkEventButton *event,
                     gpointer        user_data)
{
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);
  EphyShell *shell = ephy_shell_get_default ();
  EphyWebView *view = EPHY_WEB_VIEW (ephy_shell_get_active_web_view (shell));
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  g_autofree char *json = NULL;
  g_autofree char *script = NULL;
  EphyWebExtensionManager *self = ephy_shell_get_web_extension_manager (shell);
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (ephy_web_extension_manager_get_background_web_view (self, web_extension));

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "url");
  json_builder_add_string_value (builder, ephy_web_view_get_address (view));
  json_builder_set_member_name (builder, "id");
  json_builder_add_int_value (builder, ephy_web_view_get_uid (view));
  json_builder_end_object (builder);

  root = json_builder_get_root (builder);

  json = json_to_string (root, FALSE);

  script = g_strdup_printf ("pageActionOnClicked(%s);", json);
  webkit_web_view_run_javascript (web_view,
                                  script,
                                  NULL,
                                  NULL,
                                  NULL);

  return GDK_EVENT_STOP;
}

static GtkWidget *
create_page_action_widget (EphyWebExtensionManager *self,
                           EphyWebExtension        *web_extension)
{
  GtkWidget *image;
  GtkWidget *event_box;
  GtkStyleContext *context;

  /* Create new event box with page action */
  event_box = gtk_event_box_new ();
  image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (event_box), image);
  g_signal_connect_object (event_box, "button_press_event", G_CALLBACK (page_action_clicked), web_extension, 0);
  gtk_widget_show_all (event_box);

  context = gtk_widget_get_style_context (image);
  gtk_style_context_add_class (context, "entry_icon");

  return g_object_ref (event_box);
}

typedef struct {
  EphyWebExtension *web_extension;
  WebKitWebView *web_view;
} ScriptMessageData;

static void
ephy_web_extension_handle_script_message (WebKitUserContentManager *ucm,
                                          WebKitJavascriptResult   *js_result,
                                          gpointer                  user_data)
{
  ScriptMessageData *data = user_data;
  EphyWebExtension *web_extension = data->web_extension;
  WebKitWebView *web_view = data->web_view;
  JSCValue *value = webkit_javascript_result_get_js_value (js_result);
  g_autofree char *name_str = NULL;
  g_autoptr (JSCValue) name = NULL;
  g_autoptr (JSCValue) promise = NULL;
  g_auto (GStrv) split = NULL;
  GPtrArray *permissions = ephy_web_extension_get_permissions (web_extension);
  unsigned int idx;

  if (!jsc_value_is_object (value))
    return;

  if (!jsc_value_object_has_property (value, "promise"))
    return;

  promise = jsc_value_object_get_property (value, "promise");
  if (!jsc_value_is_number (promise))
    return;

  name = jsc_value_object_get_property (value, "fn");
  if (!name)
    return;

  name_str = jsc_value_to_string (name);
  LOG ("%s(): Called for %s, function %s\n", __FUNCTION__, ephy_web_extension_get_name (web_extension), name_str);

  split = g_strsplit (name_str, ".", 2);
  if (g_strv_length (split) != 2) {
    g_warning ("Invalid function call, aborting: %s", name_str);
    return;
  }

  for (idx = 0; idx < G_N_ELEMENTS (api_handlers); idx++) {
    EphyWebExtensionApiHandler handler = api_handlers[idx];

    if (!g_ptr_array_find (permissions, split[0], NULL)) {
      LOG ("%s(): Requested api is not part of the permissions, aborting\n", __FUNCTION__);
      /* TODO: Permissions are not working yet */
      /*return; */
    }

    if (g_strcmp0 (handler.name, split[0]) == 0) {
      g_autofree char *ret = NULL;
      g_autofree char *script = NULL;
      g_autoptr (JSCValue) args = jsc_value_object_get_property (value, "args");

      ret = handler.execute (web_extension, split[1], args);
      script = g_strdup_printf ("promises[%.f].resolve(%s);", jsc_value_to_double (promise), ret ? ret : "");
      webkit_web_view_run_javascript (web_view, script, NULL, NULL, NULL);

      return;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name_str);
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

  return data ? g_steal_pointer (&data) : g_strdup ("");
}

static void
update_translations (EphyWebExtension *web_extension)
{
  g_autofree char *data = get_translation_contents (web_extension);

  webkit_web_context_send_message_to_all_extensions (ephy_embed_shell_get_web_context (ephy_embed_shell_get_default ()),
                                                     webkit_user_message_new ("WebExtension.UpdateTranslations",
                                                                              g_variant_new ("(ss)", ephy_web_extension_get_guid (web_extension), data)));
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
        table = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)gtk_widget_destroy);
        g_hash_table_insert (self->page_action_map, web_extension, table);
      }

      g_hash_table_insert (table, web_view, g_steal_pointer (&page_action));
    }
  }

  webkit_web_view_send_message_to_page (WEBKIT_WEB_VIEW (web_view),
                                        webkit_user_message_new ("WebExtension.Initialize", g_variant_new_string (ephy_web_extension_get_guid (web_extension))),
                                        NULL, NULL, NULL);

  update_translations (web_extension);
  add_content_scripts (web_extension, web_view);
}

static void
page_attached_cb (HdyTabView *tab_view,
                  HdyTabPage *page,
                  gint        position,
                  gpointer    user_data)
{
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);
  GtkWidget *child = hdy_tab_page_get_child (page);
  EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (child));
  EphyWindow *window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tab_view)));
  EphyWebExtensionManager *self = ephy_shell_get_web_extension_manager (ephy_shell_get_default ());

  ephy_web_extension_manager_add_web_extension_to_webview (self, web_extension, window, web_view);
  ephy_web_extension_manager_update_location_entry (self, window);
}

static void
web_extension_cb (WebKitURISchemeRequest *request,
                  gpointer                user_data)
{
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);
  const char *path;
  const unsigned char *data;
  gsize length;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GError) error = NULL;

  path = webkit_uri_scheme_request_get_path (request);

  /* FIXME: This may be the place to handle predefined messages and localized CSS:
   * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Internationalization#predefined_messages
   */

  data = ephy_web_extension_get_resource (web_extension, path + 1, &length);
  if (!data) {
    error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED, "Resource not found: %s", path);
    webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  stream = g_memory_input_stream_new_from_data (data, length, NULL);
  webkit_uri_scheme_request_finish (request, stream, length, NULL);
}

static void
init_web_extension_api (WebKitWebContext *web_context,
                        EphyWebExtension *web_extension)
{
  g_autoptr (GVariant) user_data = NULL;
  g_autofree char *translations = get_translation_contents (web_extension);

#if DEVELOPER_MODE
  webkit_web_context_set_web_extensions_directory (web_context, BUILD_ROOT "/embed/web-process-extension");
#else
  webkit_web_context_set_web_extensions_directory (web_context, EPHY_WEB_PROCESS_EXTENSIONS_DIR);
#endif

  user_data = g_variant_new ("(smsbbbs)",
                             ephy_web_extension_get_guid (web_extension),
                             ephy_profile_dir_is_default () ? NULL : ephy_profile_dir (),
                             FALSE /* should_remember_passwords */,
                             FALSE /* private_profile */,
                             TRUE /* is_webextension */,
                             translations);
  webkit_web_context_set_web_extensions_initialization_user_data (web_context, g_steal_pointer (&user_data));
}

static GtkWidget *
create_web_extensions_webview (EphyWebExtension *web_extension)
{
  g_autoptr (WebKitUserContentManager) ucm = NULL;
  WebKitSettings *settings;
  WebKitWebContext *web_context;
  GtkWidget *web_view;
  ScriptMessageData *data;

  /* Create an own ucm so new scripts/css are only applied to this web_view */
  ucm = webkit_user_content_manager_new ();

  web_context = webkit_web_context_new ();
  webkit_web_context_register_uri_scheme (web_context, "ephy-webextension", web_extension_cb, web_extension, NULL);
  webkit_security_manager_register_uri_scheme_as_secure (webkit_web_context_get_security_manager (web_context),
                                                         "ephy-webextension");

  g_signal_connect_object (web_context, "initialize-web-extensions", G_CALLBACK (init_web_extension_api), web_extension, 0);

  web_view = g_object_new (EPHY_TYPE_WEB_VIEW,
                           "web-context", web_context,
                           "user-content-manager", ucm,
                           "settings", ephy_embed_prefs_get_settings (),
                           NULL);

  settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (web_view));
  webkit_settings_set_enable_write_console_messages_to_stdout (settings, TRUE);

  data = g_new0 (ScriptMessageData, 1);
  data->web_extension = web_extension;
  data->web_view = WEBKIT_WEB_VIEW (web_view);

  g_signal_connect_data (ucm, "script-message-received::epiphany", G_CALLBACK (ephy_web_extension_handle_script_message), data, (GClosureNotify)g_free, 0);
  webkit_user_content_manager_register_script_message_handler (ucm, "epiphany");

  return web_view;
}

static char *
create_base_uri_for_resource_path (EphyWebExtension *web_extension,
                                   const char       *resource_path)
{
  g_autofree char *dir_name = NULL;

  if (resource_path) {
    dir_name = g_path_get_dirname (resource_path);

    if (g_strcmp0 (dir_name, ".") != 0)
      return g_strdup_printf ("ephy-webextension://%s/%s/", ephy_web_extension_get_guid (web_extension), dir_name);
  }

  return g_strdup_printf ("ephy-webextension://%s/", ephy_web_extension_get_guid (web_extension));
}

static GtkWidget *
create_browser_popup (EphyWebExtension *web_extension)
{
  GtkWidget *web_view;
  GtkWidget *popover;
  g_autofree char *data = NULL;
  g_autofree char *base_uri = NULL;
  const char *popup;

  popover = gtk_popover_new (NULL);

  web_view = create_web_extensions_webview (web_extension);

  popup = ephy_web_extension_get_browser_popup (web_extension);
  base_uri = create_base_uri_for_resource_path (web_extension, popup);
  data = ephy_web_extension_get_resource_as_string (web_extension, popup);
  webkit_web_view_load_html (WEBKIT_WEB_VIEW (web_view), (char *)data, base_uri);
  gtk_container_add (GTK_CONTAINER (popover), web_view);
  gtk_widget_show_all (web_view);

  return popover;
}

static gboolean
on_browser_action_clicked (GtkWidget *event_box,
                           gpointer   user_data)
{
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);
  EphyWebExtensionManager *self = ephy_shell_get_web_extension_manager (ephy_shell_get_default ());
  g_autofree char *script = NULL;
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (ephy_web_extension_manager_get_background_web_view (self, web_extension));

  script = g_strdup_printf ("browserActionClicked();");

  webkit_web_view_run_javascript (web_view,
                                  script,
                                  NULL,
                                  NULL,
                                  NULL);

  return GDK_EVENT_STOP;
}


GtkWidget *
create_browser_action (EphyWebExtension *web_extension)
{
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *popover;

  if (ephy_web_extension_get_browser_popup (web_extension)) {
    button = gtk_menu_button_new ();
    image = gtk_image_new_from_pixbuf (ephy_web_extension_browser_action_get_icon (web_extension, 16));
    popover = create_browser_popup (web_extension);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);

    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_visible (button, TRUE);
  } else {
    GdkPixbuf *pixbuf = ephy_web_extension_browser_action_get_icon (web_extension, 16);

    button = gtk_button_new ();

    if (pixbuf)
      image = gtk_image_new_from_pixbuf (pixbuf);
    else
      image = gtk_image_new_from_icon_name ("application-x-addon-symbolic", GTK_ICON_SIZE_BUTTON);

    g_signal_connect_object (button, "clicked", G_CALLBACK (on_browser_action_clicked), web_extension, 0);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_visible (button, TRUE);
  }

  return button;
}

void
ephy_web_extension_manager_add_web_extension_to_window (EphyWebExtensionManager *self,
                                                        EphyWebExtension        *web_extension,
                                                        EphyWindow              *window)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (EPHY_WINDOW (window));
  HdyTabView *view = ephy_tab_view_get_tab_view (tab_view);

  if (!ephy_web_extension_manager_is_active (self, web_extension))
    return;

  /* Add page actions and add content script */
  for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
    GtkWidget *page = ephy_tab_view_get_nth_page (tab_view, i);
    EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (page));

    ephy_web_extension_manager_add_web_extension_to_webview (self, web_extension, window, web_view);
  }

  if (ephy_web_extension_has_browser_action (web_extension)) {
    GtkWidget *browser_action_widget = create_browser_action (web_extension);
    ephy_header_bar_add_browser_action (EPHY_HEADER_BAR (ephy_window_get_header_bar (window)), browser_action_widget);
    g_hash_table_insert (self->browser_action_map, web_extension, browser_action_widget);
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

  remove_content_scripts (web_extension, web_view);
  remove_custom_css (web_extension, web_view);
}

void
ephy_web_extension_manager_remove_web_extension_from_window (EphyWebExtensionManager *self,
                                                             EphyWebExtension        *web_extension,
                                                             EphyWindow              *window)
{
  EphyTabView *tab_view = ephy_window_get_tab_view (EPHY_WINDOW (window));
  HdyTabView *view = ephy_tab_view_get_tab_view (tab_view);
  GtkWidget *browser_action_widget;

  if (ephy_web_extension_manager_is_active (self, web_extension))
    return;

  for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
    GtkWidget *page = ephy_tab_view_get_nth_page (tab_view, i);
    EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (page));

    ephy_web_extension_manager_remove_web_extension_from_webview (self, web_extension, window, web_view);
  }

  browser_action_widget = g_hash_table_lookup (self->browser_action_map, web_extension);
  if (browser_action_widget) {
    g_hash_table_remove (self->browser_action_map, web_extension);
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
  WebKitUserContentManager *ucm;
  GtkWidget *background;
  g_autofree char *base_uri = NULL;
  const char *page;

  if (!ephy_web_extension_has_background_web_view (web_extension) || ephy_web_extension_manager_get_background_web_view (self, web_extension))
    return;

  page = ephy_web_extension_background_web_view_get_page (web_extension);

  /* Create new background web_view */
  background = create_web_extensions_webview (web_extension);
  ephy_web_extension_manager_set_background_web_view (self, web_extension, EPHY_WEB_VIEW (background));

  base_uri = create_base_uri_for_resource_path (web_extension, page);

  if (page) {
    g_autofree char *data = ephy_web_extension_get_resource_as_string (web_extension, page);
    if (data)
      webkit_web_view_load_html (WEBKIT_WEB_VIEW (background), (char *)data, base_uri);
  } else {
    GPtrArray *scripts = ephy_web_extension_background_web_view_get_scripts (web_extension);

    ucm = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (background));

    for (unsigned int i = 0; i < scripts->len; i++) {
      const char *script_file = g_ptr_array_index (scripts, i);
      g_autofree char *data = NULL;
      WebKitUserScript *user_script;

      if (!script_file)
        continue;

      data = ephy_web_extension_get_resource_as_string (web_extension, script_file);
      if (data) {
        user_script = webkit_user_script_new (data,
                                              WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                              WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                                              NULL,
                                              NULL);

        webkit_user_content_manager_add_script (ucm, user_script);
      }
    }
    webkit_web_view_load_html (WEBKIT_WEB_VIEW (background), "<body></body>", base_uri);
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

    if (active)
      ephy_web_extension_manager_add_web_extension_to_window (self, web_extension, window);
    else
      ephy_web_extension_manager_remove_web_extension_from_window (self, web_extension, window);
  }

  if (active) {
    if (ephy_web_extension_has_background_web_view (web_extension))
      run_background_script (self, web_extension);
  }
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

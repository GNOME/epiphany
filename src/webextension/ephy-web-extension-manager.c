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

#include "api/alarms.h"
#include "api/notifications.h"
#include "api/pageaction.h"
#include "api/runtime.h"
#include "api/storage.h"
#include "api/tabs.h"

#include <json-glib/json-glib.h>

static void handle_message_reply (EphyWebExtension *web_extension,
                                  JSCValue         *args);

struct _EphyWebExtensionManager {
  GObject parent_instance;

  GCancellable *cancellable;
  GList *web_extensions;
  GHashTable *page_action_map;
  GHashTable *browser_action_map;

  GHashTable *background_web_views;
  GHashTable *popup_web_views;

  GHashTable *pending_messages;
};

G_DEFINE_TYPE (EphyWebExtensionManager, ephy_web_extension_manager, G_TYPE_OBJECT)

EphyWebExtensionAsyncApiHandler api_handlers[] = {
  {"alarms", ephy_web_extension_api_alarms_handler},
  {"notifications", ephy_web_extension_api_notifications_handler},
  {"pageAction", ephy_web_extension_api_pageaction_handler},
  {"runtime", ephy_web_extension_api_runtime_handler},
  {"storage", ephy_web_extension_api_storage_handler},
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
main_context_web_extension_scheme_cb (WebKitURISchemeRequest *request,
                                      gpointer                user_data)
{
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (user_data);
  EphyWebExtension *web_extension = NULL;
  const char *path;
  const unsigned char *data;
  gsize length;
  g_autoptr (GInputStream) stream = NULL;
  g_auto (GStrv) split = NULL;

  path = webkit_uri_scheme_request_get_uri (request) + strlen ("ephy-webextension://");

  split = g_strsplit (path, "/", -1);
  for (GList *list = self->web_extensions; list && list->data; list = list->next) {
    EphyWebExtension *ext = EPHY_WEB_EXTENSION (list->data);

    if (strcmp (ephy_web_extension_get_guid (ext), split[0]) == 0) {
      web_extension = EPHY_WEB_EXTENSION (list->data);
      break;
    }
  }

  if (!web_extension)
    return;

  /* FIXME: This needs to be filtered by the extension manifest's "web_accessible_resources"
   * property which involves some pattern matching. */

  data = ephy_web_extension_get_resource (web_extension, path + strlen (split[0]) + 1, &length);
  if (!data)
    return;

  stream = g_memory_input_stream_new_from_data (data, length, NULL);
  webkit_uri_scheme_request_finish (request, stream, length, NULL);
}

static void
destroy_widget_list (GSList *widget_list)
{
  g_slist_free_full (widget_list, (GDestroyNotify)gtk_widget_destroy);
}

static void
ephy_web_extension_manager_constructed (GObject *object)
{
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (object);
  g_autofree char *dir = g_build_filename (ephy_default_profile_dir (), "web_extensions", NULL);

  self->background_web_views = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)gtk_widget_destroy);
  self->popup_web_views = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_ptr_array_free);
  self->page_action_map = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)g_hash_table_destroy);
  self->browser_action_map = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)destroy_widget_list);
  self->pending_messages = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_hash_table_destroy);
  self->web_extensions = NULL;

  ephy_web_extension_manager_scan_directory (self, dir);
}

static void
ephy_web_extension_manager_dispose (GObject *object)
{
  EphyWebExtensionManager *self = EPHY_WEB_EXTENSION_MANAGER (object);

  g_clear_pointer (&self->background_web_views, g_hash_table_destroy);
  g_clear_pointer (&self->popup_web_views, g_hash_table_destroy);
  g_clear_pointer (&self->page_action_map, g_hash_table_destroy);
  g_clear_pointer (&self->pending_messages, g_hash_table_destroy);
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
  WebKitWebContext *web_context;

  web_context = ephy_embed_shell_get_web_context (ephy_embed_shell_get_default ());
  webkit_web_context_register_uri_scheme (web_context, "ephy-webextension", main_context_web_extension_scheme_cb, self, NULL);
  webkit_security_manager_register_uri_scheme_as_secure (webkit_web_context_get_security_manager (web_context),
                                                         "ephy-webextension");
}

EphyWebExtensionManager *
ephy_web_extension_manager_get_default (void)
{
  static EphyWebExtensionManager *manager = NULL;

  if (!manager)
    manager = g_object_new (EPHY_TYPE_WEB_EXTENSION_MANAGER, NULL);

  return manager;
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

static void
respond_with_error (WebKitUserMessage *message,
                    const char        *error)
{
  WebKitUserMessage *reply = webkit_user_message_new ("error", g_variant_new_string (error));
  webkit_user_message_send_reply (message, reply);
}

typedef struct {
  WebKitUserMessage *message;
  JSCValue *args;
} ApiHandlerData;

static void
api_handler_data_free (ApiHandlerData *data)
{
  g_object_unref (data->message);
  g_object_unref (data->args);
  g_free (data);
}

static ApiHandlerData *
api_handler_data_new (WebKitUserMessage *message,
                      JSCValue          *args)
{
  ApiHandlerData *data = g_new (ApiHandlerData, 1);
  data->message = g_object_ref (message);
  data->args = g_object_ref (args);
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
ephy_web_extension_handle_user_message (WebKitWebContext  *context,
                                        WebKitUserMessage *message,
                                        gpointer           user_data)
{
  EphyWebExtension *web_extension = user_data;
  g_autoptr (JSCContext) js_context = NULL;
  g_autoptr (JSCValue) args = NULL;
  const char *name = webkit_user_message_get_name (message);
  g_auto (GStrv) split = NULL;
  guint page_id;
  const char *json_args;

  g_variant_get (webkit_user_message_get_parameters (message), "(u&s)", &page_id, &json_args);

  js_context = jsc_context_new ();
  args = jsc_value_new_from_json (js_context, json_args);

  LOG ("%s(): Called for %s, function %s (%s)\n", __FUNCTION__, ephy_web_extension_get_name (web_extension), name, json_args);

  /* Private API for message replies handled by the manager. */
  if (strcmp (name, "runtime._sendMessageReply") == 0) {
    handle_message_reply (web_extension, args);
    return TRUE;
  }

  split = g_strsplit (name, ".", 2);
  if (g_strv_length (split) != 2) {
    respond_with_error (message, "Invalid function name");
    return TRUE;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (api_handlers); idx++) {
    EphyWebExtensionAsyncApiHandler handler = api_handlers[idx];

    if (g_strcmp0 (handler.name, split[0]) == 0) {
      /* TODO: Cancellable */
      GTask *task = g_task_new (web_extension, NULL, (GAsyncReadyCallback)on_web_extension_api_handler_finish, NULL);
      g_task_set_task_data (task, api_handler_data_new (message, args), (GDestroyNotify)api_handler_data_free);

      handler.execute (web_extension, split[1], args, page_id, task);
      return TRUE;
    }
  }

  g_warning ("%s(): '%s' not implemented by Epiphany!", __FUNCTION__, name);
  respond_with_error (message, "Not Implemented");
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
  EphyWebExtensionManager *self = ephy_web_extension_manager_get_default ();

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

  /* FIXME: For paths on different hosts we should support web_accessible_resources. */
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
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  g_autoptr (WebKitUserContentManager) ucm = NULL;
  WebKitSettings *settings;
  WebKitWebContext *web_context;
  GtkWidget *web_view;

  /* Create an own ucm so new scripts/css are only applied to this web_view */
  ucm = webkit_user_content_manager_new ();

  web_context = webkit_web_context_new ();

  webkit_web_context_register_uri_scheme (web_context, "ephy-webextension", web_extension_cb, web_extension, NULL);
  webkit_security_manager_register_uri_scheme_as_secure (webkit_web_context_get_security_manager (web_context),
                                                         "ephy-webextension");

  g_signal_connect_object (web_context, "initialize-web-extensions", G_CALLBACK (init_web_extension_api), web_extension, 0);
  g_signal_connect (web_context, "user-message-received", G_CALLBACK (ephy_web_extension_handle_user_message), web_extension);

  web_view = g_object_new (WEBKIT_TYPE_WEB_VIEW,
                           "web-context", web_context,
                           "user-content-manager", ucm,
                           "settings", ephy_embed_prefs_get_settings (),
                           "related-view", ephy_web_extension_manager_get_background_web_view (manager, web_extension),
                           NULL);

  settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (web_view));
  webkit_settings_set_enable_write_console_messages_to_stdout (settings, TRUE);

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

static void
on_popup_load_changed (WebKitWebView   *web_view,
                       WebKitLoadEvent  load_event,
                       gpointer         user_data)
{
  /* Delay showing so the popover grows to the proper size. */
  /* TODO: Other browsers resize on DOM changes.
   *       We also need to limit this to a max size of 800x600 like Firefox. */
  if (load_event == WEBKIT_LOAD_FINISHED)
    gtk_widget_show (GTK_WIDGET (web_view));
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

static GtkWidget *
create_browser_popup (EphyWebExtension *web_extension)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  GtkWidget *web_view;
  g_autofree char *popup_uri = NULL;
  const char *popup;

  web_view = create_web_extensions_webview (web_extension);
  gtk_widget_hide (web_view); /* Shown in on_popup_load_changed. */
  ephy_web_extension_manager_register_popup_view (manager, web_extension, web_view);

  popup = ephy_web_extension_get_browser_popup (web_extension);
  popup_uri = g_strdup_printf ("ephy-webextension://%s/%s", ephy_web_extension_get_guid (web_extension), popup);
  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (web_view), popup_uri);
  g_signal_connect (web_view, "load-changed", G_CALLBACK (on_popup_load_changed), NULL);

  return web_view;
}

static gboolean
on_browser_action_clicked (GtkWidget *event_box,
                           gpointer   user_data)
{
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);
  EphyWebExtensionManager *self = ephy_web_extension_manager_get_default ();
  g_autofree char *script = NULL;
  WebKitWebView *web_view = ephy_web_extension_manager_get_background_web_view (self, web_extension);

  script = g_strdup_printf ("window.browser.browserAction.onClicked._emit();");

  webkit_web_view_run_javascript (web_view,
                                  script,
                                  NULL,
                                  NULL,
                                  NULL);

  return GDK_EVENT_STOP;
}

static void
on_browser_action_visible_changed (GtkWidget  *popover,
                                   GParamSpec *pspec,
                                   gpointer    user_data)
{
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);
  GtkWidget *child;

  if (gtk_widget_get_visible (popover)) {
    child = create_browser_popup (web_extension);
    gtk_container_add (GTK_CONTAINER (popover), child);
  } else {
    child = gtk_bin_get_child (GTK_BIN (popover));
    gtk_container_remove (GTK_CONTAINER (popover), child);
  }
}

GtkWidget *
create_browser_action (EphyWebExtension *web_extension)
{
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *popover;
  GdkPixbuf *pixbuf;

  pixbuf = ephy_web_extension_browser_action_get_icon (web_extension, 16);
  if (pixbuf)
    image = gtk_image_new_from_pixbuf (pixbuf);
  else
    image = gtk_image_new_from_icon_name ("application-x-addon-symbolic", GTK_ICON_SIZE_BUTTON);

  if (ephy_web_extension_get_browser_popup (web_extension)) {
    button = gtk_menu_button_new ();
    popover = gtk_popover_new (NULL);
    g_signal_connect (popover, "notify::visible", G_CALLBACK (on_browser_action_visible_changed), web_extension);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
    gtk_button_set_image (GTK_BUTTON (button), image);
  } else {
    button = gtk_button_new ();
    g_signal_connect_object (button, "clicked", G_CALLBACK (on_browser_action_clicked), web_extension, 0);
    gtk_button_set_image (GTK_BUTTON (button), image);
  }

  gtk_widget_set_visible (button, TRUE);

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
    GSList *widget_list = g_hash_table_lookup (self->browser_action_map, web_extension);

    ephy_header_bar_add_browser_action (EPHY_HEADER_BAR (ephy_window_get_header_bar (window)), browser_action_widget);

    g_hash_table_steal (self->browser_action_map, web_extension); /* Avoid freeing list. */
    g_hash_table_insert (self->browser_action_map, web_extension, g_slist_append (widget_list, browser_action_widget));
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
  WebKitUserContentManager *ucm;
  GtkWidget *background;
  GPtrArray *scripts;
  const char *page;
  g_autofree char *base_uri = NULL;

  if (!ephy_web_extension_has_background_web_view (web_extension) || ephy_web_extension_manager_get_background_web_view (self, web_extension))
    return;

  page = ephy_web_extension_background_web_view_get_page (web_extension);

  /* Create new background web_view */
  background = create_web_extensions_webview (web_extension);
  ephy_web_extension_manager_set_background_web_view (self, web_extension, WEBKIT_WEB_VIEW (background));

  if (page) {
    g_autofree char *page_uri = g_strdup_printf ("ephy-webextension://%s/%s", ephy_web_extension_get_guid (web_extension), page);
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (background), page_uri);
    return;
  }

  scripts = ephy_web_extension_background_web_view_get_scripts (web_extension);
  base_uri = create_base_uri_for_resource_path (web_extension, page);

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
  } else {
    g_hash_table_remove (self->browser_action_map, web_extension);
    g_hash_table_remove (self->background_web_views, web_extension);
    g_object_set_data (G_OBJECT (web_extension), "alarms", NULL); /* Set in alarms.c */
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

static void
handle_message_reply (EphyWebExtension *web_extension,
                      JSCValue         *args)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  GHashTable *pending_messages = g_hash_table_lookup (manager->pending_messages, web_extension);
  GTask *pending_task;
  g_autofree char *message_guid = NULL;
  g_autoptr (JSCValue) message_guid_value = NULL;
  g_autoptr (JSCValue) reply_value = NULL;

  g_message ("handle_message_reply");

  message_guid_value = jsc_value_object_get_property_at_index (args, 0);
  if (!jsc_value_is_string (message_guid_value)) {
    g_debug ("Received invalid message reply");
    return;
  }

  message_guid = jsc_value_to_string (message_guid_value);
  pending_task = g_hash_table_lookup (pending_messages, message_guid);
  if (!pending_task) {
    g_debug ("Received message not found in pending replies");
    return;
  }

  reply_value = jsc_value_object_get_property_at_index (args, 1);
  g_hash_table_steal (pending_messages, message_guid);
  g_task_return_pointer (pending_task, jsc_value_to_json (reply_value, 0), g_free);
}

typedef struct {
  EphyWebExtension *web_extension;
  char *message_guid; /* Owned by manager->pending_messages. */
  guint pending_view_responses;
  gboolean handled;
} PendingMessageReplyTracker;

static void
on_extension_emit_ready (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  PendingMessageReplyTracker *tracker = user_data;
  GHashTable *pending_messages;
  g_autoptr (GError) error = NULL;
  g_autoptr (WebKitJavascriptResult) js_result = NULL;

  js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (source),
                                                     result,
                                                     &error);

  if (error) {
    g_warning ("%s", error->message);
    return;
  }

  if (jsc_value_to_boolean (webkit_javascript_result_get_js_value (js_result)))
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
      g_assert (pending_task);
      g_assert (g_hash_table_steal (pending_messages, tracker->message_guid));
      g_clear_pointer (&tracker->message_guid, g_free);

      g_task_return_pointer (pending_task, NULL, NULL);
    }
    g_free (tracker);
  }
}

static void
ephy_web_extension_manager_emit_in_extension_views_internal (EphyWebExtensionManager *self,
                                                             EphyWebExtension        *web_extension,
                                                             const char              *name,
                                                             const char              *message_json,
                                                             gint64                   page_id_exception,
                                                             const char              *sender_json,
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
    message_guid = g_dbus_generate_guid ();
    tracker = g_new0 (PendingMessageReplyTracker, 1);
    script = g_strdup_printf ("window.browser.%s._emit_with_reply(%s, %s, '%s');", name, message_json, sender_json, message_guid);
  } else
    script = g_strdup_printf ("window.browser.%s._emit(%s);", name, message_json);

  if (background_view) {
    if ((gint64)webkit_web_view_get_page_id (background_view) != page_id_exception) {
      webkit_web_view_run_javascript (background_view,
                                      script,
                                      NULL,
                                      reply_task ? on_extension_emit_ready : NULL,
                                      tracker);
      pending_views++;
    }
  }

  if (popup_views) {
    for (guint i = 0; i < popup_views->len; i++) {
      WebKitWebView *popup_view = g_ptr_array_index (popup_views, i);
      if ((gint64)webkit_web_view_get_page_id (popup_view) == page_id_exception)
        continue;

      webkit_web_view_run_javascript (popup_view,
                                      script,
                                      NULL,
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
  ephy_web_extension_manager_emit_in_extension_views_internal (self, web_extension, name, json, -1, NULL, NULL);
}

void
ephy_web_extension_manager_emit_in_extension_views_except_self (EphyWebExtensionManager *self,
                                                                EphyWebExtension        *web_extension,
                                                                const char              *name,
                                                                const char              *json,
                                                                gint64                   extension_page_id)
{
  g_assert (extension_page_id > 0);
  ephy_web_extension_manager_emit_in_extension_views_internal (self, web_extension, name, json, extension_page_id, NULL, NULL);
}

void
ephy_web_extension_manager_emit_in_extension_views_with_reply (EphyWebExtensionManager *self,
                                                               EphyWebExtension        *web_extension,
                                                               const char              *name,
                                                               const char              *json,
                                                               gint64                   extension_page_id,
                                                               const char              *sender_json,
                                                               GTask                   *reply_task)
{
  g_assert (sender_json);
  g_assert (reply_task);
  g_assert (extension_page_id > 0);
  ephy_web_extension_manager_emit_in_extension_views_internal (self, web_extension, name, json, extension_page_id, sender_json, reply_task);
}

}

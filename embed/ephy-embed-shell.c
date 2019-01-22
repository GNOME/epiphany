/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2011 Igalia S.L.
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

#include <config.h>
#include "ephy-embed-shell.h"

#include "ephy-about-handler.h"
#include "ephy-dbus-util.h"
#include "ephy-debug.h"
#include "ephy-downloads-manager.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-encodings.h"
#include "ephy-file-helpers.h"
#include "ephy-filters-manager.h"
#include "ephy-flatpak-utils.h"
#include "ephy-history-service.h"
#include "ephy-password-manager.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-snapshot-service.h"
#include "ephy-tabs-catalog.h"
#include "ephy-uri-helpers.h"
#include "ephy-view-source-handler.h"
#include "ephy-web-app-utils.h"
#include "ephy-web-process-extension-proxy.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#define PAGE_SETUP_FILENAME "page-setup-gtk.ini"
#define PRINT_SETTINGS_FILENAME "print-settings.ini"

typedef struct {
  WebKitWebContext *web_context;
  EphyHistoryService *global_history_service;
  EphyGSBService *global_gsb_service;
  EphyEncodings *encodings;
  GtkPageSetup *page_setup;
  GtkPrintSettings *print_settings;
  EphyEmbedShellMode mode;
  WebKitUserContentManager *user_content;
  EphyDownloadsManager *downloads_manager;
  EphyPermissionsManager *permissions_manager;
  EphyPasswordManager *password_manager;
  EphyAboutHandler *about_handler;
  EphyViewSourceHandler *source_handler;
  char *guid;
  GDBusServer *dbus_server;
  GList *web_process_extensions;
  EphyFiltersManager *filters_manager;
  EphySearchEngineManager *search_engine_manager;
  GCancellable *cancellable;
} EphyEmbedShellPrivate;

enum {
  RESTORED_WINDOW,
  WEB_VIEW_CREATED,
  PAGE_CREATED,
  ALLOW_TLS_CERTIFICATE,
  ALLOW_UNSAFE_BROWSING,
  PASSWORD_FORM_FOCUSED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum {
  PROP_0,
  PROP_MODE,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static EphyEmbedShell *embed_shell = NULL;

static void ephy_embed_shell_tabs_catalog_iface_init (EphyTabsCatalogInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyEmbedShell, ephy_embed_shell, DZL_TYPE_APPLICATION,
                         G_ADD_PRIVATE (EphyEmbedShell)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_TABS_CATALOG,
                                                ephy_embed_shell_tabs_catalog_iface_init))

static EphyWebView *
ephy_embed_shell_get_view_for_page_id (EphyEmbedShell *self,
                                       guint64         page_id,
                                       const char     *origin)
{
  GList *windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (GList *l = windows; l && l->data; l = l->next) {
    g_autoptr(GList) tabs = ephy_embed_container_get_children (l->data);

    for (GList *t = tabs; t && t->data; t = t->next) {
      EphyWebView *ephy_view = ephy_embed_get_web_view (t->data);
      WebKitWebView *web_view = WEBKIT_WEB_VIEW (ephy_view);
      g_autofree char *real_origin = NULL;

      if (webkit_web_view_get_page_id (web_view) != page_id)
        continue;

      real_origin = ephy_uri_to_security_origin (webkit_web_view_get_uri (web_view));

      if (g_strcmp0 (real_origin, origin)) {
        g_debug ("Extension's origin '%s' doesn't match real origin '%s'", origin, real_origin);
        return NULL;
      }

      return ephy_view;
    }
  }

  return NULL;
}

static EphyWebProcessExtensionProxy *
ephy_embed_shell_get_extension_proxy_for_page_id (EphyEmbedShell *self,
                                                  guint64         page_id,
                                                  const char     *origin)
{
  EphyWebView *view = ephy_embed_shell_get_view_for_page_id (self, page_id, origin);
  return view ? ephy_web_view_get_web_process_extension_proxy (view) : NULL;
}

static GList *
tabs_catalog_get_tabs_info (EphyTabsCatalog *catalog)
{
  EphyEmbedShell *embed_shell = EPHY_EMBED_SHELL (catalog);
  WebKitFaviconDatabase *database;
  GList *windows;
  g_autoptr(GList) tabs = NULL;
  GList *tabs_info = NULL;
  const char *title;
  const char *url;
  g_autofree char *favicon = NULL;

  windows = gtk_application_get_windows (GTK_APPLICATION (embed_shell));
  database = webkit_web_context_get_favicon_database (ephy_embed_shell_get_web_context (embed_shell));

  for (GList *l = windows; l && l->data; l = l->next) {
    tabs = ephy_embed_container_get_children (l->data);

    for (GList *t = tabs; t && t->data; t = t->next) {
      title = ephy_embed_get_title (t->data);

      if (!g_strcmp0 (title, _(BLANK_PAGE_TITLE)) || !g_strcmp0 (title, _(OVERVIEW_PAGE_TITLE)))
        continue;

      url = ephy_web_view_get_display_address (ephy_embed_get_web_view (t->data));
      favicon = webkit_favicon_database_get_favicon_uri (database, url);

      tabs_info = g_list_prepend (tabs_info,
                                  ephy_tab_info_new (title, url, favicon));
    }
  }

  return tabs_info;
}

static void
ephy_embed_shell_tabs_catalog_iface_init (EphyTabsCatalogInterface *iface)
{
  iface->get_tabs_info = tabs_catalog_get_tabs_info;
}

static void
ephy_embed_shell_dispose (GObject *object)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (EPHY_EMBED_SHELL (object));

  if (priv->cancellable) {
    g_cancellable_cancel (priv->cancellable);
    g_clear_object (&priv->cancellable);
  }

  if (priv->web_process_extensions) {
    g_list_free_full (priv->web_process_extensions, g_object_unref);
    priv->web_process_extensions = NULL;
  }

  g_clear_object (&priv->encodings);
  g_clear_object (&priv->page_setup);
  g_clear_object (&priv->print_settings);
  g_clear_object (&priv->global_history_service);
  g_clear_object (&priv->global_gsb_service);
  g_clear_object (&priv->about_handler);
  g_clear_object (&priv->source_handler);
  g_clear_object (&priv->user_content);
  g_clear_object (&priv->downloads_manager);
  g_clear_object (&priv->password_manager);
  g_clear_object (&priv->permissions_manager);
  g_clear_object (&priv->web_context);
  g_clear_pointer (&priv->guid, g_free);
  g_clear_object (&priv->dbus_server);
  g_clear_object (&priv->filters_manager);
  g_clear_object (&priv->search_engine_manager);

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->dispose (object);
}

static void
web_process_extension_password_form_focused_message_received_cb (WebKitUserContentManager *manager,
                                                                 WebKitJavascriptResult   *message,
                                                                 EphyEmbedShell           *shell)
{
  guint64 page_id;
  gboolean insecure_action;
  g_autoptr(GVariant) variant = NULL;
  g_autofree char *message_str = NULL;

  message_str = jsc_value_to_string (webkit_javascript_result_get_js_value (message));
  variant = g_variant_parse (G_VARIANT_TYPE ("(tb)"), message_str, NULL, NULL, NULL);

  g_variant_get (variant, "(tb)", &page_id, &insecure_action);
  g_signal_emit (shell, signals[PASSWORD_FORM_FOCUSED], 0,
                 page_id, insecure_action);
}

static void
history_service_query_urls_cb (EphyHistoryService *service,
                               gboolean            success,
                               GList              *urls,
                               EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  if (!success)
    return;

  for (l = priv->web_process_extensions; l; l = g_list_next (l)) {
    EphyWebProcessExtensionProxy *web_process_extension = (EphyWebProcessExtensionProxy *)l->data;

    ephy_web_process_extension_proxy_history_set_urls (web_process_extension, urls);
  }

  for (l = urls; l; l = g_list_next (l))
    ephy_embed_shell_schedule_thumbnail_update (shell, (EphyHistoryURL *)l->data);
}

static void
ephy_embed_shell_update_overview_urls (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr(EphyHistoryQuery) query = NULL;

  query = ephy_history_query_new_for_overview ();
  ephy_history_service_query_urls (priv->global_history_service, query, NULL,
                                   (EphyHistoryJobCallback)history_service_query_urls_cb,
                                   shell);
}

static void
history_service_urls_visited_cb (EphyHistoryService *history,
                                 EphyEmbedShell     *shell)
{
  ephy_embed_shell_update_overview_urls (shell);
}

static void
history_set_url_hidden_cb (EphyHistoryService *service,
                           gboolean            success,
                           gpointer            result_data,
                           EphyEmbedShell     *shell)
{
  if (!success)
    return;

  ephy_embed_shell_update_overview_urls (shell);
}

static void
web_process_extension_overview_message_received_cb (WebKitUserContentManager *manager,
                                                    WebKitJavascriptResult   *message,
                                                    EphyEmbedShell           *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autofree char *url_to_remove = NULL;

  url_to_remove = jsc_value_to_string (webkit_javascript_result_get_js_value (message));

  ephy_history_service_set_url_hidden (priv->global_history_service,
                                       url_to_remove, TRUE, NULL,
                                       (EphyHistoryJobCallback)history_set_url_hidden_cb,
                                       shell);
}

static void
web_process_extension_tls_error_page_message_received_cb (WebKitUserContentManager *manager,
                                                          WebKitJavascriptResult   *message,
                                                          EphyEmbedShell           *shell)
{
  guint64 page_id;

  page_id = jsc_value_to_double (webkit_javascript_result_get_js_value (message));
  g_signal_emit (shell, signals[ALLOW_TLS_CERTIFICATE], 0, page_id);
}

static void
web_process_extension_unsafe_browsing_error_page_message_received_cb (WebKitUserContentManager *manager,
                                                                      WebKitJavascriptResult   *message,
                                                                      EphyEmbedShell           *shell)
{
  guint64 page_id;

  page_id = jsc_value_to_double (webkit_javascript_result_get_js_value (message));
  g_signal_emit (shell, signals[ALLOW_UNSAFE_BROWSING], 0, page_id);
}

static void
web_process_extension_about_apps_message_received_cb (WebKitUserContentManager *manager,
                                                      WebKitJavascriptResult   *message,
                                                      EphyEmbedShell           *shell)
{
  g_autofree char *app_id = NULL;

  app_id = jsc_value_to_string (webkit_javascript_result_get_js_value (message));
  ephy_web_application_delete (app_id);
}

typedef struct {
  EphyEmbedShell *shell;
  char *origin;
  gint32 promise_id;
  guint64 page_id;
  guint64 frame_id;
} PasswordManagerData;

static void
password_manager_data_free (PasswordManagerData *data)
{
  g_object_unref (data->shell);
  g_free (data->origin);
  g_free (data);
}

static void
password_manager_query_finished_cb (GList               *records,
                                    PasswordManagerData *data)
{
  EphyWebProcessExtensionProxy *proxy;
  EphyPasswordRecord *record;
  const char *username = NULL;
  const char *password = NULL;

  record = records && records->data ? EPHY_PASSWORD_RECORD (records->data) : NULL;
  if (record) {
    username = ephy_password_record_get_username (record);
    password = ephy_password_record_get_password (record);
  }

  proxy = ephy_embed_shell_get_extension_proxy_for_page_id (data->shell,
                                                                                   data->page_id,
                                                                                   data->origin);
  if (proxy)
    ephy_web_process_extension_proxy_password_query_response (proxy, username, password, data->promise_id, data->frame_id);

  password_manager_data_free (data);
}

static char *
property_to_string_or_null (JSCValue   *value,
                            const char *name)
{
  g_autoptr(JSCValue) prop = jsc_value_object_get_property (value, name);
  if (jsc_value_is_null (prop) || jsc_value_is_undefined (prop))
    return NULL;
  return jsc_value_to_string (prop);
}

static int
property_to_int32 (JSCValue   *value,
                   const char *name)
{
  g_autoptr(JSCValue) prop = jsc_value_object_get_property (value, name);
  return jsc_value_to_int32 (prop);
}

static int
property_to_uint64 (JSCValue   *value,
                    const char *name)
{
  g_autoptr(JSCValue) prop = jsc_value_object_get_property (value, name);
  return (guint64)jsc_value_to_double (prop);
}

static void
web_process_extension_password_manager_query_received_cb (WebKitUserContentManager *manager,
                                                          WebKitJavascriptResult   *message,
                                                          EphyEmbedShell           *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  JSCValue *value = webkit_javascript_result_get_js_value (message);
  g_autofree char *origin = property_to_string_or_null (value, "origin");
  g_autofree char *target_origin = property_to_string_or_null (value, "targetOrigin");
  g_autofree char *username = property_to_string_or_null (value, "username");
  g_autofree char *username_field = property_to_string_or_null (value, "usernameField");
  g_autofree char *password_field = property_to_string_or_null (value, "passwordField");
  gint32 promise_id = property_to_int32 (value, "promiseID");
  guint64 page_id = property_to_uint64 (value, "pageID");
  guint64 frame_id = property_to_uint64 (value, "frameID");
  PasswordManagerData *data;

  if (!origin || !target_origin || !password_field)
    return;

  /* Don't include username_field in queries unless we actually have a username
   * to go along with it, or the query will fail because we don't save
   * username_field without a corresponding username.
   */
  if (!username && username_field)
    g_clear_pointer (&username_field, g_free);

  data = g_new (PasswordManagerData, 1);
  data->shell = g_object_ref (shell);
  data->promise_id = promise_id;
  data->page_id = page_id;
  data->frame_id = frame_id;
  data->origin = g_strdup (origin);

  ephy_password_manager_query (priv->password_manager,
                               NULL,
                               origin,
                               target_origin,
                               username,
                               username_field,
                               password_field,
                               (EphyPasswordManagerQueryCallback)password_manager_query_finished_cb,
                               data);
}

typedef struct {
  EphyPasswordManager *password_manager;
  EphyPermissionsManager *permissions_manager;
  char *origin;
  char *target_origin;
  char *username;
  char *password;
  char *username_field;
  char *password_field;
  gboolean is_new;
} SaveAuthRequest;

static void
save_auth_request_free (SaveAuthRequest *request)
{
  g_object_unref (request->password_manager);
  g_object_unref (request->permissions_manager);
  g_free (request->origin);
  g_free (request->target_origin);
  g_free (request->username);
  g_free (request->password);
  g_free (request->username_field);
  g_free (request->password_field);
  g_free (request);
}

static void
save_auth_request_response_cb (gint                 response_id,
                               SaveAuthRequest      *data)
{
  if (response_id == GTK_RESPONSE_REJECT) {
    ephy_permissions_manager_set_permission (data->permissions_manager,
                                             EPHY_PERMISSION_TYPE_SAVE_PASSWORD,
                                             data->origin,
                                             EPHY_PERMISSION_DENY);
  } else if (response_id == GTK_RESPONSE_YES) {
    ephy_password_manager_save (data->password_manager, data->origin, data->target_origin,
                                data->username, data->password, data->username_field,
                                data->password_field, data->is_new);
  }
}

static void
web_process_extension_password_manager_save_real (EphyEmbedShell *shell,
                                                  JSCValue       *value,
                                                  gboolean        is_request)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autofree char *origin = property_to_string_or_null (value, "origin");
  g_autofree char *target_origin = property_to_string_or_null (value, "targetOrigin");
  g_autofree char *username = property_to_string_or_null (value, "username");
  g_autofree char *password = property_to_string_or_null (value, "password");
  g_autofree char *username_field = property_to_string_or_null (value, "usernameField");
  g_autofree char *password_field = property_to_string_or_null (value, "passwordField");
  g_autoptr(JSCValue) is_new_prop = jsc_value_object_get_property (value, "isNew");
  gboolean is_new = jsc_value_to_boolean (is_new_prop);
  guint64 page_id = property_to_uint64 (value, "pageID");
  EphyWebView *view;
  SaveAuthRequest *request;

  /* Both origin and target origin are required. */
  if (!origin || !target_origin)
    return;

  /* Both password and password field are required. */
  if (!password || !password_field)
    return;

  /* The username field is required if username is present. */
  if (username && !username_field)
    g_clear_pointer (&username, g_free);

  /* The username is required if username field is present. */
  if (!username && username_field)
    g_clear_pointer (&username_field, g_free);

  /* This also sanity checks that a page isn't saving websites for
   * other origins. Remember the request comes from the untrusted web
   * process and we have to make sure it's not being evil here. This
   * could also happen even without malice if the origin of a subframe
   * doesn't match the origin of the main frame (in which case we'll
   * refuse to save the password).
   */
  view = ephy_embed_shell_get_view_for_page_id (shell, page_id, origin);
  if (!view)
    return;

  if (!is_request) {
    ephy_password_manager_save (priv->password_manager, origin, target_origin, username,
                                password, username_field, password_field, is_new);
    return;
  }

  request = g_new (SaveAuthRequest, 1);
  request->password_manager = g_object_ref (priv->password_manager);
  request->permissions_manager = g_object_ref (priv->permissions_manager);
  request->origin = g_steal_pointer (&origin);
  request->target_origin = g_steal_pointer (&target_origin);
  request->username = g_steal_pointer (&username);
  request->password = g_steal_pointer (&password);
  request->username_field = g_steal_pointer (&username_field);
  request->password_field = g_steal_pointer (&password_field);
  request->is_new = is_new;
  ephy_web_view_show_auth_form_save_request (view, request->origin, request->username,
                                             (EphyPasswordSaveRequestCallback)save_auth_request_response_cb,
                                             request, (GDestroyNotify)save_auth_request_free);
}

static void
web_process_extension_password_manager_save_received_cb (WebKitUserContentManager *manager,
                                                         WebKitJavascriptResult   *message,
                                                         EphyEmbedShell           *shell)
{
  JSCValue *value = webkit_javascript_result_get_js_value (message);
  web_process_extension_password_manager_save_real (shell, value, FALSE);
}

static void
web_process_extension_password_manager_request_save_received_cb (WebKitUserContentManager *manager,
                                                                 WebKitJavascriptResult   *message,
                                                                 EphyEmbedShell           *shell)
{
  JSCValue *value = webkit_javascript_result_get_js_value (message);
  web_process_extension_password_manager_save_real (shell, value, TRUE);
}

static void
web_process_extension_password_manager_query_usernames_received_cb (WebKitUserContentManager *manager,
                                                                    WebKitJavascriptResult   *message,
                                                                    EphyEmbedShell           *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  JSCValue *value = webkit_javascript_result_get_js_value (message);
  g_autofree char *origin = property_to_string_or_null (value, "origin");
  gint32 promise_id = property_to_int32 (value, "promiseID");
  guint64 page_id = property_to_uint64 (value, "pageID");
  guint64 frame_id = property_to_uint64 (value, "frameID");
  GList *usernames;
  EphyWebProcessExtensionProxy *proxy;

  if (!origin)
    return;

  usernames = ephy_password_manager_get_usernames_for_origin (priv->password_manager, origin);

  proxy = ephy_embed_shell_get_extension_proxy_for_page_id (shell, page_id, origin);
  if (proxy)
    ephy_web_process_extension_proxy_password_query_usernames_response (proxy, usernames, promise_id, frame_id);
}

static void
history_service_url_title_changed_cb (EphyHistoryService *service,
                                      const char         *url,
                                      const char         *title,
                                      EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  for (l = priv->web_process_extensions; l; l = g_list_next (l)) {
    EphyWebProcessExtensionProxy *web_process_extension = (EphyWebProcessExtensionProxy *)l->data;

    ephy_web_process_extension_proxy_history_set_url_title (web_process_extension, url, title);
  }
}

static void
history_service_url_deleted_cb (EphyHistoryService *service,
                                EphyHistoryURL     *url,
                                EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  for (l = priv->web_process_extensions; l; l = g_list_next (l)) {
    EphyWebProcessExtensionProxy *web_process_extension = (EphyWebProcessExtensionProxy *)l->data;

    ephy_web_process_extension_proxy_history_delete_url (web_process_extension, url->url);
  }
}

static void
history_service_host_deleted_cb (EphyHistoryService *service,
                                 const char         *deleted_url,
                                 EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;
  g_autoptr(SoupURI) deleted_uri = NULL;

  deleted_uri = soup_uri_new (deleted_url);

  for (l = priv->web_process_extensions; l; l = g_list_next (l)) {
    EphyWebProcessExtensionProxy *web_process_extension = (EphyWebProcessExtensionProxy *)l->data;

    ephy_web_process_extension_proxy_history_delete_host (web_process_extension, soup_uri_get_host (deleted_uri));
  }
}

static void
history_service_cleared_cb (EphyHistoryService *service,
                            EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  for (l = priv->web_process_extensions; l; l = g_list_next (l)) {
    EphyWebProcessExtensionProxy *web_process_extension = (EphyWebProcessExtensionProxy *)l->data;

    ephy_web_process_extension_proxy_history_clear (web_process_extension);
  }
}

typedef struct {
  EphyWebProcessExtensionProxy *extension;
  char *url;
  char *path;
} DelayedThumbnailUpdateData;

static DelayedThumbnailUpdateData *
delayed_thumbnail_update_data_new (EphyWebProcessExtensionProxy *extension,
                                   const char                   *url,
                                   const char                   *path)
{
  DelayedThumbnailUpdateData *data = g_new (DelayedThumbnailUpdateData, 1);
  data->extension = extension;
  data->url = g_strdup (url);
  data->path = g_strdup (path);
  g_object_add_weak_pointer (G_OBJECT (extension), (gpointer *)&data->extension);
  return data;
}

static void
delayed_thumbnail_update_data_free (DelayedThumbnailUpdateData *data)
{
  if (data->extension)
    g_object_remove_weak_pointer (G_OBJECT (data->extension), (gpointer *)&data->extension);
  g_free (data->url);
  g_free (data->path);
  g_free (data);
}

static gboolean
delayed_thumbnail_update_cb (DelayedThumbnailUpdateData *data)
{
  if (!data->extension) {
    delayed_thumbnail_update_data_free (data);
    return G_SOURCE_REMOVE;
  }

  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (data->extension), "initialized"))) {
    ephy_web_process_extension_proxy_history_set_url_thumbnail (data->extension, data->url, data->path);
    delayed_thumbnail_update_data_free (data);
    return G_SOURCE_REMOVE;
  }

  /* Web process extension is not initialized yet, try again later.... */
  return G_SOURCE_CONTINUE;
}

void
ephy_embed_shell_set_thumbnail_path (EphyEmbedShell *shell,
                                     const char     *url,
                                     const char     *path)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  for (l = priv->web_process_extensions; l; l = g_list_next (l)) {
    EphyWebProcessExtensionProxy *web_process_extension = (EphyWebProcessExtensionProxy *)l->data;
    if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (web_process_extension), "initialized"))) {
      ephy_web_process_extension_proxy_history_set_url_thumbnail (web_process_extension, url, path);
    } else {
      DelayedThumbnailUpdateData *data = delayed_thumbnail_update_data_new (web_process_extension, url, path);
      g_timeout_add (50, (GSourceFunc)delayed_thumbnail_update_cb, data);
    }
  }
}

static void
got_snapshot_path_for_url_cb (EphySnapshotService *service,
                              GAsyncResult        *result,
                              char                *url)
{
  g_autofree char *snapshot = NULL;
  g_autoptr(GError) error = NULL;

  snapshot = ephy_snapshot_service_get_snapshot_path_for_url_finish (service, result, &error);
  if (snapshot) {
    ephy_embed_shell_set_thumbnail_path (ephy_embed_shell_get_default (), url, snapshot);
  } else {
    /* Bad luck, not something to warn about. */
    g_info ("Failed to get snapshot for URL %s: %s", url, error->message);
  }

  g_free (url);
}

void
ephy_embed_shell_schedule_thumbnail_update (EphyEmbedShell *shell,
                                            EphyHistoryURL *url)
{
  EphySnapshotService *service;
  const char *snapshot;

  service = ephy_snapshot_service_get_default ();
  snapshot = ephy_snapshot_service_lookup_cached_snapshot_path (service, url->url);

  if (snapshot) {
    ephy_embed_shell_set_thumbnail_path (shell, url->url, snapshot);
  } else {
    ephy_snapshot_service_get_snapshot_path_for_url_async (service,
                                                           url->url,
                                                           NULL,
                                                           (GAsyncReadyCallback)got_snapshot_path_for_url_cb,
                                                           g_strdup (url->url));
  }
}

/**
 * ephy_embed_shell_get_global_history_service:
 * @shell: the #EphyEmbedShell
 *
 * Return value: (transfer none): the global #EphyHistoryService
 **/
EphyHistoryService *
ephy_embed_shell_get_global_history_service (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  if (!priv->global_history_service) {
    g_autofree char *filename = NULL;
    EphySQLiteConnectionMode mode;

    if (priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO ||
        priv->mode == EPHY_EMBED_SHELL_MODE_AUTOMATION ||
        priv->mode == EPHY_EMBED_SHELL_MODE_SEARCH_PROVIDER)
      mode = EPHY_SQLITE_CONNECTION_MODE_READ_ONLY;
    else
      mode = EPHY_SQLITE_CONNECTION_MODE_READWRITE;

    filename = g_build_filename (ephy_profile_dir (), EPHY_HISTORY_FILE, NULL);
    priv->global_history_service = ephy_history_service_new (filename, mode);

    g_signal_connect_object (priv->global_history_service, "urls-visited",
                             G_CALLBACK (history_service_urls_visited_cb),
                             shell, 0);
    g_signal_connect_object (priv->global_history_service, "url-title-changed",
                             G_CALLBACK (history_service_url_title_changed_cb),
                             shell, 0);
    g_signal_connect_object (priv->global_history_service, "url-deleted",
                             G_CALLBACK (history_service_url_deleted_cb),
                             shell, 0);
    g_signal_connect_object (priv->global_history_service, "host-deleted",
                             G_CALLBACK (history_service_host_deleted_cb),
                             shell, 0);
    g_signal_connect_object (priv->global_history_service, "cleared",
                             G_CALLBACK (history_service_cleared_cb),
                             shell, 0);
  }

  return priv->global_history_service;
}

/**
 * ephy_embed_shell_get_global_gsb_service:
 * @shell: the #EphyEmbedShell
 *
 * Return value: (transfer none): the global #EphyGSBService
 **/
EphyGSBService *
ephy_embed_shell_get_global_gsb_service (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

  if (!priv->global_gsb_service) {
    g_autofree char *api_key = NULL;
    g_autofree char *db_path = NULL;

    api_key = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_GSB_API_KEY);
    db_path = g_build_filename (ephy_default_cache_dir (), EPHY_GSB_FILE, NULL);
    priv->global_gsb_service = ephy_gsb_service_new (api_key, db_path);
  }

  return priv->global_gsb_service;
}

/**
 * ephy_embed_shell_get_encodings:
 * @shell: the #EphyEmbedShell
 *
 * Return value: (transfer none):
 **/
EphyEncodings *
ephy_embed_shell_get_encodings (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  if (!priv->encodings)
    priv->encodings = ephy_encodings_new ();

  return priv->encodings;
}

void
ephy_embed_shell_restored_window (EphyEmbedShell *shell)
{
  g_signal_emit (shell, signals[RESTORED_WINDOW], 0);
}

static void
about_request_cb (WebKitURISchemeRequest *request,
                  EphyEmbedShell         *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  ephy_about_handler_handle_request (priv->about_handler, request);
}

static void
source_request_cb (WebKitURISchemeRequest *request,
                   EphyEmbedShell         *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  ephy_view_source_handler_handle_request (priv->source_handler, request);
}

static void
ephy_resource_request_cb (WebKitURISchemeRequest *request)
{
  const char *path;
  gsize size;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GError) error = NULL;

  path = webkit_uri_scheme_request_get_path (request);
  if (!g_resources_get_info (path, 0, &size, NULL, &error)) {
    webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  stream = g_resources_open_stream (path, 0, &error);
  if (stream)
    webkit_uri_scheme_request_finish (request, stream, size, NULL);
  else
    webkit_uri_scheme_request_finish_error (request, error);
}

static void
ftp_request_cb (WebKitURISchemeRequest *request)
{
  g_autoptr(GDesktopAppInfo) app_info = NULL;
  g_autoptr(GList) list = NULL;
  g_autoptr(GError) error = NULL;
  const char *uri;

  uri = webkit_uri_scheme_request_get_uri (request);
  g_app_info_launch_default_for_uri (uri, NULL, &error);

  if (!error) {
    g_signal_emit_by_name (webkit_uri_scheme_request_get_web_view (request), "close", NULL);
    return;
  }

  /* Default URI handler didn't work. Try nautilus before giving up. */
  app_info = g_desktop_app_info_new ("org.gnome.Nautilus.desktop");
  list = g_list_append (list, (gpointer)uri);

  if (app_info && g_app_info_launch_uris (G_APP_INFO (app_info), list, NULL, NULL))
    g_signal_emit_by_name (webkit_uri_scheme_request_get_web_view (request), "close", NULL);
  else
    webkit_uri_scheme_request_finish_error (request, error);
}

static void
initialize_web_process_extensions (WebKitWebContext *web_context,
                                   EphyEmbedShell   *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr(GVariant) user_data = NULL;
  gboolean private_profile;
  gboolean browser_mode;
  const char *address;

#if DEVELOPER_MODE
  webkit_web_context_set_web_extensions_directory (web_context, BUILD_ROOT "/embed/web-process-extension");
#else
  webkit_web_context_set_web_extensions_directory (web_context, EPHY_WEB_PROCESS_EXTENSIONS_DIR);
#endif

  address = priv->dbus_server ? g_dbus_server_get_client_address (priv->dbus_server) : NULL;

  private_profile = priv->mode == EPHY_EMBED_SHELL_MODE_PRIVATE || priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO || priv->mode == EPHY_EMBED_SHELL_MODE_AUTOMATION;
  browser_mode = priv->mode == EPHY_EMBED_SHELL_MODE_BROWSER;
  user_data = g_variant_new ("(smsmsbb)",
                             priv->guid,
                             address,
                             ephy_profile_dir_is_default () ? NULL : ephy_profile_dir (),
                             private_profile,
                             browser_mode);
  webkit_web_context_set_web_extensions_initialization_user_data (web_context, g_steal_pointer (&user_data));
}

static void
initialize_notification_permissions (WebKitWebContext *web_context,
                                     EphyEmbedShell   *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *permitted_origins;
  GList *denied_origins;

  permitted_origins = ephy_permissions_manager_get_permitted_origins (priv->permissions_manager,
                                                                      EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS);
  denied_origins = ephy_permissions_manager_get_denied_origins (priv->permissions_manager,
                                                                EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS);
  webkit_web_context_initialize_notification_permissions (web_context, permitted_origins, denied_origins);
}

static void
web_process_extension_page_created (EphyWebProcessExtensionProxy *extension,
                                    guint64                       page_id,
                                    EphyEmbedShell               *shell)
{
  g_object_set_data (G_OBJECT (extension), "initialized", GINT_TO_POINTER (TRUE));
  g_signal_emit (shell, signals[PAGE_CREATED], 0, page_id, extension);
}

static void
web_process_extension_connection_closed (EphyWebProcessExtensionProxy *extension,
                                         EphyEmbedShell               *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  priv->web_process_extensions = g_list_remove (priv->web_process_extensions, extension);
  g_object_unref (extension);
}

static gboolean
new_connection_cb (GDBusServer     *server,
                   GDBusConnection *connection,
                   EphyEmbedShell  *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr(EphyWebProcessExtensionProxy) extension = NULL;

  extension = ephy_web_process_extension_proxy_new (connection);

  g_signal_connect_object (extension, "page-created",
                           G_CALLBACK (web_process_extension_page_created), shell, 0);
  g_signal_connect_object (extension, "connection-closed",
                           G_CALLBACK (web_process_extension_connection_closed), shell, 0);

  priv->web_process_extensions = g_list_prepend (priv->web_process_extensions, g_steal_pointer (&extension));

  return TRUE;
}

static gboolean
authorize_authenticated_peer_cb (GDBusAuthObserver *observer,
                                 GIOStream         *stream,
                                 GCredentials      *credentials,
                                 EphyEmbedShell    *shell)
{
  return ephy_dbus_peer_is_authorized (credentials);
}

static void
ephy_embed_shell_setup_web_process_extensions_server (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr(GDBusAuthObserver) observer = NULL;
  g_autofree char *address = NULL;
  g_autoptr(GError) error = NULL;

  address = g_strdup_printf ("unix:tmpdir=%s", g_get_tmp_dir ());

  observer = g_dbus_auth_observer_new ();

  g_signal_connect_object (observer, "authorize-authenticated-peer",
                           G_CALLBACK (authorize_authenticated_peer_cb), shell, 0);

  /* Why sync?
   *
   * (a) The server must be started before web process extensions try to connect.
   * (b) Gio actually has no async version. Don't know why.
   */
  priv->dbus_server = g_dbus_server_new_sync (address,
                                              G_DBUS_SERVER_FLAGS_NONE,
                                              priv->guid,
                                              observer,
                                              NULL,
                                              &error);

  if (error) {
    g_warning ("Failed to start web process extension server on %s: %s", address, error->message);
    return;
  }

  g_signal_connect_object (priv->dbus_server, "new-connection",
                           G_CALLBACK (new_connection_cb), shell, 0);
  g_dbus_server_start (priv->dbus_server);
}

static void
ephy_embed_shell_create_web_context (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr(WebKitWebsiteDataManager) manager = NULL;

  if (priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    priv->web_context = webkit_web_context_new_ephemeral ();
    return;
  }

  if (priv->mode == EPHY_EMBED_SHELL_MODE_AUTOMATION) {
    priv->web_context = webkit_web_context_new_ephemeral ();
    webkit_web_context_set_automation_allowed (priv->web_context, TRUE);
    return;
  }

  manager = webkit_website_data_manager_new ("base-data-directory", ephy_profile_dir (),
                                             "base-cache-directory", ephy_cache_dir (),
                                             NULL);

  priv->web_context = webkit_web_context_new_with_website_data_manager (manager);
}

static void
download_started_cb (WebKitWebContext *web_context,
                     WebKitDownload   *download,
                     EphyEmbedShell   *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr(EphyDownload) ephy_download = NULL;
  gboolean ephy_download_set;

  /* Is download locked down? */
  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK)) {
    webkit_download_cancel (download);
    return;
  }

  /* Only create an EphyDownload for the WebKitDownload if it doesn't exist yet.
   * This can happen when the download has been started automatically by WebKit,
   * due to a context menu action or policy checker decision. Downloads started
   * explicitly by Epiphany are marked with ephy-download-set GObject data.
   */
  ephy_download_set = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (download), "ephy-download-set"));
  if (ephy_download_set)
    return;

  ephy_download = ephy_download_new (download);
  ephy_downloads_manager_add_download (priv->downloads_manager, ephy_download);
}

static void
ephy_embed_shell_startup (GApplication *application)
{
  EphyEmbedShell *shell = EPHY_EMBED_SHELL (application);
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autofree char *favicon_db_path = NULL;
  WebKitCookieManager *cookie_manager;
  g_autofree char *filename = NULL;
  g_autofree char *cookie_policy = NULL;
  g_autofree char *filters_dir = NULL;

  G_APPLICATION_CLASS (ephy_embed_shell_parent_class)->startup (application);

  ephy_embed_shell_create_web_context (shell);

  ephy_embed_shell_setup_web_process_extensions_server (shell);

  /* User content manager */
  if (priv->mode != EPHY_EMBED_SHELL_MODE_TEST)
    priv->user_content = webkit_user_content_manager_new ();

  webkit_user_content_manager_register_script_message_handler_in_world (priv->user_content,
                                                                        "overview",
                                                                        priv->guid);
  g_signal_connect_object (priv->user_content, "script-message-received::overview",
                           G_CALLBACK (web_process_extension_overview_message_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "tlsErrorPage");
  g_signal_connect_object (priv->user_content, "script-message-received::tlsErrorPage",
                           G_CALLBACK (web_process_extension_tls_error_page_message_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "unsafeBrowsingErrorPage");
  g_signal_connect_object (priv->user_content, "script-message-received::unsafeBrowsingErrorPage",
                           G_CALLBACK (web_process_extension_unsafe_browsing_error_page_message_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler_in_world (priv->user_content,
                                                                        "passwordFormFocused",
                                                                        priv->guid);
  g_signal_connect_object (priv->user_content, "script-message-received::passwordFormFocused",
                           G_CALLBACK (web_process_extension_password_form_focused_message_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "aboutApps");
  g_signal_connect_object (priv->user_content, "script-message-received::aboutApps",
                           G_CALLBACK (web_process_extension_about_apps_message_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler_in_world (priv->user_content,
                                                                        "passwordManagerQuery",
                                                                        priv->guid);
  g_signal_connect (priv->user_content, "script-message-received::passwordManagerQuery",
                    G_CALLBACK (web_process_extension_password_manager_query_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler_in_world (priv->user_content,
                                                                        "passwordManagerQueryUsernames",
                                                                        priv->guid);
  g_signal_connect (priv->user_content, "script-message-received::passwordManagerQueryUsernames",
                    G_CALLBACK (web_process_extension_password_manager_query_usernames_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler_in_world (priv->user_content,
                                                                        "passwordManagerSave",
                                                                        priv->guid);
  g_signal_connect (priv->user_content, "script-message-received::passwordManagerSave",
                    G_CALLBACK (web_process_extension_password_manager_save_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler_in_world (priv->user_content,
                                                                        "passwordManagerRequestSave",
                                                                        priv->guid);
  g_signal_connect (priv->user_content, "script-message-received::passwordManagerRequestSave",
                    G_CALLBACK (web_process_extension_password_manager_request_save_received_cb),
                    shell);

  webkit_web_context_set_process_model (priv->web_context, WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);

  g_signal_connect_object (priv->web_context, "initialize-web-extensions",
                           G_CALLBACK (initialize_web_process_extensions),
                           shell, 0);

  priv->permissions_manager = ephy_permissions_manager_new ();
  g_signal_connect_object (priv->web_context, "initialize-notification-permissions",
                           G_CALLBACK (initialize_notification_permissions),
                           shell, 0);

  priv->password_manager = ephy_password_manager_new ();

  /* Do not cache favicons in automation mode. Don't change the TLS policy either, since that's
   * handled by session capabilities in automation mode.
   */
  if (priv->mode != EPHY_EMBED_SHELL_MODE_AUTOMATION) {
    /* Favicon Database */
    favicon_db_path = g_build_filename (ephy_cache_dir (), "icondatabase", NULL);
    webkit_web_context_set_favicon_database_directory (priv->web_context, favicon_db_path);

    /* Do not ignore TLS errors. */
    webkit_web_context_set_tls_errors_policy (priv->web_context, WEBKIT_TLS_ERRORS_POLICY_FAIL);
  }

  /* about: URIs handler */
  priv->about_handler = ephy_about_handler_new ();
  webkit_web_context_register_uri_scheme (priv->web_context,
                                          EPHY_ABOUT_SCHEME,
                                          (WebKitURISchemeRequestCallback)about_request_cb,
                                          shell, NULL);

  /* Register about scheme as local so that it can contain file resources */
  webkit_security_manager_register_uri_scheme_as_local (webkit_web_context_get_security_manager (priv->web_context),
                                                        EPHY_ABOUT_SCHEME);

  /* view source handler */
  priv->source_handler = ephy_view_source_handler_new ();
  webkit_web_context_register_uri_scheme (priv->web_context, EPHY_VIEW_SOURCE_SCHEME,
                                          (WebKitURISchemeRequestCallback)source_request_cb,
                                          shell, NULL);

  /* ephy-resource handler */
  webkit_web_context_register_uri_scheme (priv->web_context, "ephy-resource",
                                          (WebKitURISchemeRequestCallback)ephy_resource_request_cb,
                                          NULL, NULL);

  /* No support for FTP, try to open in nautilus instead of failing */
  webkit_web_context_register_uri_scheme (priv->web_context, "ftp",
                                          (WebKitURISchemeRequestCallback)ftp_request_cb,
                                          NULL, NULL);

  /* Store cookies in moz-compatible SQLite format */
  cookie_manager = webkit_web_context_get_cookie_manager (priv->web_context);
  if (!webkit_web_context_is_ephemeral (priv->web_context)) {
    filename = g_build_filename (ephy_profile_dir (), "cookies.sqlite", NULL);
    webkit_cookie_manager_set_persistent_storage (cookie_manager, filename,
                                                  WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
  }

  cookie_policy = g_settings_get_string (EPHY_SETTINGS_WEB,
                                         EPHY_PREFS_WEB_COOKIES_POLICY);
  ephy_embed_prefs_set_cookie_accept_policy (cookie_manager, cookie_policy);

  filters_dir = g_build_filename (ephy_cache_dir (), "adblock", NULL);
  priv->filters_manager = ephy_filters_manager_new (filters_dir);

  g_signal_connect_object (priv->filters_manager, "filters-disabled",
                           G_CALLBACK (webkit_user_content_manager_remove_all_filters),
                           priv->user_content,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->filters_manager, "filter-ready",
                           G_CALLBACK (webkit_user_content_manager_add_filter),
                           priv->user_content,
                           G_CONNECT_SWAPPED);

  g_signal_connect (priv->web_context, "download-started",
                    G_CALLBACK (download_started_cb), shell);
}

static void
ephy_embed_shell_shutdown (GApplication *application)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (EPHY_EMBED_SHELL (application));

  G_APPLICATION_CLASS (ephy_embed_shell_parent_class)->shutdown (application);

  if (priv->dbus_server)
    g_dbus_server_stop (priv->dbus_server);

  webkit_user_content_manager_unregister_script_message_handler_in_world (priv->user_content,
                                                                          "overview",
                                                                          priv->guid);
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content,
                                                                 "tlsErrorPage");
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content,
                                                                 "unsafeBrowsingErrorPage");
  webkit_user_content_manager_unregister_script_message_handler_in_world (priv->user_content,
                                                                          "passwordManagerRequestSave",
                                                                          priv->guid);
  webkit_user_content_manager_unregister_script_message_handler_in_world (priv->user_content,
                                                                          "passwordFormFocused",
                                                                          priv->guid);
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "aboutApps");
  webkit_user_content_manager_unregister_script_message_handler_in_world (priv->user_content,
                                                                          "passwordManagerQuery",
                                                                          priv->guid);
  webkit_user_content_manager_unregister_script_message_handler_in_world (priv->user_content,
                                                                          "passwordManagerSave",
                                                                          priv->guid);
  webkit_user_content_manager_unregister_script_message_handler_in_world (priv->user_content,
                                                                          "passwordManagerQueryUsernames",
                                                                          priv->guid);

  g_object_unref (ephy_embed_prefs_get_settings ());
  ephy_embed_utils_shutdown ();
}

static void
ephy_embed_shell_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (EPHY_EMBED_SHELL (object));

  switch (prop_id) {
    case PROP_MODE:
      priv->mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_embed_shell_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (EPHY_EMBED_SHELL (object));

  switch (prop_id) {
    case PROP_MODE:
      g_value_set_enum (value, priv->mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_embed_shell_constructed (GObject *object)
{
  EphyEmbedShell *shell;
  EphyEmbedShellPrivate *priv;
  EphyEmbedShellMode mode;

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->constructed (object);

  shell = EPHY_EMBED_SHELL (object);
  priv = ephy_embed_shell_get_instance_private (shell);
  priv->guid = g_dbus_generate_guid ();
  mode = ephy_embed_shell_get_mode (shell);

  /* These do not run the EmbedShell application instance, so make sure that
     there is a web context and a user content manager for them. */
  if (mode == EPHY_EMBED_SHELL_MODE_TEST ||
      mode == EPHY_EMBED_SHELL_MODE_SEARCH_PROVIDER) {
    ephy_embed_shell_create_web_context (shell);
    priv->user_content = webkit_user_content_manager_new ();
  }
}

static void
ephy_embed_shell_init (EphyEmbedShell *shell)
{
  /* globally accessible singleton */
  g_assert (!embed_shell);
  embed_shell = shell;
}

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = ephy_embed_shell_dispose;
  object_class->set_property = ephy_embed_shell_set_property;
  object_class->get_property = ephy_embed_shell_get_property;
  object_class->constructed = ephy_embed_shell_constructed;

  application_class->startup = ephy_embed_shell_startup;
  application_class->shutdown = ephy_embed_shell_shutdown;

  object_properties[PROP_MODE] =
    g_param_spec_enum ("mode",
                       "Mode",
                       "The  global mode for this instance.",
                       EPHY_TYPE_EMBED_SHELL_MODE,
                       EPHY_EMBED_SHELL_MODE_BROWSER,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     object_properties);

/**
 * EphyEmbedShell::finished-restoring-window:
 * @shell: the #EphyEmbedShell
 *
 * The ::finished-restoring-window signal is emitted when the
 * session finishes restoring a window.
 **/
  signals[RESTORED_WINDOW] =
    g_signal_new ("window-restored",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (EphyEmbedShellClass, restored_window),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * EphyEmbedShell::web-view-created:
   * @shell: the #EphyEmbedShell
   * @view: the newly created #EphyWebView
   *
   * The ::web-view-created signal will be emitted every time a new
   * #EphyWebView is created.
   *
   **/
  signals[WEB_VIEW_CREATED] =
    g_signal_new ("web-view-created",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_WEB_VIEW);

  /**
   * EphyEmbedShell::page-created:
   * @shell: the #EphyEmbedShell
   * @page_id: the identifier of the web page created
   * @web_process_extension: the #EphyWebProcessExtensionProxy
   *
   * Emitted when a web page is created in the web process.
   */
  signals[PAGE_CREATED] =
    g_signal_new ("page-created",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_UINT64,
                  EPHY_TYPE_WEB_PROCESS_EXTENSION_PROXY);

  /**
   * EphyEmbedShell::allow-tls-certificate:
   * @shell: the #EphyEmbedShell
   * @page_id: the identifier of the web page
   *
   * Emitted when the web process extension requests an exception be
   * permitted for the invalid TLS certificate on the given page
   */
  signals[ALLOW_TLS_CERTIFICATE] =
    g_signal_new ("allow-tls-certificate",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT64);

  /**
   * EphyEmbedShell::allow-unsafe-browsing:
   * @shell: the #EphyEmbedShell
   * @page_id: the identifier of the web page
   *
   * Emitted when the web process extension requests an exception be
   * permitted for the unsafe browsing warning on the given page
   */
  signals[ALLOW_UNSAFE_BROWSING] =
    g_signal_new ("allow-unsafe-browsing",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT64);

  /**
   * EphyEmbedShell::password-form-focused
   * @shell: the #EphyEmbedShell
   * @page_id: the identifier of the web page
   * @insecure_action: whether the target of the form is http://
   *
   * Emitted when a form in a web page gains focus.
   */
  signals[PASSWORD_FORM_FOCUSED] =
    g_signal_new ("password-form-focused",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_UINT64,
                  G_TYPE_BOOLEAN);
}

/**
 * ephy_embed_shell_get_default:
 *
 * Retrieves the default #EphyEmbedShell object
 *
 * Return value: (transfer none): the default #EphyEmbedShell
 **/
EphyEmbedShell *
ephy_embed_shell_get_default (void)
{
  return embed_shell;
}

void
ephy_embed_shell_set_page_setup (EphyEmbedShell *shell,
                                 GtkPageSetup   *page_setup)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autofree char *path = NULL;

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  if (page_setup)
    g_object_ref (page_setup);
  else
    page_setup = gtk_page_setup_new ();

  if (priv->page_setup)
    g_object_unref (priv->page_setup);

  priv->page_setup = page_setup;

  path = g_build_filename (ephy_profile_dir (), PAGE_SETUP_FILENAME, NULL);
  gtk_page_setup_to_file (page_setup, path, NULL);
}

/**
 * ephy_embed_shell_get_page_setup:
 *
 * Return value: (transfer none):
 **/
GtkPageSetup *
ephy_embed_shell_get_page_setup (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  if (!priv->page_setup) {
    g_autofree char *path = NULL;

    path = g_build_filename (ephy_profile_dir (), PAGE_SETUP_FILENAME, NULL);
    priv->page_setup = gtk_page_setup_new_from_file (path, NULL);

    /* If that still didn't work, create a new, empty one */
    if (!priv->page_setup)
      priv->page_setup = gtk_page_setup_new ();
  }

  return priv->page_setup;
}

/**
 * ephy_embed_shell_set_print_settings:
 * @shell: the #EphyEmbedShell
 * @settings: the new #GtkPrintSettings object
 *
 * Sets the global #GtkPrintSettings object.
 *
 **/
void
ephy_embed_shell_set_print_settings (EphyEmbedShell   *shell,
                                     GtkPrintSettings *settings)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autofree char *path = NULL;

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  if (settings)
    g_object_ref (settings);

  if (priv->print_settings)
    g_object_unref (priv->print_settings);

  priv->print_settings = settings ? settings : gtk_print_settings_new ();

  path = g_build_filename (ephy_profile_dir (), PRINT_SETTINGS_FILENAME, NULL);
  gtk_print_settings_to_file (settings, path, NULL);
}

/**
 * ephy_embed_shell_get_print_settings:
 * @shell: the #EphyEmbedShell
 *
 * Gets the global #GtkPrintSettings object.
 *
 * Returns: (transfer none): a #GtkPrintSettings object
 **/
GtkPrintSettings *
ephy_embed_shell_get_print_settings (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  if (!priv->print_settings) {
    g_autofree char *path = NULL;

    path = g_build_filename (ephy_profile_dir (), PRINT_SETTINGS_FILENAME, NULL);
    priv->print_settings = gtk_print_settings_new_from_file (path, NULL);

    /* Note: the gtk print settings file format is the same as our
     * legacy one, so no need to migrate here.
     */

    if (!priv->print_settings)
      priv->print_settings = gtk_print_settings_new ();
  }

  return priv->print_settings;
}

/**
 * ephy_embed_shell_get_mode:
 * @shell: an #EphyEmbedShell
 *
 * Returns: the global mode of the @shell
 **/
EphyEmbedShellMode
ephy_embed_shell_get_mode (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  return priv->mode;
}

/**
 * ephy_embed_shell_clear_cache:
 * @shell: an #EphyEmbedShell
 *
 * Clears the HTTP cache (temporarily saved web pages).
 **/
void
ephy_embed_shell_clear_cache (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  webkit_web_context_clear_cache (priv->web_context);
}

WebKitUserContentManager *
ephy_embed_shell_get_user_content_manager (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  return priv->user_content;
}

const char *
ephy_embed_shell_get_guid (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  return priv->guid;
}

WebKitWebContext *
ephy_embed_shell_get_web_context (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  return priv->web_context;
}

EphyDownloadsManager *
ephy_embed_shell_get_downloads_manager (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  if (!priv->downloads_manager)
    priv->downloads_manager = EPHY_DOWNLOADS_MANAGER (g_object_new (EPHY_TYPE_DOWNLOADS_MANAGER, NULL));
  return priv->downloads_manager;
}

EphyPermissionsManager *
ephy_embed_shell_get_permissions_manager (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  return priv->permissions_manager;
}

EphySearchEngineManager *
ephy_embed_shell_get_search_engine_manager (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  if (!priv->search_engine_manager)
    priv->search_engine_manager = ephy_search_engine_manager_new ();
  return priv->search_engine_manager;
}

EphyPasswordManager *
ephy_embed_shell_get_password_manager (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  return priv->password_manager;
}

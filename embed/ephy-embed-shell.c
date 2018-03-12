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
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-encodings.h"
#include "ephy-file-helpers.h"
#include "ephy-filters-manager.h"
#include "ephy-flatpak-utils.h"
#include "ephy-history-service.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-snapshot-service.h"
#include "ephy-tabs-catalog.h"
#include "ephy-uri-helpers.h"
#include "ephy-uri-tester-shared.h"
#include "ephy-web-app-utils.h"
#include "ephy-web-extension-proxy.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#define PAGE_SETUP_FILENAME "page-setup-gtk.ini"
#define PRINT_SETTINGS_FILENAME "print-settings.ini"
#define OVERVIEW_RELOAD_DELAY 500

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
  EphyAboutHandler *about_handler;
  guint update_overview_timeout_id;
  guint hiding_overview_item;
  GDBusServer *dbus_server;
  GList *web_extensions;
  EphyFiltersManager *filters_manager;
  EphySearchEngineManager *search_engine_manager;
  GCancellable *cancellable;
  GList *app_origins;
} EphyEmbedShellPrivate;

enum {
  RESTORED_WINDOW,
  WEB_VIEW_CREATED,
  PAGE_CREATED,
  ALLOW_TLS_CERTIFICATE,
  ALLOW_UNSAFE_BROWSING,
  FORM_AUTH_DATA_SAVE_REQUESTED,
  SENSITIVE_FORM_FOCUSED,

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

G_DEFINE_TYPE_WITH_CODE (EphyEmbedShell, ephy_embed_shell, GTK_TYPE_APPLICATION,
                         G_ADD_PRIVATE (EphyEmbedShell)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_TABS_CATALOG,
                                                ephy_embed_shell_tabs_catalog_iface_init))

static GList *
tabs_catalog_get_tabs_info (EphyTabsCatalog *catalog)
{
  EphyEmbedShell *embed_shell = EPHY_EMBED_SHELL (catalog);
  WebKitFaviconDatabase *database;
  GList *windows;
  GList *tabs;
  GList *tabs_info = NULL;
  const char *title;
  const char *url;
  char *favicon;

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

      g_free (favicon);
    }

    g_list_free (tabs);
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

  if (priv->update_overview_timeout_id > 0) {
    g_source_remove (priv->update_overview_timeout_id);
    priv->update_overview_timeout_id = 0;
  }

  g_clear_object (&priv->encodings);
  g_clear_object (&priv->page_setup);
  g_clear_object (&priv->print_settings);
  g_clear_object (&priv->global_history_service);
  g_clear_object (&priv->global_gsb_service);
  g_clear_object (&priv->about_handler);
  g_clear_object (&priv->user_content);
  g_clear_object (&priv->downloads_manager);
  g_clear_object (&priv->permissions_manager);
  g_clear_object (&priv->web_context);
  g_clear_object (&priv->dbus_server);
  g_clear_object (&priv->filters_manager);
  g_clear_object (&priv->search_engine_manager);

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->dispose (object);
}

static void
ephy_embed_shell_finalize (GObject *object)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (EPHY_EMBED_SHELL (object));

  g_list_free_full (priv->app_origins, g_free);

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->dispose (object);
}

static void
web_extension_form_auth_data_message_received_cb (WebKitUserContentManager *manager,
                                                  WebKitJavascriptResult   *message,
                                                  EphyEmbedShell           *shell)
{
  guint request_id;
  guint64 page_id;
  const char *origin;
  const char *username;
  GVariant *variant;
  gchar *message_str;

  message_str = jsc_value_to_string (webkit_javascript_result_get_js_value (message));
  variant = g_variant_parse (G_VARIANT_TYPE ("(utss)"), message_str, NULL, NULL, NULL);
  g_free (message_str);

  g_variant_get (variant, "(ut&s&s)", &request_id, &page_id, &origin, &username);
  g_signal_emit (shell, signals[FORM_AUTH_DATA_SAVE_REQUESTED], 0,
                 request_id, page_id, origin, username);
  g_variant_unref (variant);
}

static void
web_extension_sensitive_form_focused_message_received_cb (WebKitUserContentManager *manager,
                                                          WebKitJavascriptResult   *message,
                                                          EphyEmbedShell           *shell)
{
  guint64 page_id;
  gboolean insecure_action;
  GVariant *variant;
  char *message_str;

  message_str = jsc_value_to_string (webkit_javascript_result_get_js_value (message));
  variant = g_variant_parse (G_VARIANT_TYPE ("(tb)"), message_str, NULL, NULL, NULL);
  g_free (message_str);

  g_variant_get (variant, "(tb)", &page_id, &insecure_action);
  g_signal_emit (shell, signals[SENSITIVE_FORM_FOCUSED], 0,
                 page_id, insecure_action);
  g_variant_unref (variant);
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

  for (l = priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_set_urls (web_extension, urls);
  }

  for (l = urls; l; l = g_list_next (l))
    ephy_embed_shell_schedule_thumbnail_update (shell, (EphyHistoryURL *)l->data);
}

static void
ephy_embed_shell_update_overview_urls (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  EphyHistoryQuery *query;

  query = ephy_history_query_new_for_overview ();
  ephy_history_service_query_urls (priv->global_history_service, query, NULL,
                                   (EphyHistoryJobCallback)history_service_query_urls_cb,
                                   shell);
  ephy_history_query_free (query);
}

static void
history_service_urls_visited_cb (EphyHistoryService *history,
                                 EphyEmbedShell     *shell)
{
  ephy_embed_shell_update_overview_urls (shell);
}

static gboolean
ephy_embed_shell_update_overview_timeout_cb (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  priv->update_overview_timeout_id = 0;

  if (priv->hiding_overview_item > 0)
    return FALSE;

  ephy_embed_shell_update_overview_urls (shell);

  return FALSE;
}

static void
history_set_url_hidden_cb (EphyHistoryService *service,
                           gboolean            success,
                           gpointer            result_data,
                           EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  priv->hiding_overview_item--;

  if (!success)
    return;

  if (priv->update_overview_timeout_id > 0)
    return;

  ephy_embed_shell_update_overview_urls (shell);
}

static void
web_extension_overview_message_received_cb (WebKitUserContentManager *manager,
                                            WebKitJavascriptResult   *message,
                                            EphyEmbedShell           *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  char *url_to_remove;

  url_to_remove = jsc_value_to_string (webkit_javascript_result_get_js_value (message));

  priv->hiding_overview_item++;
  ephy_history_service_set_url_hidden (priv->global_history_service,
                                       url_to_remove, TRUE, NULL,
                                       (EphyHistoryJobCallback)history_set_url_hidden_cb,
                                       shell);
  g_free (url_to_remove);

  if (priv->update_overview_timeout_id > 0)
    g_source_remove (priv->update_overview_timeout_id);

  /* Wait for the CSS animations to finish before refreshing */
  priv->update_overview_timeout_id =
    g_timeout_add (OVERVIEW_RELOAD_DELAY, (GSourceFunc)ephy_embed_shell_update_overview_timeout_cb, shell);
}

static void
web_extension_tls_error_page_message_received_cb (WebKitUserContentManager *manager,
                                                  WebKitJavascriptResult   *message,
                                                  EphyEmbedShell           *shell)
{
  guint64 page_id;

  page_id = jsc_value_to_double (webkit_javascript_result_get_js_value (message));
  g_signal_emit (shell, signals[ALLOW_TLS_CERTIFICATE], 0, page_id);
}

static void
web_extension_unsafe_browsing_error_page_message_received_cb (WebKitUserContentManager *manager,
                                                              WebKitJavascriptResult   *message,
                                                              EphyEmbedShell           *shell)
{
  guint64 page_id;

  page_id = jsc_value_to_double (webkit_javascript_result_get_js_value (message));
  g_signal_emit (shell, signals[ALLOW_UNSAFE_BROWSING], 0, page_id);
}

static void
web_extension_about_apps_message_received_cb (WebKitUserContentManager *manager,
                                              WebKitJavascriptResult   *message,
                                              EphyEmbedShell           *shell)
{
  char *app_id;

  app_id = jsc_value_to_string (webkit_javascript_result_get_js_value (message));
  ephy_web_application_delete (app_id);
  g_free (app_id);
}

static void
history_service_url_title_changed_cb (EphyHistoryService *service,
                                      const char         *url,
                                      const char         *title,
                                      EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  for (l = priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_set_url_title (web_extension, url, title);
  }
}

static void
history_service_url_deleted_cb (EphyHistoryService *service,
                                EphyHistoryURL     *url,
                                EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  for (l = priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_delete_url (web_extension, url->url);
  }
}

static void
history_service_host_deleted_cb (EphyHistoryService *service,
                                 const char         *deleted_url,
                                 EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;
  SoupURI *deleted_uri;

  deleted_uri = soup_uri_new (deleted_url);

  for (l = priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_delete_host (web_extension, soup_uri_get_host (deleted_uri));
  }

  soup_uri_free (deleted_uri);
}

static void
history_service_cleared_cb (EphyHistoryService *service,
                            EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  for (l = priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_clear (web_extension);
  }
}

typedef struct {
  EphyWebExtensionProxy *extension;
  char *url;
  char *path;
} DelayedThumbnailUpdateData;

static DelayedThumbnailUpdateData *
delayed_thumbnail_update_data_new (EphyWebExtensionProxy *extension,
                                   const char            *url,
                                   const char            *path)
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
    ephy_web_extension_proxy_history_set_url_thumbnail (data->extension, data->url, data->path);
    delayed_thumbnail_update_data_free (data);
    return G_SOURCE_REMOVE;
  }

  /* Web extension is not initialized yet, try again later.... */
  return G_SOURCE_CONTINUE;
}

void
ephy_embed_shell_set_thumbnail_path (EphyEmbedShell *shell,
                                     const char     *url,
                                     const char     *path)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;

  for (l = priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;
    if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (web_extension), "initialized"))) {
      ephy_web_extension_proxy_history_set_url_thumbnail (web_extension, url, path);
    } else {
      DelayedThumbnailUpdateData *data = delayed_thumbnail_update_data_new (web_extension, url, path);
      g_timeout_add (50, (GSourceFunc)delayed_thumbnail_update_cb, data);
    }
  }
}

static void
got_snapshot_path_for_url_cb (EphySnapshotService *service,
                              GAsyncResult        *result,
                              char                *url)
{
  char *snapshot;
  GError *error = NULL;

  snapshot = ephy_snapshot_service_get_snapshot_path_for_url_finish (service, result, &error);
  if (snapshot) {
    ephy_embed_shell_set_thumbnail_path (ephy_embed_shell_get_default (), url, snapshot);
    g_free (snapshot);
  } else {
    /* Bad luck, not something to warn about. */
    g_info ("Failed to get snapshot for URL %s: %s", url, error->message);
    g_error_free (error);
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

  if (priv->global_history_service == NULL) {
    char *filename;
    EphySQLiteConnectionMode mode;

    if (priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO ||
        priv->mode == EPHY_EMBED_SHELL_MODE_SEARCH_PROVIDER)
      mode = EPHY_SQLITE_CONNECTION_MODE_READ_ONLY;
    else
      mode = EPHY_SQLITE_CONNECTION_MODE_READWRITE;

    filename = g_build_filename (ephy_dot_dir (), EPHY_HISTORY_FILE, NULL);
    priv->global_history_service = ephy_history_service_new (filename, mode);
    g_free (filename);
    g_assert (priv->global_history_service);
    g_signal_connect (priv->global_history_service, "urls-visited",
                      G_CALLBACK (history_service_urls_visited_cb),
                      shell);
    g_signal_connect (priv->global_history_service, "url-title-changed",
                      G_CALLBACK (history_service_url_title_changed_cb),
                      shell);
    g_signal_connect (priv->global_history_service, "url-deleted",
                      G_CALLBACK (history_service_url_deleted_cb),
                      shell);
    g_signal_connect (priv->global_history_service, "host-deleted",
                      G_CALLBACK (history_service_host_deleted_cb),
                      shell);
    g_signal_connect (priv->global_history_service, "cleared",
                      G_CALLBACK (history_service_cleared_cb),
                      shell);
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

  if (priv->global_gsb_service == NULL) {
    char *api_key;
    char *dot_dir;
    char *db_path;

    api_key = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_GSB_API_KEY);
    dot_dir = ephy_default_dot_dir ();
    db_path = g_build_filename (dot_dir, EPHY_GSB_FILE, NULL);
    priv->global_gsb_service = ephy_gsb_service_new (api_key, db_path);

    g_free (api_key);
    g_free (dot_dir);
    g_free (db_path);
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

  if (priv->encodings == NULL)
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
ephy_resource_request_cb (WebKitURISchemeRequest *request)
{
  const char *path;
  GInputStream *stream;
  gsize size;
  GError *error = NULL;

  path = webkit_uri_scheme_request_get_path (request);
  if (!g_resources_get_info (path, 0, &size, NULL, &error)) {
    webkit_uri_scheme_request_finish_error (request, error);
    g_error_free (error);
    return;
  }

  stream = g_resources_open_stream (path, 0, &error);
  if (stream) {
    webkit_uri_scheme_request_finish (request, stream, size, NULL);
    g_object_unref (stream);
  } else {
    webkit_uri_scheme_request_finish_error (request, error);
    g_error_free (error);
  }
}

static void
ftp_request_cb (WebKitURISchemeRequest *request)
{
  GDesktopAppInfo *app_info;
  const char *uri;
  GList *list = NULL;
  GError *error = NULL;

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

  g_list_free (list);
  g_error_free (error);
  g_object_unref (app_info);
}

static void
web_extension_destroyed (EphyEmbedShell *shell,
                         GObject        *web_extension)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  priv->web_extensions = g_list_remove (priv->web_extensions, web_extension);
}

static void
ephy_embed_shell_watch_web_extension (EphyEmbedShell        *shell,
                                      EphyWebExtensionProxy *web_extension)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  priv->web_extensions = g_list_prepend (priv->web_extensions, web_extension);
  g_object_weak_ref (G_OBJECT (web_extension), (GWeakNotify)web_extension_destroyed, shell);
}

static void
ephy_embed_shell_unwatch_web_extension (EphyWebExtensionProxy *web_extension,
                                        EphyEmbedShell        *shell)
{
  g_object_weak_unref (G_OBJECT (web_extension), (GWeakNotify)web_extension_destroyed, shell);
}

static void
initialize_web_extensions (WebKitWebContext *web_context,
                           EphyEmbedShell   *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GVariant *user_data;
  gboolean private_profile;
  gboolean browser_mode;
  const char *address;

#if DEVELOPER_MODE
  webkit_web_context_set_web_extensions_directory (web_context, BUILD_ROOT "/embed/web-extension");
#else
  webkit_web_context_set_web_extensions_directory (web_context, EPHY_WEB_EXTENSIONS_DIR);
#endif

  address = priv->dbus_server ? g_dbus_server_get_client_address (priv->dbus_server) : NULL;

  private_profile = priv->mode == EPHY_EMBED_SHELL_MODE_PRIVATE || priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO;
  browser_mode = priv->mode == EPHY_EMBED_SHELL_MODE_BROWSER;
  user_data = g_variant_new ("(msssbb)",
                             address,
                             ephy_dot_dir (),
                             ephy_filters_manager_get_adblock_filters_dir (priv->filters_manager),
                             private_profile,
                             browser_mode);
  webkit_web_context_set_web_extensions_initialization_user_data (web_context, user_data);
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
web_extension_page_created (EphyWebExtensionProxy *extension,
                            guint64                page_id,
                            EphyEmbedShell        *shell)
{
  g_object_set_data (G_OBJECT (extension), "initialized", GINT_TO_POINTER (TRUE));
  g_signal_emit (shell, signals[PAGE_CREATED], 0, page_id, extension);
}

static gboolean
new_connection_cb (GDBusServer     *server,
                   GDBusConnection *connection,
                   EphyEmbedShell  *shell)
{
  EphyWebExtensionProxy *extension;

  extension = ephy_web_extension_proxy_new (connection);
  ephy_embed_shell_watch_web_extension (shell, extension);

  g_signal_connect_object (extension, "page-created",
                           G_CALLBACK (web_extension_page_created), shell, 0);

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
ephy_embed_shell_setup_web_extensions_server (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GDBusAuthObserver *observer;
  char *address;
  char *guid;
  GError *error = NULL;

  address = g_strdup_printf ("unix:tmpdir=%s", g_get_tmp_dir ());

  guid = g_dbus_generate_guid ();
  observer = g_dbus_auth_observer_new ();

  g_signal_connect (observer, "authorize-authenticated-peer",
                    G_CALLBACK (authorize_authenticated_peer_cb), shell);

  /* Why sync?
   *
   * (a) The server must be started before web extensions try to connect.
   * (b) Gio actually has no async version. Don't know why.
   */
  priv->dbus_server = g_dbus_server_new_sync (address,
                                              G_DBUS_SERVER_FLAGS_NONE,
                                              guid,
                                              observer,
                                              NULL,
                                              &error);

  if (error) {
    g_warning ("Failed to start web extension server on %s: %s", address, error->message);
    g_error_free (error);
    goto out;
  }

  g_signal_connect (priv->dbus_server, "new-connection",
                    G_CALLBACK (new_connection_cb), shell);
  g_dbus_server_start (priv->dbus_server);

 out:
  g_free (address);
  g_free (guid);
  g_object_unref (observer);
}

static void
ephy_embed_shell_setup_process_model (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  EphyPrefsProcessModel process_model;
  guint max_processes;

  if (ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_APPLICATION)
    process_model = EPHY_PREFS_PROCESS_MODEL_SHARED_SECONDARY_PROCESS;
  else
    process_model = g_settings_get_enum (EPHY_SETTINGS_MAIN, EPHY_PREFS_PROCESS_MODEL);

  switch (process_model) {
    case EPHY_PREFS_PROCESS_MODEL_SHARED_SECONDARY_PROCESS:
      max_processes = 1;
      break;
    case EPHY_PREFS_PROCESS_MODEL_ONE_SECONDARY_PROCESS_PER_WEB_VIEW:
      max_processes = g_settings_get_uint (EPHY_SETTINGS_MAIN, EPHY_PREFS_MAX_PROCESSES);
      break;
    default:
      g_assert_not_reached ();
  }

  webkit_web_context_set_process_model (priv->web_context, WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);
  webkit_web_context_set_web_process_count_limit (priv->web_context, max_processes);
}

static void
ephy_embed_shell_create_web_context (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  WebKitWebsiteDataManager *manager;
  char *data_dir;
  char *cache_dir;

  if (priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    priv->web_context = webkit_web_context_new_ephemeral ();
    return;
  }

  data_dir = g_build_filename (priv->mode == EPHY_EMBED_SHELL_MODE_PRIVATE ?
                               ephy_dot_dir () : g_get_user_data_dir (),
                               g_get_prgname (), NULL);
  cache_dir = g_build_filename (priv->mode == EPHY_EMBED_SHELL_MODE_PRIVATE ?
                                ephy_dot_dir () : g_get_user_cache_dir (),
                                g_get_prgname (), NULL);

  manager = webkit_website_data_manager_new ("base-data-directory", data_dir,
                                             "base-cache-directory", cache_dir,
                                             NULL);
  g_free (data_dir);
  g_free (cache_dir);

  priv->web_context = webkit_web_context_new_with_website_data_manager (manager);
  g_object_unref (manager);
}

static char *
adblock_filters_dir (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  char *result;

  if (priv->mode == EPHY_EMBED_SHELL_MODE_APPLICATION) {
    char *default_dot_dir = ephy_default_dot_dir ();

    result = g_build_filename (default_dot_dir, "adblock", NULL);
    g_free (default_dot_dir);
  } else
    result = g_build_filename (ephy_dot_dir (), "adblock", NULL);

  return result;
}

static void
ephy_embed_shell_startup (GApplication *application)
{
  EphyEmbedShell *shell = EPHY_EMBED_SHELL (application);
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  char *favicon_db_path;
  WebKitCookieManager *cookie_manager;
  char *filename;
  char *cookie_policy;
  char *filters_dir;

  G_APPLICATION_CLASS (ephy_embed_shell_parent_class)->startup (application);

  ephy_embed_shell_create_web_context (shell);

  ephy_embed_shell_setup_web_extensions_server (shell);

  /* User content manager */
  if (priv->mode != EPHY_EMBED_SHELL_MODE_TEST)
    priv->user_content = webkit_user_content_manager_new ();

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "overview");
  g_signal_connect (priv->user_content, "script-message-received::overview",
                    G_CALLBACK (web_extension_overview_message_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "tlsErrorPage");
  g_signal_connect (priv->user_content, "script-message-received::tlsErrorPage",
                    G_CALLBACK (web_extension_tls_error_page_message_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "unsafeBrowsingErrorPage");
  g_signal_connect (priv->user_content, "script-message-received::unsafeBrowsingErrorPage",
                    G_CALLBACK (web_extension_unsafe_browsing_error_page_message_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "formAuthData");
  g_signal_connect (priv->user_content, "script-message-received::formAuthData",
                    G_CALLBACK (web_extension_form_auth_data_message_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "sensitiveFormFocused");
  g_signal_connect (priv->user_content, "script-message-received::sensitiveFormFocused",
                    G_CALLBACK (web_extension_sensitive_form_focused_message_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler (priv->user_content,
                                                               "aboutApps");
  g_signal_connect (priv->user_content, "script-message-received::aboutApps",
                    G_CALLBACK (web_extension_about_apps_message_received_cb),
                    shell);

  ephy_embed_shell_setup_process_model (shell);
  g_signal_connect (priv->web_context, "initialize-web-extensions",
                    G_CALLBACK (initialize_web_extensions),
                    shell);

  priv->permissions_manager = ephy_permissions_manager_new ();
  g_signal_connect (priv->web_context, "initialize-notification-permissions",
                    G_CALLBACK (initialize_notification_permissions),
                    shell);

  /* Favicon Database */
  if (priv->mode == EPHY_EMBED_SHELL_MODE_PRIVATE)
    favicon_db_path = g_build_filename (ephy_dot_dir (), "icondatabase", NULL);
  else
    favicon_db_path = g_build_filename (g_get_user_cache_dir (), "epiphany", "icondatabase", NULL);
  webkit_web_context_set_favicon_database_directory (priv->web_context, favicon_db_path);
  g_free (favicon_db_path);

  /* Do not ignore TLS errors. */
  webkit_web_context_set_tls_errors_policy (priv->web_context, WEBKIT_TLS_ERRORS_POLICY_FAIL);


  /* about: URIs handler */
  priv->about_handler = ephy_about_handler_new ();
  webkit_web_context_register_uri_scheme (priv->web_context,
                                          EPHY_ABOUT_SCHEME,
                                          (WebKitURISchemeRequestCallback)about_request_cb,
                                          shell, NULL);

  /* Register about scheme as local so that it can contain file resources */
  webkit_security_manager_register_uri_scheme_as_local (webkit_web_context_get_security_manager (priv->web_context),
                                                        EPHY_ABOUT_SCHEME);

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
  if (priv->mode != EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    filename = g_build_filename (ephy_dot_dir (), "cookies.sqlite", NULL);
    webkit_cookie_manager_set_persistent_storage (cookie_manager, filename,
                                                  WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    g_free (filename);
  }

  cookie_policy = g_settings_get_string (EPHY_SETTINGS_WEB,
                                         EPHY_PREFS_WEB_COOKIES_POLICY);
  ephy_embed_prefs_set_cookie_accept_policy (cookie_manager, cookie_policy);
  g_free (cookie_policy);

  filters_dir = adblock_filters_dir (shell);
  priv->filters_manager = ephy_filters_manager_new (filters_dir);
  g_free (filters_dir);
}

static void
ephy_embed_shell_shutdown (GApplication *application)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (EPHY_EMBED_SHELL (application));

  G_APPLICATION_CLASS (ephy_embed_shell_parent_class)->shutdown (application);

  if (priv->dbus_server)
    g_dbus_server_stop (priv->dbus_server);

  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "overview");
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "tlsErrorPage");
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "formAuthData");
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "aboutApps");

  g_list_foreach (priv->web_extensions, (GFunc)ephy_embed_shell_unwatch_web_extension, application);

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
  g_assert (embed_shell == NULL);
  embed_shell = shell;
}

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = ephy_embed_shell_dispose;
  object_class->finalize = ephy_embed_shell_finalize;
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
   * @web_extension: the #EphyWebExtensionProxy
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
                  EPHY_TYPE_WEB_EXTENSION_PROXY);

  /**
   * EphyEmbedShell::allow-tls-certificate:
   * @shell: the #EphyEmbedShell
   * @page_id: the identifier of the web page
   *
   * Emitted when the web extension requests an exception be
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
   * Emitted when the web extension requests an exception be
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
   * EphyEmbedShell::form-auth-data-save-requested:
   * @shell: the #EphyEmbedShell
   * @request_id: the identifier of the request
   * @page_id: the identifier of the web page
   * @hostname: the hostname
   * @username: the username
   *
   * Emitted when a web page requests confirmation to save
   * the form authentication data for the given @hostname and
   * @username
   */
  signals[FORM_AUTH_DATA_SAVE_REQUESTED] =
    g_signal_new ("form-auth-data-save-requested",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 4,
                  G_TYPE_UINT,
                  G_TYPE_UINT64,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  /**
   * EphyEmbedShell::sensitive-form-focused
   * @shell: the #EphyEmbedShell
   * @page_id: the identifier of the web page
   * @insecure_action: whether the target of the form is http://
   *
   * Emitted when a form in a web page gains focus.
   */
  signals[SENSITIVE_FORM_FOCUSED] =
    g_signal_new ("sensitive-form-focused",
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
  char *path;

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  if (page_setup != NULL)
    g_object_ref (page_setup);
  else
    page_setup = gtk_page_setup_new ();

  if (priv->page_setup != NULL)
    g_object_unref (priv->page_setup);

  priv->page_setup = page_setup;

  path = g_build_filename (ephy_dot_dir (), PAGE_SETUP_FILENAME, NULL);
  gtk_page_setup_to_file (page_setup, path, NULL);
  g_free (path);
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

  if (priv->page_setup == NULL) {
    GError *error = NULL;
    char *path;

    path = g_build_filename (ephy_dot_dir (), PAGE_SETUP_FILENAME, NULL);
    priv->page_setup = gtk_page_setup_new_from_file (path, &error);
    g_free (path);

    if (error)
      g_error_free (error);

    /* If that still didn't work, create a new, empty one */
    if (priv->page_setup == NULL)
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
  char *path;

  g_assert (EPHY_IS_EMBED_SHELL (shell));

  if (settings != NULL)
    g_object_ref (settings);

  if (priv->print_settings != NULL)
    g_object_unref (priv->print_settings);

  priv->print_settings = settings ? settings : gtk_print_settings_new ();

  path = g_build_filename (ephy_dot_dir (), PRINT_SETTINGS_FILENAME, NULL);
  gtk_print_settings_to_file (settings, path, NULL);
  g_free (path);
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

  if (priv->print_settings == NULL) {
    GError *error = NULL;
    char *path;

    path = g_build_filename (ephy_dot_dir (), PRINT_SETTINGS_FILENAME, NULL);
    priv->print_settings = gtk_print_settings_new_from_file (path, &error);
    g_free (path);

    /* Note: the gtk print settings file format is the same as our
     * legacy one, so no need to migrate here.
     */

    if (priv->print_settings == NULL)
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
 * ephy_embed_shell_add_app_related_uri:
 * @shell: an #EphyEmbedShell
 * @uri: the URI
 **/
void
ephy_embed_shell_add_app_related_uri (EphyEmbedShell *shell, const char *uri)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  char *origin;

  g_assert (EPHY_IS_EMBED_SHELL (shell));
  g_assert (priv->mode == EPHY_EMBED_SHELL_MODE_APPLICATION);

  origin = ephy_uri_to_security_origin (uri);

  if (!g_list_find_custom (priv->app_origins, origin, (GCompareFunc)g_strcmp0))
    priv->app_origins = g_list_append (priv->app_origins, origin);
}

/**
 * ephy_embed_shell_uri_looks_related_to_application:
 * @shell: an #EphyEmbedShell
 * @uri: the URI
 *
 * Returns: %TRUE if @uri looks related, %FALSE otherwise
 **/
gboolean
ephy_embed_shell_uri_looks_related_to_app (EphyEmbedShell *shell,
                                           const char     *uri)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  g_assert (EPHY_IS_EMBED_SHELL (shell));
  g_assert (priv->mode == EPHY_EMBED_SHELL_MODE_APPLICATION);

  for (GList *iter = priv->app_origins; iter != NULL; iter = iter->next) {
    const char *iter_uri = (const char *)iter->data;
    if (ephy_embed_utils_urls_have_same_origin (iter_uri, uri))
      return TRUE;
  }

  return FALSE;
}

/**
 * ephy_embed_shell_launch_handler:
 * @shell: an #EphyEmbedShell
 * @file: a #GFile to open
 * @mime_type: the mime type of @file or %NULL
 * @user_time: user time to prevent focus stealing
 *
 * Tries to open @file with the right application, making sure we will
 * not call ourselves in the process. This is needed to avoid
 * potential infinite loops when opening unknown file types.
 *
 * Returns: %TRUE on success
 **/
gboolean
ephy_embed_shell_launch_handler (EphyEmbedShell *shell,
                                 GFile          *file,
                                 const char     *mime_type,
                                 guint32         user_time)
{
  GAppInfo *app;
  GList *list = NULL;
  gboolean ret = FALSE;

  g_assert (EPHY_IS_EMBED_SHELL (shell));
  g_assert (file || mime_type);

  if (ephy_is_running_inside_flatpak ()) {
    return ephy_file_launch_file_via_uri_handler (file);
  }

  app = ephy_file_launcher_get_app_info_for_file (file, mime_type);

  /* Do not allow recursive calls into the browser, they can lead to
   * infinite loops and they should never happen anyway. */
  if (!app || g_strcmp0 (g_app_info_get_id (app), "org.gnome.Epiphany.desktop") == 0)
    return ret;

  list = g_list_append (list, file);
  ret = ephy_file_launch_application (app, list, user_time, NULL);
  g_list_free (list);

  return ret;
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

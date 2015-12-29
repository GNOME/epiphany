/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include <config.h>
#include "ephy-embed-shell.h"

#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-private.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-encodings.h"
#include "ephy-file-helpers.h"
#include "ephy-history-service.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-snapshot-service.h"
#include "ephy-web-app-utils.h"
#include "ephy-web-extension-proxy.h"
#include "ephy-web-extension-names.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#define PAGE_SETUP_FILENAME "page-setup-gtk.ini"
#define PRINT_SETTINGS_FILENAME "print-settings.ini"
#define OVERVIEW_RELOAD_DELAY 500

#define EPHY_EMBED_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellPrivate))

struct _EphyEmbedShellPrivate
{
  WebKitWebContext *web_context;
  EphyHistoryService *global_history_service;
  EphyEncodings *encodings;
  GtkPageSetup *page_setup;
  GtkPrintSettings *print_settings;
  EphyEmbedShellMode mode;
  WebKitUserContentManager *user_content;
  EphyAboutHandler *about_handler;
  guint update_overview_timeout_id;
  guint hiding_overview_item;
  GDBusConnection *bus;
  GList *web_extensions;
  guint web_extensions_page_created_signal_id;
};

enum
{
  PREPARE_CLOSE,
  RESTORED_WINDOW,
  WEB_VIEW_CREATED,
  PAGE_CREATED,
  ALLOW_TLS_CERTIFICATE,
  FORM_AUTH_DATA_SAVE_REQUESTED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum
{
  PROP_0,
  PROP_MODE,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

EphyEmbedShell *embed_shell = NULL;

G_DEFINE_TYPE (EphyEmbedShell, ephy_embed_shell, GTK_TYPE_APPLICATION)

static void
ephy_embed_shell_dispose (GObject *object)
{
  EphyEmbedShellPrivate *priv = EPHY_EMBED_SHELL (object)->priv;

  if (priv->update_overview_timeout_id > 0) {
    g_source_remove (priv->update_overview_timeout_id);
    priv->update_overview_timeout_id = 0;
  }

  g_clear_object (&priv->encodings);
  g_clear_object (&priv->page_setup);
  g_clear_object (&priv->print_settings);
  g_clear_object (&priv->global_history_service);
  g_clear_object (&priv->about_handler);
  g_clear_object (&priv->user_content);
  g_clear_object (&priv->web_context);

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->dispose (object);
}

static gint
web_extension_compare (EphyWebExtensionProxy *proxy,
                       const char *name_owner)
{
  return g_strcmp0 (ephy_web_extension_proxy_get_name_owner (proxy), name_owner);
}

static EphyWebExtensionProxy *
ephy_embed_shell_find_web_extension (EphyEmbedShell *shell,
                                     const char *name_owner)
{
  GList *l;

  l = g_list_find_custom (shell->priv->web_extensions, name_owner, (GCompareFunc)web_extension_compare);

  if (!l)
    g_warning ("Could not find extension with name owner `%s´.", name_owner);

  return l ? EPHY_WEB_EXTENSION_PROXY (l->data) : NULL;
}

static void
web_extension_form_auth_data_message_received_cb (WebKitUserContentManager *manager,
                                                  WebKitJavascriptResult *message,
                                                  EphyEmbedShell *shell)
{
  guint request_id;
  guint64 page_id;
  const char *hostname;
  const char *username;
  GVariant *variant;
  gchar *message_str;

  message_str = ephy_embed_utils_get_js_result_as_string (message);
  variant = g_variant_parse (G_VARIANT_TYPE ("(utss)"), message_str, NULL, NULL, NULL);
  g_free (message_str);

  g_variant_get (variant, "(ut&s&s)", &request_id, &page_id, &hostname, &username);
  g_signal_emit (shell, signals[FORM_AUTH_DATA_SAVE_REQUESTED], 0,
                 request_id, page_id, hostname, username);
  g_variant_unref (variant);
}

static void
web_extension_page_created (GDBusConnection *connection,
                            const char *sender_name,
                            const char *object_path,
                            const char *interface_name,
                            const char *signal_name,
                            GVariant *parameters,
                            EphyEmbedShell *shell)
{
  EphyWebExtensionProxy *web_extension;
  guint64 page_id;

  g_variant_get (parameters, "(t)", &page_id);

  web_extension = ephy_embed_shell_find_web_extension (shell, sender_name);
  if (!web_extension)
    return;
  g_signal_emit (shell, signals[PAGE_CREATED], 0, page_id, web_extension);
}

static void
history_service_query_urls_cb (EphyHistoryService *service,
                               gboolean success,
                               GList *urls,
                               EphyEmbedShell *shell)
{
  GList *l;

  if (!success)
    return;

  for (l = shell->priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_set_urls (web_extension, urls);
  }
}

static void
ephy_embed_shell_update_overview_urls (EphyEmbedShell *shell)
{
  EphyHistoryQuery *query;

  query = ephy_history_query_new ();
  query->sort_type = EPHY_HISTORY_SORT_MOST_VISITED;
  query->limit = EPHY_ABOUT_OVERVIEW_MAX_ITEMS;
  query->ignore_hidden = TRUE;
  query->ignore_local = TRUE;

  ephy_history_service_query_urls (shell->priv->global_history_service, query, NULL,
                                   (EphyHistoryJobCallback) history_service_query_urls_cb,
                                   shell);
  ephy_history_query_free (query);
}

static void
history_service_urls_visited_cb (EphyHistoryService *history,
                                 EphyEmbedShell *shell)
{
   ephy_embed_shell_update_overview_urls (shell);
}

static gboolean
ephy_embed_shell_update_overview_timeout_cb (EphyEmbedShell *shell)
{
  shell->priv->update_overview_timeout_id = 0;

  if (shell->priv->hiding_overview_item > 0)
    return FALSE;

  ephy_embed_shell_update_overview_urls (shell);

  return FALSE;
}

static void
history_set_url_hidden_cb (EphyHistoryService *service,
                           gboolean success,
                           gpointer result_data,
                           EphyEmbedShell *shell)
{
  shell->priv->hiding_overview_item--;

  if (!success)
    return;

  if (shell->priv->update_overview_timeout_id > 0)
    return;

  ephy_embed_shell_update_overview_urls (shell);
}

static void
web_extension_overview_message_received_cb (WebKitUserContentManager *manager,
                                            WebKitJavascriptResult *message,
                                            EphyEmbedShell *shell)
{
  char *url_to_remove;

  url_to_remove = ephy_embed_utils_get_js_result_as_string (message);

  shell->priv->hiding_overview_item++;
  ephy_history_service_set_url_hidden (shell->priv->global_history_service,
                                       url_to_remove, TRUE, NULL,
                                       (EphyHistoryJobCallback) history_set_url_hidden_cb,
                                       shell);
  g_free (url_to_remove);

  if (shell->priv->update_overview_timeout_id > 0)
    g_source_remove (shell->priv->update_overview_timeout_id);

  /* Wait for the CSS animations to finish before refreshing */
  shell->priv->update_overview_timeout_id =
    g_timeout_add (OVERVIEW_RELOAD_DELAY, (GSourceFunc) ephy_embed_shell_update_overview_timeout_cb, shell);
}

static void
web_extension_tls_error_page_message_received_cb (WebKitUserContentManager *manager,
                                                  WebKitJavascriptResult *message,
                                                  EphyEmbedShell *shell)
{
  guint64 page_id;

  page_id = ephy_embed_utils_get_js_result_as_number (message);
  g_signal_emit (shell, signals[ALLOW_TLS_CERTIFICATE], 0, page_id);
}

static void
web_extension_about_apps_message_received_cb (WebKitUserContentManager *manager,
                                              WebKitJavascriptResult *message,
                                              EphyEmbedShell *shell)
{
  char *app_id;

  app_id = ephy_embed_utils_get_js_result_as_string (message);
  ephy_web_application_delete (app_id);
  g_free (app_id);
}

static void
web_extension_destroyed (EphyEmbedShell *shell,
                         GObject *web_extension)
{
  shell->priv->web_extensions = g_list_remove (shell->priv->web_extensions, web_extension);
}

static void
ephy_embed_shell_watch_web_extension (EphyEmbedShell *shell,
                                      const char *web_extension_id)
{
  EphyWebExtensionProxy *web_extension;
  char *service_name;

  if (!shell->priv->bus)
    return;

  service_name = g_strdup_printf ("%s-%s", EPHY_WEB_EXTENSION_SERVICE_NAME, web_extension_id);
  web_extension = ephy_web_extension_proxy_new (shell->priv->bus, service_name);
  shell->priv->web_extensions = g_list_prepend (shell->priv->web_extensions, web_extension);
  g_object_weak_ref (G_OBJECT (web_extension), (GWeakNotify)web_extension_destroyed, shell);
  g_free (service_name);
}

static void
ephy_embed_shell_unwatch_web_extension (EphyWebExtensionProxy *web_extension,
                                        EphyEmbedShell *shell)
{
  g_object_weak_unref (G_OBJECT (web_extension), (GWeakNotify)web_extension_destroyed, shell);
}

static void
history_service_url_title_changed_cb (EphyHistoryService *service,
                                      const char *url,
                                      const char *title,
                                      EphyEmbedShell *shell)
{
  GList *l;

  for (l = shell->priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_set_url_title (web_extension, url, title);
  }
}

static void
history_service_url_deleted_cb (EphyHistoryService *service,
                                const char *url,
                                EphyEmbedShell *shell)
{
  GList *l;

  for (l = shell->priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_delete_url (web_extension, url);
  }
}

static void
history_service_host_deleted_cb (EphyHistoryService *service,
                                 const char *deleted_url,
                                 EphyEmbedShell *shell)
{
  GList *l;
  SoupURI *deleted_uri;

  deleted_uri = soup_uri_new (deleted_url);

  for (l = shell->priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_delete_host (web_extension, soup_uri_get_host (deleted_uri));
  }

  soup_uri_free (deleted_uri);
}

static void
history_service_cleared_cb (EphyHistoryService *service,
                            EphyEmbedShell *shell)
{
  GList *l;

  for (l = shell->priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_clear (web_extension);
  }
}

void
ephy_embed_shell_set_thumbanil_path (EphyEmbedShell *shell,
                                     const char *url,
                                     time_t mtime,
                                     const char *path)
{
  GList *l;

  ephy_history_service_set_url_thumbnail_time (shell->priv->global_history_service,
                                               url, mtime,
                                               NULL, NULL, NULL);

  for (l = shell->priv->web_extensions; l; l = g_list_next (l)) {
    EphyWebExtensionProxy *web_extension = (EphyWebExtensionProxy *)l->data;

    ephy_web_extension_proxy_history_set_url_thumbnail (web_extension, url, path);
  }
}

/**
 * ephy_embed_shell_get_global_history_service:
 * @shell: the #EphyEmbedShell
 *
 * Return value: (transfer none): the global #EphyHistoryService
 **/
GObject *
ephy_embed_shell_get_global_history_service (EphyEmbedShell *shell)
{
  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

  if (shell->priv->global_history_service == NULL) {
    char *filename;

    filename = g_build_filename (ephy_dot_dir (), EPHY_HISTORY_FILE, NULL);
    shell->priv->global_history_service = ephy_history_service_new (filename,
                                                                    shell->priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO);
    g_free (filename);
    g_return_val_if_fail (shell->priv->global_history_service, NULL);
    g_signal_connect (shell->priv->global_history_service, "urls-visited",
                      G_CALLBACK (history_service_urls_visited_cb),
                      shell);
    g_signal_connect (shell->priv->global_history_service, "url-title-changed",
                      G_CALLBACK (history_service_url_title_changed_cb),
                      shell);
    g_signal_connect (shell->priv->global_history_service, "url-deleted",
                      G_CALLBACK (history_service_url_deleted_cb),
                      shell);
    g_signal_connect (shell->priv->global_history_service, "host-deleted",
                      G_CALLBACK (history_service_host_deleted_cb),
                      shell);
    g_signal_connect (shell->priv->global_history_service, "cleared",
                      G_CALLBACK (history_service_cleared_cb),
                      shell);
  }

  return G_OBJECT (shell->priv->global_history_service);
}

/**
 * ephy_embed_shell_get_encodings:
 * @shell: the #EphyEmbedShell
 *
 * Return value: (transfer none):
 **/
GObject *
ephy_embed_shell_get_encodings (EphyEmbedShell *shell)
{
  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

  if (shell->priv->encodings == NULL)
    shell->priv->encodings = ephy_encodings_new ();

  return G_OBJECT (shell->priv->encodings);
}

void
ephy_embed_shell_prepare_close (EphyEmbedShell *shell)
{
  g_signal_emit (shell, signals[PREPARE_CLOSE], 0);
}

void
ephy_embed_shell_restored_window (EphyEmbedShell *shell)
{
  g_signal_emit (shell, signals[RESTORED_WINDOW], 0);
}

static void
about_request_cb (WebKitURISchemeRequest *request,
                  EphyEmbedShell *shell)
{
  ephy_about_handler_handle_request (shell->priv->about_handler, request);
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
initialize_web_extensions (WebKitWebContext* web_context,
                           EphyEmbedShell *shell)
{
  GVariant *user_data;
  gboolean private_profile;
  char *web_extension_id;
  static guint web_extension_count = 0;

  webkit_web_context_set_web_extensions_directory (web_context, EPHY_WEB_EXTENSIONS_DIR);

  web_extension_id = g_strdup_printf ("%u-%u", getpid (), ++web_extension_count);
  ephy_embed_shell_watch_web_extension (shell, web_extension_id);

  private_profile = EPHY_EMBED_SHELL_MODE_HAS_PRIVATE_PROFILE (shell->priv->mode);
  user_data = g_variant_new ("(ssb)", web_extension_id, ephy_dot_dir (), private_profile);
  webkit_web_context_set_web_extensions_initialization_user_data (web_context, user_data);
}

static void
ephy_embed_shell_setup_web_extensions_connection (EphyEmbedShell *shell)
{
  shell->priv->bus = g_application_get_dbus_connection (G_APPLICATION (shell));
  if (!shell->priv->bus) {
    g_warning ("Application not connected to session bus");
    return;
  }

  shell->priv->web_extensions_page_created_signal_id =
    g_dbus_connection_signal_subscribe (shell->priv->bus,
                                        NULL,
                                        EPHY_WEB_EXTENSION_INTERFACE,
                                        "PageCreated",
                                        EPHY_WEB_EXTENSION_OBJECT_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        (GDBusSignalCallback)web_extension_page_created,
                                        shell,
                                        NULL);
}

static void
ephy_embed_shell_setup_process_model (EphyEmbedShell *shell)
{
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
  }

  webkit_web_context_set_process_model (shell->priv->web_context, WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);
  webkit_web_context_set_web_process_count_limit (shell->priv->web_context, max_processes);
}

static void
ephy_embed_shell_create_web_context (EphyEmbedShell *embed_shell)
{
  WebKitWebsiteDataManager *manager;
  char *data_dir;
  char *cache_dir;
  EphyEmbedShellPrivate *priv = embed_shell->priv;

  data_dir = g_build_filename (EPHY_EMBED_SHELL_MODE_HAS_PRIVATE_PROFILE (priv->mode) ?
                               ephy_dot_dir () : g_get_user_data_dir (),
                               g_get_prgname (), NULL);
  cache_dir = g_build_filename (EPHY_EMBED_SHELL_MODE_HAS_PRIVATE_PROFILE (priv->mode) ?
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

static void
ephy_embed_shell_startup (GApplication* application)
{
  EphyEmbedShell *shell = EPHY_EMBED_SHELL (application);
  EphyEmbedShellPrivate *priv = shell->priv;
  char *favicon_db_path;
  WebKitCookieManager *cookie_manager;
  char *filename;
  char *cookie_policy;

  G_APPLICATION_CLASS (ephy_embed_shell_parent_class)->startup (application);

  /* We're not remoting, setup the Web Context if we are not running in a test.
     Tests already do this after construction. */
  if (priv->mode != EPHY_EMBED_SHELL_MODE_TEST)
    ephy_embed_shell_create_web_context (embed_shell);

  ephy_embed_shell_setup_web_extensions_connection (shell);

  /* User content manager */
  if (priv->mode != EPHY_EMBED_SHELL_MODE_TEST)
    shell->priv->user_content = webkit_user_content_manager_new ();

  webkit_user_content_manager_register_script_message_handler (shell->priv->user_content,
                                                               "overview");
  g_signal_connect (shell->priv->user_content, "script-message-received::overview",
                    G_CALLBACK (web_extension_overview_message_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler (shell->priv->user_content,
                                                               "tlsErrorPage");
  g_signal_connect (shell->priv->user_content, "script-message-received::tlsErrorPage",
                    G_CALLBACK (web_extension_tls_error_page_message_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler (shell->priv->user_content,
                                                               "formAuthData");
  g_signal_connect (shell->priv->user_content, "script-message-received::formAuthData",
                    G_CALLBACK (web_extension_form_auth_data_message_received_cb),
                    shell);

  webkit_user_content_manager_register_script_message_handler (shell->priv->user_content,
                                                               "aboutApps");
  g_signal_connect (shell->priv->user_content, "script-message-received::aboutApps",
                    G_CALLBACK (web_extension_about_apps_message_received_cb),
                    shell);

  ephy_embed_shell_setup_process_model (shell);
  g_signal_connect (priv->web_context, "initialize-web-extensions",
                    G_CALLBACK (initialize_web_extensions),
                    shell);

  /* Favicon Database */
  favicon_db_path = g_build_filename (EPHY_EMBED_SHELL_MODE_HAS_PRIVATE_PROFILE (priv->mode) ?
                                      ephy_dot_dir () : g_get_user_cache_dir (),
                                      "icondatabase", NULL);
  webkit_web_context_set_favicon_database_directory (priv->web_context, favicon_db_path);
  g_free (favicon_db_path);

  /* Do not ignore TLS errors. */
  webkit_web_context_set_tls_errors_policy (priv->web_context, WEBKIT_TLS_ERRORS_POLICY_FAIL);


  /* about: URIs handler */
  shell->priv->about_handler = ephy_about_handler_new ();
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

  /* Store cookies in moz-compatible SQLite format */
  cookie_manager = webkit_web_context_get_cookie_manager (priv->web_context);
  filename = g_build_filename (ephy_dot_dir (), "cookies.sqlite", NULL);
  webkit_cookie_manager_set_persistent_storage (cookie_manager, filename,
                                                WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
  g_free (filename);

  cookie_policy = g_settings_get_string (EPHY_SETTINGS_WEB,
                                         EPHY_PREFS_WEB_COOKIES_POLICY);
  ephy_embed_prefs_set_cookie_accept_policy (cookie_manager, cookie_policy);
  g_free (cookie_policy);
}

static void
ephy_embed_shell_shutdown (GApplication* application)
{
  EphyEmbedShellPrivate *priv = EPHY_EMBED_SHELL (application)->priv;

  G_APPLICATION_CLASS (ephy_embed_shell_parent_class)->shutdown (application);

  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "overview");
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "tlsErrorPage");
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "formAuthData");
  webkit_user_content_manager_unregister_script_message_handler (priv->user_content, "aboutApps");

  if (priv->web_extensions_page_created_signal_id > 0) {
    g_dbus_connection_signal_unsubscribe (priv->bus, priv->web_extensions_page_created_signal_id);
    priv->web_extensions_page_created_signal_id = 0;
  }

  g_list_foreach (priv->web_extensions, (GFunc)ephy_embed_shell_unwatch_web_extension, application);

  g_object_unref (ephy_embed_prefs_get_settings ());
  ephy_embed_utils_shutdown ();
}

static void
ephy_embed_shell_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  EphyEmbedShell *embed_shell = EPHY_EMBED_SHELL (object);

  switch (prop_id) {
  case PROP_MODE:
    embed_shell->priv->mode = g_value_get_enum (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_embed_shell_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  EphyEmbedShell *embed_shell = EPHY_EMBED_SHELL (object);

  switch (prop_id) {
  case PROP_MODE:
    g_value_set_enum (value, embed_shell->priv->mode);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_embed_shell_constructed (GObject *object)
{
  EphyEmbedShell *embed_shell;

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->constructed (object);

  embed_shell = EPHY_EMBED_SHELL (object);
  /* These do not run the EmbedShell application instance, so make sure that
     there is a web context and a user content manager for them. */
  if (ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_TEST ||
      ephy_embed_shell_get_mode (embed_shell) == EPHY_EMBED_SHELL_MODE_SEARCH_PROVIDER) {
    ephy_embed_shell_create_web_context (embed_shell);
    embed_shell->priv->user_content = webkit_user_content_manager_new ();
  }
}

static void
ephy_embed_shell_init (EphyEmbedShell *shell)
{
  shell->priv = EPHY_EMBED_SHELL_GET_PRIVATE (shell);

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
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                       G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY);
  
  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     object_properties);

/**
 * EphyEmbed::prepare-close:
 * @shell: the #EphyEmbedShell
 * 
 * The ::prepare-close signal is emitted when epiphany is preparing to
 * quit on command from the session manager. You can use it when you need
 * to do something special (shut down a service, for example).
 **/
  signals[PREPARE_CLOSE] =
    g_signal_new ("prepare-close",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EphyEmbedShellClass, prepare_close),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

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
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
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
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
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
                  0, NULL, NULL,
                  g_cclosure_marshal_generic,
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
                  0, NULL, NULL,
                  g_cclosure_marshal_generic,
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
                  0, NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 4,
                  G_TYPE_UINT,
                  G_TYPE_UINT64,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  g_type_class_add_private (object_class, sizeof (EphyEmbedShellPrivate));
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
                                 GtkPageSetup *page_setup)
{
  EphyEmbedShellPrivate *priv;
  char *path;

  g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));
  priv = shell->priv;

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
  EphyEmbedShellPrivate *priv;

  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);
  priv = shell->priv;

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
ephy_embed_shell_set_print_settings (EphyEmbedShell *shell,
                                     GtkPrintSettings *settings)
{
  EphyEmbedShellPrivate *priv;
  char *path;

  g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));
  priv = shell->priv;

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
  EphyEmbedShellPrivate *priv;

  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);
  priv = shell->priv;

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
  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), EPHY_EMBED_SHELL_MODE_BROWSER);
  
  return shell->priv->mode;
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
                                 GFile *file,
                                 const char *mime_type,
                                 guint32 user_time)
{
  GAppInfo *app;
  GList *list = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), FALSE);
  g_return_val_if_fail (file || mime_type, FALSE);

  app = ephy_file_launcher_get_app_info_for_file (file, mime_type);

  /* Do not allow recursive calls into the browser, they can lead to
   * infinite loops and they should never happen anyway. */

  /* FIXME: eventually there should be a nice and safe way of getting
   * the app ID from the GApplication itself, but for now let's
   * hardcode the .desktop file name and use it here. */
  if (!app || g_strcmp0 (g_app_info_get_id (app), "epiphany.desktop") == 0)
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
  webkit_web_context_clear_cache (shell->priv->web_context);
}

WebKitUserContentManager *
ephy_embed_shell_get_user_content_manager (EphyEmbedShell *shell)
{
  return shell->priv->user_content;
}

WebKitWebContext *
ephy_embed_shell_get_web_context (EphyEmbedShell *shell)
{
  return shell->priv->web_context;
}

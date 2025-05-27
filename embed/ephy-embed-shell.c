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
#include "ephy-autofill-field.h"
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
#include "ephy-reader-handler.h"
#include "ephy-settings.h"
#include "ephy-snapshot-service.h"
#include "ephy-tabs-catalog.h"
#include "ephy-uri-helpers.h"
#include "ephy-view-source-handler.h"
#include "ephy-web-app-utils.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#define PAGE_SETUP_FILENAME "page-setup-gtk.ini"
#define PRINT_SETTINGS_FILENAME "print-settings.ini"

typedef struct {
  WebKitWebContext *web_context;
  WebKitNetworkSession *network_session;
  EphyHistoryService *global_history_service;
  EphyEncodings *encodings;
  GtkPageSetup *page_setup;
  GtkPrintSettings *print_settings;
  EphyEmbedShellMode mode;
  EphyDownloadsManager *downloads_manager;
  EphyPermissionsManager *permissions_manager;
  EphyPasswordManager *password_manager;
  EphyAboutHandler *about_handler;
  EphyViewSourceHandler *source_handler;
  EphyReaderHandler *reader_handler;
  char *guid;
  EphyFiltersManager *filters_manager;
  GVariant *web_extension_initialization_data;
  EphySearchEngineManager *search_engine_manager;
  GCancellable *cancellable;
} EphyEmbedShellPrivate;

enum {
  RESTORED_WINDOW,
  WEB_VIEW_CREATED,
  PASSWORD_FORM_FOCUSED,
  PASSWORD_FORM_SUBMITTED,
  AUTOFILL_SIGNAL,

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

G_DEFINE_TYPE_WITH_CODE (EphyEmbedShell, ephy_embed_shell, ADW_TYPE_APPLICATION,
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
    g_autoptr (GList) tabs = ephy_embed_container_get_children (l->data);

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

static GList *
tabs_catalog_get_tabs_info (EphyTabsCatalog *catalog)
{
  WebKitFaviconDatabase *database;
  GList *windows;
  g_autoptr (GList) tabs = NULL;
  GList *tabs_info = NULL;
  const char *title;
  const char *url;
  g_autofree char *favicon = NULL;

  g_assert ((gpointer)catalog == (gpointer)embed_shell);

  windows = gtk_application_get_windows (GTK_APPLICATION (embed_shell));
  database = ephy_embed_shell_get_favicon_database (embed_shell);

  for (GList *l = windows; l && l->data; l = l->next) {
    tabs = ephy_embed_container_get_children (l->data);

    for (GList *t = tabs; t && t->data; t = t->next) {
      title = ephy_embed_get_title (t->data);

      if (!g_strcmp0 (title, _(BLANK_PAGE_TITLE)) ||
          !g_strcmp0 (title, _(NEW_TAB_PAGE_TITLE)))
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

  g_clear_object (&priv->encodings);
  g_clear_object (&priv->page_setup);
  g_clear_object (&priv->print_settings);
  g_clear_object (&priv->global_history_service);
  g_clear_object (&priv->about_handler);
  g_clear_object (&priv->reader_handler);
  g_clear_object (&priv->source_handler);
  g_clear_object (&priv->downloads_manager);
  g_clear_object (&priv->password_manager);
  g_clear_object (&priv->permissions_manager);
  g_clear_object (&priv->web_context);
  g_clear_object (&priv->network_session);
  g_clear_pointer (&priv->guid, g_free);
  g_clear_object (&priv->filters_manager);
  g_clear_object (&priv->search_engine_manager);
  g_clear_pointer (&priv->web_extension_initialization_data, g_variant_unref);

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->dispose (object);
}

static void
web_process_extension_password_form_focused_message_received_cb (WebKitUserContentManager *manager,
                                                                 JSCValue                 *message,
                                                                 EphyEmbedShell           *shell)
{
  guint64 page_id;
  gboolean insecure_form_action;
  g_autoptr (GVariant) variant = NULL;
  g_autofree char *message_str = NULL;

  message_str = jsc_value_to_string (message);
  variant = g_variant_parse (G_VARIANT_TYPE ("(tb)"), message_str, NULL, NULL, NULL);

  g_variant_get (variant, "(tb)", &page_id, &insecure_form_action);
  g_signal_emit (shell, signals[PASSWORD_FORM_FOCUSED], 0,
                 page_id, insecure_form_action);
}

static void
history_service_query_urls_cb (EphyHistoryService *service,
                               gboolean            success,
                               GList              *urls,
                               EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  GList *l;
  GVariantBuilder builder;

  if (!success)
    return;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  for (l = urls; l; l = g_list_next (l)) {
    EphyHistoryURL *url = (EphyHistoryURL *)l->data;

    g_variant_builder_add (&builder, "(ss)", url->url, url->title);
    ephy_embed_shell_schedule_thumbnail_update (shell, (EphyHistoryURL *)l->data);
  }

  webkit_web_context_send_message_to_all_extensions (priv->web_context,
                                                     webkit_user_message_new ("History.SetURLs",
                                                                              g_variant_builder_end (&builder)));
}

static void
ephy_embed_shell_update_overview_urls (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr (EphyHistoryQuery) query = NULL;

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
                                                    JSCValue                 *message,
                                                    EphyEmbedShell           *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autofree char *url_to_remove = NULL;

  url_to_remove = jsc_value_to_string (message);

  ephy_history_service_set_url_hidden (priv->global_history_service,
                                       url_to_remove, TRUE, NULL,
                                       (EphyHistoryJobCallback)history_set_url_hidden_cb,
                                       shell);
}

static char *
property_to_string_or_null (JSCValue   *value,
                            const char *name)
{
  g_autoptr (JSCValue) prop = jsc_value_object_get_property (value, name);
  if (jsc_value_is_null (prop) || jsc_value_is_undefined (prop))
    return NULL;
  return jsc_value_to_string (prop);
}

static int
property_to_uint64 (JSCValue   *value,
                    const char *name)
{
  g_autoptr (JSCValue) prop = jsc_value_object_get_property (value, name);
  return (guint64)jsc_value_to_double (prop);
}

static gboolean
property_to_boolean (JSCValue   *value,
                     const char *name)
{
  g_autoptr (JSCValue) prop = jsc_value_object_get_property (value, name);
  return jsc_value_to_boolean (prop);
}

static void
web_process_extension_autofill_askuser_received_cb (WebKitUserContentManager *manager,
                                                    JSCValue                 *value,
                                                    EphyEmbedShell           *shell)
{
  guint64 page_id = property_to_uint64 (value, "pageId");
  char *selector = property_to_string_or_null (value, "selector");
  gboolean is_fillable_element = property_to_boolean (value, "isFillableElement");
  gboolean has_personal_fields = property_to_boolean (value, "hasPersonalFields");
  gboolean has_card_fields = property_to_boolean (value, "hasCardFields");
  guint64 x = property_to_uint64 (value, "x");
  guint64 y = property_to_uint64 (value, "y");
  guint64 element_width = property_to_uint64 (value, "width");
  guint64 element_height = property_to_uint64 (value, "height");

  g_signal_emit (shell, signals[AUTOFILL_SIGNAL], 0,
                 page_id, selector, is_fillable_element, has_personal_fields, has_card_fields, x, y, element_width, element_height);
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
  g_autoptr (JSCValue) is_new_prop = jsc_value_object_get_property (value, "isNew");
  gboolean is_new = jsc_value_to_boolean (is_new_prop);
  guint64 page_id = property_to_uint64 (value, "pageID");
  EphyWebView *view;
  EphyPasswordRequestData *request_data;

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

  /* Since we're automatically storing the password, no user interaction is required,
  so we don't need to send the data anywhere */
  if (!is_request) {
    ephy_password_manager_save (priv->password_manager, origin, target_origin, username,
                                username, password, username_field, password_field, is_new);
    return;
  } else {
    /* User interaction is required, so whatever is handling the EmbedShell should
    handle the request + saving the password */

    request_data = g_new (EphyPasswordRequestData, 1);
    request_data->origin = g_steal_pointer (&origin);
    request_data->target_origin = g_steal_pointer (&target_origin);
    request_data->username = g_steal_pointer (&username);
    request_data->password = g_steal_pointer (&password);
    request_data->usernameField = g_steal_pointer (&username_field);
    request_data->passwordField = g_steal_pointer (&password_field);
    request_data->isNew = is_new;
    g_signal_emit (shell, signals[PASSWORD_FORM_SUBMITTED], 0, request_data);
  }
}

static void
web_process_extension_password_manager_save_received_cb (WebKitUserContentManager *manager,
                                                         JSCValue                 *message,
                                                         EphyEmbedShell           *shell)
{
  web_process_extension_password_manager_save_real (shell, message, FALSE);
}

static void
web_process_extension_password_manager_request_save_received_cb (WebKitUserContentManager *manager,
                                                                 JSCValue                 *message,
                                                                 EphyEmbedShell           *shell)
{
  web_process_extension_password_manager_save_real (shell, message, TRUE);
}

static void
history_service_url_title_changed_cb (EphyHistoryService *service,
                                      const char         *url,
                                      const char         *title,
                                      EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  webkit_web_context_send_message_to_all_extensions (priv->web_context,
                                                     webkit_user_message_new ("History.SetURLTitle",
                                                                              g_variant_new ("(ss)", url, title)));
}

static void
history_service_url_deleted_cb (EphyHistoryService *service,
                                EphyHistoryURL     *url,
                                EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  webkit_web_context_send_message_to_all_extensions (priv->web_context,
                                                     webkit_user_message_new ("History.DeleteURL",
                                                                              g_variant_new ("s", url->url)));
}

static void
history_service_host_deleted_cb (EphyHistoryService *service,
                                 const char         *deleted_url,
                                 EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr (GUri) deleted_uri = NULL;

  deleted_uri = g_uri_parse (deleted_url, G_URI_FLAGS_PARSE_RELAXED, NULL);
  webkit_web_context_send_message_to_all_extensions (priv->web_context,
                                                     webkit_user_message_new ("History.DeleteHost",
                                                                              g_variant_new ("s", g_uri_get_host (deleted_uri))));
}

static void
history_service_cleared_cb (EphyHistoryService *service,
                            EphyEmbedShell     *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  webkit_web_context_send_message_to_all_extensions (priv->web_context,
                                                     webkit_user_message_new ("History.Clear",
                                                                              NULL));
}

void
ephy_embed_shell_set_thumbnail_path (EphyEmbedShell *shell,
                                     const char     *url,
                                     const char     *path)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  webkit_web_context_send_message_to_all_extensions (priv->web_context,
                                                     webkit_user_message_new ("History.SetURLThumbnail",
                                                                              g_variant_new ("(ss)", url, path)));
}

static void
got_snapshot_path_for_url_cb (EphySnapshotService *service,
                              GAsyncResult        *result,
                              char                *url)
{
  g_autofree char *snapshot = NULL;
  g_autoptr (GError) error = NULL;

  snapshot = ephy_snapshot_service_get_snapshot_path_for_url_finish (service, result, &error);
  if (snapshot) {
    ephy_embed_shell_set_thumbnail_path (ephy_embed_shell_get_default (), url, snapshot);
  } else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    /* Bad luck, not something to warn about. */
    g_info ("Failed to get snapshot for URL %s: %s", url, error->message);
  }

  g_free (url);
}

void
ephy_embed_shell_schedule_thumbnail_update (EphyEmbedShell *shell,
                                            EphyHistoryURL *url)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  EphySnapshotService *service;
  const char *snapshot;

  service = ephy_snapshot_service_get_default ();
  snapshot = ephy_snapshot_service_lookup_cached_snapshot_path (service, url->url);

  if (snapshot) {
    ephy_embed_shell_set_thumbnail_path (shell, url->url, snapshot);
  } else {
    ephy_snapshot_service_get_snapshot_path_for_url_async (service,
                                                           url->url,
                                                           priv->cancellable,
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
      mode = EPHY_SQLITE_CONNECTION_MODE_MEMORY;
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
reader_request_cb (WebKitURISchemeRequest *request,
                   EphyEmbedShell         *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  ephy_reader_handler_handle_request (priv->reader_handler, request);
}

static void
ephy_resource_request_cb (WebKitURISchemeRequest *request)
{
  const char *path;
  gsize size;
  WebKitWebView *request_view;
  const char *uri;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GError) error = NULL;

  path = webkit_uri_scheme_request_get_path (request);
  if (!g_resources_get_info (path, 0, &size, NULL, &error)) {
    webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  request_view = webkit_uri_scheme_request_get_web_view (request);
  uri = webkit_web_view_get_uri (request_view);

  /* ephy-resource:// requests bypass CORS in order to allow custom schemes to
   * access ephy-resource://. Accordingly, we need some custom security to
   * prevent websites from directly accessing ephy-resource://.
   *
   * We'll have to leave open /page-icons and /page-templates since they are
   * needed for our alternate HTML error pages.
   */
  if (g_str_has_prefix (uri, "ephy-resource:") ||
      g_str_has_prefix (path, "/org/gnome/epiphany/page-icons/") ||
      g_str_has_prefix (path, "/org/gnome/epiphany/page-templates/") ||
      (g_str_has_prefix (uri, "ephy-reader:") && g_str_has_prefix (path, "/org/gnome/epiphany/readability/")) ||
      (g_str_has_prefix (uri, EPHY_VIEW_SOURCE_SCHEME ":") && g_str_has_prefix (path, "/org/gnome/epiphany/highlightjs/"))) {
    stream = g_resources_open_stream (path, 0, &error);
    if (stream)
      webkit_uri_scheme_request_finish (request, stream, size, NULL);
    else
      webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  error = g_error_new (WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_FAILED,
                       _("URI %s not authorized to access Epiphany resource %s"),
                       uri, path);
  webkit_uri_scheme_request_finish_error (request, error);
}

static gboolean
is_private_profile_mode (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  return priv->mode == EPHY_EMBED_SHELL_MODE_PRIVATE ||
         priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO ||
         priv->mode == EPHY_EMBED_SHELL_MODE_AUTOMATION;
}

gboolean
ephy_embed_shell_should_remember_passwords (EphyEmbedShell *shell)
{
  return !is_private_profile_mode (shell) && g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_REMEMBER_PASSWORDS);
}

static void
initialize_web_process_extensions (WebKitWebContext *web_context,
                                   EphyEmbedShell   *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr (GVariant) user_data = NULL;

#if DEVELOPER_MODE
  webkit_web_context_set_web_process_extensions_directory (web_context, BUILD_ROOT "/embed/web-process-extension");
#else
  webkit_web_context_set_web_process_extensions_directory (web_context, EPHY_WEB_PROCESS_EXTENSIONS_DIR);
#endif

  user_data = g_variant_new ("(smsbv)",
                             priv->guid,
                             ephy_profile_dir_is_default () ? NULL : ephy_profile_dir (),
                             ephy_embed_shell_should_remember_passwords (shell),
                             priv->web_extension_initialization_data);
  webkit_web_context_set_web_process_extensions_initialization_user_data (web_context, g_steal_pointer (&user_data));
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
ephy_embed_shell_create_network_session_if_needed (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  if (priv->mode == EPHY_EMBED_SHELL_MODE_AUTOMATION)
    priv->network_session = g_object_ref (webkit_web_context_get_network_session_for_automation (priv->web_context));
  else if (priv->mode == EPHY_EMBED_SHELL_MODE_INCOGNITO)
    priv->network_session = webkit_network_session_new_ephemeral ();
  else {
    priv->network_session = webkit_network_session_new (ephy_profile_dir (), ephy_cache_dir ());
    webkit_network_session_set_persistent_credential_storage_enabled (priv->network_session, FALSE);
  }

  webkit_network_session_set_itp_enabled (priv->network_session,
                                          g_settings_get_boolean (EPHY_SETTINGS_WEB,
                                                                  EPHY_PREFS_WEB_ENABLE_ITP));
}

static void
ephy_embed_shell_create_web_context (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  priv->web_context = webkit_web_context_new ();

  if (priv->mode == EPHY_EMBED_SHELL_MODE_AUTOMATION)
    webkit_web_context_set_automation_allowed (priv->web_context, TRUE);
}

static void
download_started_cb (EphyEmbedShell *shell,
                     WebKitDownload *download)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  g_autoptr (EphyDownload) ephy_download = NULL;
  gboolean ephy_download_set;

  /* Is download locked down? */
  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK) ||
      ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_KIOSK) {
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
remember_passwords_setting_changed_cb (GSettings      *settings,
                                       char           *key,
                                       EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  webkit_web_context_send_message_to_all_extensions (priv->web_context,
                                                     webkit_user_message_new ("PasswordManager.SetShouldRememberPasswords",
                                                                              g_variant_new ("b", ephy_embed_shell_should_remember_passwords (shell))));
}

static void
enable_itp_setting_changed_cb (GSettings      *settings,
                               char           *key,
                               EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  webkit_network_session_set_itp_enabled (priv->network_session,
                                          g_settings_get_boolean (EPHY_SETTINGS_WEB,
                                                                  EPHY_PREFS_WEB_ENABLE_ITP));
}

static void
add_path_to_sandbox_or_die (const char       *path,
                            WebKitWebContext *context)
{
  g_autoptr (GError) error = NULL;

  ephy_ensure_dir_exists (path, &error);
  if (error)
    g_error ("Failed to create directory %s: %s", path, error->message);

  webkit_web_context_add_path_to_sandbox (context, path, TRUE);
}

static void
ephy_embed_shell_startup (GApplication *application)
{
  EphyEmbedShell *shell = EPHY_EMBED_SHELL (application);
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  WebKitWebsiteDataManager *data_manager;
  WebKitCookieManager *cookie_manager;
  g_autofree char *filename = NULL;

  G_APPLICATION_CLASS (ephy_embed_shell_parent_class)->startup (application);

  add_path_to_sandbox_or_die (ephy_profile_dir (), priv->web_context);
  add_path_to_sandbox_or_die (ephy_cache_dir (), priv->web_context);
  add_path_to_sandbox_or_die (ephy_config_dir (), priv->web_context);

#if DEVELOPER_MODE
  add_path_to_sandbox_or_die (BUILD_ROOT, priv->web_context);
#endif

  g_signal_connect_object (priv->web_context, "initialize-web-process-extensions",
                           G_CALLBACK (initialize_web_process_extensions),
                           shell, 0);

  g_signal_connect_object (priv->web_context, "initialize-notification-permissions",
                           G_CALLBACK (initialize_notification_permissions),
                           shell, 0);

  priv->password_manager = ephy_password_manager_new ();

  data_manager = webkit_network_session_get_website_data_manager (priv->network_session);
  webkit_website_data_manager_set_favicons_enabled (data_manager, TRUE);

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

  /* reader mode handler */
  priv->reader_handler = ephy_reader_handler_new ();
  webkit_web_context_register_uri_scheme (priv->web_context, EPHY_READER_SCHEME,
                                          (WebKitURISchemeRequestCallback)reader_request_cb,
                                          shell, NULL);

  /* ephy-resource handler */
  webkit_web_context_register_uri_scheme (priv->web_context, "ephy-resource",
                                          (WebKitURISchemeRequestCallback)ephy_resource_request_cb,
                                          NULL, NULL);
  webkit_security_manager_register_uri_scheme_as_secure (webkit_web_context_get_security_manager (priv->web_context),
                                                         "ephy-resource");

  /* Store cookies in moz-compatible SQLite format */
  if (!webkit_network_session_is_ephemeral (priv->network_session)) {
    cookie_manager = webkit_network_session_get_cookie_manager (priv->network_session);
    filename = g_build_filename (ephy_profile_dir (), "cookies.sqlite", NULL);
    webkit_cookie_manager_set_persistent_storage (cookie_manager, filename,
                                                  WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
  }

  g_signal_connect_object (priv->network_session, "download-started",
                           G_CALLBACK (download_started_cb), shell, G_CONNECT_SWAPPED);

  g_signal_connect_object (EPHY_SETTINGS_WEB, "changed::enable-itp",
                           G_CALLBACK (enable_itp_setting_changed_cb), shell, 0);

  if (!is_private_profile_mode (shell)) {
    g_signal_connect_object (EPHY_SETTINGS_WEB, "changed::remember-passwords",
                             G_CALLBACK (remember_passwords_setting_changed_cb), shell, 0);
  }
}

static void
ephy_embed_shell_shutdown (GApplication *application)
{
  G_APPLICATION_CLASS (ephy_embed_shell_parent_class)->shutdown (application);

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

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->constructed (object);

  shell = EPHY_EMBED_SHELL (object);
  priv = ephy_embed_shell_get_instance_private (shell);
  priv->guid = g_dbus_generate_guid ();

  ephy_embed_shell_create_web_context (shell);
  ephy_embed_shell_create_network_session_if_needed (shell);

  priv->permissions_manager = ephy_permissions_manager_new ();
  priv->filters_manager = ephy_filters_manager_new (NULL);

  ephy_embed_shell_set_web_extension_initialization_data (shell, g_variant_new ("a{sv}", NULL));
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
                       NULL, NULL,
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
                  0, NULL, NULL, NULL,
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

  /**
   * EphyEmbedShell::password-form-submitted:
   * @shell: the #EphyEmbedShell
   * @data: An #EphyPasswordRequestData containing information regarding the password
   *
   * Emitted when a Password Save request has been received, and the UI should
   * provide the user with a prompt of some form
   */
  signals[PASSWORD_FORM_SUBMITTED] =
    g_signal_new ("password-form-submitted",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  /**
   * EphyEmbedShell::autofill:
   * @shell: the #EphyEmbedShell
   * @page_id: the identifier of the web page created
   * @css_selector: css selector of input element
   * @is_fillable_element: is input element fillable
   * @has_personal_fields: does input element's form has personal fields
   * @has_card_fields: does input element's form has credit card fields
   * @element_x: x position of input element
   * @element_y: y position of input element
   * @element_width: width on input element
   * @element_height: height of input element
   *
   * Emitted when the user double clicks on:
   * 1. A fillable input element
   * 2. An input element where its form has fillable fields
   */
  signals[AUTOFILL_SIGNAL] =
    g_signal_new ("autofill",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 9,
                  G_TYPE_UINT64,
                  G_TYPE_STRING,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  G_TYPE_UINT64,
                  G_TYPE_UINT64,
                  G_TYPE_UINT64,
                  G_TYPE_UINT64);
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

EphyFiltersManager *
ephy_embed_shell_get_filters_manager (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  return priv->filters_manager;
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

WebKitNetworkSession *
ephy_embed_shell_get_network_session (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  return priv->network_session;
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

WebKitFaviconDatabase *
ephy_embed_shell_get_favicon_database (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);
  WebKitWebsiteDataManager *manager;

  manager = webkit_network_session_get_website_data_manager (priv->network_session);
  return webkit_website_data_manager_get_favicon_database (manager);
}

void
ephy_embed_shell_register_ucm (EphyEmbedShell           *shell,
                               WebKitUserContentManager *ucm)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  /* Warning: message handlers connected here should use the private script
   * world priv->guid to make them unavailable to web content. If a message
   * handler needs to be temporarily accessible to web content in order to
   * implement internal Epiphany pages, register it using
   * ephy_web_view_register_message_handler() to ensure it is unregistered
   * before any subsequent page load.
   */

  webkit_user_content_manager_register_script_message_handler (ucm,
                                                               "overview",
                                                               priv->guid);
  g_signal_connect_object (ucm, "script-message-received::overview",
                           G_CALLBACK (web_process_extension_overview_message_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler (ucm,
                                                               "passwordFormFocused",
                                                               priv->guid);
  g_signal_connect_object (ucm, "script-message-received::passwordFormFocused",
                           G_CALLBACK (web_process_extension_password_form_focused_message_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler (ucm,
                                                               "passwordManagerSave",
                                                               priv->guid);
  g_signal_connect_object (ucm, "script-message-received::passwordManagerSave",
                           G_CALLBACK (web_process_extension_password_manager_save_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler (ucm,
                                                               "passwordManagerRequestSave",
                                                               priv->guid);
  g_signal_connect_object (ucm, "script-message-received::passwordManagerRequestSave",
                           G_CALLBACK (web_process_extension_password_manager_request_save_received_cb),
                           shell, 0);

  webkit_user_content_manager_register_script_message_handler (ucm,
                                                               "autofillAskUser",
                                                               priv->guid);
  g_signal_connect_object (ucm, "script-message-received::autofillAskUser",
                           G_CALLBACK (web_process_extension_autofill_askuser_received_cb),
                           shell, 0);

  /* Filter Manager */
  g_signal_connect_object (priv->filters_manager, "filters-disabled",
                           G_CALLBACK (webkit_user_content_manager_remove_all_filters),
                           ucm,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->filters_manager, "filter-ready",
                           G_CALLBACK (webkit_user_content_manager_add_filter),
                           ucm,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->filters_manager, "filter-removed",
                           G_CALLBACK (webkit_user_content_manager_remove_filter_by_id),
                           ucm,
                           G_CONNECT_SWAPPED);

  /* User Scripts */
  ephy_embed_prefs_apply_user_style (ucm);
  ephy_embed_prefs_apply_user_javascript (ucm);
}

void
ephy_embed_shell_unregister_ucm (EphyEmbedShell           *shell,
                                 WebKitUserContentManager *ucm)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  webkit_user_content_manager_unregister_script_message_handler (ucm,
                                                                 "overview",
                                                                 priv->guid);
  webkit_user_content_manager_unregister_script_message_handler (ucm,
                                                                 "passwordFormFocused",
                                                                 priv->guid);
  webkit_user_content_manager_unregister_script_message_handler (ucm,
                                                                 "passwordManagerSave",
                                                                 priv->guid);
  webkit_user_content_manager_unregister_script_message_handler (ucm,
                                                                 "passwordManagerRequestSave",
                                                                 priv->guid);
  webkit_user_content_manager_unregister_script_message_handler (ucm,
                                                                 "autofillAskUser",
                                                                 priv->guid);
}

void
ephy_embed_shell_set_web_extension_initialization_data (EphyEmbedShell *shell,
                                                        GVariant       *data)
{
  EphyEmbedShellPrivate *priv = ephy_embed_shell_get_instance_private (shell);

  g_clear_pointer (&priv->web_extension_initialization_data, g_variant_unref);
  priv->web_extension_initialization_data = g_variant_ref_sink (data);
}

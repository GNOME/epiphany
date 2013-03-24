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
#include "ephy-download.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-private.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-encodings.h"
#include "ephy-file-helpers.h"
#include "ephy-history-service.h"
#include "ephy-profile-utils.h"
#include "ephy-request-about.h"
#include "ephy-settings.h"
#include "ephy-snapshot-service.h"
#include "ephy-web-extension.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#define PAGE_SETUP_FILENAME "page-setup-gtk.ini"
#define PRINT_SETTINGS_FILENAME "print-settings.ini"
#define NSPLUGINWRAPPER_SETUP "/usr/bin/mozilla-plugin-config"

#define EPHY_EMBED_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellPrivate))

struct _EphyEmbedShellPrivate
{
  EphyHistoryService *global_history_service;
  GList *downloads;
  EphyEncodings *encodings;
  GtkPageSetup *page_setup;
  GtkPrintSettings *print_settings;
  EphyEmbedShellMode mode;
  EphyFrecentStore *frecent_store;
  GDBusProxy *web_extension;
  guint web_extension_watch_name_id;
  guint web_extension_form_auth_save_signal_id;
};

enum
{
  DOWNLOAD_ADDED,
  DOWNLOAD_REMOVED,
  PREPARE_CLOSE,
  RESTORED_WINDOW,
  WEB_VIEW_CREATED,
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

  g_clear_object (&priv->encodings);
  g_clear_object (&priv->page_setup);
  g_clear_object (&priv->print_settings);
  g_clear_object (&priv->frecent_store);
  g_clear_object (&priv->global_history_service);

  if (priv->web_extension_watch_name_id > 0) {
    g_bus_unwatch_name (priv->web_extension_watch_name_id);
    priv->web_extension_watch_name_id = 0;
  }

  if (priv->web_extension_form_auth_save_signal_id > 0) {
    g_dbus_connection_signal_unsubscribe (g_dbus_proxy_get_connection (priv->web_extension),
                                          priv->web_extension_form_auth_save_signal_id);
    priv->web_extension_form_auth_save_signal_id = 0;
  }

  g_clear_object (&priv->web_extension);

  if (priv->downloads != NULL) {
    LOG ("Destroying downloads list");
    g_list_free_full (priv->downloads, (GDestroyNotify)g_object_unref);
    priv->downloads = NULL;
  }

  G_OBJECT_CLASS (ephy_embed_shell_parent_class)->dispose (object);
}

static void
web_extension_form_auth_save_requested (GDBusConnection *connection,
                                        const char *sender_name,
                                        const char *object_path,
                                        const char *interface_name,
                                        const char *signal_name,
                                        GVariant *parameters,
                                        EphyEmbedShell *shell)
{
  guint request_id;
  guint64 page_id;
  const char *hostname;
  const char *username;

  g_variant_get (parameters, "(ut&s&s)", &request_id, &page_id, &hostname, &username);
  g_signal_emit (shell, signals[FORM_AUTH_DATA_SAVE_REQUESTED], 0,
                 request_id, page_id, hostname, username);
}

static void
web_extension_proxy_created_cb (GDBusProxy *proxy,
                                GAsyncResult *result,
                                EphyEmbedShell *shell)
{
  GError *error = NULL;

  shell->priv->web_extension = g_dbus_proxy_new_finish (result, &error);
  if (!shell->priv->web_extension) {
    g_warning ("Error creating web extension proxy: %s\n", error->message);
    g_error_free (error);
  } else {
    shell->priv->web_extension_form_auth_save_signal_id =
      g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (shell->priv->web_extension),
                                          NULL,
                                          EPHY_WEB_EXTENSION_INTERFACE,
                                         "FormAuthDataSaveConfirmationRequired",
                                          EPHY_WEB_EXTENSION_OBJECT_PATH,
                                          NULL,
                                          G_DBUS_SIGNAL_FLAGS_NONE,
                                          (GDBusSignalCallback)web_extension_form_auth_save_requested,
                                          shell,
                                          NULL);
  }
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           EphyEmbedShell *shell)
{
  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                    G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                    G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                    NULL,
                    name,
                    EPHY_WEB_EXTENSION_OBJECT_PATH,
                    EPHY_WEB_EXTENSION_INTERFACE,
                    NULL,
                    (GAsyncReadyCallback)web_extension_proxy_created_cb,
                    shell);
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           EphyEmbedShell *shell)
{
  g_clear_object (&shell->priv->web_extension);
}

static void
ephy_embed_shell_watch_web_extension (EphyEmbedShell *shell)
{
  char *service_name;

  service_name = g_strdup_printf ("%s-%u", EPHY_WEB_EXTENSION_SERVICE_NAME, getpid ());
  shell->priv->web_extension_watch_name_id =
    g_bus_watch_name (G_BUS_TYPE_SESSION,
                      service_name,
                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                      (GBusNameAppearedCallback) web_extension_appeared_cb,
                      (GBusNameVanishedCallback) web_extension_vanished_cb,
                      shell, NULL);
  g_free (service_name);
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
    shell->priv->global_history_service = ephy_history_service_new (filename);
    g_free (filename);
    g_return_val_if_fail (shell->priv->global_history_service, NULL);
  }

  return G_OBJECT (shell->priv->global_history_service);
}

static GdkPixbuf *
ephy_embed_shell_get_overview_icon (const char *icon_name)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  const char *filename;

  filename = ephy_file (icon_name);
  pixbuf = gdk_pixbuf_new_from_file (filename, &error);

  if (!pixbuf) {
    g_warning ("Couldn't load icon: %s", error->message);
    g_error_free (error);
  }

  return pixbuf;
}

/**
 * ephy_embed_shell_get_frecent_store:
 * @shell: a #EphyEmbedShell
 *
 * Gets the #EphyFrecentStore in the shell. This can be used
 * by EphyOverview implementors.
 *
 * Returns: (transfer none): a #EphyFrecentStore
 **/
EphyFrecentStore *
ephy_embed_shell_get_frecent_store (EphyEmbedShell *shell)
{
  GdkPixbuf *default_icon;
  GdkPixbuf *frame;

  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

  if (shell->priv->frecent_store == NULL) {
    shell->priv->frecent_store = ephy_frecent_store_new ();
    default_icon = ephy_embed_shell_get_overview_icon ("missing-thumbnail.png");
    frame = ephy_embed_shell_get_overview_icon ("thumbnail-frame.png");
    g_object_set (shell->priv->frecent_store,
                  "history-service",
                  ephy_embed_shell_get_global_history_service (shell),
                  "history-length", 10,
                  "default-icon", default_icon,
                  "icon-frame", frame,
                  NULL);
    g_object_unref (default_icon);
    g_object_unref (frame);
  }

  return shell->priv->frecent_store;
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
complete_about_request_for_contents (WebKitURISchemeRequest *request,
                                     gchar *data,
                                     gsize data_length)
{
  GInputStream *stream;

  stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
  webkit_uri_scheme_request_finish (request, stream, data_length, "text/html");
  g_object_unref (stream);
}

static void
get_plugins_cb (WebKitWebContext *web_context,
                GAsyncResult *result,
                WebKitURISchemeRequest *request)
{
  GList *plugins;
  GString *data_str;
  gsize data_length;

  data_str = g_string_new("<html>");
  plugins = webkit_web_context_get_plugins_finish (web_context, result, NULL);
  _ephy_about_handler_handle_plugins (data_str, plugins);
  g_string_append (data_str, "</html>");

  data_length = data_str->len;
  complete_about_request_for_contents (request, g_string_free (data_str, FALSE), data_length);
  g_object_unref (request);
}

static void
about_request_cb (WebKitURISchemeRequest *request,
                  gpointer user_data)
{
  const gchar *path;

  path = webkit_uri_scheme_request_get_path (request);
  if (!g_strcmp0 (path, "plugins")) {
    /* Plugins API is async in WebKit2 */
    webkit_web_context_get_plugins (webkit_web_context_get_default (),
                                    NULL,
                                    (GAsyncReadyCallback)get_plugins_cb,
                                    g_object_ref (request));
  } else {
    GString *contents;
    gsize data_length;

    contents = ephy_about_handler_handle (path);
    data_length = contents->len;
    complete_about_request_for_contents (request, g_string_free (contents, FALSE), data_length);
  }
}

static void
ephy_embed_shell_init (EphyEmbedShell *shell)
{
  WebKitWebContext *web_context;
  WebKitCookieManager *cookie_manager;
  char *filename;
  char *cookie_policy;

  shell->priv = EPHY_EMBED_SHELL_GET_PRIVATE (shell);

  /* globally accessible singleton */
  g_assert (embed_shell == NULL);
  embed_shell = shell;

  shell->priv->downloads = NULL;

  /* Initialise nspluginwrapper's plugins if available. */
  if (g_file_test (NSPLUGINWRAPPER_SETUP, G_FILE_TEST_EXISTS) != FALSE)
    g_spawn_command_line_sync (NSPLUGINWRAPPER_SETUP, NULL, NULL, NULL, NULL);

  web_context = webkit_web_context_get_default ();

  /* Store cookies in moz-compatible SQLite format */
  cookie_manager = webkit_web_context_get_cookie_manager (web_context);
  filename = g_build_filename (ephy_dot_dir (), "cookies.sqlite", NULL);
  webkit_cookie_manager_set_persistent_storage (cookie_manager, filename,
                                                WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
  g_free (filename);

  cookie_policy = g_settings_get_string (EPHY_SETTINGS_WEB,
                                         EPHY_PREFS_WEB_COOKIES_POLICY);
  ephy_embed_prefs_set_cookie_accept_policy (cookie_manager, cookie_policy);
  g_free (cookie_policy);

  /* about: URIs handler */
  webkit_web_context_register_uri_scheme (web_context,
                                          EPHY_ABOUT_SCHEME,
                                          about_request_cb,
                                          NULL, NULL);

  ephy_embed_shell_watch_web_extension (shell);
}

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_embed_shell_dispose;
  object_class->set_property = ephy_embed_shell_set_property;
  object_class->get_property = ephy_embed_shell_get_property;

  object_properties[PROP_MODE] =
    g_param_spec_enum ("mode",
                       "Mode",
                       "The  global mode for this instance of Epiphany .",
                       EPHY_TYPE_EMBED_SHELL_MODE,
                       EPHY_EMBED_SHELL_MODE_BROWSER,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                       G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY);
  
  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     object_properties);
  
/**
 * EphyEmbed::download-added:
 * @shell: the #EphyEmbedShell
 * @download: the #EphyDownload added
 *
 * Emitted when a #EphyDownload has been added to the global watch list of
 * @shell, via ephy_embed_shell_add_download.
 **/
  signals[DOWNLOAD_ADDED] =
    g_signal_new ("download-added",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EphyEmbedShellClass, download_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, EPHY_TYPE_DOWNLOAD);
  
/**
 * EphyEmbed::download-removed:
 * @shell: the #EphyEmbedShell
 * @download: the #EphyDownload being removed
 *
 * Emitted when a #EphyDownload has been removed from the global watch list of
 * @shell, via ephy_embed_shell_remove_download.
 **/
  signals[DOWNLOAD_REMOVED] =
    g_signal_new ("download-removed",
                  EPHY_TYPE_EMBED_SHELL,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EphyEmbedShellClass, download_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, EPHY_TYPE_DOWNLOAD);

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

  /*
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
   **/
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
 * ephy_embed_shell_set_print_gettings:
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
 * ephy_embed_shell_get_downloads:
 * @shell: the #EphyEmbedShell
 *
 * Gets the global #GList object listing active downloads.
 *
 * Returns: (transfer none) (element-type EphyDownload): a #GList object
 **/
GList *
ephy_embed_shell_get_downloads (EphyEmbedShell *shell)
{
  EphyEmbedShellPrivate *priv;

  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);
  priv = shell->priv;

  return priv->downloads;
}

void
ephy_embed_shell_add_download (EphyEmbedShell *shell, EphyDownload *download)
{
  EphyEmbedShellPrivate *priv;

  g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));

  priv = shell->priv;
  priv->downloads = g_list_prepend (priv->downloads, download);

  g_signal_emit_by_name (shell, "download-added", download, NULL);
}

void
ephy_embed_shell_remove_download (EphyEmbedShell *shell, EphyDownload *download)
{
  EphyEmbedShellPrivate *priv;

  g_return_if_fail (EPHY_IS_EMBED_SHELL (shell));

  priv = shell->priv;
  priv->downloads = g_list_remove (priv->downloads, download);

  g_signal_emit_by_name (shell, "download-removed", download, NULL);
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
 * ephy_embed_shell_launch_application:
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

GDBusProxy *
ephy_embed_shell_get_web_extension_proxy (EphyEmbedShell *shell)
{
  g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

  return shell->priv->web_extension;
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
  webkit_web_context_clear_cache (webkit_web_context_get_default ());
}

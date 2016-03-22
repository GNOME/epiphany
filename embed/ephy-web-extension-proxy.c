/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "ephy-web-extension-proxy.h"

#include "ephy-web-extension-names.h"
#include "ephy-history-service.h"

struct _EphyWebExtensionProxy {
  GObject parent_instance;

  GCancellable *cancellable;
  GDBusProxy *proxy;
  GDBusConnection *connection;

  guint page_created_signal_id;
};

enum {
  PAGE_CREATED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyWebExtensionProxy, ephy_web_extension_proxy, G_TYPE_OBJECT)

static void
ephy_web_extension_proxy_dispose (GObject *object)
{
  EphyWebExtensionProxy *web_extension = EPHY_WEB_EXTENSION_PROXY (object);

  if (web_extension->page_created_signal_id > 0) {
    g_dbus_connection_signal_unsubscribe (web_extension->connection,
                                          web_extension->page_created_signal_id);
    web_extension->page_created_signal_id = 0;
  }

  g_clear_object (&web_extension->cancellable);
  g_clear_object (&web_extension->proxy);
  g_clear_object (&web_extension->connection);

  G_OBJECT_CLASS (ephy_web_extension_proxy_parent_class)->dispose (object);
}

static void
ephy_web_extension_proxy_init (EphyWebExtensionProxy *web_extension)
{
}

static void
ephy_web_extension_proxy_class_init (EphyWebExtensionProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_web_extension_proxy_dispose;

  /**
   * EphyWebExtensionProxy::page-created:
   * @web_extension: the #EphyWebExtensionProxy
   * @page_id: the identifier of the web page created
   *
   * Emitted when a web page is created in the web process.
   */
  signals[PAGE_CREATED] =
    g_signal_new ("page-created",
                  EPHY_TYPE_WEB_EXTENSION_PROXY,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT64);
}

static void
web_extension_page_created (GDBusConnection       *connection,
                            const char            *sender_name,
                            const char            *object_path,
                            const char            *interface_name,
                            const char            *signal_name,
                            GVariant              *parameters,
                            EphyWebExtensionProxy *web_extension)
{
  guint64 page_id;
  g_variant_get (parameters, "(t)", &page_id);
  g_signal_emit (web_extension, signals[PAGE_CREATED], 0, page_id);
}

static void
web_extension_proxy_created_cb (GDBusProxy            *proxy,
                                GAsyncResult          *result,
                                EphyWebExtensionProxy *web_extension)
{
  GError *error = NULL;

  web_extension->proxy = g_dbus_proxy_new_finish (result, &error);
  if (!web_extension->proxy) {
    g_warning ("Error creating web extension proxy: %s", error->message);
    g_error_free (error);

    /* Attempt to trigger connection_closed_cb, which will destroy us, and ensure that
     * that EphyEmbedShell will remove us from its extensions list.
     */
    g_dbus_connection_close (web_extension->connection,
                             web_extension->cancellable,
                             NULL /* GAsyncReadyCallback */,
                             NULL);
    return;
  }

  web_extension->page_created_signal_id =
    g_dbus_connection_signal_subscribe (web_extension->connection,
                                        NULL,
                                        EPHY_WEB_EXTENSION_INTERFACE,
                                        "PageCreated",
                                        EPHY_WEB_EXTENSION_OBJECT_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        (GDBusSignalCallback)web_extension_page_created,
                                        web_extension,
                                        NULL);
}

static void
connection_closed_cb (GDBusConnection       *connection,
                      gboolean               remote_peer_vanished,
                      GError                *error,
                      EphyWebExtensionProxy *web_extension)
{
  if (error) {
    if (!remote_peer_vanished)
      g_warning ("Unexpectedly lost connection to web extension: %s", error->message);
  }

  g_object_unref (web_extension);
}

EphyWebExtensionProxy *
ephy_web_extension_proxy_new (GDBusConnection *connection)
{
  EphyWebExtensionProxy *web_extension;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

  web_extension = g_object_new (EPHY_TYPE_WEB_EXTENSION_PROXY, NULL);

  g_signal_connect (connection, "closed",
                    G_CALLBACK (connection_closed_cb), web_extension);

  web_extension->cancellable = g_cancellable_new ();
  web_extension->connection = g_object_ref (connection);

  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                    NULL,
                    NULL,
                    EPHY_WEB_EXTENSION_OBJECT_PATH,
                    EPHY_WEB_EXTENSION_INTERFACE,
                    web_extension->cancellable,
                    (GAsyncReadyCallback)web_extension_proxy_created_cb,
                    web_extension);

  return web_extension;
}

void
ephy_web_extension_proxy_form_auth_data_save_confirmation_response (EphyWebExtensionProxy *web_extension,
                                                                    guint                  request_id,
                                                                    gboolean               response)
{
  g_return_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension));

  if (!web_extension->proxy)
    return;

  g_dbus_proxy_call (web_extension->proxy,
                     "FormAuthDataSaveConfirmationResponse",
                     g_variant_new ("(ub)", request_id, response),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

static void
has_modified_forms_cb (GDBusProxy   *proxy,
                       GAsyncResult *result,
                       GTask        *task)
{
  GVariant *return_value;
  gboolean retval = FALSE;

  return_value = g_dbus_proxy_call_finish (proxy, result, NULL);
  if (return_value) {
    g_variant_get (return_value, "(b)", &retval);
    g_variant_unref (return_value);
  }

  g_task_return_boolean (task, retval);
  g_object_unref (task);
}

void
ephy_web_extension_proxy_web_page_has_modified_forms (EphyWebExtensionProxy *web_extension,
                                                      guint64                page_id,
                                                      GCancellable          *cancellable,
                                                      GAsyncReadyCallback    callback,
                                                      gpointer               user_data)
{
  GTask *task;

  g_return_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension));

  task = g_task_new (web_extension, cancellable, callback, user_data);

  if (web_extension->proxy) {
    g_dbus_proxy_call (web_extension->proxy,
                       "HasModifiedForms",
                       g_variant_new ("(t)", page_id),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       web_extension->cancellable,
                       (GAsyncReadyCallback)has_modified_forms_cb,
                       g_object_ref (task));
  } else {
    g_task_return_boolean (task, FALSE);
  }

  g_object_unref (task);
}

gboolean
ephy_web_extension_proxy_web_page_has_modified_forms_finish (EphyWebExtensionProxy *web_extension,
                                                             GAsyncResult          *result,
                                                             GError               **error)
{
  g_return_val_if_fail (g_task_is_valid (result, web_extension), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
get_best_web_app_icon_cb (GDBusProxy   *proxy,
                          GAsyncResult *result,
                          GTask        *task)
{
  GVariant *retval;
  GError *error = NULL;

  retval = g_dbus_proxy_call_finish (proxy, result, &error);
  if (!retval) {
    g_task_return_error (task, error);
  } else {
    g_task_return_pointer (task, retval, (GDestroyNotify)g_variant_unref);
  }
  g_object_unref (task);
}

void
ephy_web_extension_proxy_get_best_web_app_icon (EphyWebExtensionProxy *web_extension,
                                                guint64                page_id,
                                                const char            *base_uri,
                                                GCancellable          *cancellable,
                                                GAsyncReadyCallback    callback,
                                                gpointer               user_data)
{
  GTask *task;

  g_return_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension));

  task = g_task_new (web_extension, cancellable, callback, user_data);

  if (web_extension->proxy) {
    g_dbus_proxy_call (web_extension->proxy,
                       "GetBestWebAppIcon",
                       g_variant_new ("(ts)", page_id, base_uri),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       web_extension->cancellable,
                       (GAsyncReadyCallback)get_best_web_app_icon_cb,
                       g_object_ref (task));
  } else {
    g_task_return_boolean (task, FALSE);
  }

  g_object_unref (task);
}

gboolean
ephy_web_extension_proxy_get_best_web_app_icon_finish (EphyWebExtensionProxy *web_extension,
                                                       GAsyncResult          *result,
                                                       gboolean              *icon_result,
                                                       char                 **icon_uri,
                                                       char                 **icon_color,
                                                       GError               **error)
{
  GVariant *variant;
  GTask *task = G_TASK (result);

  g_return_val_if_fail (g_task_is_valid (result, web_extension), FALSE);

  variant = g_task_propagate_pointer (task, error);
  if (!variant)
    return FALSE;

  g_variant_get (variant, "(bss)", icon_result, icon_uri, icon_color);
  g_variant_unref (variant);

  return TRUE;
}

static void
get_web_app_title_cb (GDBusProxy   *proxy,
                      GAsyncResult *result,
                      GTask        *task)
{
  GVariant *retval;
  GError *error = NULL;

  retval = g_dbus_proxy_call_finish (proxy, result, &error);
  if (!retval) {
    g_task_return_error (task, error);
  } else {
    char *title;

    g_variant_get (retval, "(s)", &title);
    g_task_return_pointer (task, title, (GDestroyNotify)g_free);
    g_variant_unref (retval);
  }
  g_object_unref (task);
}

void
ephy_web_extension_proxy_get_web_app_title (EphyWebExtensionProxy *web_extension,
                                            guint64                page_id,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data)
{
  GTask *task;

  g_return_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension));

  task = g_task_new (web_extension, cancellable, callback, user_data);

  if (web_extension->proxy) {
    g_dbus_proxy_call (web_extension->proxy,
                       "GetWebAppTitle",
                       g_variant_new ("(t)", page_id),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       web_extension->cancellable,
                       (GAsyncReadyCallback)get_web_app_title_cb,
                       g_object_ref (task));
  } else {
    g_task_return_pointer (task, NULL, NULL);
  }

  g_object_unref (task);
}

char *
ephy_web_extension_proxy_get_web_app_title_finish (EphyWebExtensionProxy *web_extension,
                                                   GAsyncResult          *result,
                                                   GError               **error)
{
  g_return_val_if_fail (g_task_is_valid (result, web_extension), FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
ephy_web_extension_proxy_history_set_urls (EphyWebExtensionProxy *web_extension,
                                           GList                 *urls)
{
  GList *l;
  GVariantBuilder builder;

  if (!web_extension->proxy)
    return;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  for (l = urls; l; l = g_list_next (l)) {
    EphyHistoryURL *url = (EphyHistoryURL *)l->data;

    g_variant_builder_add (&builder, "(ss)", url->url, url->title);
  }

  g_dbus_proxy_call (web_extension->proxy,
                     "HistorySetURLs",
                     g_variant_new ("(@a(ss))", g_variant_builder_end (&builder)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_set_url_thumbnail (EphyWebExtensionProxy *web_extension,
                                                    const char            *url,
                                                    const char            *path)
{
  if (!web_extension->proxy)
    return;

  g_dbus_proxy_call (web_extension->proxy,
                     "HistorySetURLThumbnail",
                     g_variant_new ("(ss)", url, path),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_set_url_title (EphyWebExtensionProxy *web_extension,
                                                const char            *url,
                                                const char            *title)
{
  if (!web_extension->proxy)
    return;

  g_dbus_proxy_call (web_extension->proxy,
                     "HistorySetURLTitle",
                     g_variant_new ("(ss)", url, title),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_delete_url (EphyWebExtensionProxy *web_extension,
                                             const char            *url)
{
  if (!web_extension->proxy)
    return;

  g_dbus_proxy_call (web_extension->proxy,
                     "HistoryDeleteURL",
                     g_variant_new ("(s)", url),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_delete_host (EphyWebExtensionProxy *web_extension,
                                              const char            *host)
{
  if (!web_extension->proxy)
    return;

  g_dbus_proxy_call (web_extension->proxy,
                     "HistoryDeleteHost",
                     g_variant_new ("(s)", host),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_clear (EphyWebExtensionProxy *web_extension)
{
  if (!web_extension->proxy)
    return;

  g_dbus_proxy_call (web_extension->proxy,
                     "HistoryClear",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

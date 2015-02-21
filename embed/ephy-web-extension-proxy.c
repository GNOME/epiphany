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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <config.h>
#include "ephy-web-extension-proxy.h"

#include "ephy-web-extension-names.h"
#include "ephy-history-service.h"

struct _EphyWebExtensionProxyPrivate
{
  GDBusProxy *proxy;
  gchar *name_owner;
  GCancellable *cancellable;
  guint watch_name_id;
};

G_DEFINE_TYPE (EphyWebExtensionProxy, ephy_web_extension_proxy, G_TYPE_OBJECT)

static void
ephy_web_extension_proxy_finalize (GObject *object)
{
  EphyWebExtensionProxyPrivate *priv = EPHY_WEB_EXTENSION_PROXY (object)->priv;

  g_clear_object (&priv->proxy);

  G_OBJECT_CLASS (ephy_web_extension_proxy_parent_class)->finalize (object);
}

static void
ephy_web_extension_proxy_dispose (GObject *object)
{
  EphyWebExtensionProxyPrivate *priv = EPHY_WEB_EXTENSION_PROXY (object)->priv;

  g_clear_object (&priv->cancellable);

  if (priv->watch_name_id > 0) {
    g_bus_unwatch_name (priv->watch_name_id);
    priv->watch_name_id = 0;
  }

  g_clear_pointer (&priv->name_owner, g_free);

  G_OBJECT_CLASS (ephy_web_extension_proxy_parent_class)->dispose (object);
}

static void
ephy_web_extension_proxy_init (EphyWebExtensionProxy *web_extension)
{
  web_extension->priv = G_TYPE_INSTANCE_GET_PRIVATE (web_extension, EPHY_TYPE_WEB_EXTENSION_PROXY, EphyWebExtensionProxyPrivate);
}

static void
ephy_web_extension_proxy_class_init (EphyWebExtensionProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_web_extension_proxy_finalize;
  object_class->dispose = ephy_web_extension_proxy_dispose;

  g_type_class_add_private (object_class, sizeof (EphyWebExtensionProxyPrivate));
}

static void
web_extension_proxy_created_cb (GDBusProxy *proxy,
                                GAsyncResult *result,
                                EphyWebExtensionProxy *web_extension)
{
  GError *error = NULL;

  web_extension->priv->proxy = g_dbus_proxy_new_finish (result, &error);
  if (!web_extension->priv->proxy) {
    g_warning ("Error creating web extension proxy: %s\n", error->message);
    g_error_free (error);
  }

  g_object_unref (web_extension);
}

static void
web_extension_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           EphyWebExtensionProxy *web_extension)
{
  web_extension->priv->name_owner = g_strdup (name_owner);
  web_extension->priv->cancellable = g_cancellable_new ();
  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                    G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                    G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                    NULL,
                    name,
                    EPHY_WEB_EXTENSION_OBJECT_PATH,
                    EPHY_WEB_EXTENSION_INTERFACE,
                    web_extension->priv->cancellable,
                    (GAsyncReadyCallback)web_extension_proxy_created_cb,
                    /* Ref here because the web process could crash, triggering
                     * web_extension_vanished_cb() before this finishes. */
                    g_object_ref (web_extension));
}

static void
web_extension_vanished_cb (GDBusConnection *connection,
                           const gchar *name,
                           EphyWebExtensionProxy *web_extension)
{
  if (web_extension->priv->name_owner)
    g_object_unref (web_extension);
}

static void
ephy_web_extension_proxy_watch_name (EphyWebExtensionProxy *web_extension,
                                     GDBusConnection* bus,
                                     const char *service_name)
{
  EphyWebExtensionProxyPrivate *priv = web_extension->priv;

  priv->watch_name_id =
    g_bus_watch_name_on_connection (bus,
                                    service_name,
                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                                    (GBusNameAppearedCallback)web_extension_appeared_cb,
                                    (GBusNameVanishedCallback)web_extension_vanished_cb,
                                    web_extension,
                                    NULL);
}

EphyWebExtensionProxy *
ephy_web_extension_proxy_new (GDBusConnection *bus,
                              const char *service_name)
{
  EphyWebExtensionProxy *web_extension;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (bus), NULL);
  g_return_val_if_fail (service_name != NULL, NULL);

  web_extension = g_object_new (EPHY_TYPE_WEB_EXTENSION_PROXY, NULL);
  ephy_web_extension_proxy_watch_name (web_extension, bus, service_name);

  return web_extension;
}

const char *
ephy_web_extension_proxy_get_name_owner (EphyWebExtensionProxy *web_extension)
{
  g_return_val_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension), NULL);

  return web_extension->priv->name_owner;
}

void
ephy_web_extension_proxy_form_auth_data_save_confirmation_response (EphyWebExtensionProxy *web_extension,
                                                                    guint request_id,
                                                                    gboolean response)
{
  g_return_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension));

  if (!web_extension->priv->proxy)
    return;

  g_dbus_proxy_call (web_extension->priv->proxy,
                     "FormAuthDataSaveConfirmationResponse",
                     g_variant_new ("(ub)", request_id, response),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->priv->cancellable,
                     NULL, NULL);
}

static void
has_modified_forms_cb (GDBusProxy *proxy,
                       GAsyncResult *result,
                       GTask *task)
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
                                                      guint64 page_id,
                                                      GCancellable *cancellable,
                                                      GAsyncReadyCallback callback,
                                                      gpointer user_data)
{
  GTask *task;

  g_return_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension));

  task = g_task_new (web_extension, cancellable, callback, user_data);

  if (web_extension->priv->proxy) {
    g_dbus_proxy_call (web_extension->priv->proxy,
                       "HasModifiedForms",
                       g_variant_new ("(t)", page_id),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       web_extension->priv->cancellable,
                       (GAsyncReadyCallback)has_modified_forms_cb,
                       g_object_ref (task));
  } else {
    g_task_return_boolean (task, FALSE);
  }

  g_object_unref (task);
}

gboolean
ephy_web_extension_proxy_web_page_has_modified_forms_finish (EphyWebExtensionProxy *web_extension,
                                                             GAsyncResult *result,
                                                             GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, web_extension), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
get_best_web_app_icon_cb (GDBusProxy *proxy,
                          GAsyncResult *result,
                          GTask *task)
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
                                                guint64 page_id,
                                                const char *base_uri,
                                                GCancellable *cancellable,
                                                GAsyncReadyCallback callback,
                                                gpointer user_data)
{
  GTask *task;

  g_return_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension));

  task = g_task_new (web_extension, cancellable, callback, user_data);

  if (web_extension->priv->proxy) {
    g_dbus_proxy_call (web_extension->priv->proxy,
                       "GetBestWebAppIcon",
                       g_variant_new("(ts)", page_id, base_uri),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       web_extension->priv->cancellable,
                       (GAsyncReadyCallback)get_best_web_app_icon_cb,
                       g_object_ref (task));
  } else {
    g_task_return_boolean (task, FALSE);
  }

  g_object_unref (task);
}

gboolean
ephy_web_extension_proxy_get_best_web_app_icon_finish (EphyWebExtensionProxy *web_extension,
                                                       GAsyncResult *result,
                                                       gboolean *icon_result,
                                                       char **icon_uri,
                                                       char **icon_color,
                                                       GError **error)
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
get_web_app_title_cb (GDBusProxy *proxy,
                      GAsyncResult *result,
                      GTask *task)
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
                                            guint64 page_id,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
  GTask *task;

  g_return_if_fail (EPHY_IS_WEB_EXTENSION_PROXY (web_extension));

  task = g_task_new (web_extension, cancellable, callback, user_data);

  if (web_extension->priv->proxy) {
    g_dbus_proxy_call (web_extension->priv->proxy,
                       "GetWebAppTitle",
                       g_variant_new("(t)", page_id),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       web_extension->priv->cancellable,
                       (GAsyncReadyCallback)get_web_app_title_cb,
                       g_object_ref (task));
  } else {
    g_task_return_pointer (task, NULL, NULL);
  }

  g_object_unref (task);
}

char *
ephy_web_extension_proxy_get_web_app_title_finish (EphyWebExtensionProxy *web_extension,
                                                   GAsyncResult *result,
                                                   GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, web_extension), FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
ephy_web_extension_proxy_history_set_urls (EphyWebExtensionProxy *web_extension,
                                           GList *urls)
{
  GList *l;
  GVariantBuilder builder;

  if (!web_extension->priv->proxy)
    return;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  for (l = urls; l; l = g_list_next (l)) {
    EphyHistoryURL *url = (EphyHistoryURL *)l->data;

    g_variant_builder_add (&builder, "(ss)", url->url, url->title);
  }

  g_dbus_proxy_call (web_extension->priv->proxy,
                     "HistorySetURLs",
                     g_variant_new ("(@a(ss))", g_variant_builder_end (&builder)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->priv->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_set_url_thumbnail (EphyWebExtensionProxy *web_extension,
                                                    const char *url,
                                                    const char *path)
{
  if (!web_extension->priv->proxy)
    return;

  g_dbus_proxy_call (web_extension->priv->proxy,
                     "HistorySetURLThumbnail",
                     g_variant_new ("(ss)", url, path),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->priv->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_set_url_title (EphyWebExtensionProxy *web_extension,
                                                const char *url,
                                                const char *title)
{
  if (!web_extension->priv->proxy)
    return;

  g_dbus_proxy_call (web_extension->priv->proxy,
                     "HistorySetURLTitle",
                     g_variant_new ("(ss)", url, title),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->priv->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_delete_url (EphyWebExtensionProxy *web_extension,
                                             const char *url)
{
  if (!web_extension->priv->proxy)
    return;

  g_dbus_proxy_call (web_extension->priv->proxy,
                     "HistoryDeleteURL",
                     g_variant_new ("(s)", url),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->priv->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_delete_host (EphyWebExtensionProxy *web_extension,
                                              const char *host)
{
  if (!web_extension->priv->proxy)
    return;

  g_dbus_proxy_call (web_extension->priv->proxy,
                     "HistoryDeleteHost",
                     g_variant_new ("(s)", host),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->priv->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_history_clear (EphyWebExtensionProxy *web_extension)
{
  if (!web_extension->priv->proxy)
    return;

  g_dbus_proxy_call (web_extension->priv->proxy,
                     "HistoryClear",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->priv->cancellable,
                     NULL, NULL);
}

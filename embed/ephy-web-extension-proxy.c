/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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
#include "ephy-web-extension-proxy.h"

#include "ephy-dbus-names.h"
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

  if (web_extension->cancellable) {
    g_cancellable_cancel (web_extension->cancellable);
    g_clear_object (&web_extension->cancellable);
  }

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
  g_autoptr(GError) error = NULL;

  web_extension->proxy = g_dbus_proxy_new_finish (result, &error);
  if (!web_extension->proxy) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error creating web extension proxy: %s", error->message);

    /* Attempt to trigger connection_closed_cb, which will destroy us, and ensure that
     * that EphyEmbedShell will remove us from its extensions list.
     */
    g_dbus_connection_close (web_extension->connection,
                             web_extension->cancellable,
                             NULL /* GAsyncReadyCallback */,
                             NULL);
    g_object_unref (web_extension);
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
  g_object_unref (web_extension);
}

static void
connection_closed_cb (GDBusConnection       *connection,
                      gboolean               remote_peer_vanished,
                      GError                *error,
                      EphyWebExtensionProxy *web_extension)
{
  if (error && !remote_peer_vanished)
    g_warning ("Unexpectedly lost connection to web extension: %s", error->message);

  g_object_unref (web_extension);
}

EphyWebExtensionProxy *
ephy_web_extension_proxy_new (GDBusConnection *connection)
{
  EphyWebExtensionProxy *web_extension;

  g_assert (G_IS_DBUS_CONNECTION (connection));

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
                    g_object_ref (web_extension));

  return web_extension;
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

void
ephy_web_extension_proxy_password_cached_users_response (EphyWebExtensionProxy *web_extension,
                                                         GList                 *users,
                                                         gint32                 promise_id,
                                                         guint64                page_id)
{
  if (!web_extension->proxy)
    return;

  GList *l;
  g_auto(GVariantBuilder) builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_STRING_ARRAY);
  for (l = users; l != NULL; l = l->next)
    g_variant_builder_add (&builder, "s", l->data);

  g_dbus_proxy_call (web_extension->proxy,
                     "PasswordQueryUsernamesResponse",
                     g_variant_new ("(asit)", &builder, promise_id, page_id),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

void
ephy_web_extension_proxy_password_query_response (EphyWebExtensionProxy *web_extension,
                                                  const char            *username,
                                                  const char            *password,
                                                  gint32                 promise_id,
                                                  guint64                page_id)
{
  if (!web_extension->proxy)
    return;

  g_dbus_proxy_call (web_extension->proxy,
                     "PasswordQueryResponse",
                     g_variant_new ("(ssit)", username ?: "", password ?: "", promise_id, page_id),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     web_extension->cancellable,
                     NULL, NULL);
}

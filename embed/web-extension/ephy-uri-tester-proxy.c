/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Igalia S.L.
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
#include "ephy-uri-tester-proxy.h"

#include "ephy-dbus-names.h"

struct _EphyUriTesterProxy {
  GObject parent_instance;

  GDBusProxy *proxy;
  GDBusConnection *connection;
};

G_DEFINE_TYPE (EphyUriTesterProxy, ephy_uri_tester_proxy, G_TYPE_OBJECT)

static void
ephy_uri_tester_proxy_dispose (GObject *object)
{
  EphyUriTesterProxy *uri_tester = EPHY_URI_TESTER_PROXY (object);

  g_clear_object (&uri_tester->proxy);
  g_clear_object (&uri_tester->connection);

  G_OBJECT_CLASS (ephy_uri_tester_proxy_parent_class)->dispose (object);
}

static void
ephy_uri_tester_proxy_init (EphyUriTesterProxy *uri_tester)
{
}

static void
ephy_uri_tester_proxy_class_init (EphyUriTesterProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_uri_tester_proxy_dispose;
}

EphyUriTesterProxy *
ephy_uri_tester_proxy_new (GDBusConnection *connection)
{
  EphyUriTesterProxy *uri_tester;
  GError *error = NULL;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

  uri_tester = g_object_new (EPHY_TYPE_URI_TESTER_PROXY, NULL);

  uri_tester->connection = g_object_ref (connection);

  /* It has to be sync because it must be guaranteed to be ready before the
   * first request is sent. We have to handle requests synchronously anyway.
   */
  uri_tester->proxy = g_dbus_proxy_new_sync (connection,
                                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                             NULL,
                                             NULL,
                                             EPHY_URI_TESTER_OBJECT_PATH,
                                             EPHY_URI_TESTER_INTERFACE,
                                             NULL,
                                             &error);

  /* This is fatal. */
  if (error)
    g_error ("Failed to initialize URI tester: %s", error->message);

  return uri_tester;
}

char *
ephy_uri_tester_proxy_maybe_rewrite_uri (EphyUriTesterProxy *uri_tester,
                                         const char         *request_uri,
                                         const char         *page_uri)
{
  GVariant *variant;
  char *modified_uri;
  GError *error = NULL;

  g_return_val_if_fail (EPHY_IS_URI_TESTER_PROXY (uri_tester), g_strdup (""));

  variant = g_dbus_proxy_call_sync (uri_tester->proxy,
                                    "MaybeRewriteUri",
                                    g_variant_new ("(ss)", request_uri, page_uri),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);

  if (error) {
    g_warning ("Failed to query EphyUriTester for %s: %s", request_uri, error->message);
    g_error_free (error);
    return g_strdup (request_uri);
  }

  g_variant_get (variant, "(s)", &modified_uri);

  g_variant_unref (variant);
  return modified_uri;
}

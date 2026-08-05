/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Igalia S.L.
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

/* For O_PATH */
#define _GNU_SOURCE

#include <config.h>

#include "ephy-flatpak-utils.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <libportal-gtk4/portal-gtk4.h>

#include "ephy-file-helpers.h"

static void
opened_uri (GObject      *object,
            GAsyncResult *result,
            gpointer      data)
{
  XdpPortal *portal = XDP_PORTAL (object);
  g_autoptr (GError) error = NULL;
  gboolean open_dir = GPOINTER_TO_INT (data);

  if (open_dir)
    xdp_portal_open_directory_finish (portal, result, &error);
  else
    xdp_portal_open_uri_finish (portal, result, &error);

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s", error->message);
}

static void
ephy_open_uri (const char *uri,
               gboolean    is_dir,
               gboolean    require_consent)
{
  GApplication *application;
  GtkWindow *window;
  XdpParent *parent;
  XdpPortal *portal = ephy_get_portal ();

  application = g_application_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (application));
  parent = xdp_parent_new_gtk (window);

  if (is_dir)
    xdp_portal_open_directory (portal, parent, uri,
                               require_consent ? XDP_OPEN_URI_FLAG_ASK : XDP_OPEN_URI_FLAG_NONE,
                               NULL, opened_uri, GINT_TO_POINTER (TRUE));
  else
    xdp_portal_open_uri (portal, parent, uri,
                         require_consent ? XDP_OPEN_URI_FLAG_ASK : XDP_OPEN_URI_FLAG_NONE,
                         NULL, opened_uri, GINT_TO_POINTER (FALSE));

  xdp_parent_free (parent);
}

void
ephy_open_directory_via_flatpak_portal (const char       *uri,
                                        EphyOpenUriFlags  flags)
{
  ephy_open_uri (uri, TRUE, (flags & EPHY_OPEN_URI_FLAGS_REQUIRE_USER_INTERACTION));
}

void
ephy_open_uri_via_flatpak_portal (const char       *uri,
                                  EphyOpenUriFlags  flags)
{
  ephy_open_uri (uri, FALSE, (flags & EPHY_OPEN_URI_FLAGS_REQUIRE_USER_INTERACTION));
}

gboolean
ephy_can_install_web_apps (void)
{
  static gsize portal_available = 0;
  enum {
    LAUNCHER_PORTAL_MISSING = 1,
    LAUNCHER_PORTAL_FOUND = 2
  };

  if (g_once_init_enter (&portal_available)) {
    g_autoptr (GDBusProxy) proxy = NULL;
    g_autoptr (GVariant) version = NULL;
    g_autoptr (GVariant) version_child = NULL;
    g_autoptr (GVariant) version_grandchild = NULL;

    proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                           "org.freedesktop.portal.Desktop",
                                           "/org/freedesktop/portal/desktop",
                                           "org.freedesktop.DBus.Properties",
                                           NULL, NULL);
    if (proxy)
      version = g_dbus_proxy_call_sync (proxy, "Get",
                                        g_variant_new ("(ss)", "org.freedesktop.portal.DynamicLauncher", "version"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1, NULL, NULL);
    if (version) {
      version_child = g_variant_get_child_value (version, 0);
      version_grandchild = g_variant_get_child_value (version_child, 0);
      g_debug ("Found version %d of the dynamic launcher portal", g_variant_get_uint32 (version_grandchild));
      g_once_init_leave (&portal_available, LAUNCHER_PORTAL_FOUND);
    } else {
      g_once_init_leave (&portal_available, LAUNCHER_PORTAL_MISSING);
    }
  }

  return portal_available == LAUNCHER_PORTAL_FOUND;
}

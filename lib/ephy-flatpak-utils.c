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
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#if USE_LIBPORTAL
#include <libportal/portal-gtk3.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static gboolean is_web_process = FALSE;

void
ephy_flatpak_utils_set_is_web_process_extension (void)
{
  g_assert (!is_web_process);

  is_web_process = TRUE;
}

gboolean
ephy_is_running_inside_flatpak (void)
{
  static _Thread_local gboolean decided = FALSE;
  static _Thread_local gboolean under_flatpak = FALSE;

  if (decided)
    return under_flatpak;

  /* This function cannot be used in the web process extension, because WebKit
   * creates a .flatpak-info in its web process sandbox even when we are not
   * running under flatpak. It would always return TRUE.
   */
  g_assert (!is_web_process);

  under_flatpak = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
  decided = TRUE;
  return under_flatpak;
}

#if USE_LIBPORTAL
static void
opened_uri (GObject      *object,
            GAsyncResult *result,
            gpointer      data)
{
  g_autoptr (XdpPortal) portal = XDP_PORTAL (object);
  g_autoptr (GError) error = NULL;
  gboolean open_dir = GPOINTER_TO_INT (data);
  gboolean res;

  if (open_dir)
    res = xdp_portal_open_directory_finish (portal, result, &error);
  else
    res = xdp_portal_open_uri_finish (portal, result, &error);

  if (!res)
    g_warning ("%s", error->message);
}
#endif

static void
ephy_open_uri (const char *uri,
               gboolean    is_dir)
{
#if USE_LIBPORTAL
  GApplication *application;
  GtkWindow *window;
  XdpParent *parent;
  g_autoptr (XdpPortal) portal = xdp_portal_new ();

  application = g_application_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (application));
  parent = xdp_parent_new_gtk (window);

  if (is_dir)
    xdp_portal_open_directory (g_steal_pointer (&portal), parent, uri, XDP_OPEN_URI_FLAG_ASK, NULL, opened_uri, GINT_TO_POINTER (TRUE));
  else
    xdp_portal_open_uri (g_steal_pointer (&portal), parent, uri, XDP_OPEN_URI_FLAG_ASK, NULL, opened_uri, GINT_TO_POINTER (FALSE));

  xdp_parent_free (parent);
#else
  g_warning ("Flatpak portal support disabled at compile time, cannot open %s",
             uri);
#endif
}

void
ephy_open_directory_via_flatpak_portal (const char *uri)
{
  ephy_open_uri (uri, TRUE);
}

void
ephy_open_uri_via_flatpak_portal (const char *uri)
{
  ephy_open_uri (uri, FALSE);
}

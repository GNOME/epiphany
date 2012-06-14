/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2012 - Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-private.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"
#include "ephy-shell.h"
#include "ephy-session.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>

const char *session_data = 
"<?xml version=\"1.0\"?>"
"<session>"
	 "<window x=\"94\" y=\"48\" width=\"1132\" height=\"684\" active-tab=\"0\" role=\"epiphany-window-67c6e8a5\">"
	 	 "<embed url=\"about:memory\" title=\"Memory usage\"/>"
	 "</window>"
"</session>";

static void
test_ephy_session_load ()
{
    EphySession *session;
    gboolean ret;
    GList *l;
    EphyEmbed *embed;
    EphyWebView *view;

    session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
    g_assert (session);

    ret = ephy_session_load_from_string (session, session_data, -1, 0);
    g_assert (ret);

    l = ephy_session_get_windows (session);
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 1);

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
    g_assert (embed);
    view = ephy_embed_get_web_view (embed);
    g_assert (view);
    g_assert_cmpstr (ephy_web_view_get_address (view), ==, "ephy-about:memory");
}

int
main (int argc, char *argv[])
{
  int ret;

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();
  ephy_embed_prefs_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);

  g_test_add_func ("/src/ephy-session/load",
                   test_ephy_session_load);

  ret = g_test_run ();

  g_object_unref (ephy_shell);
  ephy_file_helpers_shutdown ();

  return ret;
}

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include "config.h"
#include "ephy-debug.h"
#include "ephy-embed.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"

#include <glib.h>
#include <gtk/gtk.h>

static void
web_view_created_cb (EphyEmbedShell *shell,
                     EphyWebView    *view,
                     gpointer        user_data)
{
  gboolean *web_view_created = (gboolean *)user_data;
  *web_view_created = TRUE;
}

static void
test_ephy_embed_shell_web_view_created (void)
{
  EphyEmbedShell *embed_shell;
  GtkWidget *view;
  gboolean web_view_created = FALSE;

  embed_shell = ephy_embed_shell_get_default ();
  g_signal_connect (embed_shell, "web-view-created",
                    G_CALLBACK (web_view_created_cb), &web_view_created);

  view = g_object_ref_sink (ephy_web_view_new ());
  g_assert_true (web_view_created);
  g_object_unref (view);
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL, EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS, NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);
  g_application_register (G_APPLICATION (ephy_embed_shell_get_default ()), NULL, NULL);

  g_test_add_func ("/embed/ephy-embed-shell/web-view-created",
                   test_ephy_embed_shell_web_view_created);

  ret = g_test_run ();

  g_object_unref (ephy_embed_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}

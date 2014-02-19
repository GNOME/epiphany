/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-embed-shell-test.c
 * This file is part of Epiphany
 *
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
#include "ephy-embed.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-private.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"

#include <glib.h>
#include <gtk/gtk.h>

static void
test_ephy_embed_shell_launch_handler (void)
{
    EphyEmbedShell *embed_shell;
    gboolean ret;
    GFile *file;

    embed_shell = ephy_embed_shell_get_default ();

    ret = ephy_embed_shell_launch_handler (embed_shell, NULL, "text/html", 0);
    g_assert (ret == FALSE);

    file = g_file_new_for_path (TEST_DIR"/data/test.html");
    g_assert (file);

    ret = ephy_embed_shell_launch_handler (embed_shell, file, NULL, 0);
    g_assert (ret == FALSE);

    ret = ephy_embed_shell_launch_handler (embed_shell, file, "text/html", 0);
    g_assert (ret == FALSE);

    g_object_unref (file);
}

static void
web_view_created_cb (EphyEmbedShell *shell, EphyWebView *view, gpointer user_data)
{
    gboolean *web_view_created = (gboolean*)user_data;
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

    view = ephy_web_view_new ();
    g_assert (web_view_created);
    gtk_widget_destroy (view);
}

int
main (int argc, char *argv[])
{
  int ret;
  const char *xdg_data_dirs;
  char *new_xdg_data_dirs;
  char **dirs = NULL;
  char *schemas_dir;

  /* Save XDG_DATA_DIRS to set GSETTINGS_SCHEMA_DIR, otherwise we
   * won't find the sytem schemas. */
  xdg_data_dirs = g_getenv ("XDG_DATA_DIRS");
  if (!xdg_data_dirs)
    xdg_data_dirs = "/usr/local/share/:/usr/share/";

  dirs = g_strsplit (xdg_data_dirs, ":", -1);

  /* We can only use one directory, so use the first one or the system default. */
  schemas_dir = g_build_filename (dirs[0], "glib-2.0", "schemas", NULL);
  g_setenv ("GSETTINGS_SCHEMA_DIR", schemas_dir, TRUE);
  g_strfreev (dirs);
  g_free (schemas_dir);

  /* We need to make sure that XDG_DATA_DIRS includes a sensible
     directory where to find the mimetype database, otherwise bad
     stuff will happen. Prefix TEST_DIR for the test purposes. */
  new_xdg_data_dirs = g_strconcat (TEST_DIR, ":", xdg_data_dirs, NULL);
  g_setenv ("XDG_DATA_DIRS", new_xdg_data_dirs, TRUE);
  g_setenv ("XDG_DATA_HOME", TEST_DIR, TRUE);
  g_free (new_xdg_data_dirs);

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL, EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS, NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);
  g_application_register (G_APPLICATION (ephy_embed_shell_get_default ()), NULL, NULL);

  g_test_add_func ("/embed/ephy-embed-shell/launch_handler",
                   test_ephy_embed_shell_launch_handler);

  g_test_add_func ("/embed/ephy-embed-shell/web-view-created",
                   test_ephy_embed_shell_web_view_created);

  ret = g_test_run ();

  g_object_unref (ephy_embed_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}

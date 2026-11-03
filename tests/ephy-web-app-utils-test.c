/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 Epiphany contributors
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libportal-gtk4/portal-gtk4.h>

#include "ephy-string.h"
#include "ephy-web-app-utils.h"

#define ORIGINAL_URL "https://example.org/"

static const char *urls[] = {
  /* The URL from https://gitlab.gnome.org/GNOME/epiphany/-/issues/1812 */
  "https://archive.org/details/moviesandfilms?and%5B%5D=mediatype:%22collection%22",
  "https://example.org/a?x=%3A&y=100%25",
  "https://example.org/plain?and[]=mediatype:collection",
  "https://example.org/search?q=a&r=b;c=d#frag%5D",
};

/* Returns the last argument of the Exec line, as the browser would receive it
 * after the desktop entry parser has turned each "%%" into "%". */
static char *
get_launch_url (const char *desktop_path)
{
  g_autoptr (GKeyFile) keyfile = g_key_file_new ();
  g_autofree char *exec = NULL;
  g_auto (GStrv) argv = NULL;
  int argc;

  g_assert_true (g_key_file_load_from_file (keyfile, desktop_path, G_KEY_FILE_NONE, NULL));

  exec = g_key_file_get_value (keyfile, "Desktop Entry", "Exec", NULL);
  g_assert_nonnull (exec);

  /* Every % has to be part of a %% pair, or it is a field code. */
  for (const char *p = exec; *p; p++) {
    if (*p == '%') {
      g_assert_cmpint (p[1], ==, '%');
      p++;
    }
  }

  g_assert_true (g_shell_parse_argv (exec, &argc, &argv, NULL));
  g_assert_cmpint (argc, ==, 4);
  g_assert_cmpstr (argv[0], ==, "epiphany");
  g_assert_cmpstr (argv[1], ==, "--application-mode");
  g_assert_true (g_str_has_prefix (argv[2], "--profile="));

  return ephy_string_find_and_replace (argv[3], "%%", "%");
}

static void
test_web_app_save_url (void)
{
  g_autofree char *dir = NULL;
  g_autofree char *path = NULL;
  g_autofree char *before = NULL;
  g_autofree char *after = NULL;
  g_autofree char *profile_dir = NULL;
  g_autoptr (GKeyFile) keyfile = g_key_file_new ();
  EphyWebApplication app = { 0 };

  if (xdp_portal_running_under_sandbox ()) {
    g_test_skip ("ephy_web_application_save() cannot be used in a sandbox");
    return;
  }

  dir = g_dir_make_tmp ("ephy-web-app-utils-test-XXXXXX", NULL);
  g_assert_nonnull (dir);
  path = g_build_filename (dir, "test.desktop", NULL);

  /* A desktop file as written by ephy_web_application_create(). */
  g_key_file_set_value (keyfile, "Desktop Entry", "Name", "Test");
  g_key_file_set_value (keyfile, "Desktop Entry", "Icon", "/nonexistent/icon.png");
  g_key_file_set_value (keyfile, "Desktop Entry", "Exec",
                        "epiphany --application-mode --profile='/tmp/my profile' '" ORIGINAL_URL "'");
  g_assert_true (g_key_file_save_to_file (keyfile, path, NULL));

  app.id = (char *)"test";
  app.name = (char *)"Test";
  app.icon_path = (char *)"/nonexistent/icon.png";
  app.desktop_path = path;
  app.url = (char *)ORIGINAL_URL;

  /* Saving without changes must not touch the file. */
  g_assert_true (g_file_get_contents (path, &before, NULL, NULL));
  g_assert_false (ephy_web_application_save (&app));
  g_assert_true (g_file_get_contents (path, &after, NULL, NULL));
  g_assert_cmpstr (before, ==, after);

  for (guint i = 0; i < G_N_ELEMENTS (urls); i++) {
    g_autofree char *launch_url = NULL;

    app.url = (char *)urls[i];
    g_assert_true (ephy_web_application_save (&app));

    launch_url = get_launch_url (path);
    g_assert_cmpstr (launch_url, ==, urls[i]);

    /* The URL is now up to date, so saving again changes nothing. */
    g_assert_false (ephy_web_application_save (&app));
  }

  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/lib/ephy-web-app-utils/save_url",
                   test_web_app_save_url);

  return g_test_run ();
}

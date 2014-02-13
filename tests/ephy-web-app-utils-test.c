/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-web-app-utils-test.c
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
#include "ephy-embed-prefs.h"
#include "ephy-embed-private.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-web-app-utils.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>

char *xdg_data_home = NULL;

typedef struct {
  const char *url;
  const char *name;
} WebAppTest;

/* The EphyWebApp API does not need network access */
static const WebAppTest test_web_app[] = {
  { "http://www.gnome.org/", "GNOME.org" },

  /* Filesystem invalid characters in name */
  { "http://twitter.com/", "Twitter / Home" },
  { "http://www.gnome.org/", "/ ext3 \0 /" },
};

static void
test_web_app_lifetime (void)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (test_web_app); i++) {
    WebAppTest test = test_web_app[i];

    char *desktop_file = NULL;
    char *profile_dir = NULL;
    char *basename = NULL;
    char *desktop_link = NULL;

    GList *apps = NULL;
    EphyWebApplication *app = NULL;

    g_test_message ("NAME: %s", test.name);

    /* Test creation */
    desktop_file = ephy_web_application_create (test.url, test.name, NULL);
    g_assert (g_str_has_prefix (desktop_file, ephy_dot_dir ()));
    g_assert (g_file_test (desktop_file, G_FILE_TEST_EXISTS));

    profile_dir = ephy_web_application_get_profile_directory (test.name);
    g_assert (g_str_has_prefix (desktop_file, profile_dir));
    g_assert (g_str_has_prefix (profile_dir, ephy_dot_dir ()));
    g_assert (g_file_test (profile_dir, G_FILE_TEST_EXISTS));

    /* Test proper symlink */
    basename = g_path_get_basename (desktop_file);
    desktop_link = g_build_filename (xdg_data_home, "applications", basename, NULL);
    g_assert (g_file_test (desktop_link, G_FILE_TEST_EXISTS));
    g_assert (g_file_test (desktop_link, G_FILE_TEST_IS_SYMLINK));

    /* Test exists API */
    g_assert (ephy_web_application_exists (test.name));

    /* Test list API */
    apps = ephy_web_application_get_application_list ();
    g_assert_cmpint (g_list_length (apps), ==, 1);

    app = apps->data;
    g_assert_cmpstr (app->name, ==, test.name);
    g_assert_cmpstr (app->url, ==, test.url);
    g_assert_cmpstr (app->desktop_file, ==, basename);
    g_assert (g_str_has_prefix (app->icon_url, profile_dir));

    ephy_web_application_free_application_list (apps);

    /* Test delete API */
    g_test_message ("DELETE: %s", test.name);
    g_assert (ephy_web_application_delete (test.name));

    g_assert (g_file_test (desktop_link, G_FILE_TEST_EXISTS) == FALSE);
    g_assert (g_file_test (desktop_link, G_FILE_TEST_IS_SYMLINK) == FALSE);
    g_assert (g_file_test (profile_dir, G_FILE_TEST_EXISTS) == FALSE);
    g_assert (ephy_web_application_exists (test.name) == FALSE);

    apps = ephy_web_application_get_application_list ();
    g_assert_cmpint (g_list_length (apps), ==, 0);
    ephy_web_application_free_application_list (apps);

    g_free (desktop_link);
    g_free (basename);
    g_free (profile_dir);
    g_free (desktop_file);
  }
}

int
main (int argc, char *argv[])
{
  int ret;

  if (!ephy_file_helpers_init (NULL, EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS, NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  /* Set $XDG_DATA_HOME to point to a directory inside our disposable
   * profile directory. This allows us to test cleanly and safely.
   * Note: $XDG_DATA_HOME default value is $HOME/.local/share/ */
  xdg_data_home = g_build_filename (ephy_dot_dir (), "xdg_share", NULL);
  g_setenv ("XDG_DATA_HOME", xdg_data_home, TRUE);

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  g_test_add_func ("/embed/ephy-web-app-utils/lifetime",
                   test_web_app_lifetime);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();
  g_free (xdg_data_home);

  return ret;
}

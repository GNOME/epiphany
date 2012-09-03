/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2009 Xan López
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

#include "config.h"
#include "ephy-profile-utils.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"

#include <libsoup/soup.h>

#define PROFILE_MIGRATION_FILE ".migrated"

int
ephy_profile_utils_get_migration_version ()
{
  char *migrated_file, *contents = NULL;
  gsize size;
  int result = 0;
  int latest = 0;

  migrated_file = g_build_filename (ephy_dot_dir (),
                                    PROFILE_MIGRATION_FILE,
                                    NULL);

  if (g_file_test (migrated_file, G_FILE_TEST_EXISTS)) {
    g_file_get_contents (migrated_file, &contents, &size, NULL);

    if (contents != NULL)
      result = sscanf(contents, "%d", &latest);

    g_free (contents);

    if (result != 1)
      latest = 0;
  } else if (ephy_dot_dir_is_default () == FALSE) {
    /* Since version 8, we need to migrate also profile directories
       other than the default one. Profiles in such directories work
       perfectly fine without going through the first 7 migration
       steps, so it is safe to assume that any non-default profile
       directory without a migration file can be migrated starting
       from the step 8. */
    latest = 7;
  }

  g_free (migrated_file);

  return latest;
}

gboolean
ephy_profile_utils_set_migration_version (int version)
{
  char *migrated_file, *contents;
  gboolean result = FALSE;

  migrated_file = g_build_filename (ephy_dot_dir (),
                                    PROFILE_MIGRATION_FILE,
                                    NULL);
  contents = g_strdup_printf ("%d", version);
  result = g_file_set_contents (migrated_file, contents, -1, NULL);

  if (result == FALSE)
    LOG ("Couldn't store migration version %d in %s (%s, %s)",
         version, migrated_file, ephy_dot_dir (), PROFILE_MIGRATION_FILE);

  g_free (contents);
  g_free (migrated_file);

  return result;
}

static void
store_form_password_cb (GnomeKeyringResult result,
                        guint32 id,
                        gpointer data)
{
  /* FIXME: should we do anything if the operation failed? */
}

static void
normalize_and_prepare_uri (SoupURI *uri,
                           const char *form_username,
                           const char *form_password)
{
  g_return_if_fail (uri != NULL);

  /* We normalize https? schemes here so that we use passwords
   * we stored in https sites in their http counterparts, and
   * vice-versa. */
  if (uri->scheme == SOUP_URI_SCHEME_HTTPS)
    soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTP);

  soup_uri_set_path (uri, "/");

  /* Store the form login and password names encoded in the
   * URL. A bit of an abuse of keyring, but oh well */
  soup_uri_set_query_from_fields (uri,
                                  FORM_USERNAME_KEY,
                                  form_username,
                                  FORM_PASSWORD_KEY,
                                  form_password,
                                  NULL);
}

void
_ephy_profile_utils_store_form_auth_data (const char *uri,
                                          const char *form_username,
                                          const char *form_password,
                                          const char *username,
                                          const char *password)
{
  SoupURI *fake_uri;
  char *fake_uri_str;

  g_return_if_fail (uri);
  g_return_if_fail (form_username);
  g_return_if_fail (form_password);
  g_return_if_fail (username);
  g_return_if_fail (password);

  fake_uri = soup_uri_new (uri);
  if (fake_uri == NULL)
    return;

  normalize_and_prepare_uri (fake_uri, form_username, form_password);
  fake_uri_str = soup_uri_to_string (fake_uri, FALSE);

  gnome_keyring_set_network_password (NULL,
                                      username,
                                      NULL,
                                      fake_uri_str,
                                      NULL,
                                      fake_uri->scheme,
                                      NULL,
                                      fake_uri->port,
                                      password,
                                      (GnomeKeyringOperationGetIntCallback)store_form_password_cb,
                                      NULL,
                                      NULL);
  soup_uri_free (fake_uri);
  g_free (fake_uri_str);
}

void
_ephy_profile_utils_query_form_auth_data (const char *uri,
                                          const char *form_username,
                                          const char *form_password,
                                          GnomeKeyringOperationGetListCallback callback,
                                          gpointer data,
                                          GDestroyNotify destroy_data)
{
  SoupURI *key;
  char *key_str;

  g_return_if_fail (uri);
  g_return_if_fail (form_username);
  g_return_if_fail (form_password);

  key = soup_uri_new (uri);
  g_return_if_fail (key);

  normalize_and_prepare_uri (key, form_username, form_password);

  key_str = soup_uri_to_string (key, FALSE);

  LOG ("Querying Keyring: %s", key_str);
  gnome_keyring_find_network_password (NULL,
                                       NULL,
                                       key_str,
                                       NULL,
                                       NULL,
                                       NULL,
                                       0,
                                       callback,
                                       data,
                                       destroy_data);
  soup_uri_free (key);
  g_free (key_str);
}

#define EPHY_PROFILE_MIGRATOR "ephy-profile-migrator"

gboolean
ephy_profile_utils_do_migration (int test_to_run, gboolean debug)
{
  gboolean ret;
  GError *error = NULL;
  char *index = NULL, *version = NULL;
  int status;
  char *argv[6] = { EPHY_PROFILE_MIGRATOR, "-v" };
  int i = 2; /* index for argv, start filling at 2. */
  char *envp[1] = { "EPHY_LOG_MODULES=ephy-profile" };

  argv[i++] = version = g_strdup_printf ("%d", EPHY_PROFILE_MIGRATION_VERSION);

  if (test_to_run != -1) {
    index = g_strdup_printf ("%d", test_to_run);

    argv[i++] = "-d";
    argv[i++] = index;
  }

  argv[i++] = NULL;

  if (debug)
    argv[0] = ABS_TOP_BUILD_DIR"/lib/"EPHY_PROFILE_MIGRATOR;

  ret = g_spawn_sync (NULL, argv, envp, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL, NULL,
                      &status, &error);
  g_free (index);
  g_free (version);
    
  if (error) {
    LOG ("Failed to run migrator: %s", error->message);
    g_error_free (error);
  }

  if (status != 0)
    ret = FALSE;

  return ret;
}

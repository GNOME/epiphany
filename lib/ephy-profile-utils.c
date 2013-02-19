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

#include <glib/gi18n.h>
#include <libsoup/soup.h>

#define PROFILE_MIGRATION_FILE ".migrated"

const SecretSchema*
ephy_profile_get_form_password_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.FormPassword", SECRET_SCHEMA_NONE,
    {
      { URI_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { FORM_USERNAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { FORM_PASSWORD_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { USERNAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };
  return &schema;
}

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
normalize_and_prepare_uri (SoupURI *uri)
{
  g_return_if_fail (uri != NULL);

  /* We normalize https? schemes here so that we use passwords
   * we stored in https sites in their http counterparts, and
   * vice-versa. */
  if (uri->scheme == SOUP_URI_SCHEME_HTTPS)
    soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTP);

  soup_uri_set_path (uri, "/");
}

static GHashTable *
ephy_profile_utils_get_attributes_table (const char *uri,
                                         const char *field_username,
                                         const char *field_password,
                                         const char *username)
{
  return secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA,
                                  URI_KEY, uri,
                                  FORM_USERNAME_KEY, field_username,
                                  FORM_PASSWORD_KEY, field_password,
                                  username ? USERNAME_KEY : NULL, username,
                                  NULL);
}

static void
store_form_password_cb (SecretService *service,
                        GAsyncResult *res,
                        GSimpleAsyncResult *async)
{
  GError *error = NULL;

  secret_service_store_finish (service, res, &error);
  if (error != NULL)
    g_simple_async_result_take_error (async, error);

  g_simple_async_result_complete (async);
  g_object_unref (async);
}

void
_ephy_profile_utils_store_form_auth_data (const char *uri,
                                          const char *form_username,
                                          const char *form_password,
                                          const char *username,
                                          const char *password,
                                          GAsyncReadyCallback callback,
                                          gpointer userdata)
{
  SoupURI *fake_uri;
  char *fake_uri_str;
  SecretValue *value;
  GHashTable *attributes;
  char *label;
  GSimpleAsyncResult *res;

  g_return_if_fail (uri);
  g_return_if_fail (form_username);
  g_return_if_fail (form_password);
  g_return_if_fail (username);
  g_return_if_fail (password);

  fake_uri = soup_uri_new (uri);

  if (fake_uri == NULL)
    return;

  res = g_simple_async_result_new (NULL, callback, userdata,
                                   _ephy_profile_utils_store_form_auth_data);

  normalize_and_prepare_uri (fake_uri);
  fake_uri_str = soup_uri_to_string (fake_uri, FALSE);
  value = secret_value_new (password, -1, "text/plain");
  attributes = ephy_profile_utils_get_attributes_table (fake_uri_str, form_username,
                                                        form_password, username);
  /* Translators: The first %s is the username and the second one is the
   * hostname where this is happening. Example: gnome@gmail.com and
   * mail.google.com.
   */
  label = g_strdup_printf (_("Password for %s in a form in %s"),
                           username, fake_uri_str);
  secret_service_store (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                        attributes, NULL, label, value,
                        NULL,
                        (GAsyncReadyCallback)store_form_password_cb,
                        g_object_ref (res));

  g_free (label);
  secret_value_unref (value);
  g_hash_table_unref (attributes);
  soup_uri_free (fake_uri);
  g_free (fake_uri_str);
  g_object_unref (res);
}


gboolean
_ephy_profile_utils_store_form_auth_data_finish (GAsyncResult *result,
                                                 GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL, _ephy_profile_utils_store_form_auth_data), FALSE);

  return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error);
}

typedef struct
{
  EphyQueryFormDataCallback callback;
  gpointer data;
  GDestroyNotify destroy_data;
} EphyProfileQueryClosure;

static void
ephy_profile_query_closure_free (EphyProfileQueryClosure *closure)
{
  if (closure->destroy_data)
    closure->destroy_data (closure->data);

  g_slice_free (EphyProfileQueryClosure, closure);
}

static void
search_form_data_cb (SecretService *service,
                     GAsyncResult *res,
                     EphyProfileQueryClosure *closure)
{
  GList *results;
  SecretItem *item;
  const char* username = NULL, *password = NULL;
  SecretValue *value = NULL;
  GHashTable *attributes = NULL;
  GError *error = NULL;

  results = secret_service_search_finish (service, res, &error);
  if (error) {
    g_warning ("Couldn't retrieve form data: %s", error->message);
    g_error_free (error);
    goto out;
  }

  if (!results)
    goto out;

  item = (SecretItem*)results->data;
  attributes = secret_item_get_attributes (item);
  username = g_hash_table_lookup (attributes, USERNAME_KEY);
  value = secret_item_get_secret (item);
  password = secret_value_get (value, NULL);

  g_list_free_full (results, (GDestroyNotify)g_object_unref);

out:
  if (closure->callback)
    closure->callback (username, password, closure->data);

  if (value)
    secret_value_unref (value);
  if (attributes)
    g_hash_table_unref (attributes);

  ephy_profile_query_closure_free (closure);
}

void
_ephy_profile_utils_query_form_auth_data (const char *uri,
                                          const char *form_username,
                                          const char *form_password,
                                          EphyQueryFormDataCallback callback,
                                          gpointer data,
                                          GDestroyNotify destroy_data)
{
  SoupURI *key;
  char *key_str;
  EphyProfileQueryClosure *closure;
  GHashTable *attributes;

  g_return_if_fail (uri);
  g_return_if_fail (form_username);
  g_return_if_fail (form_password);

  key = soup_uri_new (uri);
  g_return_if_fail (key);

  normalize_and_prepare_uri (key);

  key_str = soup_uri_to_string (key, FALSE);

  attributes = ephy_profile_utils_get_attributes_table (key_str, form_username,
                                                        form_password, NULL);

  closure = g_slice_new0 (EphyProfileQueryClosure);
  closure->callback = callback;
  closure->data = data;
  closure->destroy_data = destroy_data;

  LOG ("Querying Keyring: %s", key_str);

  secret_service_search (NULL,
                         EPHY_FORM_PASSWORD_SCHEMA,
                         attributes,
                         SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                         NULL, (GAsyncReadyCallback)search_form_data_cb,
                         closure);

  g_hash_table_unref (attributes);
  soup_uri_free (key);
  g_free (key_str);
}

#define EPHY_PROFILE_MIGRATOR "ephy-profile-migrator"

gboolean
ephy_profile_utils_do_migration (const char *profile_directory, int test_to_run, gboolean debug)
{
  gboolean ret;
  GError *error = NULL;
  char *index = NULL, *version = NULL;
  int status;
  char *argv[6] = { EPHY_PROFILE_MIGRATOR, "-v" };
  int i = 2; /* index for argv, start filling at 2. */
  char **envp;

  envp = g_environ_setenv (g_get_environ (),
                           "EPHY_LOG_MODULES", "ephy-profile",
                           TRUE);

  argv[i++] = version = g_strdup_printf ("%d", EPHY_PROFILE_MIGRATION_VERSION);

  if (test_to_run != -1) {
    index = g_strdup_printf ("%d", test_to_run);

    argv[i++] = "-d";
    argv[i++] = index;
  }

  if (profile_directory != NULL) {
    argv[i++] = "-p";
    argv[i++] = (char *)profile_directory;
  }

  argv[i++] = NULL;

  if (debug)
    argv[0] = ABS_TOP_BUILD_DIR"/lib/"EPHY_PROFILE_MIGRATOR;

  ret = g_spawn_sync (NULL, argv, envp, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL, NULL,
                      &status, &error);
  g_free (index);
  g_free (version);
  g_strfreev (envp);
    
  if (error) {
    LOG ("Failed to run migrator: %s", error->message);
    g_error_free (error);
  }

  if (status != 0)
    ret = FALSE;

  return ret;
}

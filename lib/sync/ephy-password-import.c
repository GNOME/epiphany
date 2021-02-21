/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include "ephy-password-import.h"
#include "ephy-password-manager.h"
#include "ephy-sqlite-connection.h"
#include "ephy-uri-helpers.h"

#include <glib/gi18n.h>

#include <nettle/pbkdf2.h>
#include <nettle/aes.h>
#include <nettle/cbc.h>

#define PASSWORDS_IMPORT_ERROR passwords_import_error_quark ()
#define SECRET_SCHEMA  libsecret_get_schema ()

GQuark passwords_import_error_quark (void);
G_DEFINE_QUARK (ephy - passwords - import - error - quark, passwords_import_error)

typedef enum {
  PASSWORDS_IMPORT_ERROR_PASSWORDS = 1001
} PasswordsImportErrorCode;

const SecretSchema *
libsecret_get_schema (void)
{
  static const SecretSchema the_schema = {
    "chrome_libsecret_os_crypt_password_v2",
    SECRET_SCHEMA_DONT_MATCH_NAME,
    {
      {"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
      {NULL, SECRET_SCHEMA_ATTRIBUTE_STRING},
    }
  };

  return &the_schema;
}

static char *
get_libsecret_phrase (ChromeType type)
{
  g_autoptr (GError) error = NULL;
  char *phrase;

  if (type == CHROME)
    phrase = secret_password_lookup_sync (SECRET_SCHEMA, NULL, &error, "application", "chrome", NULL);
  else if (type == CHROMIUM)
    phrase = secret_password_lookup_sync (SECRET_SCHEMA, NULL, &error, "application", "chromium", NULL);
  else
    return NULL;

  if (error) {
    g_warning ("Could not read secret phrase: %s\n", error->message);

    return NULL;
  }

  return phrase;
}

static char *
decrypt (unsigned char *password,
         int            password_length,
         char          *phrase)
{
  struct CBC_CTX (struct aes128_ctx, AES_BLOCK_SIZE) aes;
  unsigned char iv[16] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
  unsigned char salt[9] = {'s', 'a', 'l', 't', 'y', 's', 'a', 'l', 't'};
  unsigned char key[16];
  char *out;

  pbkdf2_hmac_sha1 (strlen (phrase), (unsigned char *)phrase, 1, sizeof (salt), salt, sizeof (key), key);

  out = g_malloc0 (password_length + 1);
  aes128_set_decrypt_key (&aes.ctx, key);
  CBC_SET_IV (&aes, iv);
  CBC_DECRYPT (&aes, aes128_decrypt, password_length, (unsigned char *)out, password);

  /* Remove padding */
  for (int i = 0; i < password_length; i++) {
    if (!g_ascii_isprint (out[i]))
      out[i] = '\0';
  }

  return out;
}

gboolean
ephy_password_import_from_chrome (EphyPasswordManager  *manager,
                                  ChromeType            type,
                                  GError              **error)
{
  g_autoptr (EphySQLiteConnection) connection = NULL;
  g_autoptr (EphySQLiteStatement) statement = NULL;
  g_autoptr (GError) my_error = NULL;
  g_autofree char *secret_phrase = NULL;
  g_autofree char *filename = NULL;
  const char *statement_str = "SELECT origin_url, action_url, username_element, username_value, password_element, password_value FROM logins WHERE blacklisted_by_user = 0";

  if (type == CHROME)
    filename = g_build_filename (g_get_user_config_dir (), "google-chrome", "Default", "Login Data", NULL);
  else if (type == CHROMIUM)
    filename = g_build_filename (g_get_user_config_dir (), "chromium", "Default", "Login Data", NULL);
  else
    return FALSE;


  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_MEMORY, filename);
  if (!connection) {
    g_set_error (error,
                 PASSWORDS_IMPORT_ERROR,
                 PASSWORDS_IMPORT_ERROR_PASSWORDS,
                 _("Cannot create SQLite connection. Close browser and try again."));
    return FALSE;
  }

  if (!ephy_sqlite_connection_open (connection, &my_error)) {
    g_warning ("Error during opening connection: %s", my_error->message);
    g_set_error (error,
                 PASSWORDS_IMPORT_ERROR,
                 PASSWORDS_IMPORT_ERROR_PASSWORDS,
                 _("Browser password database could not be opened. Close browser and try again."));
    return FALSE;
  }

  statement = ephy_sqlite_connection_create_statement (connection, statement_str, &my_error);
  if (my_error) {
    g_warning ("Could not build password query statement: %s", my_error->message);
    g_set_error (error,
                 PASSWORDS_IMPORT_ERROR,
                 PASSWORDS_IMPORT_ERROR_PASSWORDS,
                 _("Browser password database could not be opened. Close browser and try again."));

    ephy_sqlite_connection_close (connection);
    return FALSE;
  }

  while (ephy_sqlite_statement_step (statement, &my_error)) {
    const char *origin = ephy_sqlite_statement_get_column_as_string (statement, 0);
    const char *target_origin = ephy_sqlite_statement_get_column_as_string (statement, 1);
    const char *username_field = ephy_sqlite_statement_get_column_as_string (statement, 2);
    const char *username = ephy_sqlite_statement_get_column_as_string (statement, 3);
    const char *password_field = ephy_sqlite_statement_get_column_as_string (statement, 4);
    const void *password = ephy_sqlite_statement_get_column_as_blob (statement, 5);
    int password_size = ephy_sqlite_statement_get_column_size (statement, 5);
    g_autofree char *decrypted_password = NULL;
    g_autofree char *secure_origin = NULL;
    g_autofree char *secure_target_origin = NULL;
    gboolean exists;

    /* Skip unsupported protocols */
    if (!g_str_has_prefix (origin, "http") && !g_str_has_prefix (origin, "https"))
      continue;

    if (!password)
      continue;

    if (!secret_phrase) {
      if (memcmp (password, "v11", 3) == 0) {
        /* V11: System based password manager, we only support libsecret */
        secret_phrase = get_libsecret_phrase (type);
      } else if (strncmp (password, "v10", 3) == 0) {
        /* V10: Browser based master key: peanuts */
        secret_phrase = g_strdup ("peanuts");
      }

      if (!secret_phrase)
        continue;
    }

    decrypted_password = decrypt ((unsigned char *)(password) + 3, password_size - 3, secret_phrase);
    secure_origin = ephy_uri_to_security_origin (origin);

    secure_target_origin = ephy_uri_to_security_origin (target_origin);

    if (!secure_target_origin)
      secure_target_origin = g_strdup (secure_origin);

    exists = ephy_password_manager_find (manager,
                                         secure_origin,
                                         secure_target_origin,
                                         username,
                                         username_field,
                                         password_field);

    ephy_password_manager_save (manager,
                                secure_origin,
                                secure_target_origin,
                                username,
                                decrypted_password,
                                username_field,
                                password_field,
                                !exists);
  }

  ephy_sqlite_connection_close (connection);

  return TRUE;
}

typedef struct {
  ChromeType type;
  EphyPasswordManager *manager;
} PasswordImportChromeData;

static void
ephy_password_import_from_chrome_data_free (PasswordImportChromeData *data)
{
  g_object_unref (data->manager);

  g_free (data);
}

static void
ephy_password_import_from_chrome_thread_cb (GTask        *task,
                                            gpointer      source_object,
                                            gpointer      task_data,
                                            GCancellable *cancellable)
{
  PasswordImportChromeData *data = task_data;
  GError *error = NULL;
  gboolean retval;

  retval = ephy_password_import_from_chrome (data->manager, data->type, &error);
  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, retval);
}

void
ephy_password_import_from_chrome_async (EphyPasswordManager *manager,
                                        ChromeType           type,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  PasswordImportChromeData *data = NULL;

  g_assert (manager);

  task = g_task_new (NULL, NULL, callback, user_data);
  g_task_set_source_tag (task, ephy_password_import_from_chrome_async);

  data = g_new0 (PasswordImportChromeData, 1);
  data->type = type;
  data->manager = g_object_ref (manager);

  g_task_set_task_data (task, data, (void *)ephy_password_import_from_chrome_data_free);

  g_task_run_in_thread (task, ephy_password_import_from_chrome_thread_cb);
}

gboolean
ephy_password_import_from_chrome_finish (GObject       *object,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  g_assert (g_task_is_valid (result, object));
  g_assert (error && !*error);

  return g_task_propagate_boolean (G_TASK (result), error);
}

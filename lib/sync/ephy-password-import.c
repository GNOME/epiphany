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

#include <glib/gi18n.h>

#include <nettle/pbkdf2.h>
#include <nettle/aes.h>
#include <nettle/cbc.h>

#define PASSWORDS_IMPORT_ERROR passwords_import_error_quark ()
#define SECRET_SCHEMA  libsecret_get_schema ()

GQuark passwords_import_error_quark (void);
G_DEFINE_QUARK (passwords - import - error - quark, passwords_import_error)

typedef enum {
  PASSWORDS_IMPORT_ERROR_PASSWORDS = 1001
} PasswordsImportErrorCode;

const SecretSchema *libsecret_get_schema(void)
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
get_libsecret_phrase (void)
{
  g_autoptr (GError) error = NULL;
  char *phrase;

  phrase = secret_password_lookup_sync (SECRET_SCHEMA, NULL, &error, "application", "chrome", NULL);
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
  unsigned char *salt = (unsigned char *)"saltysalt";
  g_autofree unsigned char *key = NULL;
  char *out;

  key = g_malloc0 (16);
  pbkdf2_hmac_sha1 (strlen (phrase), (unsigned char *)phrase, 1, 9, (unsigned char *)salt, 16, key);

  out = g_malloc0 (password_length + 1);
  aes128_set_decrypt_key (&aes.ctx, key);
  CBC_SET_IV (&aes, iv);
  CBC_DECRYPT (&aes, aes128_decrypt, password_length, (unsigned char *)(out), password);

  /* Remove padding */
  for (int i = 0; i < password_length; i++) {
    if (!g_ascii_isprint (out[i]))
      out[i] = '\0';
  }

  return out;
}

gboolean
ephy_password_import_from_chrome (EphyPasswordManager  *manager,
                                  char                 *filename,
                                  GError              **error)
{
  g_autoptr (EphySQLiteConnection) connection = NULL;
  g_autoptr (EphySQLiteStatement) statement = NULL;
  g_autoptr (GError) my_error = NULL;
  g_autofree char *secret_phrase = NULL;
  const char *statement_str = "SELECT origin_url, action_url, username_element, username_value, password_element, password_value, id FROM logins";

  connection = ephy_sqlite_connection_new (EPHY_SQLITE_CONNECTION_MODE_MEMORY, filename);
  if (!connection) {
    g_set_error (error,
                 PASSWORDS_IMPORT_ERROR,
                 PASSWORDS_IMPORT_ERROR_PASSWORDS,
                 _("Cannot create sqlite connection. Close Chrome and try again."));
    return FALSE;
  }

  if (!ephy_sqlite_connection_open (connection, &my_error)) {
    g_warning ("Error during opening connection: %s", my_error->message);
    g_set_error (error,
                 PASSWORDS_IMPORT_ERROR,
                 PASSWORDS_IMPORT_ERROR_PASSWORDS,
                 _("Chrome password database could not be opened. Close Chrome and try again."));
    return FALSE;
  }

  statement = ephy_sqlite_connection_create_statement (connection, statement_str, &my_error);
  if (my_error) {
    g_warning ("Could not build password query statement: %s", my_error->message);
    g_set_error (error,
                 PASSWORDS_IMPORT_ERROR,
                 PASSWORDS_IMPORT_ERROR_PASSWORDS,
                 _("Chrome password database could not be opened. Close Chrome and try again."));

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
    int id = ephy_sqlite_statement_get_column_as_int (statement, 6);

    if (!password)
      continue;

    if (!secret_phrase) {
      if (memcmp (password, "v11", 3) == 0) {
        secret_phrase = get_libsecret_phrase ();
      } else if (strncmp (password, "v10", 3) == 0) {
        secret_phrase = g_strdup ("peanuts");
      } else {
        continue;
      }
    }

    decrypted_password = decrypt ((unsigned char *)(password) + 3, password_size - 3, secret_phrase);

    ephy_password_manager_save (manager,
                                origin,
                                target_origin,
                                username,
                                decrypted_password,
                                username_field,
                                password_field,
                                TRUE);
  }

  ephy_sqlite_connection_close (connection);

  return TRUE;
}

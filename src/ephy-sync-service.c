/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ephy-debug.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-sync-crypto.h"
#include "ephy-sync-secret.h"
#include "ephy-sync-service.h"
#include "ephy-sync-utils.h"

#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

#define BASEURL     "https://api.accounts.firefox.com/v1"

struct _EphySyncService {
  GObject parent_instance;

  SoupSession *soup_session;

  gchar *user_email;
  GHashTable *tokens;
};


G_DEFINE_TYPE (EphySyncService, ephy_sync_service, G_TYPE_OBJECT);

static void
ephy_sync_service_finalize (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  g_free (self->user_email);
  g_clear_object (&self->tokens);
  g_clear_object (&self->soup_session);

  G_OBJECT_CLASS (ephy_sync_service_parent_class)->finalize (object);
}

static void
ephy_sync_service_class_init (EphySyncServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_sync_service_finalize;
}

static void
ephy_sync_service_init (EphySyncService *self)
{
  gchar *sync_user = NULL;

  self->tokens = g_hash_table_new_full (NULL, g_str_equal,
                                        NULL, g_free);
  self->soup_session = soup_session_new ();

  sync_user = g_settings_get_string (EPHY_SETTINGS_MAIN,
                                     EPHY_PREFS_SYNC_USER);

  if (sync_user && sync_user[0]) {
    ephy_sync_service_set_user_email (self, sync_user);
    ephy_sync_secret_load_tokens (self);
  }

LOG ("[%d] sync service inited", __LINE__);
}

static SoupMessage *
synchronous_post_request (EphySyncService *self,
                          const gchar     *endpoint,
                          gchar           *request_body)
{
  SoupMessage *message;

  message = soup_message_new (SOUP_METHOD_POST,
                              g_strdup_printf ("%s/%s", BASEURL, endpoint));
  soup_message_set_request (message,
                            "application/json",
                            SOUP_MEMORY_TAKE,
                            request_body,
                            strlen (request_body));
LOG ("[%d] Sending synchronous POST request to %s endpoint", __LINE__, endpoint);
  soup_session_send_message (self->soup_session, message);

  return message;
}

EphySyncService *
ephy_sync_service_new (void)
{
  return EPHY_SYNC_SERVICE (g_object_new (EPHY_TYPE_SYNC_SERVICE,
                                          NULL));
}

gchar *
ephy_sync_service_get_user_email (EphySyncService *self)
{
  return self->user_email;
}

void
ephy_sync_service_set_user_email (EphySyncService *self,
                                  const gchar     *emailUTF8)
{
  g_free (self->user_email);
  self->user_email = g_strdup (emailUTF8);
}

gchar *
ephy_sync_service_get_token (EphySyncService   *self,
                             EphySyncTokenType  token_type)
{
  const gchar *token_name;
  gchar *token_value;

  token_name = ephy_sync_utils_token_name_from_type (token_type);
  token_value = (gchar *) g_hash_table_lookup (self->tokens, token_name);

LOG ("[%d] Returning token %s with value %s", __LINE__, token_name, token_value);

  return token_value;
}

void
ephy_sync_service_save_token (EphySyncService   *self,
                              EphySyncTokenType  token_type,
                              gchar             *token_value)
{
  const gchar *token_name;

LOG ("[%d] token_type: %d", __LINE__, token_type);
  token_name = ephy_sync_utils_token_name_from_type (token_type);
  g_hash_table_insert (self->tokens,
                       (gpointer) token_name,
                       (gpointer) token_value);

LOG ("[%d] Saved token %s with value %s", __LINE__, token_name, token_value);
}

void
ephy_sync_service_delete_token (EphySyncService   *self,
                                EphySyncTokenType  token_type)
{
  const gchar *token_name;

  token_name = ephy_sync_utils_token_name_from_type (token_type);
  g_hash_table_remove (self->tokens, token_name);

LOG ("[%d] Deleted token %s", __LINE__, token_name);
}

void
ephy_sync_service_delete_all_tokens (EphySyncService *self)
{
  g_hash_table_remove_all (self->tokens);

LOG ("[%d] Deleted all tokens", __LINE__);
}

gboolean
ephy_sync_service_login (EphySyncService  *self,
                         guint            *error_code,
                         gchar           **error_message)
{
  SoupMessage *message;
  JsonParser *parser;
  JsonNode *root;
  JsonObject *object;
  gchar *request_body;
  gchar *quickStretchedPW;
  gchar *authPW;
  gchar *unwrapBKey;
  gchar *uid;
  gchar *sessionToken;
  gchar *keyFetchToken;

  authPW = ephy_sync_service_get_token (self, EPHY_SYNC_TOKEN_AUTHPW);
  g_return_val_if_fail (authPW, FALSE);

  request_body = g_strconcat ("{\"authPW\": \"", authPW,
                              "\", \"email\": \"", self->user_email,
                              "\"}", NULL);

  message = synchronous_post_request (self,
                                      "account/login?keys=true",
                                      request_body);
LOG ("[%d] status code: %u", __LINE__, message->status_code);

  parser = json_parser_new ();
  json_parser_load_from_data (parser, message->response_body->data, -1, NULL);
  root = json_parser_get_root (parser);
  g_assert (JSON_NODE_HOLDS_OBJECT (root));
  object = json_node_get_object (root);

  if (message->status_code != 200) {
    *error_message = g_strdup (json_object_get_string_member (object, "message"));
    *error_code = json_object_get_int_member (object, "errno");

LOG ("[%d] errno: %u, errmsg: %s", __LINE__, *error_code, *error_message);

    ephy_sync_service_delete_all_tokens (self);
    ephy_sync_service_set_user_email (self, NULL);

    return FALSE;
  }

  /* Extract uid, sesionToken, keyFetchToken */
  uid = g_strdup (json_object_get_string_member (object, "uid"));
  sessionToken = g_strdup (json_object_get_string_member (object, "sessionToken"));
  keyFetchToken = g_strdup (json_object_get_string_member (object, "keyFetchToken"));

  /* Save tokens in memory */
  ephy_sync_service_save_token (self,
                                EPHY_SYNC_TOKEN_UID,
                                uid);
  ephy_sync_service_save_token (self,
                                EPHY_SYNC_TOKEN_SESSIONTOKEN,
                                sessionToken);
  ephy_sync_service_save_token (self,
                                EPHY_SYNC_TOKEN_KEYFETCHTOKEN,
                                keyFetchToken);

  /* Store tokens on disk */
  quickStretchedPW = ephy_sync_service_get_token (self, EPHY_SYNC_TOKEN_QUICKSTRETCHEDPW);
  unwrapBKey = ephy_sync_service_get_token (self, EPHY_SYNC_TOKEN_UNWRAPBKEY);
  ephy_sync_secret_store_token (self->user_email,
                                EPHY_SYNC_TOKEN_AUTHPW,
                                authPW);
  ephy_sync_secret_store_token (self->user_email,
                                EPHY_SYNC_TOKEN_KEYFETCHTOKEN,
                                keyFetchToken);
  ephy_sync_secret_store_token (self->user_email,
                                EPHY_SYNC_TOKEN_SESSIONTOKEN,
                                sessionToken);
  ephy_sync_secret_store_token (self->user_email,
                                EPHY_SYNC_TOKEN_UID,
                                uid);
  ephy_sync_secret_store_token (self->user_email,
                                EPHY_SYNC_TOKEN_UNWRAPBKEY,
                                unwrapBKey);
  ephy_sync_secret_store_token (self->user_email,
                                EPHY_SYNC_TOKEN_QUICKSTRETCHEDPW,
                                quickStretchedPW);

  return TRUE;
}

void
ephy_sync_service_stretch (EphySyncService *self,
                           const gchar     *emailUTF8,
                           const gchar     *passwordUTF8)
{
  gchar *salt_stretch;
  gchar *info_auth;
  gchar *info_unwrap;
  guint8 *quickStretchedPW;
  guint8 *authPW;
  guint8 *unwrapBKey;

  salt_stretch = ephy_sync_utils_kwe ("quickStretch", emailUTF8);
  quickStretchedPW = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  ephy_sync_crypto_pbkdf2_1k ((guint8 *) passwordUTF8,
                              strlen (passwordUTF8),
                              (guint8 *) salt_stretch,
                              strlen (salt_stretch),
                              quickStretchedPW,
                              EPHY_SYNC_TOKEN_LENGTH);

ephy_sync_utils_display_hex ("quickStretchedPW", quickStretchedPW, EPHY_SYNC_TOKEN_LENGTH);

  info_auth = ephy_sync_utils_kw ("authPW");
  authPW = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  ephy_sync_crypto_hkdf (quickStretchedPW,
                         EPHY_SYNC_TOKEN_LENGTH,
                         NULL, 0,
                         (guint8 *) info_auth,
                         strlen (info_auth),
                         authPW,
                         EPHY_SYNC_TOKEN_LENGTH);

  info_unwrap = ephy_sync_utils_kw ("unwrapBkey");
  unwrapBKey = g_malloc (EPHY_SYNC_TOKEN_LENGTH);
  ephy_sync_crypto_hkdf (quickStretchedPW,
                         EPHY_SYNC_TOKEN_LENGTH,
                         NULL, 0,
                         (guint8 *) info_unwrap,
                         strlen (info_unwrap),
                         unwrapBKey,
                         EPHY_SYNC_TOKEN_LENGTH);

LOG ("[%d] Stretching done", __LINE__);

  ephy_sync_service_set_user_email (self, emailUTF8);
  ephy_sync_service_save_token (self,
                                EPHY_SYNC_TOKEN_QUICKSTRETCHEDPW,
                                ephy_sync_utils_encode_hex (quickStretchedPW,
                                                            EPHY_SYNC_TOKEN_LENGTH));
  ephy_sync_service_save_token (self,
                                EPHY_SYNC_TOKEN_AUTHPW,
                                ephy_sync_utils_encode_hex (authPW,
                                                            EPHY_SYNC_TOKEN_LENGTH));
  ephy_sync_service_save_token (self,
                                EPHY_SYNC_TOKEN_UNWRAPBKEY,
                                ephy_sync_utils_encode_hex (unwrapBKey,
                                                            EPHY_SYNC_TOKEN_LENGTH));

  g_free (salt_stretch);
  g_free (info_unwrap);
  g_free (info_auth);
  g_free (quickStretchedPW);
  g_free (authPW);
  g_free (unwrapBKey);
}

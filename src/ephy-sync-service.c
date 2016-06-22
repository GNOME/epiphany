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
#include "ephy-sync-crypto.h"
#include "ephy-sync-secret.h"
#include "ephy-sync-service.h"
#include "ephy-sync-utils.h"

#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

G_DEFINE_TYPE (EphySyncService, ephy_sync_service, G_TYPE_OBJECT);

static void
ephy_sync_service_finalize (GObject *object)
{
  EphySyncService *self = EPHY_SYNC_SERVICE (object);

  g_free (self->user_email);
  g_hash_table_destroy (self->tokens);

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
  self->tokens = g_hash_table_new_full (NULL, g_str_equal,
                                        NULL, g_free);

LOG ("%s:%d", __func__, __LINE__);
}

static void
server_response_cb (SoupSession *session,
                    SoupMessage *message,
                    gpointer     user_data)
{
  if (message->status_code == 200) {
LOG ("response body: %s", message->response_body->data);
    // TODO: parse response data using JsonParser
  } else {
LOG ("Error response from server: [%u] %s", message->status_code, message->reason_phrase);
  }
}

EphySyncService *
ephy_sync_service_new (void)
{
  return EPHY_SYNC_SERVICE (g_object_new (EPHY_TYPE_SYNC_SERVICE,
                                          NULL));
}

gchar *
ephy_sync_service_get_token (EphySyncService *self,
                             const gchar     *token_name)
{
  GHashTableIter iter;
  gchar *key, *value;

  g_hash_table_iter_init (&iter, self->tokens);

  while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &value)) {
      if (g_strcmp0 (token_name, key) == 0) {
LOG ("[%d] Returning token %s with value %s", __LINE__, key, value);
        return value;
      }
  }

  return NULL;
}

void
ephy_sync_service_set_token (EphySyncService *self,
                             const gchar     *token_name,
                             const gchar     *token_value_hex)
{
  g_hash_table_insert (self->tokens,
                       (gpointer) token_name,
                       (gpointer) token_value_hex);

LOG ("[%d] Set token %s with value %s", __LINE__, token_name, token_value_hex);
}

void
ephy_sync_service_login (EphySyncService *self)
{
  SoupSession *session;
  SoupMessage *message;
  gchar *request_body;
  gchar *authPW;

  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));

LOG ("%s:%d Preparing soup message", __func__, __LINE__);

  session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT,
                                           "test-json",
                                           NULL);
  message = soup_message_new (SOUP_METHOD_POST,
                              "https://api.accounts.firefox.com/v1/account/login");

  authPW = ephy_sync_service_get_token (self, "authPW");
  g_assert (authPW != NULL);

  request_body = g_strconcat ("{\"authPW\": \"",
                              authPW,
                              "\", \"email\": \"",
                              self->user_email,
                              "\"}",
                              NULL);

  soup_message_set_request (message,
                            "application/json",
                            SOUP_MEMORY_COPY,
                            request_body,
                            strlen (request_body));

  soup_session_queue_message (session, message, server_response_cb, NULL);
LOG ("%s:%d Queued the soup message", __func__, __LINE__);

  // TODO: find a way to safely free request_body
  // TODO: find a way to safely destroy session, message
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

  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));

LOG ("%s:%d", __func__, __LINE__);

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

  self->user_email = g_strdup (emailUTF8);
  ephy_sync_service_set_token (self, "quickStretchedPW", ephy_sync_utils_encode_hex (quickStretchedPW, EPHY_SYNC_TOKEN_LENGTH));
  ephy_sync_service_set_token (self, "authPW", ephy_sync_utils_encode_hex (authPW, EPHY_SYNC_TOKEN_LENGTH));
  ephy_sync_service_set_token (self, "unwrapBKey", ephy_sync_utils_encode_hex (unwrapBKey, EPHY_SYNC_TOKEN_LENGTH));

  ephy_sync_secret_store_token (self->user_email,
                                "quickStretchedPW",
                                ephy_sync_service_get_token (self, "quickStretchedPW"),
                                NULL, NULL);
  ephy_sync_secret_store_token (self->user_email,
                                "authPW",
                                ephy_sync_service_get_token (self, "authPW"),
                                NULL, NULL);
  ephy_sync_secret_store_token (self->user_email,
                                "unwrapBKey",
                                ephy_sync_service_get_token (self, "unwrapBKey"),
                                NULL, NULL);

  g_free (salt_stretch);
  g_free (info_unwrap);
  g_free (info_auth);
  g_free (quickStretchedPW);
  g_free (authPW);
  g_free (unwrapBKey);
}

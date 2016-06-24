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

struct _EphySyncService {
  GObject parent_instance;

  gchar *user_email;
  GHashTable *tokens;
};


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
  gchar *sync_user = NULL;

  self->tokens = g_hash_table_new_full (NULL, g_str_equal,
                                        NULL, g_free);

  sync_user = g_settings_get_string (EPHY_SETTINGS_MAIN,
                                     EPHY_PREFS_SYNC_USER);

  if (sync_user && sync_user[0]) {
    ephy_sync_service_set_user_email (self, sync_user);
    ephy_sync_secret_load_tokens (self);
  }

LOG ("[%d] sync service inited", __LINE__);
}

static void
server_response_cb (SoupSession *session,
                    SoupMessage *message,
                    gpointer     user_data)
{
  if (message->status_code == 200) {
LOG ("[%d] response body: %s", __LINE__, message->response_body->data);
    // TODO: parse response data using JsonParser
  } else {
LOG ("[%d] Error response from server: [%u] %s", __LINE__, message->status_code, message->reason_phrase);
  }
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

void
ephy_sync_service_login (EphySyncService *self)
{
  SoupSession *session;
  SoupMessage *message;
  gchar *request_body;
  gchar *authPW;


LOG ("[%d] Preparing soup message", __LINE__);

  session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT,
                                           "test-json",
                                           NULL);
  message = soup_message_new (SOUP_METHOD_POST,
                              "https://api.accounts.firefox.com/v1/account/login");

  authPW = ephy_sync_service_get_token (self, EPHY_SYNC_TOKEN_AUTHPW);
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
LOG ("[%d] Queued the soup message", __LINE__);

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

  g_free (salt_stretch);
  g_free (info_unwrap);
  g_free (info_auth);
  g_free (quickStretchedPW);
  g_free (authPW);
  g_free (unwrapBKey);
}

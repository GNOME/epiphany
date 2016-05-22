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
#include "ephy-sync-service.h"

#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string.h>

struct _EphySyncService {
  GObject parent_instance;
};

G_DEFINE_TYPE (EphySyncService, ephy_sync_service, G_TYPE_OBJECT);

static void
ephy_sync_service_class_init (EphySyncServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class = object_class; // suppress warnings

LOG ("%s:%d", __func__, __LINE__);

  // TODO: Set finalize, dispose, set/get property methods
}

static void
ephy_sync_service_init (EphySyncService *self)
{
LOG ("%s:%d", __func__, __LINE__);
}

static void
server_response_cb (SoupSession *session,
                    SoupMessage *message,
                    gpointer user_data)
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
LOG ("%s:%d", __func__, __LINE__);

  return EPHY_SYNC_SERVICE (g_object_new (EPHY_TYPE_SYNC_SERVICE,
                                          NULL));
}

void
ephy_sync_service_try_login (EphySyncService *self,
                             gboolean login_with_keys,
                             const gchar *emailUTF8,
                             guint8 *authPW,
                             guint8 *sessionToken,
                             guint8 *keyFetchToken)
{
  SoupSession *session;
  SoupMessage *message;
  char *request_body;
  char *authPW_hex;

  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));

LOG ("%s:%d", __func__, __LINE__);

  session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT,
                                           "test-json",
                                           NULL);
  message = soup_message_new (SOUP_METHOD_POST,
                              "https://api.accounts.firefox.com/v1/account/login");

  authPW_hex = ephy_sync_crypto_encode_hex (authPW, EPHY_SYNC_SERVICE_TOKEN_LENGTH);
  request_body = g_strconcat ("{\"authPW\": \"",
                              authPW_hex,
                              "\", \"email\": \"",
                              emailUTF8,
                              "\"}",
                              NULL);

  soup_message_set_request (message,
                            "application/json",
                            SOUP_MEMORY_COPY,
                            request_body,
                            strlen (request_body));

  soup_session_queue_message (session, message, server_response_cb, NULL);

  // TODO: find a way to safely free authPW_hex, request_body
  // TODO: find a way to safely destroy session, message
}

void
ephy_sync_service_stretch (EphySyncService *self,
                           const gchar *emailUTF8,
                           const gchar *passwordUTF8,
                           guint8 *authPW,
                           guint8 *unwrapBKey)
{
  gchar *salt_stretch;
  gchar *info_auth;
  gchar *info_unwrap;
  guint8 *quickStretchedPW;

  g_return_if_fail (EPHY_IS_SYNC_SERVICE (self));

LOG ("%s:%d", __func__, __LINE__);

  salt_stretch = ephy_sync_crypto_kwe ("quickStretch", emailUTF8);
  quickStretchedPW = g_malloc (EPHY_SYNC_SERVICE_TOKEN_LENGTH);
  ephy_sync_crypto_pbkdf2_1k ((guint8 *) passwordUTF8,
                              strlen (passwordUTF8),
                              (guint8 *) salt_stretch,
                              strlen (salt_stretch),
                              quickStretchedPW,
                              EPHY_SYNC_SERVICE_TOKEN_LENGTH);

ephy_sync_crypto_display_hex (quickStretchedPW, EPHY_SYNC_SERVICE_TOKEN_LENGTH, "quickStretchedPW");

  info_auth = ephy_sync_crypto_kw ("authPW");
  ephy_sync_crypto_hkdf (quickStretchedPW,
                         EPHY_SYNC_SERVICE_TOKEN_LENGTH,
                         NULL, 0,
                         (guint8 *) info_auth,
                         strlen (info_auth),
                         authPW,
                         EPHY_SYNC_SERVICE_TOKEN_LENGTH);

  info_unwrap = ephy_sync_crypto_kw ("unwrapBkey");
  ephy_sync_crypto_hkdf (quickStretchedPW,
                         EPHY_SYNC_SERVICE_TOKEN_LENGTH,
                         NULL, 0,
                         (guint8 *) info_unwrap,
                         strlen (info_unwrap),
                         unwrapBKey,
                         EPHY_SYNC_SERVICE_TOKEN_LENGTH);

  g_free (salt_stretch);
  g_free (info_unwrap);
  g_free (info_auth);
  g_free (quickStretchedPW);
}

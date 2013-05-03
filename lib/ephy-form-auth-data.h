/*
 *  Copyright © 2013 Igalia S.L.
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

#ifndef EPHY_FORM_AUTH_DATA_H
#define EPHY_FORM_AUTH_DATA_H

#define SECRET_API_SUBJECT_TO_CHANGE

#include <glib.h>
#include <libsecret/secret.h>

#define URI_KEY           "uri"
#define FORM_USERNAME_KEY "form_username"
#define FORM_PASSWORD_KEY "form_password"
#define USERNAME_KEY      "username"

void ephy_form_auth_data_store                (const char *uri,
                                               const char *form_username,
                                               const char *form_password,
                                               const char *username,
                                               const char *password,
                                               GAsyncReadyCallback callback,
                                               gpointer userdata);

gboolean ephy_form_auth_data_store_finish     (GAsyncResult *result,
                                               GError **error);

typedef void (*EphyFormAuthDataQueryCallback) (const char *username,
                                               const char *password,
                                               gpointer user_data);

void ephy_form_auth_data_query                (const char *uri,
                                               const char *form_username,
                                               const char *form_password,
                                               const char *username,
                                               EphyFormAuthDataQueryCallback callback,
                                               gpointer user_data,
                                               GDestroyNotify destroy_data);

const SecretSchema *ephy_form_auth_data_get_password_schema (void) G_GNUC_CONST;

#define EPHY_FORM_PASSWORD_SCHEMA ephy_form_auth_data_get_password_schema ()

typedef struct {
  char *form_username;
  char *form_password;
  char *username;
} EphyFormAuthData;

typedef struct _EphyFormAuthDataCache EphyFormAuthDataCache;

EphyFormAuthDataCache *ephy_form_auth_data_cache_new      (void);
void                   ephy_form_auth_data_cache_free     (EphyFormAuthDataCache *cache);
void                   ephy_form_auth_data_cache_add      (EphyFormAuthDataCache *cache,
                                                           const char            *uri,
                                                           const char            *form_username,
                                                           const char            *form_password,
                                                           const char            *username);
GSList                *ephy_form_auth_data_cache_get_list (EphyFormAuthDataCache *cache,
                                                           const char            *uri);

#endif

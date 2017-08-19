/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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

#pragma once

#include "ephy-password-record.h"

#include <glib-object.h>
#include <libsecret/secret.h>

G_BEGIN_DECLS

const SecretSchema *ephy_password_manager_get_password_schema (void) G_GNUC_CONST;

#define ID_KEY                    "id"
#define HOSTNAME_KEY              "uri"
#define USERNAME_FIELD_KEY        "form_username"
#define PASSWORD_FIELD_KEY        "form_password"
#define USERNAME_KEY              "username"
#define SERVER_TIME_MODIFIED_KEY  "server_time_modified"

#define EPHY_FORM_PASSWORD_SCHEMA ephy_password_manager_get_password_schema ()

#define EPHY_TYPE_PASSWORD_MANAGER (ephy_password_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyPasswordManager, ephy_password_manager, EPHY, PASSWORD_MANAGER, GObject)

typedef void (*EphyPasswordManagerQueryCallback) (GList *records, gpointer user_data);

EphyPasswordManager *ephy_password_manager_new                      (void);
GList               *ephy_password_manager_get_cached_users_for_uri (EphyPasswordManager *self,
                                                                     const char          *uri);
void                 ephy_password_manager_save                     (EphyPasswordManager *self,
                                                                     const char          *uri,
                                                                     const char          *username,
                                                                     const char          *password,
                                                                     const char          *username_field,
                                                                     const char          *password_field,
                                                                     gboolean             is_new);
void                 ephy_password_manager_query                    (EphyPasswordManager              *self,
                                                                     const char                       *id,
                                                                     const char                       *uri,
                                                                     const char                       *username,
                                                                     const char                       *username_field,
                                                                     const char                       *password_field,
                                                                     EphyPasswordManagerQueryCallback  callback,
                                                                     gpointer                          user_data);
void                 ephy_password_manager_forget                    (EphyPasswordManager *self,
                                                                      const char          *id);
void                 ephy_password_manager_forget_all                (EphyPasswordManager *self);
/* Note: Below functions are deprecated and should not be used in newly written code.
 * The only reason they still exist is that the profile migrator expects them. */
void                 ephy_password_manager_store_raw                 (const char          *uri,
                                                                      const char          *username,
                                                                      const char          *password,
                                                                      const char          *username_field,
                                                                      const char          *password_field,
                                                                      GAsyncReadyCallback  callback,
                                                                      gpointer             user_data);
gboolean             ephy_password_manager_store_finish              (GAsyncResult  *result,
                                                                      GError       **error);

G_END_DECLS

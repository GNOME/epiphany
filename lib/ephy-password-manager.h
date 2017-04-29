/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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

#define URI_KEY           "uri"
#define FORM_USERNAME_KEY "form_username"
#define FORM_PASSWORD_KEY "form_password"
#define USERNAME_KEY      "username"

#define EPHY_FORM_PASSWORD_SCHEMA ephy_password_manager_get_password_schema ()

#define EPHY_TYPE_PASSWORD_MANAGER (ephy_password_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyPasswordManager, ephy_password_manager, EPHY, PASSWORD_MANAGER, GObject)

typedef void (*EphyPasswordManagerQueryCallback) (GSList *records, gpointer user_data);

EphyPasswordManager *ephy_password_manager_new                (void);
GSList              *ephy_password_manager_get_cached_by_uri  (EphyPasswordManager *self,
                                                               const char          *uri);
void                 ephy_password_manager_save               (EphyPasswordManager *self,
                                                               const char          *uri,
                                                               const char          *form_username,
                                                               const char          *form_password,
                                                               const char          *username,
                                                               const char          *password);
void                 ephy_password_manager_query              (EphyPasswordManager              *self,
                                                               const char                       *uri,
                                                               const char                       *form_username,
                                                               const char                       *form_password,
                                                               const char                       *username,
                                                               EphyPasswordManagerQueryCallback  callback,
                                                               gpointer                          user_data);
void                 ephy_password_manager_store               (const char          *uri,
                                                                const char          *form_username,
                                                                const char          *form_password,
                                                                const char          *username,
                                                                const char          *password,
                                                                GAsyncReadyCallback  callback,
                                                                gpointer             user_data);
gboolean             ephy_password_manager_store_finish        (GAsyncResult  *result,
                                                                GError       **error);
void                 ephy_password_manager_forget              (EphyPasswordManager *self,
                                                                const char          *origin,
                                                                const char          *form_username,
                                                                const char          *form_password,
                                                                const char          *username);
void                 ephy_password_manager_forget_all          (EphyPasswordManager *self);

G_END_DECLS

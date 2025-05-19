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
#include <jsc/jsc.h>
#include <libsecret/secret.h>

G_BEGIN_DECLS

const SecretSchema *ephy_password_manager_get_password_schema (void) G_GNUC_CONST;

#define ID_KEY                    "id"
#define ORIGIN_KEY                "uri" /* TODO: Rename to "origin". Requires migration. */
#define TARGET_ORIGIN_KEY         "target_origin"
#define USERNAME_FIELD_KEY        "form_username"
#define PASSWORD_FIELD_KEY        "form_password"
#define USERNAME_KEY              "username"
#define SERVER_TIME_MODIFIED_KEY  "server_time_modified"

#define EPHY_FORM_PASSWORD_SCHEMA ephy_password_manager_get_password_schema ()

#define EPHY_TYPE_PASSWORD_MANAGER (ephy_password_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyPasswordManager, ephy_password_manager, EPHY, PASSWORD_MANAGER, GObject)

typedef void (*EphyPasswordManagerQueryCallback) (GList *records, gpointer user_data);

typedef struct {
  char *origin;
  char *target_origin;
  char *username;
  char *password;
  char *usernameField;
  char *passwordField;
  gboolean isNew;
} EphyPasswordRequestData;

void                 ephy_password_request_data_free                (EphyPasswordRequestData *request_data);

EphyPasswordManager *ephy_password_manager_new                      (void);
GList               *ephy_password_manager_get_usernames_for_origin (EphyPasswordManager *self,
                                                                     const char          *origin);
void                 ephy_password_manager_save                     (EphyPasswordManager *self,
                                                                     const char          *origin,
                                                                     const char          *target_origin,
                                                                     const char          *username,
                                                                     const char          *new_username,
                                                                     const char          *password,
                                                                     const char          *username_field,
                                                                     const char          *password_field,
                                                                     gboolean             is_new);
void                 ephy_password_manager_query                    (EphyPasswordManager              *self,
                                                                     const char                       *id,
                                                                     const char                       *origin,
                                                                     const char                       *target_origin,
                                                                     const char                       *username,
                                                                     const char                       *username_field,
                                                                     const char                       *password_field,
                                                                     EphyPasswordManagerQueryCallback  callback,
                                                                     gpointer                          user_data);
gboolean             ephy_password_manager_find                     (EphyPasswordManager              *self,
                                                                     const char                       *origin,
                                                                     const char                       *target_origin,
                                                                     const char                       *username,
                                                                     const char                       *username_field,
                                                                     const char                       *password_field);
void                 ephy_password_manager_forget                    (EphyPasswordManager *self,
                                                                      const char          *id,
                                                                      GCancellable        *cancellable,
                                                                      GAsyncReadyCallback  callback,
                                                                      gpointer             user_data);
gboolean             ephy_password_manager_forget_finish             (EphyPasswordManager  *self,
                                                                      GAsyncResult         *result,
                                                                      GError              **error);
void                 ephy_password_manager_forget_all                (EphyPasswordManager *self);

G_END_DECLS

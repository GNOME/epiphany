/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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

#include "ephy-sync-service.h"

#include <glib-object.h>
#include <libsecret/secret.h>

G_BEGIN_DECLS

const SecretSchema *ephy_sync_secret_get_token_schema (void) G_GNUC_CONST;

#define EMAIL_KEY       "email_utf8"
#define TOKEN_TYPE_KEY  "token_type"
#define TOKEN_NAME_KEY  "token_name"

#define EPHY_SYNC_TOKEN_SCHEMA (ephy_sync_secret_get_token_schema ())

void ephy_sync_secret_forget_tokens (void);
void ephy_sync_secret_load_tokens   (EphySyncService *service);
void ephy_sync_secret_store_token   (const char        *email,
                                     char              *value,
                                     EphySyncTokenType  type);

G_END_DECLS

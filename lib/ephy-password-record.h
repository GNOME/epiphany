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

#include <glib-object.h>
#include <libsecret/secret.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PASSWORD_RECORD (ephy_password_record_get_type ())

G_DECLARE_FINAL_TYPE (EphyPasswordRecord, ephy_password_record, EPHY, PASSWORD_RECORD, GObject)

EphyPasswordRecord *ephy_password_record_new                (const char *origin,
                                                             const char *form_username,
                                                             const char *form_passwords,
                                                             const char *username,
                                                             const char *password);
const char         *ephy_password_record_get_origin         (EphyPasswordRecord *self);
const char         *ephy_password_record_get_form_username  (EphyPasswordRecord *self);
const char         *ephy_password_record_get_form_password  (EphyPasswordRecord *self);
const char         *ephy_password_record_get_username       (EphyPasswordRecord *self);
const char         *ephy_password_record_get_password       (EphyPasswordRecord *self);
void                ephy_password_record_set_password       (EphyPasswordRecord *self,
                                                             const char         *password);

G_END_DECLS

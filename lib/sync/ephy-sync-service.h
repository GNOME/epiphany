/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <gabrielivascu@gnome.org>
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

#include "ephy-synchronizable-manager.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SYNC_SERVICE (ephy_sync_service_get_type ())

G_DECLARE_FINAL_TYPE (EphySyncService, ephy_sync_service, EPHY, SYNC_SERVICE, GObject)

EphySyncService *ephy_sync_service_new                (gboolean sync_periodically);
void             ephy_sync_service_sign_in            (EphySyncService *self,
                                                       const char      *email,
                                                       const char      *uid,
                                                       const char      *session_token,
                                                       const char      *key_fetch_token,
                                                       const char      *unwrap_kb);
void             ephy_sync_service_sign_out           (EphySyncService *self);
void             ephy_sync_service_sync               (EphySyncService *self);
void             ephy_sync_service_start_sync         (EphySyncService *self);
void             ephy_sync_service_update_device_name (EphySyncService *self,
                                                       const char      *name);
void             ephy_sync_service_register_manager   (EphySyncService           *self,
                                                       EphySynchronizableManager *manager);
void             ephy_sync_service_unregister_manager (EphySyncService           *self,
                                                       EphySynchronizableManager *manager);

G_END_DECLS

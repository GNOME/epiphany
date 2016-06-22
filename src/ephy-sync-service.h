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

#ifndef EPHY_SYNC_SERVICE_H
#define EPHY_SYNC_SERVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

struct _EphySyncService {
  GObject parent_instance;

  gchar *user_email;
  GHashTable *tokens;
};

#define EPHY_TYPE_SYNC_SERVICE (ephy_sync_service_get_type ())

G_DECLARE_FINAL_TYPE (EphySyncService, ephy_sync_service, EPHY, SYNC_SERVICE, GObject)

EphySyncService *ephy_sync_service_new       (void);

gchar           *ephy_sync_service_get_token (EphySyncService *self,
                                              const gchar     *token_name);

void             ephy_sync_service_set_token (EphySyncService *self,
                                              const gchar     *token_name,
                                              const gchar     *token_value_hex);

void             ephy_sync_service_stretch   (EphySyncService *self,
                                              const gchar     *emailUTF8,
                                              const gchar     *passwordUTF8);

void             ephy_sync_service_login     (EphySyncService *self);

G_END_DECLS

#endif

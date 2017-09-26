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

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_GSB_SERVICE (ephy_gsb_service_get_type ())

G_DECLARE_FINAL_TYPE (EphyGSBService, ephy_gsb_service, EPHY, GSB_SERVICE, GObject)

/**
 * EphyGSBServiceVerifyURLCallback:
 * @threats: the browsing lists where the URL is considered unsafe
 * @user_data: user data for callback
 *
 * The callback to be called when ephy_gsb_service_verify_url() completes the
 * verification of the URL. @threats is a hash table used a set (see
 * g_hash_table_add()) of #EphyGSBThreatList that contains the threat lists
 * where the URL is active. The hash table will never be %NULL (if the URL is
 * safe, then the hash table will be empty). Use g_hash_table_unref() when done
 * using it.
 **/
typedef void (*EphyGSBServiceVerifyURLCallback) (GHashTable *threats,
                                                 gpointer    user_data);

EphyGSBService *ephy_gsb_service_new        (const char *api_key,
                                             const char *db_path);
void            ephy_gsb_service_verify_url (EphyGSBService                  *self,
                                             const char                      *url,
                                             EphyGSBServiceVerifyURLCallback  callback,
                                             gpointer                         user_data);

G_END_DECLS

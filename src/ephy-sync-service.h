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

#include "ephy-bookmark.h"
#include "ephy-sync-utils.h"

#include <glib-object.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SYNC_SERVICE (ephy_sync_service_get_type ())

G_DECLARE_FINAL_TYPE (EphySyncService, ephy_sync_service, EPHY, SYNC_SERVICE, GObject)

EphySyncService *ephy_sync_service_new                          (void);
gboolean         ephy_sync_service_is_signed_in                 (EphySyncService *self);
gchar           *ephy_sync_service_get_user_email               (EphySyncService *self);
void             ephy_sync_service_set_user_email               (EphySyncService *self,
                                                                 const gchar     *email);
double           ephy_sync_service_get_sync_time                (EphySyncService *self);
void             ephy_sync_service_set_sync_time                (EphySyncService *self,
                                                                 double           time);
guint            ephy_sync_service_get_sync_frequency           (EphySyncService *self);
void             ephy_sync_service_set_sync_frequency           (EphySyncService *self,
                                                                 guint            sync_frequency);
gchar           *ephy_sync_service_get_token                    (EphySyncService   *self,
                                                                 EphySyncTokenType  type);
void             ephy_sync_service_set_token                    (EphySyncService   *self,
                                                                 gchar             *value,
                                                                 EphySyncTokenType  type);
void             ephy_sync_service_set_and_store_tokens         (EphySyncService   *self,
                                                                 gchar             *value,
                                                                 EphySyncTokenType  type,
                                                                 ...) G_GNUC_NULL_TERMINATED;
void             ephy_sync_service_clear_storage_credentials    (EphySyncService *self);
void             ephy_sync_service_clear_tokens                 (EphySyncService *self);
void             ephy_sync_service_destroy_session              (EphySyncService *self,
                                                                 const gchar     *sessionToken);
gboolean         ephy_sync_service_fetch_sync_keys              (EphySyncService *self,
                                                                 const gchar     *email,
                                                                 const gchar     *keyFetchToken,
                                                                 const gchar     *unwrapBKey);
void             ephy_sync_service_send_storage_message         (EphySyncService     *self,
                                                                 gchar               *endpoint,
                                                                 const gchar         *method,
                                                                 gchar               *request_body,
                                                                 double               modified_since,
                                                                 double               unmodified_since,
                                                                 SoupSessionCallback  callback,
                                                                 gpointer             user_data);
void             ephy_sync_service_release_next_storage_message (EphySyncService *self);
void             ephy_sync_service_upload_bookmark              (EphySyncService *self,
                                                                 EphyBookmark    *bookmark,
                                                                 gboolean         force);
void             ephy_sync_service_download_bookmark            (EphySyncService *self,
                                                                 EphyBookmark    *bookmark);
void             ephy_sync_service_delete_bookmark              (EphySyncService *self,
                                                                 EphyBookmark    *bookmark,
                                                                 gboolean         conditional);
void             ephy_sync_service_sync_bookmarks               (EphySyncService *self,
                                                                 gboolean         first);
gboolean         ephy_sync_service_do_periodical_sync           (EphySyncService *self);

G_END_DECLS

#endif

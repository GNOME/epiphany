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

#include <glib.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

void        ephy_sync_debug_view_secrets            (void);
void        ephy_sync_debug_view_collection         (const char *collection,
                                                     gboolean    decrypt);
void        ephy_sync_debug_view_record             (const char *collection,
                                                     const char *id,
                                                     gboolean    decrypt);
void        ephy_sync_debug_upload_record           (const char *collection,
                                                     const char *id,
                                                     const char *body);
void        ephy_sync_debug_delete_collection       (const char *collection);
void        ephy_sync_debug_delete_record           (const char *collection,
                                                     const char *id);
void        ephy_sync_debug_erase_collection        (const char *collection);
void        ephy_sync_debug_erase_record            (const char *collection,
                                                     const char *id);
void        ephy_sync_debug_view_collection_info    (void);
void        ephy_sync_debug_view_quota_info         (void);
void        ephy_sync_debug_view_collection_usage   (void);
void        ephy_sync_debug_view_collection_counts  (void);
void        ephy_sync_debug_view_configuration_info (void);
void        ephy_sync_debug_view_meta_global_record (void);
void        ephy_sync_debug_view_crypto_keys_record (void);
void        ephy_sync_debug_view_connected_devices  (void);
JsonObject *ephy_sync_debug_get_current_device      (void);

G_END_DECLS

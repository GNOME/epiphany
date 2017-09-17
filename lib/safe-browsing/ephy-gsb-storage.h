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

#include "ephy-gsb-utils.h"

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_GSB_STORAGE (ephy_gsb_storage_get_type ())

G_DECLARE_FINAL_TYPE (EphyGSBStorage, ephy_gsb_storage, EPHY, GSB_STORAGE, GObject)

EphyGSBStorage *ephy_gsb_storage_new                    (const char *db_path);
gboolean        ephy_gsb_storage_is_operable            (EphyGSBStorage *self);
gint64          ephy_gsb_storage_get_next_update_time   (EphyGSBStorage *self);
void            ephy_gsb_storage_set_next_update_time   (EphyGSBStorage *self,
                                                         gint64          next_update_time);
GList          *ephy_gsb_storage_get_threat_lists       (EphyGSBStorage *self);
char           *ephy_gsb_storage_compute_checksum       (EphyGSBStorage    *self,
                                                         EphyGSBThreatList *list);
void            ephy_gsb_storage_update_client_state    (EphyGSBStorage    *self,
                                                         EphyGSBThreatList *list,
                                                         gboolean           clear);
void            ephy_gsb_storage_clear_hash_prefixes    (EphyGSBStorage    *self,
                                                         EphyGSBThreatList *list);
void            ephy_gsb_storage_delete_hash_prefixes   (EphyGSBStorage    *self,
                                                         EphyGSBThreatList *list,
                                                         JsonArray         *indices);
void            ephy_gsb_storage_insert_hash_prefixes   (EphyGSBStorage    *self,
                                                         EphyGSBThreatList *list,
                                                         gsize              prefix_len,
                                                         const char        *prefixes_b64);
GList          *ephy_gsb_storage_lookup_hash_prefixes   (EphyGSBStorage *self,
                                                         GList          *cues);
GList          *ephy_gsb_storage_lookup_full_hashes     (EphyGSBStorage *self,
                                                         GList          *hashes);
void            ephy_gsb_storage_insert_full_hash       (EphyGSBStorage    *self,
                                                         EphyGSBThreatList *list,
                                                         const guint8      *hash,
                                                         gint64             duration);

G_END_DECLS

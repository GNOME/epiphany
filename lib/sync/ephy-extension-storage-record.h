/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 John Cardullo <john@jcarmedia.org>
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
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EXTENSION_STORAGE_RECORD (ephy_extension_storage_record_get_type ())

G_DECLARE_FINAL_TYPE (EphyExtensionStorageRecord, ephy_extension_storage_record, EPHY, EXTENSION_STORAGE_RECORD, GObject)

EphyExtensionStorageRecord *ephy_extension_storage_record_new             (const char *id,
                                                                           const char *extension_id,
                                                                           JsonNode   *storage);
const char                 *ephy_extension_storage_record_get_id          (EphyExtensionStorageRecord *self);
const char                 *ephy_extension_storage_record_get_extension_id(EphyExtensionStorageRecord *self);
JsonNode                   *ephy_extension_storage_record_get_storage     (EphyExtensionStorageRecord *self);
void                        ephy_extension_storage_record_set_storage     (EphyExtensionStorageRecord *self,
                                                                           JsonNode   *storage);

G_END_DECLS

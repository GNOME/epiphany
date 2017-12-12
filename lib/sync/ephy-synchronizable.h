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

#include "ephy-sync-crypto.h"

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SYNCHRONIZABLE (ephy_synchronizable_get_type ())

G_DECLARE_INTERFACE (EphySynchronizable, ephy_synchronizable, EPHY, SYNCHRONIZABLE, JsonSerializable)

struct _EphySynchronizableInterface {
  GTypeInterface parent_iface;

  const char * (*get_id)                   (EphySynchronizable  *synchronizable);
  gint64       (*get_server_time_modified) (EphySynchronizable  *synchronizable);
  void         (*set_server_time_modified) (EphySynchronizable  *synchronizable,
                                            gint64               time_modified);
  JsonNode *   (*to_bso)                   (EphySynchronizable  *synchronizable,
                                            SyncCryptoKeyBundle *bundle);
};

const char *ephy_synchronizable_get_id                    (EphySynchronizable  *synchronizable);
gint64      ephy_synchronizable_get_server_time_modified  (EphySynchronizable  *synchronizable);
void        ephy_synchronizable_set_server_time_modified  (EphySynchronizable  *synchronizable,
                                                           gint64               time_modified);
JsonNode   *ephy_synchronizable_to_bso                    (EphySynchronizable  *synchronizable,
                                                           SyncCryptoKeyBundle *bundle);
/* This can't be an interface method because we lack the EphySynchronizable object. */
GObject    *ephy_synchronizable_from_bso                  (JsonNode            *bso,
                                                           GType                gtype,
                                                           SyncCryptoKeyBundle *bundle,
                                                           gboolean            *is_deleted);
/* Default implementations. */
JsonNode   *ephy_synchronizable_default_to_bso            (EphySynchronizable  *synchronizable,
                                                           SyncCryptoKeyBundle *bundle);

G_END_DECLS

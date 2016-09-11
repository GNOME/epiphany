/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_NODE_DB (ephy_node_db_get_type ())

G_DECLARE_FINAL_TYPE (EphyNodeDb, ephy_node_db, EPHY, NODE_DB, GObject)

EphyNodeDb   *ephy_node_db_new                   (const char *name);

#include "ephy-node.h"

gboolean      ephy_node_db_load_from_file        (EphyNodeDb *db,
                                                  const char *xml_file,
                                                  const xmlChar *xml_root,
                                                  const xmlChar *xml_version);

int           ephy_node_db_write_to_xml_safe     (EphyNodeDb *db,
                                                  const xmlChar *filename,
                                                  const xmlChar *root,
                                                  const xmlChar *version,
                                                  const xmlChar *comment,
                                                  EphyNode *node, ...);

const char   *ephy_node_db_get_name              (EphyNodeDb *db);

gboolean      ephy_node_db_is_immutable          (EphyNodeDb *db);

void          ephy_node_db_set_immutable         (EphyNodeDb *db,
                                                  gboolean immutable);

EphyNode     *ephy_node_db_get_node_from_id      (EphyNodeDb *db,
                                                  guint id);

guint         _ephy_node_db_new_id               (EphyNodeDb *db);

void          _ephy_node_db_add_id               (EphyNodeDb *db,
                                                  guint id,
                                                  EphyNode *node);

void          _ephy_node_db_remove_id            (EphyNodeDb *db,
                                                  guint id);

G_END_DECLS

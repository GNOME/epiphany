/*
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_NODE_DB_H
#define EPHY_NODE_DB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_NODE_DB	  (ephy_node_db_get_type ())
#define EPHY_NODE_DB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_NODE_DB, EphyNodeDb))
#define EPHY_NODE_DB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_NODE_DB, EphyNodeDbClass))
#define EPHY_IS_NODE_DB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_NODE_DB))
#define EPHY_IS_NODE_DB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_NODE_DB))
#define EPHY_NODE_DB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_NODE_DB, EphyNodeDbClass))

typedef struct EphyNodeDb EphyNodeDb;
typedef struct EphyNodeDbPrivate EphyNodeDbPrivate;

struct EphyNodeDb
{
	GObject parent;

	EphyNodeDbPrivate *priv;
};

typedef struct
{
	GObjectClass parent;

} EphyNodeDbClass;

#include "ephy-node.h"

GType         ephy_node_db_get_type		(void);

EphyNodeDb   *ephy_node_db_get_by_name		(const char *name);

EphyNodeDb   *ephy_node_db_new			(const char *name,
						 const char *version);

const char   *ephy_node_db_get_name		(EphyNodeDb *db);

EphyNode     *ephy_node_db_get_node_from_id	(EphyNodeDb *db,
						 long id);

EphyNode     *ephy_node_db_get_root_from_id     (EphyNodeDb *db,
						 long id);

gboolean      ephy_node_db_load_from_xml        (EphyNodeDb *db,
						 const char *xml_file);

gboolean      ephy_node_db_save_to_xml          (EphyNodeDb *db,
						 const char *xml_file);

long	      _ephy_node_db_new_id		(EphyNodeDb *db);

void	      _ephy_node_db_add_id		(EphyNodeDb *db,
						 long id,
						 EphyNode *node);

void	      _ephy_node_db_remove_id		(EphyNodeDb *db,
						 long id);

G_END_DECLS

#endif /* __EPHY_NODE_DB_H */

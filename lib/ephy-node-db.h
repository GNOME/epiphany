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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

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

typedef struct _EphyNodeDb EphyNodeDb;
typedef struct _EphyNodeDbPrivate EphyNodeDbPrivate;

struct _EphyNodeDb
{
	GObject parent;

	/*< private >*/
	EphyNodeDbPrivate *priv;
};

typedef struct
{
	GObjectClass parent;

} EphyNodeDbClass;

#include "ephy-node.h"

GType         ephy_node_db_get_type		(void);

EphyNodeDb   *ephy_node_db_new			(const char *name);

gboolean      ephy_node_db_load_from_file	(EphyNodeDb *db,
						 const char *xml_file,
						 const xmlChar *xml_root,
						 const xmlChar *xml_version);

int           ephy_node_db_write_to_xml_safe	(EphyNodeDb *db,
						 const xmlChar *filename,
						 const xmlChar *root,
						 const xmlChar *version,
						 const xmlChar *comment,
						 EphyNode *node, ...);

const char   *ephy_node_db_get_name		(EphyNodeDb *db);

gboolean      ephy_node_db_is_immutable		(EphyNodeDb *db);

void	      ephy_node_db_set_immutable	(EphyNodeDb *db,
						 gboolean immutable);

EphyNode     *ephy_node_db_get_node_from_id	(EphyNodeDb *db,
						 guint id);

guint	      _ephy_node_db_new_id		(EphyNodeDb *db);

void	      _ephy_node_db_add_id		(EphyNodeDb *db,
						 guint id,
						 EphyNode *node);

void	      _ephy_node_db_remove_id		(EphyNodeDb *db,
						 guint id);

G_END_DECLS

#endif /* __EPHY_NODE_DB_H */

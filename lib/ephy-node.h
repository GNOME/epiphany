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


#ifndef EPHY_NODE_H
#define EPHY_NODE_H

#include <libxml/tree.h>

G_BEGIN_DECLS

/* convenience macro to check node validity */

#define EPHY_IS_NODE(o)	(o != NULL)

typedef struct EphyNode EphyNode;

typedef enum
{
	EPHY_NODE_DESTROY,           /* RBNode *node */
	EPHY_NODE_RESTORED,          /* RBNode *node */
	EPHY_NODE_CHILD_ADDED,       /* RBNode *node, RBNode *child */
	EPHY_NODE_CHILD_CHANGED,     /* RBNode *node, RBNode *child */
	EPHY_NODE_CHILD_REMOVED,     /* RBNode *node, RBNode *child, guint old_index */
	EPHY_NODE_CHILDREN_REORDERED /* RBNode *node, int *new_order */
} EphyNodeSignalType;

#include "ephy-node-db.h"

typedef void (*EphyNodeCallback) (EphyNode *node, ...);

EphyNode   *ephy_node_new                   (EphyNodeDb *db);

EphyNode   *ephy_node_new_with_id           (EphyNodeDb *db,
					     gulong reserved_id);

EphyNodeDb *ephy_node_get_db		    (EphyNode *node);

/* unique node ID */
long        ephy_node_get_id                (EphyNode *node);

/* refcounting */
void        ephy_node_ref                   (EphyNode *node);
void        ephy_node_unref                 (EphyNode *node);

/* signals */
int         ephy_node_signal_connect_object (EphyNode *node,
					     EphyNodeSignalType type,
					     EphyNodeCallback callback,
					     GObject *object);

void        ephy_node_signal_disconnect     (EphyNode *node,
					     int signal_id);

/* properties */
void        ephy_node_set_property          (EphyNode *node,
				             guint property_id,
				             const GValue *value);
gboolean    ephy_node_get_property          (EphyNode *node,
				             guint property_id,
				             GValue *value);

const char *ephy_node_get_property_string   (EphyNode *node,
					     guint property_id);
gboolean    ephy_node_get_property_boolean  (EphyNode *node,
					     guint property_id);
long        ephy_node_get_property_long     (EphyNode *node,
					     guint property_id);
int         ephy_node_get_property_int      (EphyNode *node,
					     guint property_id);
double      ephy_node_get_property_double   (EphyNode *node,
					     guint property_id);
float       ephy_node_get_property_float    (EphyNode *node,
					     guint property_id);
EphyNode   *ephy_node_get_property_node     (EphyNode *node,
					     guint property_id);
/* free return value */
char       *ephy_node_get_property_time     (EphyNode *node,
					     guint property_id);

/* xml storage */
void          ephy_node_save_to_xml         (EphyNode *node,
					     xmlNodePtr parent_xml_node);
EphyNode     *ephy_node_new_from_xml        (EphyNodeDb *db,
					     xmlNodePtr xml_node);

/* DAG structure */
void        ephy_node_add_child             (EphyNode *node,
					     EphyNode *child);
void        ephy_node_remove_child          (EphyNode *node,
					     EphyNode *child);
void	    ephy_node_sort_children	    (EphyNode *node,
					     GCompareFunc compare_func);
gboolean    ephy_node_has_child             (EphyNode *node,
					     EphyNode *child);

void	    ephy_node_reorder_children	    (EphyNode *node,
					     int *new_order);

/* Note that ephy_node_get_children freezes the node; you'll have to thaw it when done.
 * This is to prevent the data getting changed from another thread. */
GPtrArray    *ephy_node_get_children        (EphyNode *node);
int           ephy_node_get_n_children      (EphyNode *node);
EphyNode     *ephy_node_get_nth_child       (EphyNode *node,
					     guint n);
int           ephy_node_get_child_index     (EphyNode *node,
					     EphyNode *child);
EphyNode     *ephy_node_get_next_child      (EphyNode *node,
					     EphyNode *child);
EphyNode     *ephy_node_get_previous_child  (EphyNode *node,
					     EphyNode *child);

G_END_DECLS

#endif /* __EPHY_NODE_H */

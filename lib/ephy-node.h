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

#ifndef EPHY_NODE_H
#define EPHY_NODE_H

#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

G_BEGIN_DECLS

#define EPHY_TYPE_NODE	(ephy_node_get_type ())
#define EPHY_IS_NODE(o)	(o != NULL)

typedef struct _EphyNode EphyNode;

typedef enum
{
	EPHY_NODE_DESTROY,           /* EphyNode *node */
	EPHY_NODE_RESTORED,          /* EphyNode *node */
	EPHY_NODE_CHANGED,           /* EphyNode *node, guint property_id */
	EPHY_NODE_CHILD_ADDED,       /* EphyNode *node, EphyNode *child */
	EPHY_NODE_CHILD_CHANGED,     /* EphyNode *node, EphyNode *child, guint property_id */
	EPHY_NODE_CHILD_REMOVED,     /* EphyNode *node, EphyNode *child, guint old_index */
	EPHY_NODE_CHILDREN_REORDERED /* EphyNode *node, int *new_order */
} EphyNodeSignalType;

#include "ephy-node-db.h"

typedef void (*EphyNodeCallback) (EphyNode *node, ...);
typedef gboolean (*EphyNodeFilterFunc) (EphyNode *, gpointer);

GType	    ephy_node_get_type		    (void) G_GNUC_CONST;

EphyNode   *ephy_node_new                   (EphyNodeDb *db);

EphyNode   *ephy_node_new_with_id           (EphyNodeDb *db,
					     guint reserved_id);

EphyNodeDb *ephy_node_get_db		    (EphyNode *node);

/* unique node ID */
guint       ephy_node_get_id                (EphyNode *node);

/* refcounting */
void        ephy_node_ref                   (EphyNode *node);
void        ephy_node_unref                 (EphyNode *node);

/* signals */
int         ephy_node_signal_connect_object (EphyNode *node,
					     EphyNodeSignalType type,
					     EphyNodeCallback callback,
					     GObject *object);

guint       ephy_node_signal_disconnect_object (EphyNode *node,
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
void        ephy_node_set_property_string   (EphyNode *node,
					     guint property_id,
					     const char *value);
gboolean    ephy_node_get_property_boolean  (EphyNode *node,
					     guint property_id);
void        ephy_node_set_property_boolean  (EphyNode *node,
					     guint property_id,
					     gboolean value);
long        ephy_node_get_property_long     (EphyNode *node,
					     guint property_id);
void        ephy_node_set_property_long     (EphyNode *node,
					     guint property_id,
					     long value);
int         ephy_node_get_property_int      (EphyNode *node,
					     guint property_id);
void        ephy_node_set_property_int      (EphyNode *node,
					     guint property_id,
					     int value);
double      ephy_node_get_property_double   (EphyNode *node,
					     guint property_id);
void        ephy_node_set_property_double   (EphyNode *node,
					     guint property_id,
					     double value);
float       ephy_node_get_property_float    (EphyNode *node,
					     guint property_id);
void        ephy_node_set_property_float    (EphyNode *node,
					     guint property_id,
					     float value);
EphyNode   *ephy_node_get_property_node     (EphyNode *node,
					     guint property_id);

/* xml storage */
int           ephy_node_write_to_xml	    (EphyNode *node,
					     xmlTextWriterPtr writer);
EphyNode     *ephy_node_new_from_xml        (EphyNodeDb *db,
					     xmlNodePtr xml_node);

/* DAG structure */
void          ephy_node_add_child           (EphyNode *node,
					     EphyNode *child);
void          ephy_node_remove_child        (EphyNode *node,
					     EphyNode *child);
void	      ephy_node_sort_children	    (EphyNode *node,
					     GCompareFunc compare_func);
gboolean      ephy_node_has_child           (EphyNode *node,
					     EphyNode *child);
void	      ephy_node_reorder_children    (EphyNode *node,
					     int *new_order);
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
void	      ephy_node_set_is_drag_source  (EphyNode *node,
					     gboolean allow);
gboolean      ephy_node_get_is_drag_source  (EphyNode *node);
void	      ephy_node_set_is_drag_dest    (EphyNode *node,
					     gboolean allow);
gboolean      ephy_node_get_is_drag_dest    (EphyNode *node);

G_END_DECLS

#endif /* __EPHY_NODE_H */

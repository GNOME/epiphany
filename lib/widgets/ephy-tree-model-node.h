/*
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
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

#ifndef __EPHY_TREE_MODEL_NODE_H
#define __EPHY_TREE_MODEL_NODE_H

#include <gtk/gtk.h>

#include "ephy-node.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TREE_MODEL_NODE         (ephy_tree_model_node_get_type ())
#define EPHY_TREE_MODEL_NODE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TREE_MODEL_NODE, EphyTreeModelNode))
#define EPHY_TREE_MODEL_NODE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TREE_MODEL_NODE, EphyTreeModelNodeClass))
#define EPHY_IS_TREE_MODEL_NODE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TREE_MODEL_NODE))
#define EPHY_IS_TREE_MODEL_NODE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TREE_MODEL_NODE))
#define EPHY_TREE_MODEL_NODE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TREE_MODEL_NODE, EphyTreeModelNodeClass))

typedef void (*EphyTreeModelNodeValueFunc) (EphyNode *node, GValue *value, gpointer user_data);

typedef struct _EphyTreeModelNode EphyTreeModelNode;
typedef struct _EphyTreeModelNodeClass EphyTreeModelNodeClass;
typedef struct _EphyTreeModelNodePrivate EphyTreeModelNodePrivate;

struct _EphyTreeModelNode
{
	GObject parent;

	/*< private >*/
	EphyTreeModelNodePrivate *priv;
};

struct _EphyTreeModelNodeClass
{
	GObjectClass parent;
};

GType              ephy_tree_model_node_get_type         (void);

EphyTreeModelNode *ephy_tree_model_node_new              (EphyNode *root);

int		   ephy_tree_model_node_add_column_full  (EphyTreeModelNode* model,
							  GType value_type,
							  int prop_id,
							  EphyTreeModelNodeValueFunc func,
							  gpointer user_data);

int                ephy_tree_model_node_add_prop_column  (EphyTreeModelNode *model,
						          GType value_type,
						          int prop_id);

int                ephy_tree_model_node_add_func_column  (EphyTreeModelNode *model,
						          GType value_type,
						          EphyTreeModelNodeValueFunc func,
						          gpointer user_data);

EphyNode          *ephy_tree_model_node_node_from_iter   (EphyTreeModelNode *model,
						          GtkTreeIter *iter);

void               ephy_tree_model_node_iter_from_node   (EphyTreeModelNode *model,
						          EphyNode *node,
						          GtkTreeIter *iter);

G_END_DECLS

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#endif /* EPHY_TREE_MODEL_NODE_H */

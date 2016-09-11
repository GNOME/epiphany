/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#include "ephy-node.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TREE_MODEL_NODE (ephy_tree_model_node_get_type ())

G_DECLARE_FINAL_TYPE (EphyTreeModelNode, ephy_tree_model_node, EPHY, TREE_MODEL_NODE, GObject)

typedef void (*EphyTreeModelNodeValueFunc) (EphyNode *node, GValue *value, gpointer user_data);

EphyTreeModelNode *ephy_tree_model_node_new              (EphyNode *root);

int                ephy_tree_model_node_add_column_full  (EphyTreeModelNode* model,
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

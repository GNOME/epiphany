/*  Copyright © 2002 Olivier Martin <omartin@ifrance.com>
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TREE_MODEL_SORT (ephy_tree_model_sort_get_type ())

G_DECLARE_FINAL_TYPE (EphyTreeModelSort, ephy_tree_model_sort, EPHY, TREE_MODEL_SORT, GtkTreeModelSort)

GtkTreeModel   *ephy_tree_model_sort_new		      (GtkTreeModel *child_model);

void		ephy_tree_model_sort_set_base_drag_column_id  (EphyTreeModelSort *ms,
							       int id);
void		ephy_tree_model_sort_set_extra_drag_column_id (EphyTreeModelSort *ms,
							       int id);

G_END_DECLS

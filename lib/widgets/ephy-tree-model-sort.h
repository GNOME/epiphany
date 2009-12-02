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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_TREE_MODEL_SORT_H
#define EPHY_TREE_MODEL_SORT_H

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TREE_MODEL_SORT         (ephy_tree_model_sort_get_type ())
#define EPHY_TREE_MODEL_SORT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TREE_MODEL_SORT, EphyTreeModelSort))
#define EPHY_TREE_MODEL_SORT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TREE_MODEL_SORT, EphyTreeModelSortClass))
#define EPHY_IS_TREE_MODEL_SORT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TREE_MODEL_SORT))
#define EPHY_IS_TREE_MODEL_SORT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TREE_MODEL_SORT))
#define EPHY_TREE_MODEL_SORT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TREE_MODEL_SORT, EphyTreeModelSortClass))

typedef struct _EphyTreeModelSort EphyTreeModelSort;
typedef struct _EphyTreeModelSortClass EphyTreeModelSortClass;
typedef struct _EphyTreeModelSortPrivate EphyTreeModelSortPrivate;

struct _EphyTreeModelSort
{
	GtkTreeModelSort parent;

	/*< private >*/
	EphyTreeModelSortPrivate *priv;
};

struct _EphyTreeModelSortClass
{
	GtkTreeModelSortClass parent_class;

	void (*node_from_iter) (EphyTreeModelSort *model, GtkTreeIter *iter, void **node);
};

GType		ephy_tree_model_sort_get_type		      (void);

GtkTreeModel   *ephy_tree_model_sort_new		      (GtkTreeModel *child_model);

void		ephy_tree_model_sort_set_base_drag_column_id  (EphyTreeModelSort *ms,
							       int id);
void		ephy_tree_model_sort_set_extra_drag_column_id (EphyTreeModelSort *ms,
							       int id);

G_END_DECLS

#endif /* EPHY_TREE_MODEL_SORT_H */

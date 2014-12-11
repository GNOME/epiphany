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

#ifndef __EPHY_NODE_VIEW_H
#define __EPHY_NODE_VIEW_H

#include <gtk/gtk.h>

#include "ephy-tree-model-node.h"
#include "ephy-node-filter.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NODE_VIEW         (ephy_node_view_get_type ())
#define EPHY_NODE_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_NODE_VIEW, EphyNodeView))
#define EPHY_NODE_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_NODE_VIEW, EphyNodeViewClass))
#define EPHY_IS_NODE_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_NODE_VIEW))
#define EPHY_IS_NODE_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_NODE_VIEW))
#define EPHY_NODE_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_NODE_VIEW, EphyNodeViewClass))

typedef struct _EphyNodeView		EphyNodeView;
typedef struct _EphyNodeViewClass	EphyNodeViewClass;
typedef struct _EphyNodeViewPrivate	EphyNodeViewPrivate;

struct _EphyNodeView
{
	GtkTreeView parent;

	/*< private >*/
	EphyNodeViewPrivate *priv;
};

struct _EphyNodeViewClass
{
	GtkTreeViewClass parent;

	void (*node_toggled)	    (EphyNodeView *view, EphyNode *node, gboolean checked);
	void (*node_activated)      (EphyNodeView *view, EphyNode *node);
	void (*node_selected)       (EphyNodeView *view, EphyNode *node);
	void (*node_dropped)        (EphyNodeView *view, EphyNode *node, GList *uris);
	void (*node_middle_clicked) (EphyNodeView *view, EphyNode *node);
};

typedef enum
{
	EPHY_NODE_VIEW_ALL_PRIORITY,
	EPHY_NODE_VIEW_SPECIAL_PRIORITY,
	EPHY_NODE_VIEW_NORMAL_PRIORITY
} EphyNodeViewPriority;

typedef enum
{
	EPHY_NODE_VIEW_SHOW_PRIORITY = 1 << 0,
	EPHY_NODE_VIEW_SORTABLE = 1 << 1,
	EPHY_NODE_VIEW_EDITABLE = 1 << 2,
	EPHY_NODE_VIEW_SEARCHABLE = 1 << 3,
	EPHY_NODE_VIEW_ELLIPSIZED = 1 << 4
} EphyNodeViewFlags;

GType      ephy_node_view_get_type	      (void);

GtkWidget *ephy_node_view_new                 (EphyNode *root,
					       EphyNodeFilter *filter);

void	   ephy_node_view_add_toggle	      (EphyNodeView *view,
					       EphyTreeModelNodeValueFunc value_func,
					       gpointer data);

int	   ephy_node_view_add_column_full     (EphyNodeView *view,
					       const char *title,
					       GType value_type,
					       guint prop_id,
					       EphyNodeViewFlags flags,
					       EphyTreeModelNodeValueFunc func,
					       gpointer user_data,
					       EphyTreeModelNodeValueFunc icon_func,
					       GtkTreeViewColumn **ret);

int	   ephy_node_view_add_column	      (EphyNodeView *view,
					       const char  *title,
					       GType value_type,
					       guint prop_id,
					       EphyNodeViewFlags flags,
					       EphyTreeModelNodeValueFunc icon_func,
					       GtkTreeViewColumn **ret);

int	   ephy_node_view_add_data_column     (EphyNodeView *view,
			                       GType value_type,
					       guint prop_id,
			                       EphyTreeModelNodeValueFunc func,
					       gpointer data);

void	   ephy_node_view_set_sort            (EphyNodeView *view,
			                       GType value_type,
					       guint prop_id,
					       GtkSortType sort_type);

void	   ephy_node_view_set_priority        (EphyNodeView *view,
					       EphyNodeViewPriority priority_prop_id);

void	   ephy_node_view_remove              (EphyNodeView *view);

GList     *ephy_node_view_get_selection       (EphyNodeView *view);

void	   ephy_node_view_select_node         (EphyNodeView *view,
			                       EphyNode *node);

void	   ephy_node_view_enable_drag_source  (EphyNodeView *view,
					       const GtkTargetEntry *types,
					       int n_types,
					       int base_drag_column_id,
					       int extra_drag_column_id);

void	   ephy_node_view_enable_drag_dest    (EphyNodeView *view,
					       const GtkTargetEntry *types,
					       int n_types);

void	   ephy_node_view_edit		      (EphyNodeView *view,
					       gboolean remove_if_cancelled);

gboolean   ephy_node_view_is_target	      (EphyNodeView *view);

void	   ephy_node_view_popup		      (EphyNodeView *view,
					       GtkWidget *menu);

gboolean   ephy_node_view_get_iter_for_node   (EphyNodeView *view,
					       GtkTreeIter *iter,
					       EphyNode *node);

G_END_DECLS

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#endif /* EPHY_NODE_VIEW_H */

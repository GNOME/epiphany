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

#ifndef __EPHY_NODE_VIEW_H
#define __EPHY_NODE_VIEW_H

#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkdnd.h>

#include "ephy-tree-model-node.h"
#include "ephy-node-filter.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NODE_VIEW         (ephy_node_view_get_type ())
#define EPHY_NODE_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_NODE_VIEW, EphyNodeView))
#define EPHY_NODE_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_NODE_VIEW, EphyNodeViewClass))
#define EPHY_IS_NODE_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_NODE_VIEW))
#define EPHY_IS_NODE_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_NODE_VIEW))
#define EPHY_NODE_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_NODE_VIEW, EphyNodeViewClass))

typedef struct EphyNodeViewPrivate EphyNodeViewPrivate;

typedef struct
{
	GtkScrolledWindow parent;

	EphyNodeViewPrivate *priv;
} EphyNodeView;

typedef struct
{
	GtkScrolledWindowClass parent;

	void (*node_activated) (EphyNodeView *view, EphyNode *node);
	void (*node_selected)  (EphyNodeView *view, EphyNode *node);
	void (*show_popup)     (EphyNodeView *view);
} EphyNodeViewClass;

GType         ephy_node_view_get_type                 (void);

EphyNodeView *ephy_node_view_new                      (EphyNode *root,
					               EphyNodeFilter *filter);

void	      ephy_node_view_enable_dnd		      (EphyNodeView *view);

void	      ephy_node_view_add_column		      (EphyNodeView *view,
						       const char  *title,
						       EphyTreeModelNodeColumn column,
						       gboolean sortable,
						       gboolean editable);

void	      ephy_node_view_add_icon_column	      (EphyNodeView *view,
						       EphyTreeModelNodeColumn column);

void	      ephy_node_view_remove		      (EphyNodeView *view);

GList        *ephy_node_view_get_selection            (EphyNodeView *view);

void	      ephy_node_view_set_browse_mode	      (EphyNodeView *view);

void	      ephy_node_view_select_node              (EphyNodeView *view,
			                               EphyNode *node);

void	      ephy_node_view_enable_drag_source       (EphyNodeView *view,
						       GtkTargetEntry *types,
						       int n_types,
						       guint prop_id);

void	      ephy_node_view_set_hinted		      (EphyNodeView *view,
						       gboolean hinted);

void	      ephy_node_view_edit		      (EphyNodeView *view);

G_END_DECLS

#endif /* EPHY_NODE_VIEW_H */

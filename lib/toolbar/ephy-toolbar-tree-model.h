/* 
 *  Copyright (C) 2002  Ricardo Fernándezs Pascual <ric@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EPHY_TOOLBAR_TREE_MODEL_H
#define EPHY_TOOLBAR_TREE_MODEL_H

#include <gtk/gtktreemodel.h>
#include "ephy-toolbar.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbTreeModel EphyTbTreeModel;
typedef struct _EphyTbTreeModelClass EphyTbTreeModelClass;
typedef struct _EphyTbTreeModelPrivate EphyTbTreeModelPrivate;

typedef enum {
	EPHY_TB_TREE_MODEL_COL_ICON,
	EPHY_TB_TREE_MODEL_COL_NAME,
	EPHY_TB_TREE_MODEL_NUM_COLUMS
} EphyTbTreeModelColumn;

/**
 * Tb tree model object
 */

#define EPHY_TYPE_TB_TREE_MODEL		    (ephy_tb_tree_model_get_type())
#define EPHY_TB_TREE_MODEL(object)	    (G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_TB_TREE_MODEL,\
					     EphyTbTreeModel))
#define EPHY_TB_TREE_MODEL_CLASS(klass)	    (G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TB_TREE_MODEL,\
					     EphyTbTreeModelClass))
#define EPHY_IS_TB_TREE_MODEL(object)	    (G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_TB_TREE_MODEL))
#define EPHY_IS_TB_TREE_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TB_TREE_MODEL))
#define EPHY_TB_TREE_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TB_TREE_MODEL,\
					    EphyTbTreeModelClass))

struct _EphyTbTreeModel
{
	GObject parent;

	EphyTbTreeModelPrivate *priv;
	gint stamp;
};

struct _EphyTbTreeModelClass
{
	GObjectClass parent_class;
};


GtkType			ephy_tb_tree_model_get_type		(void);
EphyTbTreeModel *	ephy_tb_tree_model_new			(void);
void			ephy_tb_tree_model_set_toolbar		(EphyTbTreeModel *tm, EphyToolbar *tb);
EphyTbItem *		ephy_tb_tree_model_item_from_iter	(EphyTbTreeModel *tm, GtkTreeIter *iter);

G_END_DECLS

#endif

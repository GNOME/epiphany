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

#ifndef __EPHY_HISTORY_MODEL_H
#define __EPHY_HISTORY_MODEL_H

#include <gtk/gtktreemodel.h>

#include "ephy-node.h"
#include "ephy-node-filter.h"

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_MODEL         (ephy_history_model_get_type ())
#define EPHY_HISTORY_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_HISTORY_MODEL, EphyHistoryModel))
#define EPHY_HISTORY_MODEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_HISTORY_MODEL, EphyHistoryModelClass))
#define EPHY_IS_HISTORY_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_HISTORY_MODEL))
#define EPHY_IS_HISTORY_MODEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_HISTORY_MODEL))
#define EPHY_HISTORY_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_HISTORY_MODEL, EphyHistoryModelClass))

typedef enum
{
	EPHY_HISTORY_MODEL_COL_TITLE,
	EPHY_HISTORY_MODEL_COL_LOCATION,
	EPHY_HISTORY_MODEL_COL_VISITS,
	EPHY_HISTORY_MODEL_COL_FIRST_VISIT,
	EPHY_HISTORY_MODEL_COL_LAST_VISIT,
	EPHY_HISTORY_MODEL_COL_VISIBLE,
	EPHY_HISTORY_MODEL_NUM_COLUMNS
} EphyHistoryModelColumn;

GType ephy_history_model_column_get_type (void);

#define EPHY_TYPE_HISTORY_MODEL_COLUMN (ephy_history_model_column_get_type ())

typedef struct EphyHistoryModelPrivate EphyHistoryModelPrivate;

typedef struct
{
	GObject parent;

	EphyHistoryModelPrivate *priv;

	int stamp;
} EphyHistoryModel;

typedef struct
{
	GObjectClass parent;
} EphyHistoryModelClass;

GType              ephy_history_model_get_type         (void);

EphyHistoryModel  *ephy_history_model_new              (EphyNode *root,
							EphyNode *pages,
						        EphyNodeFilter *filter);

EphyNode          *ephy_history_model_node_from_iter   (EphyHistoryModel *model,
						        GtkTreeIter *iter);

void               ephy_history_model_iter_from_node   (EphyHistoryModel *model,
						        EphyNode *node,
						        GtkTreeIter *iter);

G_END_DECLS

#endif /* EPHY_HISTORY_MODEL_H */

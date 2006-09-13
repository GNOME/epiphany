/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
 *
 *  $Id$
 */

#ifndef EPHY_TOOLBARS_MODEL_H
#define EPHY_TOOLBARS_MODEL_H

#include "egg-toolbars-model.h"
#include "ephy-bookmarks.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TOOLBARS_MODEL	(ephy_toolbars_model_get_type ())
#define EPHY_TOOLBARS_MODEL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TOOLBARS_MODEL, EphyToolbarsModel))
#define EPHY_TOOLBARS_MODEL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TOOLBARS_MODEL, EphyToolbarsModelClass))
#define EPHY_IS_TOOLBARS_MODEL(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TOOLBARS_MODEL))
#define EPHY_IS_TOOLBARS_MODEL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TOOLBARS_MODEL))
#define EPHY_TOOLBARS_MODEL_GET_CLASS(o)(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TOOLBARS_MODEL, EphyToolbarsModelClass))

typedef struct _EphyToolbarsModelClass		EphyToolbarsModelClass;
typedef struct _EphyToolbarsModel		EphyToolbarsModel;
typedef struct _EphyToolbarsModelPrivate	EphyToolbarsModelPrivate;

struct _EphyToolbarsModel
{
	EggToolbarsModel parent_object;

	/*< private >*/
	EphyToolbarsModelPrivate *priv;
};

struct _EphyToolbarsModelClass
{
	EggToolbarsModelClass parent_class;
};

GType		  ephy_toolbars_model_get_type	(void);

EggToolbarsModel *ephy_toolbars_model_new	(void);

void		  ephy_toolbars_model_load	(EphyToolbarsModel *model);

G_END_DECLS

#endif

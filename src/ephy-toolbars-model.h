/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#ifndef EPHY_TOOLBARS_MODEL_H
#define EPHY_TOOLBARS_MODEL_H

#include "egg-toolbars-model.h"
#include "ephy-bookmarks.h"

G_BEGIN_DECLS

typedef struct EphyToolbarsModelClass EphyToolbarsModelClass;

#define EPHY_TOOLBARS_MODEL_TYPE             (ephy_toolbars_model_get_type ())
#define EPHY_TOOLBARS_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TOOLBARS_MODEL_TYPE, EphyToolbarsModel))
#define EPHY_TOOLBARS_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TOOLBARS_MODEL_TYPE, EphyToolbarsModelClass))
#define IS_EPHY_TOOLBARS_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TOOLBARS_MODEL_TYPE))
#define IS_EPHY_TOOLBARS_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TOOLBARS_MODEL_TYPE))
#define EPHY_TOOLBARS_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TOOLBARS_MODEL_TYPE, EphyToolbarsModelClass))

typedef struct EphyToolbarsModel EphyToolbarsModel;
typedef struct EphyToolbarsModelPrivate EphyToolbarsModelPrivate;

struct EphyToolbarsModel
{
  EggToolbarsModel parent_object;
  EphyToolbarsModelPrivate *priv;
};

struct EphyToolbarsModelClass
{
  EggToolbarsModelClass parent_class;
};

GType		   ephy_toolbars_model_get_type        (void);

EphyToolbarsModel *ephy_toolbars_model_new	       (EphyBookmarks *bookmarks);

void		   ephy_toolbars_model_add_bookmark    (EphyToolbarsModel *model,
				                        gboolean topic,
				                        long id);

gboolean	   ephy_toolbars_model_has_bookmark    (EphyToolbarsModel *model,
				                        gboolean topic,
				                        long id);

void               ephy_toolbars_model_remove_bookmark (EphyToolbarsModel *model,
				                        gboolean topic,
				                        long id);

char              *ephy_toolbars_model_get_action_name (EphyToolbarsModel *model,
							gboolean topic,
							long id);

void		   ephy_toolbars_model_set_flag        (EphyToolbarsModel *model,
							EggTbModelFlags flags);

void		   ephy_toolbars_model_unset_flag      (EphyToolbarsModel *model,
							EggTbModelFlags flags);

G_END_DECLS

#endif

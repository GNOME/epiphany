/*
 *  Copyright (C) 2003-2004 Marco Pesenti Gritti
 *  Copyright (C) 2003-2004 Christian Persch
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

#ifndef EPHY_BOOKMARKSBAR_MODEL_H
#define EPHY_BOOKMARKSBAR_MODEL_H

#include "egg-toolbars-model.h"
#include "ephy-bookmarks.h"

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARKSBAR_MODEL		(ephy_bookmarksbar_model_get_type ())
#define EPHY_BOOKMARKSBAR_MODEL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_BOOKMARKSBAR_MODEL, EphyBookmarksBarModel))
#define EPHY_BOOKMARKSBAR_MODEL_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_BOOKMARKSBAR_MODEL, EphyBookmarksBarModelClass))
#define EPHY_IS_BOOKMARKSBAR_MODEL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_BOOKMARKSBAR_MODEL))
#define EPHY_IS_BOOKMARKSBAR_MODEL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_BOOKMARKSBAR_MODEL))
#define EPHY_BOOKMARKSBAR_MODEL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_BOOKMARKSBAR_MODEL, EphyBookmarksBarModelClass))

typedef struct EphyBookmarksBarModelClass		EphyBookmarksBarModelClass;
typedef struct EphyBookmarksBarModel		EphyBookmarksBarModel;
typedef struct EphyBookmarksBarModelPrivate	EphyBookmarksBarModelPrivate;

struct EphyBookmarksBarModel
{
	EggToolbarsModel parent_object;

	/*< private >*/
	EphyBookmarksBarModelPrivate *priv;
};

struct EphyBookmarksBarModelClass
{
	EggToolbarsModelClass parent_class;
};

GType		  ephy_bookmarksbar_model_get_type	(void);

EggToolbarsModel *ephy_bookmarksbar_model_new		(EphyBookmarks *bookmarks);

char		 *ephy_bookmarksbar_model_get_action_name	(EphyBookmarksBarModel *model,
								 long id);

EphyNode	 *ephy_bookmarksbar_model_get_node	(EphyBookmarksBarModel *model,
								 const char *action_name);

void		  ephy_bookmarksbar_model_add_bookmark	(EphyBookmarksBarModel *model,
								 gboolean topic,
								 long id);

void		  ephy_bookmarksbar_model_remove_bookmark	(EphyBookmarksBarModel *model,
								 long id);

gboolean	  ephy_bookmarksbar_model_has_bookmark	(EphyBookmarksBarModel *model,
								 long id);

G_END_DECLS

#endif

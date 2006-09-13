/*
 *  Copyright © 2003 Marco Pesenti Gritti <marco@gnome.org>
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

#ifndef EPHY_COMPLETION_MODEL_H
#define EPHY_COMPLETION_MODEL_H

#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define EPHY_TYPE_COMPLETION_MODEL         (ephy_completion_model_get_type ())
#define EPHY_COMPLETION_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_COMPLETION_MODEL, EphyCompletionModel))
#define EPHY_COMPLETION_MODEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_COMPLETION_MODEL, EphyCompletionModelClass))
#define EPHY_IS_COMPLETION_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_COMPLETION_MODEL))
#define EPHY_IS_COMPLETION_MODEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_COMPLETION_MODEL))
#define EPHY_COMPLETION_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_COMPLETION_MODEL, EphyCompletionModelClass))

typedef struct _EphyCompletionModelPrivate EphyCompletionModelPrivate;

typedef enum
{
	EPHY_COMPLETION_TEXT_COL,
	EPHY_COMPLETION_ACTION_COL,
	EPHY_COMPLETION_KEYWORDS_COL,
	EPHY_COMPLETION_RELEVANCE_COL,
	N_COL
} EphyCompletionColumn;

typedef struct
{
	GObject parent;

	/*< private >*/
	EphyCompletionModelPrivate *priv;
} EphyCompletionModel;

typedef struct
{
	GObjectClass parent;
} EphyCompletionModelClass;

GType                ephy_completion_model_get_type	(void);

EphyCompletionModel *ephy_completion_model_new		(void);

G_END_DECLS

#endif /* EPHY_COMPLETION_MODEL_H */

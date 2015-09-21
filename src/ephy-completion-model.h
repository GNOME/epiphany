/*
 *  Copyright © 2012 Igalia S.L.
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

#ifndef EPHY_COMPLETION_MODEL_H
#define EPHY_COMPLETION_MODEL_H

#include "ephy-bookmarks.h"
#include "ephy-history-service.h"

#include <gtk/gtk.h>

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
	EPHY_COMPLETION_URL_COL,
	EPHY_COMPLETION_EXTRA_COL,
	EPHY_COMPLETION_FAVICON_COL,
	N_COL
} EphyCompletionColumn;

typedef struct
{
	GtkListStore parent;

	/*< private >*/
	EphyCompletionModelPrivate *priv;
} EphyCompletionModel;

typedef struct
{
	GtkListStoreClass parent;
} EphyCompletionModelClass;

GType                ephy_completion_model_get_type	     (void);

EphyCompletionModel *ephy_completion_model_new		     (EphyHistoryService *history_service,
                                                              EphyBookmarks *bookmarks,
                                                              gboolean use_markup);

void                 ephy_completion_model_update_for_string (EphyCompletionModel *model,
                                                              const char *string,
                                                              EphyHistoryJobCallback callback,
                                                              gpointer data);
G_END_DECLS

#endif /* EPHY_COMPLETION_MODEL_H */

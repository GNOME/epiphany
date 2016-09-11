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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "ephy-bookmarks.h"
#include "ephy-history-service.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_COMPLETION_MODEL (ephy_completion_model_get_type ())

G_DECLARE_FINAL_TYPE (EphyCompletionModel, ephy_completion_model, EPHY, COMPLETION_MODEL, GtkListStore)

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

EphyCompletionModel *ephy_completion_model_new		     (EphyHistoryService *history_service,
                                                              EphyBookmarks *bookmarks);

void                 ephy_completion_model_update_for_string (EphyCompletionModel *model,
                                                              const char *string,
                                                              EphyHistoryJobCallback callback,
                                                              gpointer data);
G_END_DECLS

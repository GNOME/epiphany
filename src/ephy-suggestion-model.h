/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gio/gio.h>

#include "ephy-bookmarks-manager.h"
#include "ephy-history-service.h"
#include "ephy-suggestion.h"

G_BEGIN_DECLS

#define EPHY_TYPE_SUGGESTION_MODEL (ephy_suggestion_model_get_type())

G_DECLARE_FINAL_TYPE (EphySuggestionModel, ephy_suggestion_model, EPHY, SUGGESTION_MODEL, GObject)

EphySuggestionModel *ephy_suggestion_model_new                     (EphyHistoryService    *history_service,
                                                                    EphyBookmarksManager  *bookmarks_manager);
void                 ephy_suggestion_model_query_async             (EphySuggestionModel   *self,
                                                                    const gchar           *query,
                                                                    gboolean               include_search_engines,
                                                                    gboolean               include_search_engines_suggestions,
                                                                    GCancellable          *cancellable,
                                                                    GAsyncReadyCallback    callback,
                                                                    gpointer               user_data);
gboolean             ephy_suggestion_model_query_finish            (EphySuggestionModel   *self,
                                                                    GAsyncResult          *result,
                                                                    GError               **error);
EphySuggestion      *ephy_suggestion_model_get_suggestion_with_uri (EphySuggestionModel *self,
                                                                    const char          *uri);

G_END_DECLS

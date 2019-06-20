/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gnome.org>
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "ephy-bookmarks-manager.h"

G_BEGIN_DECLS

void            ephy_bookmarks_export        (EphyBookmarksManager  *manager,
                                              const char            *filename,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data);
gboolean        ephy_bookmarks_export_finish (EphyBookmarksManager  *manager,
                                              GAsyncResult          *result,
                                              GError               **error);

G_END_DECLS

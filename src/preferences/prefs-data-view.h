/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Purism SPC
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

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_DATA_VIEW (prefs_data_view_get_type ())

G_DECLARE_DERIVABLE_TYPE (PrefsDataView, prefs_data_view, EPHY, PREFS_DATA_VIEW, GtkBin)

struct _PrefsDataViewClass
{
  GtkBinClass parent_class;
};

const gchar *prefs_data_view_get_clear_all_description (PrefsDataView *self);
void         prefs_data_view_set_clear_all_description (PrefsDataView *self,
                                                        const gchar   *description);

gboolean prefs_data_view_get_is_loading (PrefsDataView *self);
void     prefs_data_view_set_is_loading (PrefsDataView *self,
                                         gboolean       is_loading);

gboolean prefs_data_view_get_has_data (PrefsDataView *self);
void     prefs_data_view_set_has_data (PrefsDataView *self,
                                       gboolean       has_data);

gboolean prefs_data_view_get_has_search_results (PrefsDataView *self);
void     prefs_data_view_set_has_search_results (PrefsDataView *self,
                                                 gboolean       has_search_results);

gboolean prefs_data_view_get_can_clear (PrefsDataView *self);
void     prefs_data_view_set_can_clear (PrefsDataView *self,
                                        gboolean       can_clear);

const gchar *prefs_data_view_get_search_text (PrefsDataView *self);

G_END_DECLS

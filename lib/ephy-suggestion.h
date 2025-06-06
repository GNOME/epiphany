/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2017 Igalia S.L.
 * Copyright © 2018 Jan-Michael Brummer
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

#include "dzl-suggestion.h"

G_BEGIN_DECLS

#define EPHY_TYPE_SUGGESTION (ephy_suggestion_get_type())

G_DECLARE_FINAL_TYPE (EphySuggestion, ephy_suggestion, EPHY, SUGGESTION, DzlSuggestion)

EphySuggestion *ephy_suggestion_new                      (const char *title_markup,
                                                          const char *unescaped_title,
                                                          const char *uri,
                                                          gboolean    is_completion);
EphySuggestion *ephy_suggestion_new_with_custom_subtitle (const char *title_markup,
                                                          const char *unescaped_title,
                                                          const char *subtitle,
                                                          const char *uri);
EphySuggestion *ephy_suggestion_new_without_subtitle     (const char *title_markup,
                                                          const char *unescaped_title,
                                                          const char *uri);
const char     *ephy_suggestion_get_unescaped_title      (EphySuggestion *self);
const char     *ephy_suggestion_get_uri                  (EphySuggestion *self);
const char     *ephy_suggestion_get_subtitle             (EphySuggestion *self);

void            ephy_suggestion_set_favicon              (EphySuggestion  *self,
                                                          cairo_surface_t *favicon);
void            ephy_suggestion_set_icon                 (EphySuggestion  *self,
                                                          const char      *icon_name);
void            ephy_suggestion_set_secondary_icon       (EphySuggestion  *self,
                                                          const char      *icon_name);
gboolean        ephy_suggestion_is_completion            (EphySuggestion *self);

G_END_DECLS

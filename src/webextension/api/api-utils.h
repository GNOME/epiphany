/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Igalia S.L.
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

#include <jsc/jsc.h>

typedef enum {
  API_VALUE_UNSET = -1,
  API_VALUE_FALSE = 0,
  API_VALUE_TRUE = 1,
} ApiTriStateValue;

char *                  api_utils_get_string_property                (JSCValue   *obj,
                                                                      const char *name,
                                                                      const char *default_value);

gboolean                api_utils_get_boolean_property               (JSCValue   *obj,
                                                                      const char *name,
                                                                      gboolean    default_value);

gint32                  api_utils_get_int32_property                 (JSCValue   *obj,
                                                                      const char *name,
                                                                      gint32      default_value);

GPtrArray *             api_utils_get_string_array_property          (JSCValue   *obj,
                                                                      const char *name);

ApiTriStateValue        api_utils_get_tri_state_value_property       (JSCValue   *obj,
                                                                      const char *name);
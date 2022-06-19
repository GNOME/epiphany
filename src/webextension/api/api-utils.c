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

#include "api-utils.h"

char *
api_utils_get_string_property (JSCValue   *obj,
                               const char *name,
                               const char *default_value)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (obj, name);

  if (!jsc_value_is_string (value))
    return g_strdup (default_value);

  return jsc_value_to_string (value);
}

ApiTriStateValue
api_utils_get_tri_state_value_property (JSCValue   *obj,
                                        const char *name)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (obj, name);

  if (jsc_value_is_undefined (value))
    return API_VALUE_UNSET;

  return jsc_value_to_boolean (value);
}

gboolean
api_utils_get_boolean_property (JSCValue   *obj,
                                const char *name,
                                gboolean    default_value)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (obj, name);

  g_assert (default_value == TRUE || default_value == FALSE);

  if (jsc_value_is_undefined (value))
    return default_value;

  return jsc_value_to_boolean (value);
}

gint32
api_utils_get_int32_property (JSCValue   *obj,
                              const char *name,
                              gint32      default_value)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (obj, name);

  if (!jsc_value_is_number (value))
    return default_value;

  return jsc_value_to_int32 (value);
}

GPtrArray *
api_utils_get_string_array_property (JSCValue   *obj,
                                     const char *name)
{
  g_autoptr (JSCValue) value = jsc_value_object_get_property (obj, name);
  GPtrArray *strings = g_ptr_array_new_full (2, g_free);

  if (!jsc_value_is_array (value))
    return strings;

  for (guint i = 0; ; i++) {
    g_autoptr (JSCValue) indexed_value = jsc_value_object_get_property_at_index (value, i);
    if (!jsc_value_is_string (indexed_value))
      break;
    g_ptr_array_add (strings, jsc_value_to_string (indexed_value));
  }

  return strings;
}

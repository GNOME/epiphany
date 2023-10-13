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

#include "ephy-json-utils.h"

/**
 * ephy_json_object_get_string:
 * @object: A valid #JsonObject
 * @name: The member name
 *
 * This safely looks up a string member of a #JsonObject.
 *
 * Returns: (transfer none): A string or %NULL if member was missing or not a string.
 */
const char *
ephy_json_object_get_string (JsonObject *object,
                             const char *name)
{
  JsonNode *node = json_object_get_member (object, name);

  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return NULL;

  if (json_node_get_value_type (node) != G_TYPE_STRING)
    return NULL;

  return json_node_get_string (node);
}

/**
 * ephy_json_object_dup_string:
 * @object: A valid #JsonObject
 * @name: The member name
 *
 * This safely looks up a string member of a #JsonObject.
 *
 * Returns: (transfer full): An allocated string or %NULL if member was missing or not a string.
 */
char *
ephy_json_object_dup_string (JsonObject *object,
                             const char *name)
{
  return g_strdup (ephy_json_object_get_string (object, name));
}

/**
 * ephy_json_object_get_int:
 * @object: A valid #JsonObject
 * @name: The member name
 *
 * This safely looks up an int member of a #JsonObject.
 *
 * Returns: A number or `-1` if member was missing or not a number.
 */
gint64
ephy_json_object_get_int (JsonObject *object,
                          const char *name)
{
  JsonNode *node = json_object_get_member (object, name);

  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return -1;

  if (json_node_get_value_type (node) == G_TYPE_STRING)
    return -1;

  return json_node_get_int (node);
}

/**
 * ephy_json_object_get_double:
 * @object: A valid #JsonObject
 * @name: The member name
 * @default: Default value
 *
 * This safely looks up a double member of a #JsonObject.
 *
 * Returns: A number or @default if member was missing or not a number.
 */
double
ephy_json_object_get_double_with_default (JsonObject *object,
                                          const char *name,
                                          double      default_value)
{
  JsonNode *node = json_object_get_member (object, name);

  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return default_value;

  if (json_node_get_value_type (node) == G_TYPE_STRING)
    return default_value;

  return json_node_get_double (node);
}

/**
 * ephy_json_object_get_double:
 * @object: A valid #JsonObject
 * @name: The member name
 *
 * This safely looks up a double member of a #JsonObject.
 *
 * Returns: A number or `-1.0` if member was missing or not a number.
 */
double
ephy_json_object_get_double (JsonObject *object,
                             const char *name)
{
  return ephy_json_object_get_double_with_default (object, name, -1.0);
}

/**
 * ephy_json_object_get_array:
 * @object: A valid #JsonObject
 * @name: The member name
 *
 * This safely looks up an array member of a #JsonObject.
 *
 * Returns: (transfer none): An array or %NULL if not an array or not found.
 */
JsonArray *
ephy_json_object_get_array (JsonObject *object,
                            const char *name)
{
  JsonNode *node = json_object_get_member (object, name);

  if (!node || !JSON_NODE_HOLDS_ARRAY (node))
    return NULL;

  return json_node_get_array (node);
}

/**
 * ephy_json_object_get_object:
 * @object: A valid #JsonObject
 * @name: The member name
 *
 * This safely looks up an object member of a #JsonObject.
 *
 * Returns: (transfer none): An object or %NULL if not an object or not found.
 */
JsonObject *
ephy_json_object_get_object (JsonObject *object,
                             const char *name)
{
  JsonNode *node = json_object_get_member (object, name);

  if (!node || !JSON_NODE_HOLDS_OBJECT (node))
    return NULL;

  return json_node_get_object (node);
}

/**
 * ephy_json_object_get_boolean:
 * @object: A valid #JsonObject
 * @name: The member name
 * @default_value: The default value
 *
 * This safely looks up a boolean member of a #JsonObject.
 *
 * Returns: A number or @default_value if member was missing or not a boolean.
 */
gboolean
ephy_json_object_get_boolean (JsonObject *object,
                              const char *name,
                              gboolean    default_value)
{
  JsonNode *node = json_object_get_member (object, name);

  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return default_value;

  if (json_node_get_value_type (node) == G_TYPE_STRING)
    return default_value;

  return json_node_get_boolean (node);
}

/**
 * ephy_json_node_get_object:
 * @node: (nullable): A #JsonNode or %NULL
 *
 * This safely turns a #JsonNode into a #JsonObject.
 *
 * Returns: (transfer none): A #JsonObject or %NULL if not an object.
 */
JsonObject *
ephy_json_node_get_object (JsonNode *node)
{
  if (!node || !JSON_NODE_HOLDS_OBJECT (node))
    return NULL;

  return json_node_get_object (node);
}

/**
 * ephy_json_node_to_string:
 * @node: (nullable): A #JsonNode or %NULL
 *
 * This safely turns a #JsonNode into a string.
 *
 * Returns: (transfer none): A string or %NULL if not a string.
 */
const char *
ephy_json_node_to_string (JsonNode *node)
{
  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return NULL;

  if (json_node_get_value_type (node) != G_TYPE_STRING)
    return NULL;

  return json_node_get_string (node);
}

/**
 * ephy_json_node_get_int:
 * @node: (nullable): A #JsonNode or %NULL
 *
 * This safely turns a #JsonNode into an int.
 *
 * Returns: (transfer none): A int or -1 if not a number.
 */
gint64
ephy_json_node_get_int (JsonNode *node)
{
  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return -1;

  if (json_node_get_value_type (node) == G_TYPE_STRING)
    return -1;

  return json_node_get_int (node);
}

/**
 * ephy_json_node_get_double:
 * @node: (nullable): A #JsonNode or %NULL
 *
 * This safely turns a #JsonNode into a double.
 *
 * Returns: (transfer none): A double or -1.0 if not a number.
 */
double
ephy_json_node_get_double (JsonNode *node)
{
  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return -1.0;

  if (json_node_get_value_type (node) == G_TYPE_STRING)
    return -1.0;

  return json_node_get_double (node);
}

/**
 * ephy_json_array_get_string_with_default:
 * @array: A #JsonArray
 * @default_value: Default string value
 *
 * This safely gets a string from an array.
 *
 * Returns: (transfer none): A string or @default_value if not a string.
 */
const char *
ephy_json_array_get_string_with_default (JsonArray  *array,
                                         guint       index,
                                         const char *default_value)
{
  const char *value = ephy_json_node_to_string (json_array_get_element (array, index));
  if (!value)
    return default_value;

  return value;
}

/**
 * ephy_json_array_get_element:
 * @array: A #JsonArray
 *
 * This safely returns an element from an array.
 *
 * Returns: (transfer none): A #JsonNode or %NULL if out of bounds.
 */
JsonNode *
ephy_json_array_get_element (JsonArray *array,
                             guint      index)
{
  if (index >= json_array_get_length (array))
    return NULL;

  return json_array_get_element (array, index);
}

/**
 * ephy_json_array_get_string:
 * @array: A #JsonArray
 *
 * This safely gets a string from an array.
 *
 * Returns: (transfer none): A string or %NULL if not a string.
 */
const char *
ephy_json_array_get_string (JsonArray *array,
                            guint      index)
{
  return ephy_json_node_to_string (ephy_json_array_get_element (array, index));
}

/**
 * ephy_json_array_get_object:
 * @array: A #JsonArray
 *
 * This safely gets an object from an array.
 *
 * Returns: (transfer none): An object or %NULL if not a object.
 */
JsonObject *
ephy_json_array_get_object (JsonArray *array,
                            guint      index)
{
  return ephy_json_node_get_object (ephy_json_array_get_element (array, index));
}

/**
 * ephy_json_array_get_int:
 * @array: A #JsonArray
 *
 * This safely gets an int from an array.
 *
 * Returns: (transfer none): An int or -1 if not a number.
 */
gint64
ephy_json_array_get_int (JsonArray *array,
                         guint      index)
{
  return ephy_json_node_get_int (ephy_json_array_get_element (array, index));
}

/**
 * ephy_json_array_get_double:
 * @array: A #JsonArray
 *
 * This safely gets a double from an array.
 *
 * Returns: (transfer none): An double or -1.0 if not a number.
 */
double
ephy_json_array_get_double (JsonArray *array,
                            guint      index)
{
  return ephy_json_node_get_double (ephy_json_array_get_element (array, index));
}

/**
 * ephy_json_object_get_string_array:
 * @object: A #JsonObject
 * @name: Property name
 *
 * This safely gets a string array from an object.
 *
 * Returns: (transfer full): A valid, maybe empty, #GPtrArray of strings.
 */
GPtrArray *
ephy_json_object_get_string_array (JsonObject *object,
                                   const char *name)
{
  JsonArray *array = ephy_json_object_get_array (object, name);
  GPtrArray *strings;

  if (!array)
    return g_ptr_array_new ();

  strings = g_ptr_array_new_full (json_array_get_length (array), g_free);
  for (guint i = 0; i < json_array_get_length (array); i++) {
    const char *value = ephy_json_array_get_string (array, i);

    if (!value)
      break;

    g_ptr_array_add (strings, g_strdup (value));
  }

  return strings;
}

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

  if (json_node_get_value_type (node) != G_TYPE_INT64)
    return -1;

  return json_node_get_int (node);
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
  JsonNode *node = json_object_get_member (object, name);

  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return -1;

  if (json_node_get_value_type (node) != G_TYPE_DOUBLE)
    return -1;

  return json_node_get_int (node);
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
 * Returns: A number or @default if member was missing or not a boolean.
 */
gboolean
ephy_json_object_get_boolean (JsonObject *object,
                              const char *name,
                              gboolean    default_value)
{
  JsonNode *node = json_object_get_member (object, name);

  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return default_value;

  if (json_node_get_value_type (node) != G_TYPE_BOOLEAN)
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
  return ephy_json_node_to_string (json_array_get_element (array, index));
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
  return ephy_json_node_get_object (json_array_get_element (array, index));
}

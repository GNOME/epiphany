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

#include <json-glib/json-glib.h>

const char    *ephy_json_object_get_string           (JsonObject          *object,
                                                      const char          *name);

char          *ephy_json_object_dup_string           (JsonObject          *object,
                                                      const char          *name);

gboolean      ephy_json_object_get_boolean          (JsonObject          *object,
                                                     const char          *name,
                                                     gboolean             default_value);

double        ephy_json_object_get_double           (JsonObject          *object,
                                                     const char          *name);

double        ephy_json_object_get_double_with_default
                                                    (JsonObject          *object,
                                                     const char          *name,
                                                     double               default_value);

gint64        ephy_json_object_get_int              (JsonObject          *object,
                                                     const char          *name);

JsonArray    *ephy_json_object_get_array            (JsonObject          *object,
                                                     const char          *name);

JsonObject   *ephy_json_object_get_object           (JsonObject          *object,
                                                     const char          *name);

GPtrArray    *ephy_json_object_get_string_array     (JsonObject          *object,
                                                     const char          *name);

JsonObject   *ephy_json_node_get_object             (JsonNode            *node);

const char   *ephy_json_node_to_string              (JsonNode            *node);

gint64       ephy_json_node_get_int                 (JsonNode            *node);

double       ephy_json_node_get_double              (JsonNode            *node);

const char   *ephy_json_array_get_string            (JsonArray           *array,
                                                     guint                index);

const char   *ephy_json_array_get_string_with_default
                                                    (JsonArray           *array,
                                                     guint                index,
                                                     const char          *default_value);

JsonObject   *ephy_json_array_get_object            (JsonArray           *array,
                                                     guint                index);

gint64       ephy_json_array_get_int                (JsonArray           *array,
                                                     guint                index);

double       ephy_json_array_get_double             (JsonArray           *array,
                                                     guint                index);

JsonNode    *ephy_json_array_get_element            (JsonArray           *array,
                                                     guint                index);

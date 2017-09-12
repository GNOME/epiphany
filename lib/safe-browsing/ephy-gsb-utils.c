/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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

#include "config.h"
#include "ephy-gsb-utils.h"

#include <json-glib/json-glib.h>

EphyGSBThreatList *
ephy_gsb_threat_list_new (const char *threat_type,
                          const char *platform_type,
                          const char *threat_entry_type,
                          const char *client_state,
                          gint64      timestamp)
{
  EphyGSBThreatList *list;

  g_assert (threat_type);
  g_assert (platform_type);
  g_assert (threat_entry_type);

  list = g_slice_new (EphyGSBThreatList);
  list->threat_type = g_strdup (threat_type);
  list->platform_type = g_strdup (platform_type);
  list->threat_entry_type = g_strdup (threat_entry_type);
  list->client_state = g_strdup (client_state);
  list->timestamp = timestamp;

  return list;
}
void
ephy_gsb_threat_list_free (EphyGSBThreatList *list)
{
  g_assert (list);

  g_free (list->threat_type);
  g_free (list->platform_type);
  g_free (list->threat_entry_type);
  g_free (list->client_state);
  g_slice_free (EphyGSBThreatList, list);
}

static JsonObject *
ephy_gsb_utils_make_client_info (void)
{
  JsonObject *client_info;

  client_info = json_object_new ();
  json_object_set_string_member (client_info, "clientId", "Epiphany");
  json_object_set_string_member (client_info, "clientVersion", VERSION);

  return client_info;
}

static JsonObject *
ephy_gsb_utils_make_contraints (void)
{
  JsonObject *constraints;
  JsonArray *compressions;

  compressions = json_array_new ();
  json_array_add_string_element (compressions, "RAW");

  constraints = json_object_new ();
  /* No restriction for the number of update entries. */
  json_object_set_int_member (constraints, "maxUpdateEntries", 0);
  /* No restriction for the number of database entries. */
  json_object_set_int_member (constraints, "maxDatabaseEntries", 0);
  /* Let the server pick the geographic region automatically. */
  json_object_set_null_member (constraints, "region");
  json_object_set_array_member (constraints, "supportedCompressions", compressions);

  return constraints;
}

char *
ephy_gsb_utils_make_list_updates_request (GList *threat_lists)
{
  JsonArray *requests;
  JsonObject *body_obj;
  JsonNode *body_node;
  char *retval;

  g_assert (threat_lists);

  requests = json_array_new ();
  for (GList *l = threat_lists; l && l->data; l = l->next) {
    EphyGSBThreatList *list = (EphyGSBThreatList *)l->data;
    JsonObject *request = json_object_new ();

    json_object_set_string_member (request, "threatType", list->threat_type);
    json_object_set_string_member (request, "platformType", list->platform_type);
    json_object_set_string_member (request, "threatEntryType", list->threat_entry_type);
    json_object_set_string_member (request, "state", list->client_state);
    json_object_set_object_member (request, "constraints", ephy_gsb_utils_make_contraints ());
    json_array_add_object_element (requests, request);
  }

  body_obj = json_object_new ();
  json_object_set_object_member (body_obj, "client", ephy_gsb_utils_make_client_info ());
  json_object_set_array_member (body_obj, "listUpdateRequests", requests);

  body_node = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (body_node, body_obj);
  retval = json_to_string (body_node, FALSE);

  json_object_unref (body_obj);
  json_node_unref (body_node);

  return retval;
}

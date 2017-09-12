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
#include "ephy-gsb-service.h"

#include "ephy-debug.h"
#include "ephy-gsb-storage.h"
#include "ephy-user-agent.h"

#include <libsoup/soup.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define API_PREFIX        "https://safebrowsing.googleapis.com/v4/"
#define CURRENT_TIME      (g_get_real_time () / 1000000)  /* seconds */
#define DEFAULT_WAIT_TIME (30 * 60)                       /* seconds */

struct _EphyGSBService {
  GObject parent_instance;

  char           *api_key;
  EphyGSBStorage *storage;
  SoupSession    *session;
};

G_DEFINE_TYPE (EphyGSBService, ephy_gsb_service, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_API_KEY,
  PROP_GSB_STORAGE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static inline gboolean
json_object_has_non_null_string_member (JsonObject *object,
                                        const char *member)
{
  JsonNode *node;

  node = json_object_get_member (object, member);
  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return FALSE;

  return json_node_get_string (node) != NULL;
}

static inline gboolean
json_object_has_non_null_array_member (JsonObject *object,
                                       const char *member)
{
  JsonNode *node;

  node = json_object_get_member (object, member);
  if (!node)
    return FALSE;

  return JSON_NODE_HOLDS_ARRAY (node);
}

static void
update_threat_lists_cb (SoupSession *session,
                        SoupMessage *msg,
                        gpointer     user_data)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (user_data);
  JsonNode *node;
  JsonObject *object;
  JsonArray *responses;
  gint64 next_update_time;

  if (msg->status_code != 200) {
    LOG ("Cannot update GSB threat lists. Server responded: %u, %s",
         msg->status_code, msg->response_body->data);
    return;
  }

  node = json_from_string (msg->response_body->data, NULL);
  object = json_node_get_object (node);
  responses = json_object_get_array_member (object, "listUpdateResponses");

  for (guint i = 0; i < json_array_get_length (responses); i++) {
    EphyGSBThreatList *list;
    JsonObject *lur = json_array_get_object_element (responses, i);
    const char *type = json_object_get_string_member (lur, "responseType");
    JsonObject *checksum = json_object_get_object_member (lur, "checksum");
    const char *remote_checksum = json_object_get_string_member (checksum, "sha256");
    char *local_checksum;

    list = ephy_gsb_threat_list_new (json_object_get_string_member (lur, "threatType"),
                                     json_object_get_string_member (lur, "platformType"),
                                     json_object_get_string_member (lur, "threatEntryType"),
                                     json_object_get_string_member (lur, "newClientState"),
                                     CURRENT_TIME);
    LOG ("Updating list %s/%s/%s", list->threat_type, list->platform_type, list->threat_entry_type);

    /* If full update, clear all previous hash prefixes for the given list. */
    if (!g_strcmp0 (type, "FULL_UPDATE")) {
      LOG ("FULL UPDATE, clearing all previous hash prefixes...");
      ephy_gsb_storage_clear_hash_prefixes (self->storage, list);
    }

    /* Removals need to be handled before additions. */
    if (json_object_has_non_null_array_member (lur, "removals")) {
      JsonArray *removals = json_object_get_array_member (lur, "removals");
      for (guint k = 0; k < json_array_get_length (removals); k++) {
        JsonObject *tes = json_array_get_object_element (removals, k);
        JsonObject *raw_indices = json_object_get_object_member (tes, "rawIndices");
        JsonArray *indices = json_object_get_array_member (raw_indices, "indices");
        ephy_gsb_storage_delete_hash_prefixes (self->storage, list, indices);
      }
    }

    /* Handle additions. */
    if (json_object_has_non_null_array_member (lur, "additions")) {
      JsonArray *additions = json_object_get_array_member (lur, "additions");
      for (guint k = 0; k < json_array_get_length (additions); k++) {
        JsonObject *tes = json_array_get_object_element (additions, k);
        JsonObject *raw_hashes = json_object_get_object_member (tes, "rawHashes");
        gint64 prefix_size = json_object_get_int_member (raw_hashes, "prefixSize");
        const char *hashes = json_object_get_string_member (raw_hashes, "rawHashes");
        ephy_gsb_storage_insert_hash_prefixes (self->storage, list, prefix_size, hashes);
      }
    }

    /* Verify checksum. */
    local_checksum = ephy_gsb_storage_compute_checksum (self->storage, list);
    if (!g_strcmp0 (local_checksum, remote_checksum)) {
      LOG ("Local checksum matches the remote checksum, updating client state...");
      ephy_gsb_storage_update_client_state (self->storage, list, FALSE);
    } else {
      LOG ("Local checksum does NOT match the remote checksum, clearing list...");
      ephy_gsb_storage_clear_hash_prefixes (self->storage, list);
      ephy_gsb_storage_update_client_state (self->storage, list, TRUE);
    }

    g_free (local_checksum);
    ephy_gsb_threat_list_free (list);
  }

  /* Update next update time. */
  if (json_object_has_non_null_string_member (object, "minimumWaitDuration")) {
    const char *duration_str;
    double duration;

    duration_str = json_object_get_string_member (object, "minimumWaitDuration");
    /* Handle the trailing 's' character. */
    sscanf (duration_str, "%lfs", &duration);
    next_update_time = CURRENT_TIME + (gint64)ceil (duration);
  } else {
    next_update_time = CURRENT_TIME + DEFAULT_WAIT_TIME;
  }

  ephy_gsb_storage_set_next_update_time (self->storage, next_update_time);
  /* TODO: Schedule a next update in (next_update_time - CURRENT_TIME) seconds. */

  json_node_unref (node);
}

static void
ephy_gsb_service_update_threat_lists (EphyGSBService *self)
{
  SoupMessage *msg;
  GList *threat_lists;
  char *url;
  char *body;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (ephy_gsb_storage_is_operable (self->storage));

  threat_lists = ephy_gsb_storage_get_threat_lists (self->storage);
  if (!threat_lists)
    return;

  body = ephy_gsb_utils_make_list_updates_request (threat_lists);
  url = g_strdup_printf ("%sthreatListUpdates:fetch?key=%s", API_PREFIX, self->api_key);
  msg = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (msg, "application/json", SOUP_MEMORY_TAKE, body, strlen (body));
  soup_session_queue_message (self->session, msg, update_threat_lists_cb, self);

  g_free (url);
  g_list_free_full (threat_lists, (GDestroyNotify)ephy_gsb_threat_list_free);
}

static void
ephy_gsb_service_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  switch (prop_id) {
    case PROP_API_KEY:
      g_free (self->api_key);
      self->api_key = g_strdup (g_value_get_string (value));
      break;
    case PROP_GSB_STORAGE:
      if (self->storage)
        g_object_unref (self->storage);
      self->storage = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_gsb_service_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  switch (prop_id) {
    case PROP_API_KEY:
      g_value_set_string (value, self->api_key);
      break;
    case PROP_GSB_STORAGE:
      g_value_set_object (value, self->storage);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_gsb_service_finalize (GObject *object)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  g_free (self->api_key);

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->finalize (object);
}

static void
ephy_gsb_service_dispose (GObject *object)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  g_clear_object (&self->storage);
  g_clear_object (&self->session);

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->dispose (object);
}

static void
ephy_gsb_service_constructed (GObject *object)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);
  gint64 next_update_time;

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->constructed (object);

  if (!ephy_gsb_storage_is_operable (self->storage))
    return;

  next_update_time = ephy_gsb_storage_get_next_update_time (self->storage);
  if (CURRENT_TIME >= next_update_time) {
    /* TODO: This takes too long, needs to run in a separate thread. */
    ephy_gsb_service_update_threat_lists (self);
  }
}

static void
ephy_gsb_service_init (EphyGSBService *self)
{
  self->session = soup_session_new ();
  g_object_set (self->session, "user-agent", ephy_user_agent_get_internal (), NULL);
}

static void
ephy_gsb_service_class_init (EphyGSBServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_gsb_service_set_property;
  object_class->get_property = ephy_gsb_service_get_property;
  object_class->constructed = ephy_gsb_service_constructed;
  object_class->dispose = ephy_gsb_service_dispose;
  object_class->finalize = ephy_gsb_service_finalize;

  obj_properties[PROP_API_KEY] =
    g_param_spec_string ("api-key",
                         "API key",
                         "The API key to access the Google Safe Browsing API",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_GSB_STORAGE] =
    g_param_spec_object ("gsb-storage",
                         "GSB filename",
                         "Handler object for the Google Safe Browsing database",
                         EPHY_TYPE_GSB_STORAGE,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyGSBService *
ephy_gsb_service_new (const char *api_key,
                      const char *db_path)
{
  EphyGSBService *service;
  EphyGSBStorage *storage;

  storage = ephy_gsb_storage_new (db_path);
  service = g_object_new (EPHY_TYPE_GSB_SERVICE,
                          "api-key", api_key,
                          "gsb-storage", storage,
                          NULL);
  g_object_unref (storage);

  return service;
}

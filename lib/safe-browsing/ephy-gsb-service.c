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

  gboolean        is_updating;
  guint           source_id;

  gint64          next_full_hashes_request_time;
  gint64          back_off_mode_exit_time;
  gint64          num_fails;

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

static gboolean ephy_gsb_service_update (EphyGSBService *self);

typedef struct {
  EphyGSBService                  *service;
  GHashTable                      *threats;
  GList                           *matching_prefixes;
  GList                           *matching_hashes;
  EphyGSBServiceVerifyURLCallback  callback;
  gpointer                         user_data;
} FindFullHashesData;

static FindFullHashesData *
find_full_hashes_data_new (EphyGSBService                  *service,
                           GHashTable                      *threats,
                           GList                           *matching_prefixes,
                           GList                           *matching_hashes,
                           EphyGSBServiceVerifyURLCallback  callback,
                           gpointer                         user_data)
{
  FindFullHashesData *data;

  g_assert (EPHY_IS_GSB_SERVICE (service));
  g_assert (threats);
  g_assert (matching_prefixes);
  g_assert (matching_hashes);
  g_assert (callback);

  data = g_slice_new (FindFullHashesData);
  data->service = g_object_ref (service);
  data->threats = g_hash_table_ref (threats);
  data->matching_prefixes = g_list_copy_deep (matching_prefixes,
                                              (GCopyFunc)g_bytes_ref,
                                              NULL);
  data->matching_hashes = g_list_copy_deep (matching_hashes,
                                            (GCopyFunc)g_bytes_ref,
                                            NULL);
  data->callback = callback;
  data->user_data = user_data;

  return data;
}

static void
find_full_hashes_data_free (FindFullHashesData *data)
{
  g_assert (data);

  g_object_unref (data->service);
  g_hash_table_unref (data->threats);
  g_list_free_full (data->matching_prefixes, (GDestroyNotify)g_bytes_unref);
  g_list_free_full (data->matching_hashes, (GDestroyNotify)g_bytes_unref);
  g_slice_free (FindFullHashesData, data);
}

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

/*
 * https://developers.google.com/safe-browsing/v4/request-frequency#back-off-mode
 */
static inline void
ephy_gsb_service_update_back_off_mode (EphyGSBService *self)
{
  gint64 duration;

  g_assert (EPHY_IS_GSB_SERVICE (self));

  duration = (1 << self->num_fails++) * 15 * 60 * (g_random_double () + 1);
  self->back_off_mode_exit_time = CURRENT_TIME + MIN (duration, 24 * 60 * 60);

  LOG ("Set back-off mode for %ld seconds", duration);
}

static inline void
ephy_gsb_service_reset_back_off_mode (EphyGSBService *self)
{
  g_assert (EPHY_IS_GSB_SERVICE (self));

  self->num_fails = self->back_off_mode_exit_time = 0;
}

static inline gboolean
ephy_gsb_service_is_back_off_mode (EphyGSBService *self)
{
  g_assert (EPHY_IS_GSB_SERVICE (self));

  return self->num_fails > 0 && CURRENT_TIME < self->back_off_mode_exit_time;
}

static void
ephy_gsb_service_schedule_update (EphyGSBService *self,
                                  gint64          interval)
{
  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (ephy_gsb_storage_is_operable (self->storage));
  g_assert (interval > 0);

  self->source_id = g_timeout_add_seconds (interval,
                                           (GSourceFunc)ephy_gsb_service_update,
                                           self);
  LOG ("Next update scheduled in %ld seconds", interval);
}

static void
ephy_gsb_service_update_thread (GTask          *task,
                                EphyGSBService *self,
                                gpointer        task_data,
                                GCancellable   *cancellable)
{
  JsonNode *body_node;
  JsonObject *body_obj;
  JsonArray *responses;
  SoupMessage *msg = NULL;
  GList *threat_lists = NULL;
  gint64 next_update_time = CURRENT_TIME + DEFAULT_WAIT_TIME;
  char *url = NULL;
  char *body;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (ephy_gsb_storage_is_operable (self->storage));

  ephy_gsb_storage_delete_old_full_hashes (self->storage);

  threat_lists = ephy_gsb_storage_get_threat_lists (self->storage);
  if (!threat_lists)
    goto out;

  body = ephy_gsb_utils_make_list_updates_request (threat_lists);
  url = g_strdup_printf ("%sthreatListUpdates:fetch?key=%s", API_PREFIX, self->api_key);
  msg = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (msg, "application/json", SOUP_MEMORY_TAKE, body, strlen (body));
  soup_session_send_message (self->session, msg);

  /* Handle unsuccessful responses. */
  if (msg->status_code != 200) {
    LOG ("Cannot update threat lists, got: %u, %s", msg->status_code, msg->response_body->data);
    ephy_gsb_service_update_back_off_mode (self);
    next_update_time = self->back_off_mode_exit_time;
    goto out;
  }

  /* Successful response, reset back-off mode. */
  ephy_gsb_service_reset_back_off_mode (self);

  body_node = json_from_string (msg->response_body->data, NULL);
  body_obj = json_node_get_object (body_node);
  responses = json_object_get_array_member (body_obj, "listUpdateResponses");

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
  if (json_object_has_non_null_string_member (body_obj, "minimumWaitDuration")) {
    const char *duration_str;
    double duration;

    duration_str = json_object_get_string_member (body_obj, "minimumWaitDuration");
    /* Handle the trailing 's' character. */
    sscanf (duration_str, "%lfs", &duration);
    next_update_time = CURRENT_TIME + (gint64)ceil (duration);
  }

  ephy_gsb_storage_set_next_update_time (self->storage, next_update_time);

  json_node_unref (body_node);
out:
  g_free (url);
  if (msg)
    g_object_unref (msg);
  g_list_free_full (threat_lists, (GDestroyNotify)ephy_gsb_threat_list_free);

  g_task_return_int (task, next_update_time);
}

static void
ephy_gsb_service_update_finished_cb (EphyGSBService *self,
                                     GAsyncResult   *result,
                                     gpointer        user_data)
{
  gint64 next_update_time;

  next_update_time = g_task_propagate_int (G_TASK (result), NULL);
  ephy_gsb_service_schedule_update (self, next_update_time - CURRENT_TIME);
  self->is_updating = FALSE;
}

static gboolean
ephy_gsb_service_update (EphyGSBService *self)
{
  GTask *task;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (ephy_gsb_storage_is_operable (self->storage));

  self->is_updating = TRUE;
  task = g_task_new (self, NULL,
                     (GAsyncReadyCallback)ephy_gsb_service_update_finished_cb,
                     NULL);
  g_task_run_in_thread (task, (GTaskThreadFunc)ephy_gsb_service_update_thread);
  g_object_unref (task);

  return G_SOURCE_REMOVE;
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

  if (self->source_id != 0) {
    g_source_remove (self->source_id);
    self->source_id = 0;
  }

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->dispose (object);
}

static void
ephy_gsb_service_constructed (GObject *object)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);
  gint64 interval;

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->constructed (object);

  if (!ephy_gsb_storage_is_operable (self->storage))
    return;

  interval = ephy_gsb_storage_get_next_update_time (self->storage) - CURRENT_TIME;

  if (interval <= 0)
    ephy_gsb_service_update (self);
  else
    ephy_gsb_service_schedule_update (self, interval);
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

static void
ephy_gsb_service_find_full_hashes_cb (SoupSession *session,
                                      SoupMessage *msg,
                                      gpointer     user_data)
{
  FindFullHashesData *data = (FindFullHashesData *)user_data;
  EphyGSBService *self = data->service;
  JsonNode *body_node = NULL;
  JsonObject *body_obj;
  JsonArray *matches;
  GList *hashes_lookup = NULL;
  const char *duration_str;
  double duration;

  /* Handle unsuccessful responses. */
  if (msg->status_code != 200) {
    LOG ("Cannot update full hashes, got: %u, %s", msg->status_code, msg->response_body->data);
    ephy_gsb_service_update_back_off_mode (self);
    goto out;
  }

  /* Successful response, reset back-off mode. */
  ephy_gsb_service_reset_back_off_mode (self);

  body_node = json_from_string (msg->response_body->data, NULL);
  body_obj = json_node_get_object (body_node);
  matches = json_object_get_array_member (body_obj, "matches");

  /* Update full hashes in database. */
  for (guint i = 0; i < json_array_get_length (matches); i++) {
    EphyGSBThreatList *list;
    JsonObject *match = json_array_get_object_element (matches, i);
    const char *threat_type = json_object_get_string_member (match, "threatType");
    const char *platform_type = json_object_get_string_member (match, "platformType");
    const char *threat_entry_type = json_object_get_string_member (match, "threatEntryType");
    JsonObject *threat = json_object_get_object_member (match, "threat");
    const char *hash_b64 = json_object_get_string_member (threat, "hash");
    const char *positive_duration;
    guint8 *hash;
    gsize length;

    list = ephy_gsb_threat_list_new (threat_type, platform_type, threat_entry_type, NULL, 0);
    hash = g_base64_decode (hash_b64, &length);
    positive_duration = json_object_get_string_member (match, "cacheDuration");
    sscanf (positive_duration, "%lfs", &duration);

    ephy_gsb_storage_insert_full_hash (self->storage, list, hash, floor (duration));

    g_free (hash);
    ephy_gsb_threat_list_free (list);
  }

  /* Update negative cache duration. */
  duration_str = json_object_get_string_member (body_obj, "negativeCacheDuration");
  sscanf (duration_str, "%lfs", &duration);
  for (GList *l = data->matching_prefixes; l && l->data; l = l->next)
    ephy_gsb_storage_update_hash_prefix_expiration (self->storage, l->data, floor (duration));

  /* Handle minimum wait duration. */
  if (json_object_has_non_null_string_member (body_obj, "minimumWaitDuration")) {
    duration_str = json_object_get_string_member (body_obj, "minimumWaitDuration");
    sscanf (duration_str, "%lfs", &duration);
    self->next_full_hashes_request_time = CURRENT_TIME + (gint64)ceil (duration);
  }

  /* Repeat the full hash verification. */
  hashes_lookup = ephy_gsb_storage_lookup_full_hashes (self->storage, data->matching_hashes);
  for (GList *l = hashes_lookup; l && l->data; l = l->next) {
    EphyGSBHashFullLookup *lookup = (EphyGSBHashFullLookup *)l->data;
    EphyGSBThreatList *list;

    if (!lookup->expired) {
      list = ephy_gsb_threat_list_new (lookup->threat_type,
                                       lookup->platform_type,
                                       lookup->threat_entry_type,
                                       NULL, 0);
      g_hash_table_add (data->threats, list);
    }
  }

out:
  data->callback (data->threats, data->user_data);

  if (body_node)
    json_node_unref (body_node);
  g_list_free_full (hashes_lookup, (GDestroyNotify)ephy_gsb_hash_full_lookup_free);
  find_full_hashes_data_free (data);
}

static void
ephy_gsb_service_find_full_hashes (EphyGSBService                  *self,
                                   GHashTable                      *threats,
                                   GList                           *matching_prefixes,
                                   GList                           *matching_hashes,
                                   EphyGSBServiceVerifyURLCallback  callback,
                                   gpointer                         user_data)
{
  FindFullHashesData *data;
  SoupMessage *msg;
  GList *threat_lists;
  char *url;
  char *body;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (ephy_gsb_storage_is_operable (self->storage));
  g_assert (threats);
  g_assert (matching_prefixes);
  g_assert (matching_hashes);
  g_assert (callback);

  if (CURRENT_TIME < self->next_full_hashes_request_time) {
    LOG ("Cannot send fullHashes:find request. Requests are restricted for %ld seconds",
         self->next_full_hashes_request_time - CURRENT_TIME);
    callback (threats, user_data);
    return;
  }

   if (ephy_gsb_service_is_back_off_mode (self)) {
    LOG ("Cannot send fullHashes:find request. Back-off mode is enabled for %ld seconds",
         self->back_off_mode_exit_time - CURRENT_TIME);
    callback (threats, user_data);
    return;
   }

  threat_lists = ephy_gsb_storage_get_threat_lists (self->storage);
  if (!threat_lists) {
    callback (threats, user_data);
    return;
  }

  body = ephy_gsb_utils_make_full_hashes_request (threat_lists, matching_prefixes);
  url = g_strdup_printf ("%sfullHashes:find?key=%s", API_PREFIX, self->api_key);
  msg = soup_message_new (SOUP_METHOD_POST, url);
  soup_message_set_request (msg, "application/json", SOUP_MEMORY_TAKE, body, strlen (body));

  data = find_full_hashes_data_new (self, threats,
                                    matching_prefixes, matching_hashes,
                                    callback, user_data);
  soup_session_queue_message (self->session, msg,
                              ephy_gsb_service_find_full_hashes_cb, data);

  g_free (url);
  g_list_free_full (threat_lists, (GDestroyNotify)ephy_gsb_threat_list_free);
}

static void
ephy_gsb_service_verify_hashes (EphyGSBService                  *self,
                                GList                           *hashes,
                                GHashTable                      *threats,
                                EphyGSBServiceVerifyURLCallback  callback,
                                gpointer                         user_data)
{
  GList *cues;
  GList *prefixes_lookup = NULL;
  GList *hashes_lookup = NULL;
  GList *matching_prefixes = NULL;
  GList *matching_hashes = NULL;
  GHashTable *matching_prefixes_set;
  GHashTable *matching_hashes_set;
  GHashTableIter iter;
  gpointer value;
  gboolean has_matching_expired_hashes = FALSE;
  gboolean has_matching_expired_prefixes = FALSE;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (ephy_gsb_storage_is_operable (self->storage));
  g_assert (threats);
  g_assert (hashes);
  g_assert (callback);

  matching_prefixes_set = g_hash_table_new (g_bytes_hash, g_bytes_equal);
  matching_hashes_set = g_hash_table_new (g_bytes_hash, g_bytes_equal);

  /* Check for hash prefixes in database that match any of the full hashes. */
  cues = ephy_gsb_utils_get_hash_cues (hashes);
  prefixes_lookup = ephy_gsb_storage_lookup_hash_prefixes (self->storage, cues);
  for (GList *p = prefixes_lookup; p && p->data; p = p->next) {
    EphyGSBHashPrefixLookup *lookup = (EphyGSBHashPrefixLookup *)p->data;

    for (GList *h = hashes; h && h->data; h = h->next) {
      if (ephy_gsb_utils_hash_has_prefix (h->data, lookup->prefix)) {
        value = g_hash_table_lookup (matching_prefixes_set, lookup->prefix);

        /* Consider the prefix expired if it's expired in at least one threat list. */
        g_hash_table_replace (matching_prefixes_set,
                              lookup->prefix,
                              GINT_TO_POINTER (GPOINTER_TO_INT (value) || lookup->negative_expired));
        g_hash_table_add (matching_hashes_set, h->data);
      }
    }
  }

  /* If there are no database matches, then the URL is safe. */
  if (g_hash_table_size (matching_hashes_set) == 0) {
    LOG ("No database match, URL is safe");
    goto return_result;
  }

  /* Check for full hashes matches.
   * All unexpired full hash matches are added directly to the result set.
   */
  matching_hashes = g_hash_table_get_keys (matching_hashes_set);
  hashes_lookup = ephy_gsb_storage_lookup_full_hashes (self->storage, matching_hashes);
  for (GList *l = hashes_lookup; l && l->data; l = l->next) {
    EphyGSBHashFullLookup *lookup = (EphyGSBHashFullLookup *)l->data;
    EphyGSBThreatList *list;

    if (lookup->expired) {
      has_matching_expired_hashes = TRUE;
    } else {
      list = ephy_gsb_threat_list_new (lookup->threat_type,
                                       lookup->platform_type,
                                       lookup->threat_entry_type,
                                       NULL, 0);
      g_hash_table_add (threats, list);
    }
  }

  /* Check for positive cache hit.
   * That is, there is at least one unexpired full hash match.
   */
  if (g_hash_table_size (threats) > 0) {
    LOG ("Positive cache hit, URL is not safe");
    goto return_result;
  }

  /* Check for negative cache hit. That is, there are no expired
   * full hash matches and all hash prefix matches are negative-unexpired.
   */
  g_hash_table_iter_init (&iter, matching_prefixes_set);
  while (g_hash_table_iter_next (&iter, NULL, &value)) {
    if (GPOINTER_TO_INT (value) == TRUE) {
      has_matching_expired_prefixes = TRUE;
      break;
    }
  }
  if (!has_matching_expired_hashes && !has_matching_expired_prefixes) {
    LOG ("Negative cache hit, URL is safe");
    goto return_result;
  }

  /* At this point we have either expired full hash matches and/or
   * negative-expired hash prefix matches, so we need to find from
   * the server whether the URL is safe or not. We do this by updating
   * the full hashes of the matching prefixes with fresh values from
   * server and re-checking for positive cache hits.
   * See ephy_gsb_service_find_full_hashes_cb().
   */
  matching_prefixes = g_hash_table_get_keys (matching_prefixes_set);
  ephy_gsb_service_find_full_hashes (self, threats,
                                     matching_prefixes, matching_hashes,
                                     callback, user_data);
  goto out;

return_result:
  callback (threats, user_data);

out:
  g_list_free (matching_prefixes);
  g_list_free (matching_hashes);
  g_list_free_full (cues, (GDestroyNotify)g_bytes_unref);
  g_list_free_full (prefixes_lookup, (GDestroyNotify)ephy_gsb_hash_prefix_lookup_free);
  g_list_free_full (hashes_lookup, (GDestroyNotify)ephy_gsb_hash_full_lookup_free);
  g_hash_table_unref (matching_prefixes_set);
  g_hash_table_unref (matching_hashes_set);
}

void
ephy_gsb_service_verify_url (EphyGSBService                  *self,
                             const char                      *url,
                             EphyGSBServiceVerifyURLCallback  callback,
                             gpointer                         user_data)
{
  GHashTable *threats;
  GList *hashes;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (url);

  if (!callback)
    return;

  threats = g_hash_table_new_full (g_direct_hash,
                                   (GEqualFunc)ephy_gsb_threat_list_equal,
                                   (GDestroyNotify)ephy_gsb_threat_list_free,
                                   NULL);

  /* If the local database is broken or an update is in course, we cannot
   * really verify the URL, so we have no choice other than to consider it safe.
   */
  if (!ephy_gsb_storage_is_operable (self->storage) || self->is_updating) {
    LOG ("Local GSB storage is not available at the moment, cannot verify URL");
    callback (threats, user_data);
    return;
  }

  hashes = ephy_gsb_utils_compute_hashes (url);
  if (!hashes) {
    callback (threats, user_data);
    return;
  }

  ephy_gsb_service_verify_hashes (self, hashes, threats, callback, user_data);
  g_list_free_full (hashes, (GDestroyNotify)g_bytes_unref);
}

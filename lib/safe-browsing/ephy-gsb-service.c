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

/* See comment in ephy_gsb_service_schedule_update(). */
#define JITTER            2                               /* seconds */
#define CURRENT_TIME      (g_get_real_time () / 1000000)  /* seconds */
#define DEFAULT_WAIT_TIME (30 * 60)                       /* seconds */

struct _EphyGSBService {
  GObject parent_instance;

  char *api_key;
  EphyGSBStorage *storage;

  gboolean is_updating;
  guint source_id;

  gint64 next_full_hashes_time;
  gint64 next_list_updates_time;
  gint64 back_off_exit_time;
  gint64 back_off_num_fails;

  GThread *update_thread;
  GMainLoop *update_loop;
  SoupSession *session;
};

G_DEFINE_TYPE (EphyGSBService, ephy_gsb_service, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_API_KEY,
  PROP_GSB_STORAGE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  UPDATE_FINISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static gboolean ephy_gsb_service_update (EphyGSBService *self);

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

  duration = (1 << self->back_off_num_fails++) * 15 * 60 * (g_random_double () + 1);
  self->back_off_exit_time = CURRENT_TIME + MIN (duration, 24 * 60 * 60);

  ephy_gsb_storage_set_metadata (self->storage, "back_off_exit_time", self->back_off_exit_time);
  ephy_gsb_storage_set_metadata (self->storage, "back_off_num_fails", self->back_off_num_fails);

  LOG ("Set back-off mode for %ld seconds", duration);
}

static inline void
ephy_gsb_service_reset_back_off_mode (EphyGSBService *self)
{
  g_assert (EPHY_IS_GSB_SERVICE (self));

  self->back_off_num_fails = self->back_off_exit_time = 0;
}

static inline gboolean
ephy_gsb_service_is_back_off_mode (EphyGSBService *self)
{
  g_assert (EPHY_IS_GSB_SERVICE (self));

  return self->back_off_num_fails > 0 && self->back_off_exit_time > CURRENT_TIME;
}

static void
ephy_gsb_service_schedule_update (EphyGSBService *self)
{
  gint64 interval;

  g_assert (EPHY_IS_GSB_SERVICE (self));

  if (!ephy_gsb_storage_is_operable (self->storage)) {
    self->source_id = 0;
    LOG ("Local GSB database is broken, cannot schedule update");
    return;
  }

  /* This function should only be called when self->next_list_updates_time is
   * greater than CURRENT_TIME. However, asserting (self->next_list_updates_time
   * - CURRENT_TIME) to be greater than 0 can be faulty in the (very rare, but
   * not impossible) case when the value returned by CURRENT_TIME changes while
   * calling this function to become equal to self->next_list_updates_time, i.e.
   * when opening Epiphany at the exact same second as next_list_updates_time
   * value read from disk. To prevent a crash in that situation, add a jitter
   * value to the difference between next_list_updates_time and CURRENT_TIME.
   */
  interval = self->next_list_updates_time - CURRENT_TIME + JITTER;
  g_assert (interval > 0);

  self->source_id = g_timeout_add_seconds (interval,
                                           (GSourceFunc)ephy_gsb_service_update,
                                           self);
  g_source_set_name_by_id (self->source_id, "[epiphany] gsb_service_update");

  LOG ("Next update scheduled in %ld seconds", interval);
}

static gboolean
ephy_gsb_service_update_finished_cb (EphyGSBService *self)
{
  g_atomic_int_set (&self->is_updating, FALSE);
  g_signal_emit (self, signals[UPDATE_FINISHED], 0);
  ephy_gsb_service_schedule_update (self);

  return G_SOURCE_REMOVE;
}

static gboolean
ephy_gsb_service_update_in_thread (EphyGSBService *self)
{
  JsonNode *body_node = NULL;
  JsonObject *body_obj;
  JsonArray *responses;
  SoupMessage *msg = NULL;
  GList *threat_lists = NULL;
  char *url = NULL;
  char *body;
  g_autoptr (GBytes) response_body = NULL;
  guint status_code;
#if SOUP_CHECK_VERSION (2, 99, 4)
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;
#endif

  g_assert (EPHY_IS_GSB_SERVICE (self));

  if (!ephy_gsb_storage_is_operable (self->storage)) {
    LOG ("Local GSB database is broken, cannot update it");
    goto out;
  }

  /* Set up a default next update time in case of failure or non-existent
   * minimum wait duration.
   */
  self->next_list_updates_time = CURRENT_TIME + DEFAULT_WAIT_TIME;

  ephy_gsb_storage_delete_old_full_hashes (self->storage);

  threat_lists = ephy_gsb_storage_get_threat_lists (self->storage);
  if (!threat_lists) {
    LOG ("No threat lists to update");
    goto out;
  }

  body = ephy_gsb_utils_make_list_updates_request (threat_lists);
  url = g_strdup_printf ("%sthreatListUpdates:fetch?key=%s", API_PREFIX, self->api_key);
  msg = soup_message_new (SOUP_METHOD_POST, url);
#if SOUP_CHECK_VERSION (2, 99, 4)
  bytes = g_bytes_new_take (body, strlen (body));
  soup_message_set_request_body_from_bytes (msg, "application/json", bytes);
  response_body = soup_session_send_and_read (self->session, msg, NULL, &error);
  if (!response_body) {
    LOG ("Cannot update threat lists: %s", error->message);
    ephy_gsb_service_update_back_off_mode (self);
    self->next_list_updates_time = self->back_off_exit_time;
    goto out;
  }

  status_code = soup_message_get_status (msg);
#else
  soup_message_set_request (msg, "application/json", SOUP_MEMORY_TAKE, body, strlen (body));
  soup_session_send_message (self->session, msg);
  status_code = msg->status_code;
  response_body = g_bytes_new_static (msg->response_body->data, msg->response_body->length);
#endif

  /* Handle unsuccessful responses. */
  if (status_code != 200) {
    LOG ("Cannot update threat lists, got: %u, %s", status_code, (const char *)g_bytes_get_data (response_body, NULL));
    ephy_gsb_service_update_back_off_mode (self);
    self->next_list_updates_time = self->back_off_exit_time;
    goto out;
  }

  /* Successful response, reset back-off mode. */
  ephy_gsb_service_reset_back_off_mode (self);

  body_node = json_from_string (g_bytes_get_data (response_body, NULL), NULL);
  if (!body_node || !JSON_NODE_HOLDS_OBJECT (body_node)) {
    g_warning ("Response is not a valid JSON object");
    goto out;
  }

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
                                     json_object_get_string_member (lur, "newClientState"));
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
        ephy_gsb_storage_delete_hash_prefixes (self->storage, list, tes);
      }
    }

    /* Handle additions. */
    if (json_object_has_non_null_array_member (lur, "additions")) {
      JsonArray *additions = json_object_get_array_member (lur, "additions");
      for (guint k = 0; k < json_array_get_length (additions); k++) {
        JsonObject *tes = json_array_get_object_element (additions, k);
        ephy_gsb_storage_insert_hash_prefixes (self->storage, list, tes);
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
    /* g_ascii_strtod() ignores trailing characters, i.e. 's' character. */
    duration = g_ascii_strtod (duration_str, NULL);
    self->next_list_updates_time = CURRENT_TIME + (gint64)ceil (duration);
  }

out:
  g_free (url);
  if (msg)
    g_object_unref (msg);
  if (body_node)
    json_node_unref (body_node);
  g_list_free_full (threat_lists, (GDestroyNotify)ephy_gsb_threat_list_free);

  ephy_gsb_storage_set_metadata (self->storage, "next_list_updates_time", self->next_list_updates_time);

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   (GSourceFunc)ephy_gsb_service_update_finished_cb,
                   g_object_ref (self),
                   (GDestroyNotify)g_object_unref);

  return G_SOURCE_REMOVE;
}

static gpointer
run_update_thread (EphyGSBService *self)
{
  GMainContext *context;

  context = g_main_loop_get_context (self->update_loop);
  g_main_context_push_thread_default (context);
  self->session = soup_session_new_with_options ("user-agent", ephy_user_agent_get (), NULL);
  g_main_loop_run (self->update_loop);
  g_object_unref (self->session);
  g_main_context_pop_thread_default (context);

  return NULL;
}

static gboolean
ephy_gsb_service_update (EphyGSBService *self)
{
  GSource *source;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (ephy_gsb_storage_is_operable (self->storage));

  g_atomic_int_set (&self->is_updating, TRUE);
  source = g_timeout_source_new (0);
  g_source_set_name (source, "[epiphany] gsb_service_update_in_thread");
  g_source_set_callback (source,
                         (GSourceFunc)ephy_gsb_service_update_in_thread,
                         g_object_ref (self),
                         (GDestroyNotify)g_object_unref);
  g_source_attach (source, g_main_loop_get_context (self->update_loop));
  g_source_unref (source);

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
      self->api_key = g_value_dup_string (value);
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

  g_thread_join (self->update_thread);
  g_main_loop_unref (self->update_loop);

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->finalize (object);
}

static void
ephy_gsb_service_dispose (GObject *object)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  g_clear_object (&self->storage);

  g_clear_handle_id (&self->source_id, g_source_remove);

  if (g_main_loop_is_running (self->update_loop))
    g_main_loop_quit (self->update_loop);

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->dispose (object);
}

static void
ephy_gsb_service_constructed (GObject *object)
{
  EphyGSBService *self = EPHY_GSB_SERVICE (object);

  G_OBJECT_CLASS (ephy_gsb_service_parent_class)->constructed (object);

  if (!ephy_gsb_storage_is_operable (self->storage))
    return;

  /* Restore back-off parameters. */
  self->back_off_exit_time = ephy_gsb_storage_get_metadata (self->storage,
                                                            "back_off_exit_time",
                                                            CURRENT_TIME);
  self->back_off_num_fails = ephy_gsb_storage_get_metadata (self->storage,
                                                            "back_off_num_fails",
                                                            0);

  /* Restore next fullHashes:find request time. */
  self->next_full_hashes_time = ephy_gsb_storage_get_metadata (self->storage,
                                                               "next_full_hashes_time",
                                                               CURRENT_TIME);

  /* Restore next threatListUpdates:fetch request time. */
  self->next_list_updates_time = ephy_gsb_storage_get_metadata (self->storage,
                                                                "next_list_updates_time",
                                                                CURRENT_TIME);

  if (ephy_gsb_service_is_back_off_mode (self))
    self->next_list_updates_time = self->back_off_exit_time;
  else
    ephy_gsb_service_reset_back_off_mode (self);

  if (self->next_list_updates_time > CURRENT_TIME)
    ephy_gsb_service_schedule_update (self);
  else
    ephy_gsb_service_update (self);
}

static void
ephy_gsb_service_init (EphyGSBService *self)
{
  GMainContext *context = g_main_context_new ();

  self->update_loop = g_main_loop_new (context, FALSE);
  self->update_thread = g_thread_new ("EphyGSBService", (GThreadFunc)run_update_thread, self);
  g_main_context_unref (context);
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

  signals[UPDATE_FINISHED] =
    g_signal_new ("update-finished",
                  EPHY_TYPE_GSB_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

EphyGSBService *
ephy_gsb_service_new (const char *api_key,
                      const char *db_path)
{
#if ENABLE_GSB
  EphyGSBService *service;
  EphyGSBStorage *storage;

  storage = ephy_gsb_storage_new (db_path);
  service = g_object_new (EPHY_TYPE_GSB_SERVICE,
                          "api-key", api_key,
                          "gsb-storage", storage,
                          NULL);
  g_object_unref (storage);

  return service;
#else
  return NULL;
#endif
}

typedef struct {
  EphyGSBService *self;
  GList *prefixes;
  GMutex mutex;
  GCond condition;
} UpdateFullHashesData;

static gboolean
ephy_gsb_service_update_full_hashes_in_thread (UpdateFullHashesData *data)
{
  EphyGSBService *self = data->self;
  SoupMessage *msg;
  GList *threat_lists;
  JsonNode *body_node;
  JsonObject *body_obj;
  JsonArray *matches;
  const char *duration_str;
  char *url;
  char *body;
  double duration;
  g_autoptr (GBytes) response_body = NULL;
  guint status_code;
#if SOUP_CHECK_VERSION (2, 99, 4)
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;
#endif

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (ephy_gsb_storage_is_operable (self->storage));
  g_assert (data->prefixes);

  g_mutex_lock (&data->mutex);

  if (self->next_full_hashes_time > CURRENT_TIME) {
    LOG ("Cannot send fullHashes:find request. Requests are restricted for %ld seconds",
         self->next_full_hashes_time - CURRENT_TIME);
    goto unlock;
  }

  if (ephy_gsb_service_is_back_off_mode (self)) {
    LOG ("Cannot send fullHashes:find request. Back-off mode is enabled for %ld seconds",
         self->back_off_exit_time - CURRENT_TIME);
    goto unlock;
  }

  threat_lists = ephy_gsb_storage_get_threat_lists (self->storage);
  if (!threat_lists)
    goto unlock;

  body = ephy_gsb_utils_make_full_hashes_request (threat_lists, data->prefixes);
  url = g_strdup_printf ("%sfullHashes:find?key=%s", API_PREFIX, self->api_key);
  msg = soup_message_new (SOUP_METHOD_POST, url);
#if SOUP_CHECK_VERSION (2, 99, 4)
  bytes = g_bytes_new_take (body, strlen (body));
  soup_message_set_request_body_from_bytes (msg, "application/json", bytes);
  response_body = soup_session_send_and_read (self->session, msg, NULL, &error);
  if (!response_body) {
    LOG ("Cannot update full hashes: %s", error->message);
    ephy_gsb_service_update_back_off_mode (self);
    self->next_list_updates_time = self->back_off_exit_time;
    goto out;
  }

  status_code = soup_message_get_status (msg);
#else
  soup_message_set_request (msg, "application/json", SOUP_MEMORY_TAKE, body, strlen (body));
  soup_session_send_message (self->session, msg);
  status_code = msg->status_code;
  response_body = g_bytes_new_static (msg->response_body->data, msg->response_body->length);
#endif

  /* Handle unsuccessful responses. */
  if (status_code != 200) {
    LOG ("Cannot update full hashes, got: %u, %s", status_code, (const char *)g_bytes_get_data (response_body, NULL));
    ephy_gsb_service_update_back_off_mode (self);
    goto out;
  }

  /* Successful response, reset back-off mode. */
  ephy_gsb_service_reset_back_off_mode (self);

  body_node = json_from_string (g_bytes_get_data (response_body, NULL), NULL);
  if (!body_node || !JSON_NODE_HOLDS_OBJECT (body_node)) {
    g_warning ("Response is not a valid JSON object");
    goto out;
  }

  body_obj = json_node_get_object (body_node);

  if (json_object_has_non_null_array_member (body_obj, "matches")) {
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

      list = ephy_gsb_threat_list_new (threat_type, platform_type, threat_entry_type, NULL);
      hash = g_base64_decode (hash_b64, &length);
      positive_duration = json_object_get_string_member (match, "cacheDuration");
      /* g_ascii_strtod() ignores trailing characters, i.e. 's' character. */
      duration = g_ascii_strtod (positive_duration, NULL);

      ephy_gsb_storage_insert_full_hash (self->storage, list, hash, floor (duration));

      g_free (hash);
      ephy_gsb_threat_list_free (list);
    }
  }

  /* Update negative cache duration. */
  duration_str = json_object_get_string_member (body_obj, "negativeCacheDuration");
  /* g_ascii_strtod() ignores trailing characters, i.e. 's' character. */
  duration = g_ascii_strtod (duration_str, NULL);
  for (GList *l = data->prefixes; l && l->data; l = l->next)
    ephy_gsb_storage_update_hash_prefix_expiration (self->storage, l->data, floor (duration));

  /* Handle minimum wait duration. */
  if (json_object_has_non_null_string_member (body_obj, "minimumWaitDuration")) {
    duration_str = json_object_get_string_member (body_obj, "minimumWaitDuration");
    /* g_ascii_strtod() ignores trailing characters, i.e. 's' character. */
    duration = g_ascii_strtod (duration_str, NULL);
    self->next_full_hashes_time = CURRENT_TIME + (gint64)ceil (duration);
    ephy_gsb_storage_set_metadata (self->storage, "next_full_hashes_time", self->next_full_hashes_time);
  }

  json_node_unref (body_node);
out:
  g_free (url);
  g_list_free_full (threat_lists, (GDestroyNotify)ephy_gsb_threat_list_free);
  g_clear_object (&msg);

unlock:
  g_cond_signal (&data->condition);
  g_mutex_unlock (&data->mutex);

  return G_SOURCE_REMOVE;
}

static void
ephy_gsb_service_update_full_hashes_sync (EphyGSBService *self,
                                          GList          *prefixes)
{
  UpdateFullHashesData data = { self, prefixes, { }, { } };
  GSource *source;

  g_mutex_lock (&data.mutex);

  source = g_timeout_source_new (0);
  g_source_set_name (source, "[epiphany] gsb_service_update_full_hashes_in_thread");
  g_source_set_callback (source,
                         (GSourceFunc)ephy_gsb_service_update_full_hashes_in_thread,
                         &data, NULL);
  g_source_attach (source, g_main_loop_get_context (self->update_loop));
  g_source_unref (source);

  g_cond_wait (&data.condition, &data.mutex);
  g_mutex_unlock (&data.mutex);
}

static void
ephy_gsb_service_verify_url_thread (GTask          *task,
                                    EphyGSBService *self,
                                    const char     *url,
                                    GCancellable   *cancellable)
{
  GList *hashes = NULL;
  GList *cues = NULL;
  GList *prefixes_lookup = NULL;
  GList *hashes_lookup = NULL;
  GList *matching_prefixes = NULL;
  GList *matching_hashes = NULL;
  GHashTable *matching_prefixes_set = NULL;
  GHashTable *matching_hashes_set = NULL;
  GHashTableIter iter;
  gpointer value;
  gboolean has_matching_expired_hashes = FALSE;
  gboolean has_matching_expired_prefixes = FALSE;
  GList *threats = NULL;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (G_IS_TASK (task));
  g_assert (url);

  /* If the local database is broken or an update is in course, we cannot
   * really verify the URL, so we have no choice other than to consider it safe.
   */
  if (g_atomic_int_get (&self->is_updating)) {
    LOG ("Local GSB database is being updated, cannot verify URL");
    goto out;
  }

  if (!ephy_gsb_storage_is_operable (self->storage)) {
    LOG ("Local GSB database is broken, cannot verify URL");
    goto out;
  }

  hashes = ephy_gsb_utils_compute_hashes (url);
  if (!hashes)
    goto out;

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
    goto out;
  }

  /* Check for full hashes matches.
   * All unexpired full hash matches are added directly to the result set.
   */
  matching_hashes = g_hash_table_get_keys (matching_hashes_set);
  hashes_lookup = ephy_gsb_storage_lookup_full_hashes (self->storage, matching_hashes);
  for (GList *l = hashes_lookup; l && l->data; l = l->next) {
    EphyGSBHashFullLookup *lookup = (EphyGSBHashFullLookup *)l->data;

    if (lookup->expired)
      has_matching_expired_hashes = TRUE;
    else if (!g_list_find_custom (threats, lookup->threat_type, (GCompareFunc)g_strcmp0))
      threats = g_list_append (threats, g_strdup (lookup->threat_type));
  }

  /* Check for positive cache hit.
   * That is, there is at least one unexpired full hash match.
   */
  if (threats) {
    LOG ("Positive cache hit, URL is not safe");
    goto out;
  }

  /* Check for negative cache hit, i.e. there are no expired full hash
   * matches and all hash prefix matches are negative-unexpired.
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
    goto out;
  }

  /* At this point we have either expired full hash matches and/or
   * negative-expired hash prefix matches, so we need to find from
   * the server whether the URL is safe or not. We do this by updating
   * the full hashes of the matching prefixes with fresh values from
   * server and re-checking for positive cache hits.
   */
  matching_prefixes = g_hash_table_get_keys (matching_prefixes_set);
  ephy_gsb_service_update_full_hashes_sync (self, matching_prefixes);

  /* Repeat the full hash verification. */
  g_list_free_full (hashes_lookup, (GDestroyNotify)ephy_gsb_hash_full_lookup_free);
  hashes_lookup = ephy_gsb_storage_lookup_full_hashes (self->storage, matching_hashes);
  for (GList *l = hashes_lookup; l && l->data; l = l->next) {
    EphyGSBHashFullLookup *lookup = (EphyGSBHashFullLookup *)l->data;

    if (!lookup->expired &&
        !g_list_find_custom (threats, lookup->threat_type, (GCompareFunc)g_strcmp0))
      threats = g_list_append (threats, g_strdup (lookup->threat_type));
  }

out:
  g_task_return_pointer (task, threats, NULL);

  g_list_free (matching_prefixes);
  g_list_free (matching_hashes);
  g_list_free_full (hashes, (GDestroyNotify)g_bytes_unref);
  g_list_free_full (cues, (GDestroyNotify)g_bytes_unref);
  g_list_free_full (prefixes_lookup, (GDestroyNotify)ephy_gsb_hash_prefix_lookup_free);
  g_list_free_full (hashes_lookup, (GDestroyNotify)ephy_gsb_hash_full_lookup_free);
  if (matching_prefixes_set)
    g_hash_table_unref (matching_prefixes_set);
  if (matching_hashes_set)
    g_hash_table_unref (matching_hashes_set);
}

void
ephy_gsb_service_verify_url (EphyGSBService      *self,
                             const char          *url,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GTask *task;

  g_assert (EPHY_IS_GSB_SERVICE (self));
  g_assert (url);
  g_assert (callback);

  task = g_task_new (self, NULL, callback, user_data);
  g_task_set_task_data (task, g_strdup (url), g_free);
  g_task_run_in_thread (task, (GTaskThreadFunc)ephy_gsb_service_verify_url_thread);
  g_object_unref (task);
}

GList *
ephy_gsb_service_verify_url_finish (EphyGSBService *self,
                                    GAsyncResult   *result)
{
  g_assert (g_task_is_valid (result, self));

  return g_task_propagate_pointer (G_TASK (result), NULL);
}

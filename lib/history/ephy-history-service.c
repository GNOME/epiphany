/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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
#include "ephy-history-service.h"

#include "ephy-history-service-private.h"
#include "ephy-history-types.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-sqlite-connection.h"
#include "ephy-sync-utils.h"

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>

typedef gboolean (*EphyHistoryServiceMethod)      (EphyHistoryService *self,
                                                   gpointer            data,
                                                   gpointer           *result);

typedef enum {
  /* WRITE */
  SET_URL_TITLE,
  SET_URL_ZOOM_LEVEL,
  SET_URL_HIDDEN,
  ADD_VISIT,
  ADD_VISITS,
  DELETE_URLS,
  DELETE_HOST,
  CLEAR,
  /* QUIT */
  QUIT,
  /* READ */
  GET_URL,
  GET_HOST_FOR_URL,
  QUERY_URLS,
  QUERY_VISITS,
  GET_HOSTS,
  QUERY_HOSTS
} EphyHistoryServiceMessageType;

enum {
  VISIT_URL,
  URLS_VISITED,
  CLEARED,
  URL_TITLE_CHANGED,
  URL_DELETED,
  HOST_DELETED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct _EphyHistoryServiceMessage {
  EphyHistoryService *service;
  EphyHistoryServiceMessageType type;
  gpointer *method_argument;
  gboolean success;
  gpointer result;
  gpointer user_data;
  GCancellable *cancellable;
  GDestroyNotify method_argument_cleanup;
  GDestroyNotify result_cleanup;
  EphyHistoryJobCallback callback;
} EphyHistoryServiceMessage;

static gpointer run_history_service_thread (EphyHistoryService *self);
static void ephy_history_service_process_message (EphyHistoryService        *self,
                                                  EphyHistoryServiceMessage *message);
static gboolean ephy_history_service_execute_quit (EphyHistoryService *self,
                                                   gpointer            data,
                                                   gpointer           *result);
static void ephy_history_service_quit (EphyHistoryService    *self,
                                       EphyHistoryJobCallback callback,
                                       gpointer               user_data);

enum {
  PROP_0,
  PROP_HISTORY_FILENAME,
  PROP_MEMORY,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_FINAL_TYPE (EphyHistoryService, ephy_history_service, G_TYPE_OBJECT);

static void
ephy_history_service_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EphyHistoryService *self = EPHY_HISTORY_SERVICE (object);

  switch (property_id) {
    case PROP_HISTORY_FILENAME:
      g_free (self->history_filename);
      self->history_filename = g_value_dup_string (value);
      break;
    case PROP_MEMORY:
      self->in_memory = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
      break;
  }
}

static void
ephy_history_service_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  EphyHistoryService *self = EPHY_HISTORY_SERVICE (object);
  switch (property_id) {
    case PROP_HISTORY_FILENAME:
      g_value_set_string (value, self->history_filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
ephy_history_service_finalize (GObject *object)
{
  EphyHistoryService *self = EPHY_HISTORY_SERVICE (object);

  ephy_history_service_quit (self, NULL, NULL);

  if (self->history_thread)
    g_thread_join (self->history_thread);

  g_free (self->history_filename);

  G_OBJECT_CLASS (ephy_history_service_parent_class)->finalize (object);
}

static void
ephy_history_service_dispose (GObject *object)
{
  EphyHistoryService *self = EPHY_HISTORY_SERVICE (object);

  g_clear_handle_id (&self->queue_urls_visited_id, g_source_remove);

  G_OBJECT_CLASS (ephy_history_service_parent_class)->dispose (object);
}

static void
ephy_history_service_constructed (GObject *object)
{
  EphyHistoryService *self = EPHY_HISTORY_SERVICE (object);

  G_OBJECT_CLASS (ephy_history_service_parent_class)->constructed (object);

  self->queue = g_async_queue_new ();

  /* This value is checked in several functions to verify that they are only
   * ever run on the history thread. Accordingly, we'd better be sure it's set
   * before it is checked for the first time. That requires a lock here. */
  g_mutex_lock (&self->history_thread_mutex);
  self->history_thread = g_thread_new ("EphyHistoryService", (GThreadFunc)run_history_service_thread, self);

  /* Additionally, make sure the SQLite connection has really been opened before
   * returning. We need this so that we can test that using a read-only service
   * at the same time as a read/write service does not cause the read/write
   * service to break. This delay is required because we need to be sure the
   * read/write service has completed initialization before attempting to open
   * the read-only service, or initializing the read-only service will fail.
   * This isn't needed except in test mode, because only tests might run
   * multiple history services, but it's harmless and cleaner to do always.
   */
  while (!self->history_thread_initialized)
    g_cond_wait (&self->history_thread_initialized_condition, &self->history_thread_mutex);

  g_mutex_unlock (&self->history_thread_mutex);
}

static gboolean
emit_urls_visited (EphyHistoryService *self)
{
  g_signal_emit (self, signals[URLS_VISITED], 0);
  self->queue_urls_visited_id = 0;

  return FALSE;
}

static void
ephy_history_service_queue_urls_visited (EphyHistoryService *self)
{
  if (self->queue_urls_visited_id)
    return;

  self->queue_urls_visited_id =
    g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)emit_urls_visited, self, NULL);
}

static void
ephy_history_service_class_init (EphyHistoryServiceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = ephy_history_service_finalize;
  gobject_class->dispose = ephy_history_service_dispose;
  gobject_class->constructed = ephy_history_service_constructed;
  gobject_class->get_property = ephy_history_service_get_property;
  gobject_class->set_property = ephy_history_service_set_property;

  signals[VISIT_URL] =
    g_signal_new ("visit-url",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyHistoryService::urls-visited:
 * @service: the #EphyHistoryService that received the signal
 *
 * The ::urls-visited signal is emitted after one or more visits to
 * URLS have taken place. This signal is intended for use-cases when
 * precise information of the actual URLS visited is not important and
 * there is only interest in the fact that there have been changes in
 * the history. For more precise information, you can use ::visit-url
 **/
  signals[URLS_VISITED] =
    g_signal_new ("urls-visited",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals[CLEARED] =
    g_signal_new ("cleared",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals[URL_TITLE_CHANGED] =
    g_signal_new ("url-title-changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[URL_DELETED] =
    g_signal_new ("url-deleted",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[HOST_DELETED] =
    g_signal_new ("host-deleted",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  obj_properties[PROP_HISTORY_FILENAME] =
    g_param_spec_string ("history-filename",
                         NULL, NULL,
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_MEMORY] =
    g_param_spec_boolean ("memory",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_properties);
}

static void
ephy_history_service_init (EphyHistoryService *self)
{
}

EphyHistoryService *
ephy_history_service_new (const char               *history_filename,
                          EphySQLiteConnectionMode  mode)
{
  return EPHY_HISTORY_SERVICE (g_object_new (EPHY_TYPE_HISTORY_SERVICE,
                                             "history-filename", history_filename,
                                             "memory", mode == EPHY_SQLITE_CONNECTION_MODE_MEMORY,
                                             NULL));
}

static gint
sort_messages (EphyHistoryServiceMessage *a,
               EphyHistoryServiceMessage *b,
               gpointer                   user_data)
{
  return a->type > b->type ? 1 : a->type == b->type ? 0 : -1;
}

static EphyHistoryServiceMessage *
ephy_history_service_message_new (EphyHistoryService            *service,
                                  EphyHistoryServiceMessageType  type,
                                  gpointer                       method_argument,
                                  GDestroyNotify                 method_argument_cleanup,
                                  GDestroyNotify                 result_cleanup,
                                  GCancellable                  *cancellable,
                                  EphyHistoryJobCallback         callback,
                                  gpointer                       user_data)
{
  EphyHistoryServiceMessage *message = g_new0 (EphyHistoryServiceMessage, 1);

  message->service = service;
  message->type = type;
  message->method_argument = method_argument;
  message->method_argument_cleanup = method_argument_cleanup;
  message->result_cleanup = result_cleanup;
  message->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  message->callback = callback;
  message->user_data = user_data;

  return message;
}

static void
ephy_history_service_message_free (EphyHistoryServiceMessage *message)
{
  if (message->method_argument_cleanup)
    message->method_argument_cleanup (message->method_argument);

  if (message->result_cleanup)
    message->result_cleanup (message->result);

  if (message->cancellable)
    g_object_unref (message->cancellable);

  g_free (message);
}

static void
ephy_history_service_send_message (EphyHistoryService        *self,
                                   EphyHistoryServiceMessage *message)
{
  g_async_queue_push_sorted (self->queue, message, (GCompareDataFunc)sort_messages, NULL);
}

static void
ephy_history_service_open_transaction (EphyHistoryService *self)
{
  GError *error = NULL;
  g_assert (self->history_thread == g_thread_self ());

  if (!self->history_database)
    return;

  ephy_sqlite_connection_begin_transaction (self->history_database, &error);
  if (error) {
    g_warning ("Could not open history database transaction: %s", error->message);
    g_error_free (error);
  }
}

static void
ephy_history_service_commit_transaction (EphyHistoryService *self)
{
  GError *error = NULL;
  g_assert (self->history_thread == g_thread_self ());

  if (!self->history_database)
    return;

  ephy_sqlite_connection_commit_transaction (self->history_database, &error);
  if (error) {
    g_warning ("Could not commit history database transaction: %s", error->message);
    g_error_free (error);
  }
}

static gboolean
ephy_history_service_open_database_connections (EphyHistoryService *self)
{
  GError *error = NULL;

  g_assert (self->history_thread == g_thread_self ());

  if (self->history_database)
    g_object_unref (self->history_database);

  self->history_database = ephy_sqlite_connection_new (self->in_memory ? EPHY_SQLITE_CONNECTION_MODE_MEMORY
                                                                       : EPHY_SQLITE_CONNECTION_MODE_READWRITE,
                                                       self->history_filename);
  ephy_sqlite_connection_open (self->history_database, &error);
  if (error) {
    g_object_unref (self->history_database);
    self->history_database = NULL;

    if (!g_error_matches (error, EPHY_SQLITE_ERROR, SQLITE_CANTOPEN) ||
        g_file_test (self->history_filename, G_FILE_TEST_EXISTS)) {
      g_warning ("Could not open history database at %s: %s", self->history_filename, error->message);
    }
    g_error_free (error);
    return FALSE;
  } else {
    ephy_sqlite_connection_enable_foreign_keys (self->history_database);
  }

  return (ephy_history_service_initialize_hosts_table (self) &&
          ephy_history_service_initialize_urls_table (self) &&
          ephy_history_service_initialize_visits_table (self));
}

static void
ephy_history_service_close_database_connections (EphyHistoryService *self)
{
  g_assert (self->history_thread == g_thread_self ());

  ephy_sqlite_connection_close (self->history_database);
  g_object_unref (self->history_database);
  self->history_database = NULL;
}

static gboolean
ephy_history_service_execute_quit (EphyHistoryService *self,
                                   gpointer            data,
                                   gpointer           *result)
{
  g_assert (self->history_thread == g_thread_self ());

  g_async_queue_unref (self->queue);

  self->scheduled_to_quit = TRUE;

  return FALSE;
}

static gpointer
run_history_service_thread (EphyHistoryService *self)
{
  EphyHistoryServiceMessage *message;
  gboolean success;

  /* Note that self->history_thread is only written once, and that's guaranteed
   * to have occurred before we enter this critical section due to this mutex.
   * Accordingly, we do not need to use the mutex when performing these
   * assertions in other functions.
   */
  g_mutex_lock (&self->history_thread_mutex);
  g_assert (self->history_thread == g_thread_self ());

  success = ephy_history_service_open_database_connections (self);

  self->history_thread_initialized = TRUE;
  g_cond_signal (&self->history_thread_initialized_condition);
  g_mutex_unlock (&self->history_thread_mutex);

  if (!success)
    return NULL;

  do {
    message = g_async_queue_try_pop (self->queue);
    if (!message) {
      /* Block the thread until there's data in the queue. */
      message = g_async_queue_pop (self->queue);
    }

    /* Process item. */
    ephy_history_service_process_message (self, message);
  } while (!self->scheduled_to_quit);

  ephy_history_service_close_database_connections (self);

  return NULL;
}

static gboolean
ephy_history_service_execute_job_callback (gpointer data)
{
  EphyHistoryServiceMessage *message = (EphyHistoryServiceMessage *)data;

  g_assert (message->callback || message->type == CLEAR);

  if (g_cancellable_is_cancelled (message->cancellable)) {
    ephy_history_service_message_free (message);
    return FALSE;
  }

  if (message->callback)
    message->callback (message->service, message->success, message->result, message->user_data);

  if (message->type == CLEAR)
    g_signal_emit (message->service, signals[CLEARED], 0);

  ephy_history_service_message_free (message);

  return FALSE;
}

typedef struct {
  EphyHistoryService *service;
  gpointer user_data;
  GDestroyNotify destroy_func;
} SignalEmissionContext;

static void
signal_emission_context_free (SignalEmissionContext *ctx)
{
  g_object_unref (ctx->service);
  if (ctx->destroy_func && ctx->user_data)
    ctx->destroy_func (ctx->user_data);
  g_free (ctx);
}

static SignalEmissionContext *
signal_emission_context_new (EphyHistoryService *service,
                             gpointer            user_data,
                             GDestroyNotify      destroy_func)
{
  SignalEmissionContext *ctx = g_new0 (SignalEmissionContext, 1);

  ctx->service = g_object_ref (service);
  ctx->user_data = user_data;
  ctx->destroy_func = destroy_func;

  return ctx;
}

static gboolean
ephy_history_service_execute_add_visit_helper (EphyHistoryService   *self,
                                               EphyHistoryPageVisit *visit)
{
  if (!visit->url->host) {
    visit->url->host = ephy_history_service_get_host_row_from_url (self, visit->url->url);
  } else if (visit->url->host->id == -1) {
    /* This will happen when we migrate the old history to the new
     * format. We need to store a zoom level for a not-yet-created
     * host, so we'll end up here. Ugly, but it works. */
    double zoom_level = visit->url->host->zoom_level;
    ephy_history_host_free (visit->url->host);
    visit->url->host = ephy_history_service_get_host_row_from_url (self, visit->url->url);
    visit->url->host->zoom_level = zoom_level;
  }

  visit->url->host->visit_count++;
  ephy_history_service_update_host_row (self, visit->url->host);

  /* A NULL return here means that the URL does not yet exist in the database.
   * This overwrites visit->url so we have to test the sync id against NULL on
   * both branches. */
  if (!ephy_history_service_get_url_row (self, visit->url->url, visit->url)) {
    visit->url->last_visit_time = visit->visit_time;
    visit->url->visit_count = 1;

    if (!visit->url->sync_id)
      visit->url->sync_id = ephy_sync_utils_get_random_sync_id ();

    ephy_history_service_add_url_row (self, visit->url);

    if (!self->in_memory && visit->url->id == -1) {
      g_warning ("Adding visit failed after failed URL addition.");
      return FALSE;
    }
  } else {
    visit->url->visit_count++;

    if (visit->visit_time > visit->url->last_visit_time)
      visit->url->last_visit_time = visit->visit_time;

    if (!visit->url->sync_id)
      visit->url->sync_id = ephy_sync_utils_get_random_sync_id ();

    ephy_history_service_update_url_row (self, visit->url);
  }

  if (visit->url->notify_visit)
    g_signal_emit (self, signals[VISIT_URL], 0, visit->url);

  ephy_history_service_add_visit_row (self, visit);
  return visit->id != -1;
}

static gboolean
ephy_history_service_execute_add_visit (EphyHistoryService   *self,
                                        EphyHistoryPageVisit *visit,
                                        gpointer             *result)
{
  gboolean success;
  g_assert (self->history_thread == g_thread_self ());

  success = ephy_history_service_execute_add_visit_helper (self, visit);
  return success;
}

static gboolean
ephy_history_service_execute_add_visits (EphyHistoryService *self,
                                         GList              *visits,
                                         gpointer           *result)
{
  gboolean success = TRUE;
  g_assert (self->history_thread == g_thread_self ());

  while (visits) {
    success = success && ephy_history_service_execute_add_visit_helper (self, (EphyHistoryPageVisit *)visits->data);
    visits = visits->next;
  }

  return success;
}

static gboolean
ephy_history_service_execute_find_visits (EphyHistoryService *self,
                                          EphyHistoryQuery   *query,
                                          gpointer           *result)
{
  GList *visits = ephy_history_service_find_visit_rows (self, query);
  GList *current = visits;

  /* FIXME: We don't have a good way to tell the difference between failures and empty returns */
  while (current) {
    EphyHistoryPageVisit *visit = (EphyHistoryPageVisit *)current->data;
    if (!ephy_history_service_get_url_row (self, NULL, visit->url)) {
      ephy_history_page_visit_list_free (visits);
      g_warning ("Tried to process an orphaned page visit");
      return FALSE;
    }

    current = current->next;
  }

  *result = visits;
  return TRUE;
}

static gboolean
ephy_history_service_execute_get_hosts (EphyHistoryService *self,
                                        gpointer            pointer,
                                        gpointer           *results)
{
  GList *hosts;

  hosts = ephy_history_service_get_all_hosts (self);
  *results = hosts;

  return TRUE;
}

static gboolean
ephy_history_service_execute_query_hosts (EphyHistoryService *self,
                                          EphyHistoryQuery   *query,
                                          gpointer           *results)
{
  GList *hosts;

  hosts = ephy_history_service_find_host_rows (self, query);
  *results = hosts;

  return TRUE;
}

void
ephy_history_service_add_visit (EphyHistoryService     *self,
                                EphyHistoryPageVisit   *visit,
                                GCancellable           *cancellable,
                                EphyHistoryJobCallback  callback,
                                gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (visit);

  message = ephy_history_service_message_new (self, ADD_VISIT,
                                              ephy_history_page_visit_copy (visit),
                                              (GDestroyNotify)ephy_history_page_visit_free,
                                              NULL,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

void
ephy_history_service_add_visits (EphyHistoryService     *self,
                                 GList                  *visits,
                                 GCancellable           *cancellable,
                                 EphyHistoryJobCallback  callback,
                                 gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (visits);

  message = ephy_history_service_message_new (self, ADD_VISITS,
                                              ephy_history_page_visit_list_copy (visits),
                                              (GDestroyNotify)ephy_history_page_visit_list_free,
                                              NULL,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

void
ephy_history_service_find_visits_in_time (EphyHistoryService     *self,
                                          gint64                  from,
                                          gint64                  to,
                                          GCancellable           *cancellable,
                                          EphyHistoryJobCallback  callback,
                                          gpointer                user_data)
{
  EphyHistoryQuery *query;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));

  query = ephy_history_query_new ();
  query->from = from;
  query->to = to;

  ephy_history_service_query_visits (self, query, cancellable, callback, user_data);
  ephy_history_query_free (query);
}

void
ephy_history_service_query_visits (EphyHistoryService     *self,
                                   EphyHistoryQuery       *query,
                                   GCancellable           *cancellable,
                                   EphyHistoryJobCallback  callback,
                                   gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (query);

  message = ephy_history_service_message_new (self, QUERY_VISITS,
                                              ephy_history_query_copy (query),
                                              (GDestroyNotify)ephy_history_query_free,
                                              (GDestroyNotify)ephy_history_page_visit_list_free,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

static gboolean
ephy_history_service_execute_query_urls (EphyHistoryService *self,
                                         EphyHistoryQuery   *query,
                                         gpointer           *result)
{
  GList *urls = ephy_history_service_find_url_rows (self, query);

  *result = urls;

  return TRUE;
}

void
ephy_history_service_query_urls (EphyHistoryService     *self,
                                 EphyHistoryQuery       *query,
                                 GCancellable           *cancellable,
                                 EphyHistoryJobCallback  callback,
                                 gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (query);

  message = ephy_history_service_message_new (self, QUERY_URLS,
                                              ephy_history_query_copy (query),
                                              (GDestroyNotify)ephy_history_query_free,
                                              (GDestroyNotify)ephy_history_url_list_free,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

void
ephy_history_service_get_hosts (EphyHistoryService     *self,
                                GCancellable           *cancellable,
                                EphyHistoryJobCallback  callback,
                                gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));

  message = ephy_history_service_message_new (self, GET_HOSTS,
                                              NULL, NULL,
                                              (GDestroyNotify)ephy_history_host_list_free,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

void
ephy_history_service_query_hosts (EphyHistoryService     *self,
                                  EphyHistoryQuery       *query,
                                  GCancellable           *cancellable,
                                  EphyHistoryJobCallback  callback,
                                  gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));

  message = ephy_history_service_message_new (self, QUERY_HOSTS,
                                              ephy_history_query_copy (query),
                                              (GDestroyNotify)ephy_history_query_free,
                                              (GDestroyNotify)ephy_history_host_list_free,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

static gboolean
set_url_title_signal_emit (SignalEmissionContext *ctx)
{
  EphyHistoryURL *url = (EphyHistoryURL *)ctx->user_data;

  g_signal_emit (ctx->service, signals[URL_TITLE_CHANGED], 0, url->url, url->title);

  return FALSE;
}

static gboolean
ephy_history_service_execute_set_url_title (EphyHistoryService *self,
                                            EphyHistoryURL     *url,
                                            gpointer           *result)
{
  char *title = g_strdup (url->title);

  if (!ephy_history_service_get_url_row (self, NULL, url)) {
    /* The URL is not yet in the database, so we can't update it.. */
    g_free (title);
    return FALSE;
  } else {
    SignalEmissionContext *ctx;

    g_free (url->title);
    url->title = title;
    ephy_history_service_update_url_row (self, url);

    ctx = signal_emission_context_new (self,
                                       ephy_history_url_copy (url),
                                       (GDestroyNotify)ephy_history_url_free);
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                     (GSourceFunc)set_url_title_signal_emit,
                     ctx, (GDestroyNotify)signal_emission_context_free);
    return TRUE;
  }
}

void
ephy_history_service_set_url_title (EphyHistoryService     *self,
                                    const char             *orig_url,
                                    const char             *title,
                                    GCancellable           *cancellable,
                                    EphyHistoryJobCallback  callback,
                                    gpointer                user_data)
{
  EphyHistoryURL *url;
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (orig_url);
  g_assert (title);
  g_assert (*title != '\0');

  url = ephy_history_url_new (orig_url, title, 0, 0, 0);
  message = ephy_history_service_message_new (self, SET_URL_TITLE,
                                              url, (GDestroyNotify)ephy_history_url_free,
                                              NULL, cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

static gboolean
ephy_history_service_execute_set_url_zoom_level (EphyHistoryService *self,
                                                 GVariant           *variant,
                                                 gpointer           *result)
{
  char *url_string;
  double zoom_level;
  EphyHistoryHost *host;

  g_variant_get (variant, "(sd)", &url_string, &zoom_level);

  host = ephy_history_service_get_host_row_from_url (self, url_string);
  g_free (url_string);

  g_assert (host);

  host->zoom_level = zoom_level;
  ephy_history_service_update_host_row (self, host);

  return TRUE;
}

void
ephy_history_service_set_url_zoom_level (EphyHistoryService     *self,
                                         const char             *url,
                                         double                  zoom_level,
                                         GCancellable           *cancellable,
                                         EphyHistoryJobCallback  callback,
                                         gpointer                user_data)
{
  EphyHistoryServiceMessage *message;
  GVariant *variant;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (url);

  /* Ensure that a change value which equals default zoom level is stored as 0.0 */
  if (zoom_level == g_settings_get_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL))
    zoom_level = 0.0f;

  variant = g_variant_new ("(sd)", url, zoom_level);

  message = ephy_history_service_message_new (self, SET_URL_ZOOM_LEVEL,
                                              variant, (GDestroyNotify)g_variant_unref,
                                              NULL, cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

static gboolean
ephy_history_service_execute_set_url_hidden (EphyHistoryService *self,
                                             EphyHistoryURL     *url,
                                             gpointer           *result)
{
  gboolean hidden;

  hidden = url->hidden;

  if (!ephy_history_service_get_url_row (self, NULL, url)) {
    /* The URL is not yet in the database, so we can't update it.. */
    return FALSE;
  } else {
    url->hidden = hidden;
    ephy_history_service_update_url_row (self, url);
    return TRUE;
  }
}

void
ephy_history_service_set_url_hidden (EphyHistoryService     *self,
                                     const char             *orig_url,
                                     gboolean                hidden,
                                     GCancellable           *cancellable,
                                     EphyHistoryJobCallback  callback,
                                     gpointer                user_data)
{
  EphyHistoryServiceMessage *message;
  EphyHistoryURL *url;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (orig_url);

  url = ephy_history_url_new (orig_url, NULL, 0, 0, 0);
  url->hidden = hidden;

  message = ephy_history_service_message_new (self, SET_URL_HIDDEN,
                                              url, (GDestroyNotify)ephy_history_url_free,
                                              NULL, cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

static gboolean
ephy_history_service_execute_get_url (EphyHistoryService *self,
                                      const gchar        *orig_url,
                                      gpointer           *result)
{
  EphyHistoryURL *url;

  url = ephy_history_service_get_url_row (self, orig_url, NULL);

  *result = url;

  return !!url;
}

void
ephy_history_service_get_url (EphyHistoryService     *self,
                              const char             *url,
                              GCancellable           *cancellable,
                              EphyHistoryJobCallback  callback,
                              gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (url);

  message = ephy_history_service_message_new (self, GET_URL,
                                              g_strdup (url), g_free, (GDestroyNotify)ephy_history_url_free,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

static gboolean
ephy_history_service_execute_get_host_for_url (EphyHistoryService *self,
                                               const gchar        *url,
                                               gpointer           *result)
{
  EphyHistoryHost *host;

  host = ephy_history_service_get_host_row_from_url (self, url);
  g_assert (host);

  *result = host;

  return !!host;
}

void
ephy_history_service_get_host_for_url (EphyHistoryService     *self,
                                       const char             *url,
                                       GCancellable           *cancellable,
                                       EphyHistoryJobCallback  callback,
                                       gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (url);

  message = ephy_history_service_message_new (self, GET_HOST_FOR_URL,
                                              g_strdup (url), g_free, (GDestroyNotify)ephy_history_host_free,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

static gboolean
delete_urls_signal_emit (SignalEmissionContext *ctx)
{
  EphyHistoryURL *url = (EphyHistoryURL *)ctx->user_data;

  g_signal_emit (ctx->service, signals[URL_DELETED], 0, url);

  return FALSE;
}

static gboolean
ephy_history_service_execute_delete_urls (EphyHistoryService *self,
                                          GList              *urls,
                                          gpointer           *result)
{
  GList *l;
  EphyHistoryURL *url;
  SignalEmissionContext *ctx;

  for (l = urls; l; l = l->next) {
    url = l->data;
    ephy_history_service_delete_url (self, url);

    if (url->notify_delete) {
      ctx = signal_emission_context_new (self, ephy_history_url_copy (url),
                                         (GDestroyNotify)ephy_history_url_free);
      g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                       (GSourceFunc)delete_urls_signal_emit,
                       ctx,
                       (GDestroyNotify)signal_emission_context_free);
    }
  }

  ephy_history_service_delete_orphan_hosts (self);

  return TRUE;
}

static gboolean
delete_host_signal_emit (SignalEmissionContext *ctx)
{
  char *host = (char *)ctx->user_data;

  g_signal_emit (ctx->service, signals[HOST_DELETED], 0, host);

  return FALSE;
}

static gboolean
ephy_history_service_execute_delete_host (EphyHistoryService     *self,
                                          EphyHistoryHost        *host,
                                          EphyHistoryJobCallback  callback,
                                          gpointer                user_data)
{
  SignalEmissionContext *ctx;

  ephy_history_service_delete_host_row (self, host);

  ctx = signal_emission_context_new (self, g_strdup (host->url),
                                     (GDestroyNotify)g_free);
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                   (GSourceFunc)delete_host_signal_emit,
                   ctx,
                   (GDestroyNotify)signal_emission_context_free);

  return TRUE;
}

static gboolean
ephy_history_service_execute_clear (EphyHistoryService *self,
                                    gpointer            pointer,
                                    gpointer           *result)
{
  if (!self->history_database)
    return FALSE;

  ephy_history_service_commit_transaction (self);
  ephy_sqlite_connection_close (self->history_database);
  ephy_sqlite_connection_delete_database (self->history_database);

  ephy_history_service_open_database_connections (self);
  ephy_history_service_open_transaction (self);

  return TRUE;
}

void
ephy_history_service_delete_urls (EphyHistoryService     *self,
                                  GList                  *urls,
                                  GCancellable           *cancellable,
                                  EphyHistoryJobCallback  callback,
                                  gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (urls);

  message = ephy_history_service_message_new (self, DELETE_URLS,
                                              ephy_history_url_list_copy (urls), (GDestroyNotify)ephy_history_url_list_free,
                                              NULL, cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

void
ephy_history_service_delete_host (EphyHistoryService     *self,
                                  EphyHistoryHost        *host,
                                  GCancellable           *cancellable,
                                  EphyHistoryJobCallback  callback,
                                  gpointer                user_data)
{
  EphyHistoryServiceMessage *message =
    ephy_history_service_message_new (self, DELETE_HOST,
                                      ephy_history_host_copy (host), (GDestroyNotify)ephy_history_host_free,
                                      NULL, cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

void
ephy_history_service_clear (EphyHistoryService     *self,
                            GCancellable           *cancellable,
                            EphyHistoryJobCallback  callback,
                            gpointer                user_data)
{
  EphyHistoryServiceMessage *message;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));

  message = ephy_history_service_message_new (self, CLEAR,
                                              NULL, NULL, NULL,
                                              cancellable, callback, user_data);
  ephy_history_service_send_message (self, message);
}

static void
ephy_history_service_quit (EphyHistoryService     *self,
                           EphyHistoryJobCallback  callback,
                           gpointer                user_data)
{
  EphyHistoryServiceMessage *message =
    ephy_history_service_message_new (self, QUIT,
                                      NULL, NULL, NULL, NULL,
                                      callback, user_data);
  ephy_history_service_send_message (self, message);
}

static EphyHistoryServiceMethod methods[] = {
  (EphyHistoryServiceMethod)ephy_history_service_execute_set_url_title,
  (EphyHistoryServiceMethod)ephy_history_service_execute_set_url_zoom_level,
  (EphyHistoryServiceMethod)ephy_history_service_execute_set_url_hidden,
  (EphyHistoryServiceMethod)ephy_history_service_execute_add_visit,
  (EphyHistoryServiceMethod)ephy_history_service_execute_add_visits,
  (EphyHistoryServiceMethod)ephy_history_service_execute_delete_urls,
  (EphyHistoryServiceMethod)ephy_history_service_execute_delete_host,
  (EphyHistoryServiceMethod)ephy_history_service_execute_clear,
  (EphyHistoryServiceMethod)ephy_history_service_execute_quit,
  (EphyHistoryServiceMethod)ephy_history_service_execute_get_url,
  (EphyHistoryServiceMethod)ephy_history_service_execute_get_host_for_url,
  (EphyHistoryServiceMethod)ephy_history_service_execute_query_urls,
  (EphyHistoryServiceMethod)ephy_history_service_execute_find_visits,
  (EphyHistoryServiceMethod)ephy_history_service_execute_get_hosts,
  (EphyHistoryServiceMethod)ephy_history_service_execute_query_hosts
};

static gboolean
ephy_history_service_message_is_write (EphyHistoryServiceMessage *message)
{
  return message->type < QUIT;
}

static void
ephy_history_service_process_message (EphyHistoryService        *self,
                                      EphyHistoryServiceMessage *message)
{
  EphyHistoryServiceMethod method;

  g_assert (self->history_thread == g_thread_self ());

  if (g_cancellable_is_cancelled (message->cancellable) &&
      !ephy_history_service_message_is_write (message)) {
    ephy_history_service_message_free (message);
    return;
  }

  method = methods[message->type];
  message->result = NULL;
  if (message->service->history_database) {
    ephy_history_service_open_transaction (self);
    message->success = method (message->service, message->method_argument, &message->result);
    ephy_history_service_commit_transaction (self);
  } else {
    message->success = FALSE;
  }

  if (message->callback || message->type == CLEAR)
    g_idle_add ((GSourceFunc)ephy_history_service_execute_job_callback, message);
  else
    ephy_history_service_message_free (message);

  return;
}

/* Public API. */

void
ephy_history_service_find_urls (EphyHistoryService     *self,
                                gint64                  from,
                                gint64                  to,
                                guint                   limit,
                                gint                    host,
                                GList                  *substring_list,
                                EphyHistorySortType     sort_type,
                                GCancellable           *cancellable,
                                EphyHistoryJobCallback  callback,
                                gpointer                user_data)
{
  EphyHistoryQuery *query;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));

  query = ephy_history_query_new ();
  query->from = from;
  query->to = to;
  query->substring_list = substring_list;
  query->sort_type = sort_type;
  query->host = host;

  if (limit != 0)
    query->limit = limit;

  ephy_history_service_query_urls (self,
                                   query, cancellable,
                                   callback, user_data);
  ephy_history_query_free (query);
}

void
ephy_history_service_visit_url (EphyHistoryService       *self,
                                const char               *url,
                                const char               *sync_id,
                                gint64                    visit_time,
                                EphyHistoryPageVisitType  visit_type,
                                gboolean                  should_notify)
{
  EphyHistoryPageVisit *visit;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));
  g_assert (url);
  g_assert (visit_time > 0);

  visit = ephy_history_page_visit_new (url, visit_time, visit_type);
  visit->url->sync_id = g_strdup (sync_id);
  visit->url->notify_visit = should_notify;
  ephy_history_service_add_visit (self, visit, NULL, NULL, NULL);
  ephy_history_page_visit_free (visit);

  ephy_history_service_queue_urls_visited (self);
}

void
ephy_history_service_find_hosts (EphyHistoryService     *self,
                                 gint64                  from,
                                 gint64                  to,
                                 GCancellable           *cancellable,
                                 EphyHistoryJobCallback  callback,
                                 gpointer                user_data)
{
  EphyHistoryQuery *query;

  g_assert (EPHY_IS_HISTORY_SERVICE (self));

  query = ephy_history_query_new ();

  query->from = from;
  query->to = to;

  ephy_history_service_query_hosts (self, query,
                                    cancellable, callback, user_data);
  ephy_history_query_free (query);
}

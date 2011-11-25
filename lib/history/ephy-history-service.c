/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* *  Copyright © 2011 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ephy-history-service.h"

#include "ephy-history-service-private.h"
#include "ephy-history-types.h"
#include "ephy-sqlite-connection.h"

typedef gboolean (*EphyHistoryServiceMethod)                              (EphyHistoryService *self, gpointer data, gpointer *result);

typedef enum {
  /* WRITE */
  SET_URL_TITLE,
  SET_URL_ZOOM_LEVEL,
  SET_URL_PROPERTY, /* We only need this SET_ ? */
  ADD_VISIT,
  ADD_VISITS,
  DELETE_URLS,
  /* QUIT */
  QUIT,
  /* READ */
  GET_URL,
  QUERY_URLS,
  QUERY_VISITS,
} EphyHistoryServiceMessageType;
  
typedef struct _EphyHistoryServiceMessage {
  EphyHistoryService *service;
  EphyHistoryServiceMessageType type;
  gpointer *method_argument;
  gboolean success;
  gpointer result;
  gpointer user_data;
  GDestroyNotify method_argument_cleanup;
  EphyHistoryJobCallback callback;
} EphyHistoryServiceMessage;

static gpointer run_history_service_thread                                (EphyHistoryService *self);
static void ephy_history_service_process_message                          (EphyHistoryService *self, EphyHistoryServiceMessage *message);
static gboolean ephy_history_service_execute_quit                         (EphyHistoryService *self, gpointer data, gpointer *result);
static void ephy_history_service_quit                                     (EphyHistoryService *self, EphyHistoryJobCallback callback, gpointer user_data);

enum {
  PROP_0,
  PROP_HISTORY_FILENAME,
};

#define EPHY_HISTORY_SERVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), EPHY_TYPE_HISTORY_SERVICE, EphyHistoryServicePrivate))

G_DEFINE_TYPE (EphyHistoryService, ephy_history_service, G_TYPE_OBJECT);

static void
ephy_history_service_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  EphyHistoryService *self = EPHY_HISTORY_SERVICE (object);

  switch (property_id) {
    case PROP_HISTORY_FILENAME:
      g_free (self->priv->history_filename);
      self->priv->history_filename = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
      break;
  }
}

static void
ephy_history_service_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  EphyHistoryService *self = EPHY_HISTORY_SERVICE (object);
  switch (property_id) {
    case PROP_HISTORY_FILENAME:
      g_value_set_string (value, self->priv->history_filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
ephy_history_service_finalize (GObject *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;

  ephy_history_service_quit (EPHY_HISTORY_SERVICE (self), NULL, NULL);

  if (priv->history_thread)
    g_thread_join (priv->history_thread);

  g_free (priv->history_filename);

  G_OBJECT_CLASS (ephy_history_service_parent_class)->finalize (self);
}

static void
ephy_history_service_class_init (EphyHistoryServiceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ephy_history_service_finalize;
  gobject_class->get_property = ephy_history_service_get_property;
  gobject_class->set_property = ephy_history_service_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_HISTORY_FILENAME,
                                   g_param_spec_string ("history-filename",
                                                        "History filename",
                                                        "The filename of the SQLite file holding containing history",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (EphyHistoryServicePrivate));
}

static void
ephy_history_service_init (EphyHistoryService *self)
{
  self->priv = EPHY_HISTORY_SERVICE_GET_PRIVATE (self);

  self->priv->active = TRUE;
  self->priv->history_thread = g_thread_new ("EphyHistoryService", (GThreadFunc) run_history_service_thread, self);
  self->priv->queue = g_async_queue_new ();
}

EphyHistoryService *
ephy_history_service_new (const char *history_filename)
{
  return EPHY_HISTORY_SERVICE (g_object_new (EPHY_TYPE_HISTORY_SERVICE,
                                             "history-filename", history_filename,
                                              NULL));
}

static gint
sort_messages (EphyHistoryServiceMessage* a, EphyHistoryServiceMessage* b, gpointer user_data)
{
  return a->type > b->type ? 1 : a->type == b->type ? 0 : -1;
}

static void
ephy_history_service_send_message (EphyHistoryService *self, gpointer data)
{
  EphyHistoryServicePrivate *priv = self->priv;

  g_async_queue_push_sorted (priv->queue, data, (GCompareDataFunc)sort_messages, NULL);
}

static void
ephy_history_service_commit (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = self->priv;
  GError *error = NULL;
  g_assert (priv->history_thread == g_thread_self ());

  if (NULL == priv->history_database)
    return;

  ephy_sqlite_connection_commit_transaction (priv->history_database, &error);
  if (NULL != error) {
    g_error ("Could not commit idle history database transaction: %s", error->message);
    g_error_free (error);
  }
  ephy_sqlite_connection_begin_transaction (priv->history_database, &error);
  if (NULL != error) {
    g_error ("Could not start long-running history database transaction: %s", error->message);
    g_error_free (error);
  }

  self->priv->scheduled_to_commit = FALSE;
}

static void
ephy_history_service_enable_foreign_keys (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  GError *error = NULL;

  if (NULL == priv->history_database)
    return;

  ephy_sqlite_connection_execute (priv->history_database,
                                  "PRAGMA foreign_keys = ON", &error);

  if (error) {
    g_error ("Could not enable foreign keys pragma: %s", error->message);
    g_error_free (error);
  }
}

static gboolean
ephy_history_service_open_database_connections (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  GError *error = NULL;

  g_assert (priv->history_thread == g_thread_self ());

  priv->history_database = ephy_sqlite_connection_new ();
  ephy_sqlite_connection_open (priv->history_database, priv->history_filename, &error);
  if (error) {
    g_object_unref (priv->history_database);
    priv->history_database = NULL;
    g_error ("Could not open history database: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  ephy_history_service_enable_foreign_keys (self);

  ephy_sqlite_connection_begin_transaction (priv->history_database, &error);
  if (error) {
    g_error ("Could not begin long running transaction in history database: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  if ((ephy_history_service_initialize_hosts_table (self) == FALSE) ||
      (ephy_history_service_initialize_urls_table (self) == FALSE) ||
      (ephy_history_service_initialize_visits_table (self) == FALSE))
    return FALSE;

  return TRUE;
}

static void
ephy_history_service_close_database_connections (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;

  g_assert (priv->history_thread == g_thread_self ());

  ephy_sqlite_connection_close (priv->history_database);
  g_object_unref (priv->history_database);
  priv->history_database = NULL;
}

static gboolean
ephy_history_service_is_scheduled_to_quit (EphyHistoryService *self)
{
  return self->priv->scheduled_to_quit;
}

static gboolean
ephy_history_service_is_scheduled_to_commit (EphyHistoryService *self)
{
  return self->priv->scheduled_to_commit;
}

void
ephy_history_service_schedule_commit (EphyHistoryService *self)
{
  self->priv->scheduled_to_commit = TRUE;
}

static gboolean
ephy_history_service_execute_quit (EphyHistoryService *self, gpointer data, gpointer *result)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  g_assert (priv->history_thread == g_thread_self ());

  if (ephy_history_service_is_scheduled_to_commit (self))
    ephy_history_service_commit (self);

  g_async_queue_unref (priv->queue);

  self->priv->scheduled_to_quit = TRUE;

  return FALSE;
}

static gpointer
run_history_service_thread (EphyHistoryService *self)
{
  EphyHistoryServicePrivate *priv = EPHY_HISTORY_SERVICE (self)->priv;
  EphyHistoryServiceMessage *message;

  g_assert (priv->history_thread == g_thread_self ());

  if (ephy_history_service_open_database_connections (self) == FALSE)
    return NULL;

  do {
    message = g_async_queue_try_pop (priv->queue);
    if (!message) {
      /* Schedule commit if needed. */
      if (ephy_history_service_is_scheduled_to_commit (self))
        ephy_history_service_commit (self);

      /* Block the thread until there's data in the queue. */
      message = g_async_queue_pop (priv->queue);
    }

    /* Process item. */
    ephy_history_service_process_message (self, message);

  } while (!ephy_history_service_is_scheduled_to_quit (self));

  ephy_history_service_close_database_connections (self);
  ephy_history_service_execute_quit (self, NULL, NULL);

  return NULL;
}

static EphyHistoryServiceMessage *
ephy_history_service_message_new (EphyHistoryService *service,
                                  EphyHistoryServiceMessageType type,
                                  gpointer method_argument,
                                  GDestroyNotify method_argument_cleanup,
                                  EphyHistoryJobCallback callback,
                                  gpointer user_data)
{
  EphyHistoryServiceMessage *details = g_slice_alloc0 (sizeof (EphyHistoryServiceMessage));

  details->service = service; 
  details->type = type;
  details->method_argument = method_argument;
  details->method_argument_cleanup = method_argument_cleanup;
  details->callback = callback;
  details->user_data = user_data;

  return details;
}

static void
ephy_history_service_message_free (EphyHistoryServiceMessage *details)
{
  if (details->method_argument_cleanup)
    details->method_argument_cleanup (details->method_argument);

  g_slice_free1 (sizeof (EphyHistoryServiceMessage), details);
}

static gboolean
ephy_history_service_execute_job_callback (gpointer data)
{
  EphyHistoryServiceMessage *details = (EphyHistoryServiceMessage*) data;

  g_assert (details->callback);
  details->callback (details->service, details->success, details->result, details->user_data);
  ephy_history_service_message_free (details);

  return FALSE;
}

static gboolean
ephy_history_service_execute_add_visit_helper (EphyHistoryService *self, EphyHistoryPageVisit *visit)
{
  if (visit->url->host == NULL)
    visit->url->host = ephy_history_service_get_host_row_from_url (self, visit->url->url);

  visit->url->host->visit_count++;
  ephy_history_service_update_host_row (self, visit->url->host);

  /* A NULL return here means that the URL does not yet exist in the database */
  if (NULL == ephy_history_service_get_url_row (self, visit->url->url, visit->url)) {
    visit->url->last_visit_time = visit->visit_time;
    ephy_history_service_add_url_row (self, visit->url);

    if (visit->url->id == -1) {
      g_error ("Adding visit failed after failed URL addition.");
      return FALSE;
    }

  } else {
    visit->url->visit_count++;

    if (visit->visit_time > visit->url->last_visit_time)
      visit->url->last_visit_time = visit->visit_time;

    ephy_history_service_update_url_row (self, visit->url);
  }

  ephy_history_service_add_visit_row (self, visit);
  return visit->id != -1;
}

static gboolean
ephy_history_service_execute_add_visit (EphyHistoryService *self, EphyHistoryPageVisit *visit, gpointer *result)
{
  gboolean success;
  g_assert (self->priv->history_thread == g_thread_self ());

  success = ephy_history_service_execute_add_visit_helper (self, visit);
  return success;
}

static gboolean
ephy_history_service_execute_add_visits (EphyHistoryService *self, GList *visits, gpointer *result)
{
  gboolean success = TRUE;
  g_assert (self->priv->history_thread == g_thread_self ());

  while (visits) {
    success = success && ephy_history_service_execute_add_visit_helper (self, (EphyHistoryPageVisit *) visits->data);
    visits = visits->next;
  }

  ephy_history_service_schedule_commit (self);

  return success;
}

static gboolean
ephy_history_service_execute_find_visits (EphyHistoryService *self, EphyHistoryQuery *query, gpointer *result)
{
  GList *visits = ephy_history_service_find_visit_rows (self, query);
  GList *current = visits;

  /* FIXME: We don't have a good way to tell the difference between failures and empty returns */
  while (current) {
    EphyHistoryPageVisit *visit = (EphyHistoryPageVisit *) current->data;
    if (NULL == ephy_history_service_get_url_row (self, NULL, visit->url)) {
      ephy_history_page_visit_list_free (visits);
      g_error ("Tried to process an orphaned page visit");
      return FALSE;
    }

    current = current->next;
  }

  *result = visits;
  return TRUE;
}

void
ephy_history_service_add_visit (EphyHistoryService *self, EphyHistoryPageVisit *visit, EphyHistoryJobCallback callback, gpointer user_data)
{
  EphyHistoryServiceMessage *details = 
    ephy_history_service_message_new (self, ADD_VISIT,
                                      ephy_history_page_visit_copy (visit),
                                      (GDestroyNotify) ephy_history_page_visit_free,
                                      callback, user_data);
  ephy_history_service_send_message (self, details);
}

void
ephy_history_service_add_visits (EphyHistoryService *self, GList *visits, EphyHistoryJobCallback callback, gpointer user_data)
{
  EphyHistoryServiceMessage *details = 
    ephy_history_service_message_new (self, ADD_VISITS,
                                      ephy_history_page_visit_list_copy (visits),
                                      (GDestroyNotify) ephy_history_page_visit_list_free,
                                      callback, user_data);
  ephy_history_service_send_message (self, details);
}

void
ephy_history_service_find_visits_in_time (EphyHistoryService *self, gint64 from, gint64 to, EphyHistoryJobCallback callback, gpointer user_data)
{
  EphyHistoryQuery *query = ephy_history_query_new ();
  query->from = from;
  query->to = to;

  ephy_history_service_query_visits (self, query, callback, user_data);
  ephy_history_query_free (query);
}

void
ephy_history_service_query_visits (EphyHistoryService *self, EphyHistoryQuery *query, EphyHistoryJobCallback callback, gpointer user_data)
{
  EphyHistoryServiceMessage *details;

  details = ephy_history_service_message_new (self, QUERY_VISITS,
                                              ephy_history_query_copy (query), (GDestroyNotify) ephy_history_query_free, callback, user_data);
  ephy_history_service_send_message (self, details);
}

static gboolean
ephy_history_service_execute_query_urls (EphyHistoryService *self, EphyHistoryQuery *query, gpointer *result)
{
  GList *urls = ephy_history_service_find_url_rows (self, query);

  *result = urls;

  return TRUE;
}

void
ephy_history_service_query_urls (EphyHistoryService *self, EphyHistoryQuery *query, EphyHistoryJobCallback callback, gpointer user_data)
{
  EphyHistoryServiceMessage *details;

  details = ephy_history_service_message_new (self, QUERY_URLS,
                                              ephy_history_query_copy (query), (GDestroyNotify) ephy_history_query_free, callback, user_data);
  ephy_history_service_send_message (self, details);
}

static gboolean
ephy_history_service_execute_set_url_title (EphyHistoryService *self,
                                            EphyHistoryURL *url,
                                            gpointer *result)
{
  char *title = g_strdup (url->title);

  if (NULL == ephy_history_service_get_url_row (self, NULL, url)) {
    /* The URL is not yet in the database, so we can't update it.. */
    g_free (title);
    return FALSE;
  } else {
    g_free (url->title);
    url->title = title;
    ephy_history_service_update_url_row (self, url);
    ephy_history_service_schedule_commit (self);
    return TRUE;
  }
}

void
ephy_history_service_set_url_title (EphyHistoryService *self,
                                    const char *orig_url,
                                    const char *title,
                                    EphyHistoryJobCallback callback,
                                    gpointer user_data)
{
  EphyHistoryURL *url = ephy_history_url_new (orig_url, title, 0, 0, 0, 1.0);

  EphyHistoryServiceMessage *details =
    ephy_history_service_message_new (self, SET_URL_TITLE,
                                      url, (GDestroyNotify) ephy_history_url_free,
                                      callback, user_data);
  ephy_history_service_send_message (self, details);
}

static gboolean
ephy_history_service_execute_set_url_zoom_level (EphyHistoryService *self,
                                                 EphyHistoryURL *url,
                                                 gpointer *result)
{
  double zoom_level = url->zoom_level;

  if (NULL == ephy_history_service_get_url_row (self, NULL, url)) {
    /* The URL is not yet in the database, so we can't update it.. */
    return FALSE;
  } else {
    url->zoom_level = zoom_level;
    ephy_history_service_update_url_row (self, url);
    ephy_history_service_schedule_commit (self);
    return TRUE;
  }
}

void
ephy_history_service_set_url_zoom_level (EphyHistoryService *self,
                                         const char *orig_url,
                                         const double zoom_level,
                                         EphyHistoryJobCallback callback,
                                         gpointer user_data)
{
  EphyHistoryURL *url = ephy_history_url_new (orig_url, NULL, 0, 0, 0, zoom_level);

  EphyHistoryServiceMessage *details =
    ephy_history_service_message_new (self, SET_URL_ZOOM_LEVEL,
                                      url, (GDestroyNotify) ephy_history_url_free,
                                      callback, user_data);
  ephy_history_service_send_message (self, details);
}

static gboolean
ephy_history_service_execute_get_url (EphyHistoryService *self,
                                      const gchar *orig_url,
                                      gpointer *result)
{
  EphyHistoryURL *url;

  url = ephy_history_service_get_url_row (self, orig_url, NULL);

  *result = url;

  return url != NULL;
}

void
ephy_history_service_get_url (EphyHistoryService *self,
                              const char *url,
                              EphyHistoryJobCallback callback,
                              gpointer user_data)
{
  EphyHistoryServiceMessage *details =
    ephy_history_service_message_new (self, GET_URL,
                                      g_strdup (url), g_free,
                                      callback, user_data);
  ephy_history_service_send_message (self, details);
}

static gboolean
ephy_history_service_execute_set_url_property (EphyHistoryService *self,
                                               GVariant *variant,
                                               gpointer *result)
{
  GVariant *value, *mvalue;
  gchar *url_string;
  EphyHistoryURL *url;
  EphyHistoryURLProperty property;

  g_variant_get (variant, "(s(iv))", &url_string, &property, &value);

  url = ephy_history_url_new (url_string, NULL, 0, 0, 0, 1.0);
  g_free (url_string);

  if (NULL == ephy_history_service_get_url_row (self, NULL, url)) {
    g_variant_unref (value);
    ephy_history_url_free (url);

    return FALSE;
  }

  switch (property) {
  case EPHY_HISTORY_URL_TITLE:
    if (url->title)
      g_free (url->title);
    mvalue = g_variant_get_maybe (value);
    if (mvalue) {
      url->title = g_variant_dup_string (mvalue, NULL);
      g_variant_unref (mvalue);
    } else {
      url->title = NULL;
    }
    break;
  case EPHY_HISTORY_URL_ZOOM_LEVEL:
    url->zoom_level = g_variant_get_double (value);
    break;
  default:
    g_assert_not_reached();
  }
  g_variant_unref (value);

  ephy_history_service_update_url_row (self, url);
  ephy_history_service_schedule_commit (self);

  return TRUE;
}

void
ephy_history_service_set_url_property (EphyHistoryService *self,
                                       const char *url,
                                       EphyHistoryURLProperty property,
                                       GVariant *value,
                                       EphyHistoryJobCallback callback,
                                       gpointer user_data)
{
  GVariant *variant = g_variant_new ("(s(iv))", url, property, value);

  EphyHistoryServiceMessage *details =
    ephy_history_service_message_new (self, SET_URL_PROPERTY,
                                      variant, (GDestroyNotify)g_variant_unref,
                                      callback, user_data);

  ephy_history_service_send_message (self, details);
}

static gboolean
ephy_history_service_execute_delete_urls (EphyHistoryService *self,
                                          GList *urls,
                                          gpointer *result)
{
  GList *l;
  EphyHistoryURL *url;

  for (l = urls; l != NULL; l = l->next) {
    url = l->data;
    ephy_history_service_delete_url (self, url);
  }

  ephy_history_service_schedule_commit (self);

  return TRUE;
}

void
ephy_history_service_delete_urls (EphyHistoryService *self,
                                  GList *urls,
                                  EphyHistoryJobCallback callback,
                                  gpointer user_data)
{
  EphyHistoryServiceMessage *details =
    ephy_history_service_message_new (self, DELETE_URLS, 
                                      ephy_history_url_list_copy (urls), (GDestroyNotify)ephy_history_url_list_free,
                                      callback, user_data);
  ephy_history_service_send_message (self, details);
}

static void
ephy_history_service_quit (EphyHistoryService *self,
                           EphyHistoryJobCallback callback,
                           gpointer user_data)
{
  EphyHistoryServiceMessage *details =
    ephy_history_service_message_new (self, QUIT, 
                                      NULL, NULL,
                                      callback, user_data);
  ephy_history_service_send_message (self, details);
}

static EphyHistoryServiceMethod methods[] = {
  (EphyHistoryServiceMethod)ephy_history_service_execute_set_url_title,
  (EphyHistoryServiceMethod)ephy_history_service_execute_set_url_zoom_level,
  (EphyHistoryServiceMethod)ephy_history_service_execute_set_url_property,
  (EphyHistoryServiceMethod)ephy_history_service_execute_add_visit,
  (EphyHistoryServiceMethod)ephy_history_service_execute_add_visits,
  (EphyHistoryServiceMethod)ephy_history_service_execute_delete_urls,
  (EphyHistoryServiceMethod)ephy_history_service_execute_quit,
  (EphyHistoryServiceMethod)ephy_history_service_execute_get_url,
  (EphyHistoryServiceMethod)ephy_history_service_execute_query_urls,
  (EphyHistoryServiceMethod)ephy_history_service_execute_find_visits
};

static void
ephy_history_service_process_message (EphyHistoryService *self,
                                      EphyHistoryServiceMessage *message)
{
  EphyHistoryServiceMethod method;

  g_assert (self->priv->history_thread == g_thread_self ());

  method = methods[message->type];
  message->result = NULL;
  message->success = method (message->service, message->method_argument, &message->result);

  if (message->callback)
    g_idle_add ((GSourceFunc)ephy_history_service_execute_job_callback, message);
  else
    ephy_history_service_message_free (message);

  return;
}

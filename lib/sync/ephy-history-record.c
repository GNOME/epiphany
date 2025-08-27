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
#include "ephy-history-record.h"

#include "ephy-history-types.h"
#include "ephy-synchronizable.h"

struct _EphyHistoryRecord {
  GObject parent_instance;

  char *id;
  char *title;
  char *uri;
  GSequence *visits;
};

static void json_serializable_iface_init (JsonSerializableIface *iface);
static void ephy_synchronizable_iface_init (EphySynchronizableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyHistoryRecord, ephy_history_record, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE,
                                                      json_serializable_iface_init)
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE,
                                                      ephy_synchronizable_iface_init))

enum {
  PROP_0,
  PROP_ID,
  PROP_TITLE,
  PROP_URI,
  PROP_VISITS,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

typedef struct {
  gint64 timestamp; /* UNIX time in microseconds. */
  guint type;       /* Transition type.*/
} EphyHistoryRecordVisit;

static EphyHistoryRecordVisit *
ephy_history_record_visit_new (gint64 timestamp,
                               guint  type)
{
  EphyHistoryRecordVisit *visit;

  visit = g_new (EphyHistoryRecordVisit, 1);
  visit->timestamp = timestamp;
  visit->type = type;

  return visit;
}

static void
ephy_history_record_visit_free (EphyHistoryRecordVisit *visit)
{
  g_assert (visit);

  g_free (visit);
}

static int
ephy_history_record_visit_compare (EphyHistoryRecordVisit *visit1,
                                   EphyHistoryRecordVisit *visit2,
                                   gpointer                user_data)
{
  g_assert (visit1);
  g_assert (visit2);

  /* We keep visits sorted in descending order by timestamp. */
  return visit2->timestamp - visit1->timestamp;
}

static void
ephy_history_record_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EphyHistoryRecord *self = EPHY_HISTORY_RECORD (object);

  switch (prop_id) {
    case PROP_ID:
      g_free (self->id);
      self->id = g_value_dup_string (value);
      break;
    case PROP_TITLE:
      g_free (self->title);
      self->title = g_value_dup_string (value);
      break;
    case PROP_URI:
      g_free (self->uri);
      self->uri = g_value_dup_string (value);
      break;
    case PROP_VISITS:
      if (self->visits)
        g_sequence_free (self->visits);
      self->visits = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_history_record_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EphyHistoryRecord *self = EPHY_HISTORY_RECORD (object);

  switch (prop_id) {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;
    case PROP_URI:
      g_value_set_string (value, self->uri);
      break;
    case PROP_VISITS:
      g_value_set_pointer (value, self->visits);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_history_record_finalize (GObject *object)
{
  EphyHistoryRecord *self = EPHY_HISTORY_RECORD (object);

  g_free (self->id);
  g_free (self->title);
  g_free (self->uri);

  if (self->visits)
    g_sequence_free (self->visits);

  G_OBJECT_CLASS (ephy_history_record_parent_class)->finalize (object);
}

static void
ephy_history_record_class_init (EphyHistoryRecordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_history_record_set_property;
  object_class->get_property = ephy_history_record_get_property;
  object_class->finalize = ephy_history_record_finalize;

  obj_properties[PROP_ID] =
    g_param_spec_string ("id",
                         NULL, NULL,
                         "Default id",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         NULL, NULL,
                         "Default title",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_URI] =
    g_param_spec_string ("histUri",
                         NULL, NULL,
                         "Default history uri",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_VISITS] =
    g_param_spec_pointer ("visits",
                          NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_history_record_init (EphyHistoryRecord *self)
{
}

EphyHistoryRecord *
ephy_history_record_new (const char *id,
                         const char *title,
                         const char *uri,
                         gint64      last_visit_time)
{
  EphyHistoryRecordVisit *visit;
  GSequence *visits;

  /* We only use link transition for now. */
  visit = ephy_history_record_visit_new (last_visit_time, EPHY_PAGE_VISIT_LINK);
  visits = g_sequence_new ((GDestroyNotify)ephy_history_record_visit_free);
  g_sequence_prepend (visits, visit);

  return EPHY_HISTORY_RECORD (g_object_new (EPHY_TYPE_HISTORY_RECORD,
                                            "id", id,
                                            "title", title,
                                            "histUri", uri,
                                            "visits", visits,
                                            NULL));
}

void
ephy_history_record_set_id (EphyHistoryRecord *self,
                            const char        *id)
{
  g_assert (EPHY_IS_HISTORY_RECORD (self));
  g_assert (id);

  g_free (self->id);
  self->id = g_strdup (id);
}

const char *
ephy_history_record_get_id (EphyHistoryRecord *self)
{
  g_assert (EPHY_IS_HISTORY_RECORD (self));

  return self->id;
}

const char *
ephy_history_record_get_title (EphyHistoryRecord *self)
{
  g_assert (EPHY_IS_HISTORY_RECORD (self));

  return self->title;
}

const char *
ephy_history_record_get_uri (EphyHistoryRecord *self)
{
  g_assert (EPHY_IS_HISTORY_RECORD (self));

  return self->uri;
}

gint64
ephy_history_record_get_last_visit_time (EphyHistoryRecord *self)
{
  EphyHistoryRecordVisit *visit;

  g_assert (EPHY_IS_HISTORY_RECORD (self));
  g_assert (self->visits);

  if (g_sequence_is_empty (self->visits))
    return -1;

  /* Visits are sorted in descending order by date. */
  visit = (EphyHistoryRecordVisit *)g_sequence_get (g_sequence_get_begin_iter (self->visits));

  return visit->timestamp;
}

gboolean
ephy_history_record_add_visit_time (EphyHistoryRecord *self,
                                    gint64             visit_time)
{
  EphyHistoryRecordVisit *visit;

  g_assert (EPHY_IS_HISTORY_RECORD (self));

  visit = ephy_history_record_visit_new (visit_time, EPHY_PAGE_VISIT_LINK);
  if (g_sequence_lookup (self->visits, visit,
                         (GCompareDataFunc)ephy_history_record_visit_compare,
                         NULL)) {
    ephy_history_record_visit_free (visit);
    return FALSE;
  }

  g_sequence_insert_sorted (self->visits, visit,
                            (GCompareDataFunc)ephy_history_record_visit_compare,
                            NULL);

  return TRUE;
}

static JsonNode *
serializable_serialize_property (JsonSerializable *serializable,
                                 const char       *name,
                                 const GValue     *value,
                                 GParamSpec       *pspec)
{
  if (G_VALUE_HOLDS_STRING (value) && !g_value_get_string (value)) {
    JsonNode *node = json_node_new (JSON_NODE_VALUE);
    json_node_set_string (node, "");
    return node;
  }

  if (!g_strcmp0 (name, "visits")) {
    JsonNode *node = json_node_new (JSON_NODE_ARRAY);
    JsonArray *array = json_array_new ();
    GSequence *visits = g_value_get_pointer (value);
    GSequenceIter *it;

    if (visits) {
      for (it = g_sequence_get_begin_iter (visits); !g_sequence_iter_is_end (it); it = g_sequence_iter_next (it)) {
        EphyHistoryRecordVisit *visit = g_sequence_get (it);
        JsonObject *object = json_object_new ();
        json_object_set_int_member (object, "date", visit->timestamp);
        json_object_set_int_member (object, "type", visit->type);
        json_array_add_object_element (array, object);
      }
    }

    json_node_set_array (node, array);

    return node;
  }

  return json_serializable_default_serialize_property (serializable, name, value, pspec);
}

static gboolean
serializable_deserialize_property (JsonSerializable *serializable,
                                   const char       *name,
                                   GValue           *value,
                                   GParamSpec       *pspec,
                                   JsonNode         *node)
{
  if (G_VALUE_HOLDS_STRING (value) && JSON_NODE_HOLDS_NULL (node)) {
    g_value_set_string (value, "");
    return TRUE;
  }

  if (!g_strcmp0 (name, "visits")) {
    JsonArray *array = json_node_get_array (node);
    GSequence *visits = g_sequence_new ((GDestroyNotify)ephy_history_record_visit_free);

    for (guint i = 0; i < json_array_get_length (array); i++) {
      JsonObject *object = json_node_get_object (json_array_get_element (array, i));
      gint64 timestamp = json_object_get_int_member (object, "date");
      guint type = json_object_get_int_member (object, "type");
      EphyHistoryRecordVisit *visit = ephy_history_record_visit_new (timestamp, type);
      g_sequence_insert_sorted (visits, visit, (GCompareDataFunc)ephy_history_record_visit_compare, NULL);
    }

    g_value_set_pointer (value, visits);

    return TRUE;
  }

  return json_serializable_default_deserialize_property (serializable, name, value, pspec, node);
}

static void
json_serializable_iface_init (JsonSerializableIface *iface)
{
  iface->serialize_property = serializable_serialize_property;
  iface->deserialize_property = serializable_deserialize_property;
}

static const char *
synchronizable_get_id (EphySynchronizable *synchronizable)
{
  return ephy_history_record_get_id (EPHY_HISTORY_RECORD (synchronizable));
}

static gint64
synchronizable_get_server_time_modified (EphySynchronizable *synchronizable)
{
  /* No implementation.
   * We don't care about the server time modified of history records.
   */
  return 0;
}

static void
synchronizable_set_server_time_modified (EphySynchronizable *synchronizable,
                                         gint64              server_time_modified)
{
  /* No implementation.
   * We don't care about the server time modified of history records.
   */
}

static void
ephy_synchronizable_iface_init (EphySynchronizableInterface *iface)
{
  iface->get_id = synchronizable_get_id;
  iface->get_server_time_modified = synchronizable_get_server_time_modified;
  iface->set_server_time_modified = synchronizable_set_server_time_modified;
  iface->to_bso = ephy_synchronizable_default_to_bso;
}

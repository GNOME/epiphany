/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 John Cardullo <john@jcarmedia.org>
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
#include "ephy-extension-storage-record.h"

#include "ephy-synchronizable.h"

struct _EphyExtensionStorageRecord {
  GObject parent_instance;

  char *id;
  char *extension_id;
  JsonNode *storage;
  gint64 server_time_modified;
};

static void json_serializable_iface_init (JsonSerializableIface *iface);
static void ephy_synchronizable_iface_init (EphySynchronizableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyExtensionStorageRecord, ephy_extension_storage_record, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE,
                                                      json_serializable_iface_init)
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE,
                                                      ephy_synchronizable_iface_init))

typedef enum {
  PROP_ID = 1,
  PROP_EXTENSION_ID,
  PROP_STORAGE,
} EphyExtensionStorageRecordProps;

static GParamSpec *obj_properties[PROP_STORAGE + 1];

static void
ephy_extension_storage_record_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  EphyExtensionStorageRecord *self = EPHY_EXTENSION_STORAGE_RECORD (object);

  switch ((EphyExtensionStorageRecordProps)prop_id) {
    case PROP_ID:
      g_free (self->id);
      self->id = g_value_dup_string (value);
      break;
    case PROP_EXTENSION_ID:
      g_free (self->extension_id);
      self->extension_id = g_value_dup_string (value);
      break;
    case PROP_STORAGE:
      g_clear_pointer (&self->storage, json_node_unref);
      if (g_value_get_pointer (value))
        self->storage = json_node_ref (g_value_get_pointer (value));
      break;
  }
}

static void
ephy_extension_storage_record_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  EphyExtensionStorageRecord *self = EPHY_EXTENSION_STORAGE_RECORD (object);

  switch ((EphyExtensionStorageRecordProps)prop_id) {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;
    case PROP_EXTENSION_ID:
      g_value_set_string (value, self->extension_id);
      break;
    case PROP_STORAGE:
      g_value_set_pointer (value, self->storage);
      break;
  }
}

static void
ephy_extension_storage_record_finalize (GObject *object)
{
  EphyExtensionStorageRecord *self = EPHY_EXTENSION_STORAGE_RECORD (object);

  g_free (self->id);
  g_free (self->extension_id);
  g_clear_pointer (&self->storage, json_node_unref);

  G_OBJECT_CLASS (ephy_extension_storage_record_parent_class)->finalize (object);
}

static void
ephy_extension_storage_record_class_init (EphyExtensionStorageRecordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_extension_storage_record_set_property;
  object_class->get_property = ephy_extension_storage_record_get_property;
  object_class->finalize = ephy_extension_storage_record_finalize;

  obj_properties[PROP_ID] =
    g_param_spec_string ("id",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_EXTENSION_ID] =
    g_param_spec_string ("extensionId",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_STORAGE] =
    g_param_spec_pointer ("storage",
                          NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (obj_properties), obj_properties);
}

static void
ephy_extension_storage_record_init (EphyExtensionStorageRecord *self)
{
}

EphyExtensionStorageRecord *
ephy_extension_storage_record_new (const char *id,
                                   const char *extension_id,
                                   JsonNode   *storage)
{
  return EPHY_EXTENSION_STORAGE_RECORD (g_object_new (EPHY_TYPE_EXTENSION_STORAGE_RECORD,
                                                      "id", id,
                                                      "extensionId", extension_id,
                                                      "storage", storage,
                                                      NULL));
}

const char *
ephy_extension_storage_record_get_id (EphyExtensionStorageRecord *self)
{
  g_assert (EPHY_IS_EXTENSION_STORAGE_RECORD (self));

  return self->id;
}

const char *
ephy_extension_storage_record_get_extension_id (EphyExtensionStorageRecord *self)
{
  g_assert (EPHY_IS_EXTENSION_STORAGE_RECORD (self));

  return self->extension_id;
}

JsonNode *
ephy_extension_storage_record_get_storage (EphyExtensionStorageRecord *self)
{
  g_assert (EPHY_IS_EXTENSION_STORAGE_RECORD (self));

  return self->storage;
}

void
ephy_extension_storage_record_set_storage (EphyExtensionStorageRecord *self,
                                           JsonNode                   *storage)
{
  g_assert (EPHY_IS_EXTENSION_STORAGE_RECORD (self));

  g_clear_pointer (&self->storage, json_node_unref);
  if (storage)
    self->storage = json_node_ref (storage);
}

static JsonNode *
serializable_serialize_property (JsonSerializable *serializable,
                                 const char       *name,
                                 const GValue     *value,
                                 GParamSpec       *pspec)
{
  if (g_strcmp0 (name, "storage") == 0) {
    JsonNode *node = g_value_get_pointer (value);

    if (node)
      return json_node_ref (node);

    node = json_node_new (JSON_NODE_OBJECT);
    json_node_take_object (node, json_object_new ());
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
    g_value_set_static_string (value, "");
    return TRUE;
  }

  if (g_strcmp0 (name, "storage") == 0) {
    JsonNode *storage = NULL;

    if (!JSON_NODE_HOLDS_NULL (node))
      storage = json_node_ref (node);
    else {
      storage = json_node_new (JSON_NODE_OBJECT);
      json_node_take_object (storage, json_object_new ());
    }

    g_value_set_pointer (value, storage);
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
  return EPHY_EXTENSION_STORAGE_RECORD (synchronizable)->id;
}

static gint64
synchronizable_get_server_time_modified (EphySynchronizable *synchronizable)
{
  return EPHY_EXTENSION_STORAGE_RECORD (synchronizable)->server_time_modified;
}

static void
synchronizable_set_server_time_modified (EphySynchronizable *synchronizable,
                                         gint64              server_time_modified)
{
  EPHY_EXTENSION_STORAGE_RECORD (synchronizable)->server_time_modified = server_time_modified;
}

static void
ephy_synchronizable_iface_init (EphySynchronizableInterface *iface)
{
  iface->get_id = synchronizable_get_id;
  iface->get_server_time_modified = synchronizable_get_server_time_modified;
  iface->set_server_time_modified = synchronizable_set_server_time_modified;
  iface->to_bso = ephy_synchronizable_default_to_bso;
}

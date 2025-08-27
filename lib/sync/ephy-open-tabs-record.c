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
#include "ephy-open-tabs-record.h"

#include "ephy-synchronizable.h"

struct _EphyOpenTabsRecord {
  GObject parent_instance;

  char *id;
  char *client_name;

  /* List of JSON objects. Each object describes a tab and has the fields:
   * @title: a string representing the title of the current page
   * @urlHistory: a JSON array of strings (the page URLs in tab's history)
   * @icon: a string representing the favicon URI of the tab, i.e. the favicon
   *        URI of the most recent website in tab (the first item in urlHistory)
   * @lastUsed: an integer representing the UNIX time in seconds at which the
   *            tab was last accessed, or 0
   */
  GList *tabs;
};

static void json_serializable_iface_init (JsonSerializableIface *iface);
static void ephy_synchronizable_iface_init (EphySynchronizableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyOpenTabsRecord, ephy_open_tabs_record, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE,
                                                      json_serializable_iface_init)
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE,
                                                      ephy_synchronizable_iface_init))

enum {
  PROP_0,
  PROP_ID,
  PROP_CLIENT_NAME,
  PROP_TABS,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_open_tabs_record_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphyOpenTabsRecord *self = EPHY_OPEN_TABS_RECORD (object);

  switch (prop_id) {
    case PROP_ID:
      g_free (self->id);
      self->id = g_value_dup_string (value);
      break;
    case PROP_CLIENT_NAME:
      g_free (self->client_name);
      self->client_name = g_value_dup_string (value);
      break;
    case PROP_TABS:
      g_list_free_full (self->tabs, (GDestroyNotify)json_object_unref);
      self->tabs = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_open_tabs_record_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EphyOpenTabsRecord *self = EPHY_OPEN_TABS_RECORD (object);

  switch (prop_id) {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;
    case PROP_CLIENT_NAME:
      g_value_set_string (value, self->client_name);
      break;
    case PROP_TABS:
      g_value_set_pointer (value, self->tabs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_open_tabs_record_finalize (GObject *object)
{
  EphyOpenTabsRecord *self = EPHY_OPEN_TABS_RECORD (object);

  g_free (self->id);
  g_free (self->client_name);
  g_list_free_full (self->tabs, (GDestroyNotify)json_object_unref);

  G_OBJECT_CLASS (ephy_open_tabs_record_parent_class)->finalize (object);
}

static void
ephy_open_tabs_record_class_init (EphyOpenTabsRecordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_open_tabs_record_set_property;
  object_class->get_property = ephy_open_tabs_record_get_property;
  object_class->finalize = ephy_open_tabs_record_finalize;

  obj_properties[PROP_ID] =
    g_param_spec_string ("id",
                         NULL, NULL,
                         "Default id",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_CLIENT_NAME] =
    g_param_spec_string ("clientName",
                         NULL, NULL,
                         "Default client name",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_TABS] =
    g_param_spec_pointer ("tabs",
                          NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_open_tabs_record_init (EphyOpenTabsRecord *self)
{
}

EphyOpenTabsRecord *
ephy_open_tabs_record_new (const char *id,
                           const char *client_name)
{
  return EPHY_OPEN_TABS_RECORD (g_object_new (EPHY_TYPE_OPEN_TABS_RECORD,
                                              "id", id,
                                              "clientName", client_name,
                                              NULL));
}

const char *
ephy_open_tabs_record_get_id (EphyOpenTabsRecord *self)
{
  g_assert (EPHY_IS_OPEN_TABS_RECORD (self));

  return self->id;
}

const char *
ephy_open_tabs_record_get_client_name (EphyOpenTabsRecord *self)
{
  g_assert (EPHY_IS_OPEN_TABS_RECORD (self));

  return self->client_name;
}

GList *
ephy_open_tabs_record_get_tabs (EphyOpenTabsRecord *self)
{
  g_assert (EPHY_IS_OPEN_TABS_RECORD (self));

  return self->tabs;
}

void
ephy_open_tabs_record_add_tab (EphyOpenTabsRecord *self,
                               const char         *title,
                               const char         *url,
                               const char         *favicon)
{
  JsonObject *tab;
  JsonArray *url_history;

  g_assert (EPHY_IS_OPEN_TABS_RECORD (self));
  g_assert (title);
  g_assert (url);

  tab = json_object_new ();
  json_object_set_string_member (tab, "title", title);
  /* Only use the most recent URL. */
  url_history = json_array_new ();
  json_array_add_string_element (url_history, url);
  json_object_set_array_member (tab, "urlHistory", url_history);
  json_object_set_string_member (tab, "icon", favicon);
  json_object_set_int_member (tab, "lastUsed", g_get_real_time () / 1000000);

  self->tabs = g_list_prepend (self->tabs, tab);
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

  if (!g_strcmp0 (name, "tabs")) {
    JsonNode *node = json_node_new (JSON_NODE_ARRAY);
    JsonArray *array = json_array_new ();

    for (GList *l = g_value_get_pointer (value); l && l->data; l = l->next)
      json_array_add_object_element (array, json_object_ref (l->data));

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

  if (!g_strcmp0 (name, "tabs")) {
    JsonArray *array;
    GList *tabs = NULL;

    array = json_node_get_array (node);
    for (guint i = 0; i < json_array_get_length (array); i++)
      tabs = g_list_prepend (tabs, json_object_ref (json_array_get_object_element (array, i)));

    g_value_set_pointer (value, tabs);

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
  return EPHY_OPEN_TABS_RECORD (synchronizable)->id;
}

static gint64
synchronizable_get_server_time_modified (EphySynchronizable *synchronizable)
{
  /* No implementation.
   * We don't care about the server time modified of open tabs records.
   */
  return 0;
}

static void
synchronizable_set_server_time_modified (EphySynchronizable *synchronizable,
                                         gint64              server_time_modified)
{
  /* No implementation.
   * We don't care about the server time modified of open tabs records.
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

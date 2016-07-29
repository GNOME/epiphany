/*
 * Copyright (C) 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-bookmark.h"

#include "ephy-shell.h"
#include "ephy-sync-crypto.h"
#include "ephy-sync-utils.h"

#include <json-glib/json-glib.h>
#include <string.h>

struct _EphyBookmark {
  GObject      parent_instance;

  char        *url;
  char        *title;
  GSequence   *tags;
  gint64       time_added;

  char        *id;
  double       modified;
};

static JsonSerializableIface *serializable_iface = NULL;

static void json_serializable_iface_init (gpointer g_iface);

G_DEFINE_TYPE_EXTENDED (EphyBookmark, ephy_bookmark, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE, json_serializable_iface_init))

enum {
  PROP_0,
  PROP_TAGS,
  PROP_TIME_ADDED,
  PROP_TITLE,
  PROP_URL,
  LAST_PROP
};

enum {
  TAG_ADDED,
  TAG_REMOVED,
  LAST_SIGNAL
};

static GParamSpec *obj_properties[LAST_PROP];
static guint       signals[LAST_SIGNAL];

static void
ephy_bookmark_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EphyBookmark *self = EPHY_BOOKMARK (object);

  switch (prop_id) {
    case PROP_TAGS:
      self->tags = g_value_get_pointer (value);
      break;
    case PROP_TIME_ADDED:
      ephy_bookmark_set_time_added (self, g_value_get_int64 (value));
      break;
    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;
    case PROP_URL:
      self->url = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmark_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
  EphyBookmark *self = EPHY_BOOKMARK (object);

  switch (prop_id) {
    case PROP_TAGS:
      g_value_set_pointer (value, ephy_bookmark_get_tags (self));
      break;
    case PROP_TIME_ADDED:
      g_value_set_int64 (value, ephy_bookmark_get_time_added (self));
      break;
    case PROP_TITLE:
      g_value_set_string (value, ephy_bookmark_get_title (self));
      break;
    case PROP_URL:
      g_value_set_string (value, ephy_bookmark_get_url (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmark_finalize (GObject *object)
{
  EphyBookmark *self = EPHY_BOOKMARK (object);

  g_clear_pointer (&self->url, g_free);
  g_clear_pointer (&self->title, g_free);

  g_sequence_free (self->tags);

  G_OBJECT_CLASS (ephy_bookmark_parent_class)->finalize (object);
}

static void
ephy_bookmark_class_init (EphyBookmarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_bookmark_set_property;
  object_class->get_property = ephy_bookmark_get_property;
  object_class->finalize = ephy_bookmark_finalize;

  obj_properties[PROP_TAGS] =
    g_param_spec_pointer ("tags",
                          "Tags",
                          "The bookmark's tags",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  obj_properties[PROP_TIME_ADDED] =
    g_param_spec_int64 ("time-added",
                        "Time added",
                        "The bookmark's creation time",
                        0,
                        G_MAXINT64,
                        0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  obj_properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The bookmark's title",
                         "Default bookmark title",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  obj_properties[PROP_URL] =
    g_param_spec_string ("url",
                         "URL",
                         "The bookmark's URL",
                         "about:overview",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  signals[TAG_ADDED] =
    g_signal_new ("tag-added",
                  EPHY_TYPE_BOOKMARK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[TAG_REMOVED] =
    g_signal_new ("tag-removed",
                  EPHY_TYPE_BOOKMARK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);
}

static void
ephy_bookmark_init (EphyBookmark *self)
{
  self->id = ephy_sync_crypto_generate_random_string (12);
}

static JsonNode *
ephy_bookmark_json_serializable_serialize_property (JsonSerializable *serializable,
                                                    const gchar *name,
                                                    const GValue *value,
                                                    GParamSpec *pspec)
{
  JsonNode *node = NULL;

  if (g_strcmp0 (name, "tags") == 0) {
    GSequence *tags;
    GSequenceIter *iter;
    JsonArray *array;

    node = json_node_new (JSON_NODE_ARRAY);
    array = json_array_new ();
    tags = g_value_get_pointer (value);

    for (iter = g_sequence_get_begin_iter (tags);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter)) {
      json_array_add_string_element (array, g_sequence_get (iter));
    }

    json_node_set_array (node, array);
  } else {
    node = serializable_iface->serialize_property (serializable, name,
                                                   value, pspec);
  }

  return node;
}

static gboolean
ephy_bookmark_json_serializable_deserialize_property (JsonSerializable *serializable,
                                                      const gchar *name,
                                                      GValue *value,
                                                      GParamSpec *pspec,
                                                      JsonNode *node)
{
  if (g_strcmp0 (name, "tags") == 0) {
    GSequence *tags;
    JsonArray *array;
    const char *tag;

    g_assert (JSON_NODE_HOLDS_ARRAY (node));
    array = json_node_get_array (node);
    tags = g_sequence_new (g_free);

    for (gsize i = 0; i < json_array_get_length (array); i++) {
      tag = json_node_get_string (json_array_get_element (array, i));
      g_sequence_insert_sorted (tags, g_strdup (tag),
                                (GCompareDataFunc)ephy_bookmark_tags_compare, NULL);
    }

    g_value_set_pointer (value, tags);
  } else {
    serializable_iface->deserialize_property (serializable, name,
                                              value, pspec, node);
  }

  return TRUE;
}

static void
json_serializable_iface_init (gpointer g_iface)
{
  JsonSerializableIface *iface = g_iface;

  serializable_iface = g_type_default_interface_peek (JSON_TYPE_SERIALIZABLE);

  iface->serialize_property = ephy_bookmark_json_serializable_serialize_property;
  iface->deserialize_property = ephy_bookmark_json_serializable_deserialize_property;
}

EphyBookmark *
ephy_bookmark_new (char *url, char *title, GSequence *tags)
{
  return g_object_new (EPHY_TYPE_BOOKMARK,
                       "url", url,
                       "title", title,
                       "tags", tags,
                       "time-added", g_get_real_time (),
                       NULL);
}

void
ephy_bookmark_set_time_added (EphyBookmark *self,
                              gint64        time_added)
{
  g_return_if_fail (EPHY_IS_BOOKMARK (self));
  g_assert (time_added >= 0);

  self->time_added = time_added;
}

gint64
ephy_bookmark_get_time_added (EphyBookmark *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), 0);

  return self->time_added;
}


void
ephy_bookmark_set_url (EphyBookmark *self, const char *url)
{
  g_return_if_fail (EPHY_IS_BOOKMARK (self));

  self->url = g_strdup (url);
}

const char *
ephy_bookmark_get_url (EphyBookmark *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), NULL);

  return self->url;
}

void
ephy_bookmark_set_title (EphyBookmark *self, const char *title)
{
  g_return_if_fail (EPHY_IS_BOOKMARK (self));

  self->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_TITLE]);
}

const char *
ephy_bookmark_get_title (EphyBookmark *bookmark)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (bookmark), NULL);

  return bookmark->title;
}

const char *
ephy_bookmark_get_id (EphyBookmark *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), NULL);

  return self->id;
}

void
ephy_bookmark_set_modified (EphyBookmark *self,
                            double        modified)
{
  g_return_if_fail (EPHY_IS_BOOKMARK (self));

  self->modified = modified;
}

double
ephy_bookmark_get_modified (EphyBookmark *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), -1);

  return self->modified;
}

void
ephy_bookmark_add_tag (EphyBookmark *self,
                       const char   *tag)
{
  GSequenceIter *tag_iter;
  GSequenceIter *prev_tag_iter;

  g_return_if_fail (EPHY_IS_BOOKMARK (self));
  g_return_if_fail (tag != NULL);

  tag_iter = g_sequence_search (self->tags,
                                (gpointer)tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);

  prev_tag_iter = g_sequence_iter_prev (tag_iter);
  if (g_sequence_iter_is_end (prev_tag_iter)
      || g_strcmp0 (g_sequence_get (prev_tag_iter), tag) != 0)
    g_sequence_insert_before (tag_iter, g_strdup (tag));

  g_signal_emit (self, signals[TAG_ADDED], 0);
}

void
ephy_bookmark_remove_tag (EphyBookmark *self,
                          const char   *tag)
{
  GSequenceIter *tag_iter;

  g_return_if_fail (EPHY_IS_BOOKMARK (self));
  g_return_if_fail (tag != NULL);

  tag_iter = g_sequence_lookup (self->tags,
                                (gpointer)tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);

  if (tag_iter)
    g_sequence_remove (tag_iter);

  g_signal_emit (self, signals[TAG_REMOVED], 0, tag);
}

gboolean
ephy_bookmark_has_tag (EphyBookmark *self, const char *tag)
{
  GSequenceIter *tag_iter;

  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), FALSE);
  g_return_val_if_fail (tag != NULL, FALSE);

  tag_iter = g_sequence_lookup (self->tags,
                                (gpointer)tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);

  return tag_iter != NULL;
}

GSequence *
ephy_bookmark_get_tags (EphyBookmark *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), NULL);

  return self->tags;
}

int
ephy_bookmark_bookmarks_sort_func (EphyBookmark *bookmark1,
                                   EphyBookmark *bookmark2)
{
  gint64 time1;
  gint64 time2;

  g_assert (bookmark1 != NULL);
  g_assert (bookmark2 != NULL);

  time1 = ephy_bookmark_get_time_added (bookmark1);
  time2 = ephy_bookmark_get_time_added (bookmark2);

  return time2 - time1;
}

int
ephy_bookmark_tags_compare (const char *tag1, const char *tag2)
{
  int result;

  g_assert (tag1 != NULL);
  g_assert (tag2 != NULL);

  result = g_strcmp0 (tag1, tag2);

  if (result == 0)
    return 0;

  if (g_strcmp0 (tag1, "Favorites") == 0)
    return -1;
  if (g_strcmp0 (tag2, "Favorites") == 0)
    return 1;

  return result;
}

char *
ephy_bookmark_to_bso (EphyBookmark *self)
{
  EphySyncService *service;
  guint8 *encrypted;
  guint8 *sync_key;
  char *serialized;
  char *payload;
  char *bso;
  gsize length;

  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), NULL);

  service = ephy_shell_get_global_sync_service (ephy_shell_get_default ());
  sync_key = ephy_sync_crypto_decode_hex (ephy_sync_service_get_token (service, TOKEN_KB));
  serialized = json_gobject_to_data (G_OBJECT (self), NULL);
  encrypted = ephy_sync_crypto_aes_256 (AES_256_MODE_ENCRYPT, sync_key,
                                        (guint8 *)serialized, strlen (serialized), &length);
  payload = ephy_sync_crypto_base64_urlsafe_encode (encrypted, length, FALSE);
  bso = ephy_sync_utils_create_bso_json (self->id, payload);

  g_free (sync_key);
  g_free (serialized);
  g_free (encrypted);
  g_free (payload);

  return bso;
}

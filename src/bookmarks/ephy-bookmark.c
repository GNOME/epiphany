/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
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

#include "ephy-bookmark.h"

#include "ephy-shell.h"
#include "ephy-sync-crypto.h"
#include "ephy-sync-utils.h"

#include <string.h>

#define ID_LEN 32

struct _EphyBookmark {
  GObject      parent_instance;

  char        *url;
  char        *title;
  GSequence   *tags;
  gint64       time_added;

  /* Keep the modified timestamp as double, and not float, to
   * preserve the precision enforced by the Storage Server. */
  char        *id;
  double       modified;
  gboolean     uploaded;
};

static JsonSerializableIface *serializable_iface = NULL;

static void json_serializable_iface_init (gpointer g_iface);

G_DEFINE_TYPE_WITH_CODE (EphyBookmark, ephy_bookmark, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE,
                                               json_serializable_iface_init))

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
      if (self->tags != NULL)
        g_sequence_free (self->tags);
      self->tags = g_value_get_pointer (value);
      break;
    case PROP_TIME_ADDED:
      ephy_bookmark_set_time_added (self, g_value_get_int64 (value));
      break;
    case PROP_TITLE:
      ephy_bookmark_set_title (self, g_value_get_string (value));
      break;
    case PROP_URL:
      ephy_bookmark_set_url (self, g_value_get_string (value));
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

  g_free (self->url);
  g_free (self->title);
  g_free (self->id);

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
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TIME_ADDED] =
    g_param_spec_int64 ("time-added",
                        "Time added",
                        "The bookmark's creation time",
                        0,
                        G_MAXINT64,
                        0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The bookmark's title",
                         "Default bookmark title",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_URL] =
    g_param_spec_string ("url",
                         "URL",
                         "The bookmark's URL",
                         "about:overview",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  signals[TAG_ADDED] =
    g_signal_new ("tag-added",
                  EPHY_TYPE_BOOKMARK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

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
  self->id = g_malloc0 (ID_LEN + 1);
  ephy_sync_crypto_random_hex_gen (NULL, ID_LEN, (guint8 *)self->id);
}

static JsonNode *
ephy_bookmark_json_serializable_serialize_property (JsonSerializable *serializable,
                                                    const char       *name,
                                                    const GValue     *value,
                                                    GParamSpec       *pspec)
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
                                                      const char       *name,
                                                      GValue           *value,
                                                      GParamSpec       *pspec,
                                                      JsonNode         *node)
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
ephy_bookmark_new (const char *url, const char *title, GSequence *tags)
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

  g_free (self->url);
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

  g_free (self->title);
  self->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_TITLE]);
}

const char *
ephy_bookmark_get_title (EphyBookmark *bookmark)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (bookmark), NULL);

  return bookmark->title;
}

void
ephy_bookmark_set_id (EphyBookmark *self,
                      const char   *id)
{
  g_return_if_fail (EPHY_IS_BOOKMARK (self));
  g_return_if_fail (id != NULL);

  g_free (self->id);
  self->id = g_strdup (id);
}

const char *
ephy_bookmark_get_id (EphyBookmark *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), NULL);

  return self->id;
}

void
ephy_bookmark_set_modification_time (EphyBookmark *self,
                                     double        modified)
{
  g_return_if_fail (EPHY_IS_BOOKMARK (self));

  self->modified = modified;
}

double
ephy_bookmark_get_modification_time (EphyBookmark *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), -1);

  return self->modified;
}

void
ephy_bookmark_set_is_uploaded (EphyBookmark *self,
                               gboolean      uploaded)
{
  g_return_if_fail (EPHY_IS_BOOKMARK (self));

  self->uploaded = uploaded;
}

gboolean
ephy_bookmark_is_uploaded (EphyBookmark *self)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), FALSE);

  return self->uploaded;
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

  g_signal_emit (self, signals[TAG_ADDED], 0, tag);
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
ephy_bookmark_bookmarks_compare_func (EphyBookmark *bookmark1,
                                      EphyBookmark *bookmark2)
{
  gint64 time1;
  gint64 time2;
  const char *title1;
  const char *title2;
  int title_result;
  const char *id1;
  const char *id2;

  g_assert (EPHY_IS_BOOKMARK (bookmark1));
  g_assert (EPHY_IS_BOOKMARK (bookmark2));

  time1 = ephy_bookmark_get_time_added (bookmark1);
  time2 = ephy_bookmark_get_time_added (bookmark2);
  if (time2 - time1 != 0)
    return time2 - time1;

  title1 = ephy_bookmark_get_title (bookmark1);
  title2 = ephy_bookmark_get_title (bookmark2);
  title_result = g_strcmp0 (title1, title2);
  if (title_result != 0)
    return title_result;

  id1 = ephy_bookmark_get_id (bookmark1);
  id2 = ephy_bookmark_get_id (bookmark2);

  return g_strcmp0 (id1, id2);
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

  if (g_strcmp0 (tag1, EPHY_BOOKMARKS_FAVORITES_TAG) == 0)
    return -1;
  if (g_strcmp0 (tag2, EPHY_BOOKMARKS_FAVORITES_TAG) == 0)
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

  /* Convert a Bookmark object to a BSO (Basic Store Object). That is a generic
   * JSON wrapper around all items passed into and out of the SyncStorage server.
   * The current flow is:
   * 1. Serialize the Bookmark to a JSON string.
   * 2. Encrypt the JSON string using the sync key from the sync service.
   * 3. Encode the encrypted bytes to base64 url safe.
   * 4. Create a new JSON string that contains the id of the Bookmark and the
        encoded bytes as payload. This is actually the BSO that is going to be
        stored on the SyncStorage server.
   * See https://docs.services.mozilla.com/storage/apis-1.5.html
   */

  service = ephy_shell_get_sync_service (ephy_shell_get_default ());
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

EphyBookmark *
ephy_bookmark_from_bso (JsonObject *bso)
{
  EphySyncService *service;
  EphyBookmark *bookmark = NULL;
  GObject *object;
  GError *error = NULL;
  guint8 *sync_key;
  guint8 *decoded;
  gsize decoded_len;
  char *decrypted;

  g_return_val_if_fail (bso != NULL, NULL);

  /* Convert a BSO to a Bookmark object. The flow is similar to the one from
   * ephy_bookmark_to_bso(), only that the steps are reversed:
   * 1. Decode the payload from base64 url safe to raw bytes.
   * 2. Decrypt the bytes using the sync key to obtain the serialized Bookmark.
   * 3. Deserialize the JSON string into a Bookmark object.
   */

  service = ephy_shell_get_sync_service (ephy_shell_get_default ());
  sync_key = ephy_sync_crypto_decode_hex (ephy_sync_service_get_token (service, TOKEN_KB));
  decoded = ephy_sync_crypto_base64_urlsafe_decode (json_object_get_string_member (bso, "payload"),
                                                    &decoded_len, FALSE);
  decrypted = (char *)ephy_sync_crypto_aes_256 (AES_256_MODE_DECRYPT, sync_key,
                                                decoded, decoded_len, NULL);
  object = json_gobject_from_data (EPHY_TYPE_BOOKMARK, decrypted, strlen (decrypted), &error);

  if (object == NULL) {
    g_warning ("Failed to create GObject from data: %s", error->message);
    g_error_free (error);
    goto out;
  }

  bookmark = EPHY_BOOKMARK (object);
  ephy_bookmark_set_id (bookmark, json_object_get_string_member (bso, "id"));
  ephy_bookmark_set_modification_time (bookmark, json_object_get_double_member (bso, "modified"));
  ephy_bookmark_set_is_uploaded (bookmark, TRUE);

out:
  g_free (decoded);
  g_free (decrypted);

  return bookmark;
}

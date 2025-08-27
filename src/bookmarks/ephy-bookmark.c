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

#include "ephy-bookmarks-manager.h"
#include "ephy-shell.h"
#include "ephy-synchronizable.h"
#include "ephy-sync-utils.h"

#include <string.h>

#define BOOKMARK_TYPE_VAL            "bookmark"
#define BOOKMARK_PARENT_ID_VAL       "toolbar"
#define BOOKMARK_PARENT_NAME_VAL     "Bookmarks Toolbar"
#define BOOKMARK_LOAD_IN_SIDEBAR_VAL FALSE

struct _EphyBookmark {
  GObject parent_instance;

  char *url;
  char *title;
  GSequence *tags;
  gint64 time_added;

  /* Firefox Sync specific fields. */
  char *id;
  char *type;
  char *parent_id;
  char *parent_name;
  gboolean load_in_sidebar;
  gint64 server_time_modified;
};

static void json_serializable_iface_init (JsonSerializableIface *iface);
static void ephy_synchronizable_iface_init (EphySynchronizableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyBookmark, ephy_bookmark, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE,
                                                      json_serializable_iface_init)
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_SYNCHRONIZABLE,
                                                      ephy_synchronizable_iface_init))

enum {
  PROP_0,
  PROP_TIME_ADDED,      /* Epiphany */
  PROP_ID,              /* Firefox Sync */
  PROP_TITLE,           /* Epiphany && Firefox Sync */
  PROP_BMK_URI,         /* Epiphany && Firefox Sync */
  PROP_TAGS,            /* Epiphany && Firefox Sync */
  PROP_TYPE,            /* Firefox Sync */
  PROP_PARENT_ID,       /* Firefox Sync */
  PROP_PARENT_NAME,     /* Firefox Sync */
  PROP_LOAD_IN_SIDEBAR, /* Firefox Sync */
  LAST_PROP
};

enum {
  TAG_ADDED,
  TAG_REMOVED,
  LAST_SIGNAL
};

static GParamSpec *obj_properties[LAST_PROP];
static guint signals[LAST_SIGNAL];

static void
ephy_bookmark_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EphyBookmark *self = EPHY_BOOKMARK (object);

  switch (prop_id) {
    case PROP_TIME_ADDED:
      ephy_bookmark_set_time_added (self, g_value_get_int64 (value));
      break;
    case PROP_TITLE:
      ephy_bookmark_set_title (self, g_value_get_string (value));
      break;
    case PROP_BMK_URI:
      ephy_bookmark_set_url (self, g_value_get_string (value));
      break;
    case PROP_TAGS:
      g_sequence_free (self->tags);
      self->tags = g_value_get_pointer (value);
      if (!self->tags)
        self->tags = g_sequence_new (g_free);
      break;
    case PROP_TYPE:
      g_free (self->type);
      self->type = g_value_dup_string (value);
      break;
    case PROP_PARENT_ID:
      g_free (self->parent_id);
      self->parent_id = g_value_dup_string (value);
      break;
    case PROP_PARENT_NAME:
      g_free (self->parent_name);
      self->parent_name = g_value_dup_string (value);
      break;
    case PROP_LOAD_IN_SIDEBAR:
      self->load_in_sidebar = g_value_get_boolean (value);
      break;
    case PROP_ID:
      ephy_bookmark_set_id (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmark_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EphyBookmark *self = EPHY_BOOKMARK (object);

  switch (prop_id) {
    case PROP_TIME_ADDED:
      g_value_set_int64 (value, ephy_bookmark_get_time_added (self));
      break;
    case PROP_TITLE:
      g_value_set_string (value, ephy_bookmark_get_title (self));
      break;
    case PROP_BMK_URI:
      g_value_set_string (value, ephy_bookmark_get_url (self));
      break;
    case PROP_TAGS:
      g_value_set_pointer (value, ephy_bookmark_get_tags (self));
      break;
    case PROP_TYPE:
      g_value_set_string (value, self->type);
      break;
    case PROP_PARENT_ID:
      g_value_set_string (value, self->parent_id);
      break;
    case PROP_PARENT_NAME:
      g_value_set_string (value, self->parent_name);
      break;
    case PROP_LOAD_IN_SIDEBAR:
      g_value_set_boolean (value, self->load_in_sidebar);
      break;
    case PROP_ID:
      g_value_set_string (value, ephy_bookmark_get_id (self));
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

  g_sequence_free (self->tags);

  g_free (self->id);
  g_free (self->type);
  g_free (self->parent_id);
  g_free (self->parent_name);

  G_OBJECT_CLASS (ephy_bookmark_parent_class)->finalize (object);
}

static void
ephy_bookmark_class_init (EphyBookmarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_bookmark_set_property;
  object_class->get_property = ephy_bookmark_get_property;
  object_class->finalize = ephy_bookmark_finalize;

  obj_properties[PROP_TIME_ADDED] =
    g_param_spec_int64 ("time-added",
                        NULL, NULL,
                        0,
                        G_MAXINT64,
                        0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_ID] =
    g_param_spec_string ("id",
                         NULL, NULL,
                         "Default bookmark id",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         NULL, NULL,
                         "Default bookmark title",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_BMK_URI] =
    g_param_spec_string ("bmkUri",
                         NULL, NULL,
                         "about:overview",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TAGS] =
    g_param_spec_pointer ("tags",
                          NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TYPE] =
    g_param_spec_string ("type",
                         NULL, NULL,
                         "default",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_PARENT_ID] =
    g_param_spec_string ("parentid",
                         NULL, NULL,
                         "default",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_PARENT_NAME] =
    g_param_spec_string ("parentName",
                         NULL, NULL,
                         "default",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_LOAD_IN_SIDEBAR] =
    g_param_spec_boolean ("loadInSidebar",
                          NULL, NULL,
                          TRUE,
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
  self->tags = g_sequence_new (g_free);
}

EphyBookmark *
ephy_bookmark_new (const char *url,
                   const char *title,
                   GSequence  *tags,
                   const char *id)
{
  return g_object_new (EPHY_TYPE_BOOKMARK,
                       "time-added", g_get_real_time (),
                       "title", title,
                       "bmkUri", url,
                       "tags", tags,
                       "type", BOOKMARK_TYPE_VAL,
                       "parentid", BOOKMARK_PARENT_ID_VAL,
                       "parentName", BOOKMARK_PARENT_NAME_VAL,
                       "loadInSidebar", BOOKMARK_LOAD_IN_SIDEBAR_VAL,
                       "id", id,
                       NULL);
}

void
ephy_bookmark_set_time_added (EphyBookmark *self,
                              gint64        time_added)
{
  g_assert (EPHY_IS_BOOKMARK (self));

  self->time_added = (time_added >= 0 ? time_added : g_get_real_time ());
}

gint64
ephy_bookmark_get_time_added (EphyBookmark *self)
{
  g_assert (EPHY_IS_BOOKMARK (self));

  return self->time_added;
}


void
ephy_bookmark_set_url (EphyBookmark *self,
                       const char   *url)
{
  g_assert (EPHY_IS_BOOKMARK (self));

  g_free (self->url);
  self->url = g_strdup (url);
}

const char *
ephy_bookmark_get_url (EphyBookmark *self)
{
  g_assert (EPHY_IS_BOOKMARK (self));

  return self->url;
}

void
ephy_bookmark_set_title (EphyBookmark *self,
                         const char   *title)
{
  g_assert (EPHY_IS_BOOKMARK (self));

  g_free (self->title);
  self->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_TITLE]);
}

const char *
ephy_bookmark_get_title (EphyBookmark *bookmark)
{
  g_assert (EPHY_IS_BOOKMARK (bookmark));

  return bookmark->title;
}

void
ephy_bookmark_set_id (EphyBookmark *self,
                      const char   *id)
{
  g_assert (EPHY_IS_BOOKMARK (self));
  g_assert (id);

  g_free (self->id);
  self->id = g_strdup (id);
}

const char *
ephy_bookmark_get_id (EphyBookmark *self)
{
  g_assert (EPHY_IS_BOOKMARK (self));

  return self->id;
}

void
ephy_bookmark_set_is_uploaded (EphyBookmark *self,
                               gboolean      uploaded)
{
  /* FIXME: This is no longer used for Firefox Sync, but bookmarks import/export
   * expects it. We need to delete it and write a migrator for bookmarks. */
  g_assert (EPHY_IS_BOOKMARK (self));
}

gboolean
ephy_bookmark_is_uploaded (EphyBookmark *self)
{
  /* FIXME: This is no longer used for Firefox Sync, but bookmarks import/export
   * expects it. We need to delete it and write a migrator for bookmarks. */
  g_assert (EPHY_IS_BOOKMARK (self));

  return FALSE;
}

void
ephy_bookmark_add_tag (EphyBookmark *self,
                       const char   *tag)
{
  GSequenceIter *tag_iter;
  GSequenceIter *prev_tag_iter;

  g_assert (EPHY_IS_BOOKMARK (self));
  g_assert (tag);

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

  g_assert (EPHY_IS_BOOKMARK (self));
  g_assert (tag);

  tag_iter = g_sequence_lookup (self->tags,
                                (gpointer)tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);

  if (tag_iter)
    g_sequence_remove (tag_iter);

  g_signal_emit (self, signals[TAG_REMOVED], 0, tag);
}

gboolean
ephy_bookmark_has_tag (EphyBookmark *self,
                       const char   *tag)
{
  GSequenceIter *tag_iter;

  g_assert (EPHY_IS_BOOKMARK (self));
  g_assert (tag);

  tag_iter = g_sequence_lookup (self->tags,
                                (gpointer)tag,
                                (GCompareDataFunc)ephy_bookmark_tags_compare,
                                NULL);

  return !!tag_iter;
}

GSequence *
ephy_bookmark_get_tags (EphyBookmark *self)
{
  g_assert (EPHY_IS_BOOKMARK (self));
  g_assert (self->tags);

  return self->tags;
}

int
ephy_bookmark_bookmarks_compare_func (EphyBookmark *bookmark1,
                                      EphyBookmark *bookmark2)
{
  g_autofree char *title1 = NULL;
  g_autofree char *title2 = NULL;
  int result;
  const char *url1;
  const char *url2;
  gint64 time1;
  gint64 time2;

  g_assert (EPHY_IS_BOOKMARK (bookmark1));
  g_assert (EPHY_IS_BOOKMARK (bookmark2));

  if (ephy_bookmark_has_tag (bookmark1, EPHY_BOOKMARKS_FAVORITES_TAG) &&
      !ephy_bookmark_has_tag (bookmark2, EPHY_BOOKMARKS_FAVORITES_TAG))
    return -1;
  if (!ephy_bookmark_has_tag (bookmark1, EPHY_BOOKMARKS_FAVORITES_TAG) &&
      ephy_bookmark_has_tag (bookmark2, EPHY_BOOKMARKS_FAVORITES_TAG))
    return 1;

  title1 = g_utf8_casefold (ephy_bookmark_get_title (bookmark1), -1);
  title2 = g_utf8_casefold (ephy_bookmark_get_title (bookmark2), -1);
  result = g_strcmp0 (title1, title2);
  if (result != 0)
    return result;

  url1 = ephy_bookmark_get_url (bookmark1);
  url2 = ephy_bookmark_get_url (bookmark2);
  result = g_strcmp0 (url1, url2);
  if (result != 0)
    return result;

  time1 = ephy_bookmark_get_time_added (bookmark1);
  time2 = ephy_bookmark_get_time_added (bookmark2);
  return time2 - time1;
}

int
ephy_bookmark_tags_compare (const char *tag1,
                            const char *tag2)
{
  int result;
  int result_casefolded;

  g_assert (tag1);
  g_assert (tag2);

  result = g_strcmp0 (tag1, tag2);
  result_casefolded = g_strcmp0 (g_utf8_casefold (tag1, -1),
                                 g_utf8_casefold (tag2, -1));

  if (result == 0)
    return 0;

  if (g_strcmp0 (tag1, EPHY_BOOKMARKS_FAVORITES_TAG) == 0)
    return -1;
  if (g_strcmp0 (tag2, EPHY_BOOKMARKS_FAVORITES_TAG) == 0)
    return 1;

  if (result_casefolded == 0)
    return result;

  return result_casefolded;
}

char *
ephy_bookmark_generate_random_id (void)
{
  char *id = NULL;
  EphyBookmarksManager *manager;

  manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());

  while (!id) {
    id = ephy_sync_utils_get_random_sync_id ();

    /* Check if the generated id isn't used already. */
    if (ephy_bookmarks_manager_get_bookmark_by_id (manager, id))
      g_clear_pointer (&id, g_free);
  }

  return id;
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

  if (g_strcmp0 (name, "tags") == 0) {
    JsonNode *node = json_node_new (JSON_NODE_ARRAY);
    JsonArray *array = json_array_new ();
    GSequence *tags = g_value_get_pointer (value);
    GSequenceIter *iter;

    if (tags) {
      for (iter = g_sequence_get_begin_iter (tags);
           !g_sequence_iter_is_end (iter);
           iter = g_sequence_iter_next (iter)) {
        json_array_add_string_element (array, g_sequence_get (iter));
      }
    }

    json_node_set_array (node, array);

    return node;
  }

  /* This is not a Firefox bookmark property, skip it. */
  if (!g_strcmp0 (name, "time-added"))
    return NULL;

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

  if (g_strcmp0 (name, "tags") == 0) {
    GSequence *tags = g_sequence_new (g_free);
    JsonArray *array = json_node_get_array (node);
    const char *tag;

    for (gsize i = 0; i < json_array_get_length (array); i++) {
      tag = json_node_get_string (json_array_get_element (array, i));
      g_sequence_insert_sorted (tags, g_strdup (tag),
                                (GCompareDataFunc)ephy_bookmark_tags_compare, NULL);
    }

    g_value_set_pointer (value, tags);

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
  return ephy_bookmark_get_id (EPHY_BOOKMARK (synchronizable));
}

static gint64
synchronizable_get_server_time_modified (EphySynchronizable *synchronizable)
{
  return EPHY_BOOKMARK (synchronizable)->server_time_modified;
}

static void
synchronizable_set_server_time_modified (EphySynchronizable *synchronizable,
                                         gint64              server_time_modified)
{
  EPHY_BOOKMARK (synchronizable)->server_time_modified = server_time_modified;
}

static void
ephy_synchronizable_iface_init (EphySynchronizableInterface *iface)
{
  iface->get_id = synchronizable_get_id;
  iface->get_server_time_modified = synchronizable_get_server_time_modified;
  iface->set_server_time_modified = synchronizable_set_server_time_modified;
  iface->to_bso = ephy_synchronizable_default_to_bso;
}

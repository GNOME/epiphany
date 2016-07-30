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

struct _EphyBookmark {
  GObject      parent_instance;

  char        *url;
  char        *title;
  GSequence   *tags;
};

G_DEFINE_TYPE (EphyBookmark, ephy_bookmark, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TAGS,
  PROP_TITLE,
  PROP_URL,
  LAST_PROP
};

enum {
  REMOVED,
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
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

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

  signals[REMOVED] =
    g_signal_new ("removed",
                  EPHY_TYPE_BOOKMARK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
ephy_bookmark_init (EphyBookmark *self)
{
}

EphyBookmark *
ephy_bookmark_new (char *url, char *title, GSequence *tags)
{
  return g_object_new (EPHY_TYPE_BOOKMARK,
                       "url", url,
                       "title", title,
                       "tags", tags,
                       NULL);
}

const char *
ephy_bookmark_get_url (EphyBookmark *self) {
  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), NULL);

  return self->url;
}

const char *
ephy_bookmark_get_title (EphyBookmark *bookmark)
{
  g_return_val_if_fail (EPHY_IS_BOOKMARK (bookmark), NULL);

  return bookmark->title;
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
}

gboolean
ephy_bookmark_has_tag (EphyBookmark *self, const char *tag)
{
  GSequenceIter *tag_iter;

  g_return_val_if_fail (EPHY_IS_BOOKMARK (self), FALSE);
  g_return_val_if_fail (tag != NULL, FALSE);

  tag_iter = g_sequence_lookup (self->tags,
                                (gpointer)tag,
                                (GCompareDataFunc)g_strcmp0,
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

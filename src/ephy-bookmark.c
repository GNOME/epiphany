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
  GObject parent_instance;

  char *url;
  char *title;
};

G_DEFINE_TYPE (EphyBookmark, ephy_bookmark, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TITLE,
  PROP_URL,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_bookmark_finalize (GObject *object)
{
  EphyBookmark *self = EPHY_BOOKMARK (object);

  g_clear_pointer (&self->url, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ephy_bookmark_parent_class)->finalize (object);
}

static void
ephy_bookmark_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EphyBookmark *self = EPHY_BOOKMARK (object);

  switch (prop_id) {
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
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;
    case PROP_URL:
      g_value_set_string (value, self->url);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_bookmark_class_init (EphyBookmarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_bookmark_set_property;
  object_class->get_property = ephy_bookmark_get_property;
  object_class->finalize = ephy_bookmark_finalize;

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
}

static void
ephy_bookmark_init (EphyBookmark *self)
{
}

EphyBookmark *
ephy_bookmark_new (char *url, char *title)
{
  return g_object_new (EPHY_TYPE_BOOKMARK,
                       "url", url,
                       "title", title,
                       NULL);
}

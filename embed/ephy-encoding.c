/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-encoding.h"

#include <string.h>

struct _EphyEncoding {
  GObject parent_instance;

  char *title;
  char *title_elided;
  char *collation_key;
  char *encoding;
  int language_groups;
};

G_DEFINE_FINAL_TYPE (EphyEncoding, ephy_encoding, G_TYPE_OBJECT)

enum {
  PROP_0,

  PROP_TITLE,
  PROP_TITLE_ELIDED,
  PROP_COLLATION_KEY,
  PROP_ENCODING,
  PROP_LANGUAGE_GROUPS,

  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_encoding_finalize (GObject *object)
{
  EphyEncoding *encoding = EPHY_ENCODING (object);

  g_free (encoding->title);
  g_free (encoding->title_elided);
  g_free (encoding->collation_key);
  g_free (encoding->encoding);

  G_OBJECT_CLASS (ephy_encoding_parent_class)->finalize (object);
}

static void
ephy_encoding_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EphyEncoding *encoding = EPHY_ENCODING (object);

  switch (prop_id) {
    case PROP_TITLE:
      g_value_set_string (value, encoding->title);
      break;
    case PROP_TITLE_ELIDED:
      g_value_set_string (value, encoding->title_elided);
      break;
    case PROP_COLLATION_KEY:
      g_value_set_string (value, encoding->collation_key);
      break;
    case PROP_ENCODING:
      g_value_set_string (value, encoding->encoding);
      break;
    case PROP_LANGUAGE_GROUPS:
      g_value_set_int (value, encoding->language_groups);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Copied from egg-toolbar-editor.c */
static char *
elide_underscores (const char *original)
{
  char *q, *result;
  const char *p;
  gboolean last_underscore;

  q = result = g_malloc (strlen (original) + 1);
  last_underscore = FALSE;

  for (p = original; *p; p++) {
    if (!last_underscore && *p == '_') {
      last_underscore = TRUE;
    } else {
      last_underscore = FALSE;
      *q++ = *p;
    }
  }

  *q = '\0';

  return result;
}

static void
ephy_encoding_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EphyEncoding *encoding = EPHY_ENCODING (object);

  switch (prop_id) {
    case PROP_TITLE: {
      char *elided, *collate_key, *normalised;

      g_free (encoding->title);
      encoding->title = g_value_dup_string (value);

      elided = elide_underscores (encoding->title);
      normalised = g_utf8_normalize (elided, -1, G_NORMALIZE_DEFAULT);
      collate_key = g_utf8_collate_key (normalised, -1);

      g_object_set (object,
                    "title-elided", elided,
                    "collation-key", collate_key,
                    NULL);

      g_free (collate_key);
      g_free (normalised);
      g_free (elided);

      break;
    }
    case PROP_TITLE_ELIDED:
      g_free (encoding->title_elided);
      encoding->title_elided = g_value_dup_string (value);
      break;
    case PROP_COLLATION_KEY:
      g_free (encoding->collation_key);
      encoding->collation_key = g_value_dup_string (value);
      break;
    case PROP_ENCODING:
      g_free (encoding->encoding);
      encoding->encoding = g_value_dup_string (value);
      break;
    case PROP_LANGUAGE_GROUPS:
      encoding->language_groups = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_encoding_class_init (EphyEncodingClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = ephy_encoding_finalize;
  gobject_class->get_property = ephy_encoding_get_property;
  gobject_class->set_property = ephy_encoding_set_property;

  obj_properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TITLE_ELIDED] =
    g_param_spec_string ("title-elided",
                         NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_COLLATION_KEY] =
    g_param_spec_string ("collation-key",
                         NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_ENCODING] =
    g_param_spec_string ("encoding",
                         NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_LANGUAGE_GROUPS] =
    g_param_spec_int ("language-groups",
                      NULL, NULL,
                      LG_NONE, LG_ALL,
                      LG_NONE,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_properties);
}

static void
ephy_encoding_init (EphyEncoding *encoding)
{
}

const char *
ephy_encoding_get_title (EphyEncoding *encoding)
{
  g_assert (EPHY_IS_ENCODING (encoding));

  return encoding->title;
}

const char *
ephy_encoding_get_title_elided (EphyEncoding *encoding)
{
  g_assert (EPHY_IS_ENCODING (encoding));

  return encoding->title_elided;
}

const char *
ephy_encoding_get_collation_key (EphyEncoding *encoding)
{
  g_assert (EPHY_IS_ENCODING (encoding));

  return encoding->collation_key;
}

const char *
ephy_encoding_get_encoding (EphyEncoding *encoding)
{
  g_assert (EPHY_IS_ENCODING (encoding));

  return encoding->encoding;
}

int
ephy_encoding_get_language_groups (EphyEncoding *encoding)
{
  g_assert (EPHY_IS_ENCODING (encoding));

  return encoding->language_groups;
}

EphyEncoding *
ephy_encoding_new (const char *encoding,
                   const char *title,
                   int         language_groups)
{
  return g_object_new (EPHY_TYPE_ENCODING,
                       "encoding", encoding,
                       "title", title,
                       "language-groups", language_groups,
                       NULL);
}

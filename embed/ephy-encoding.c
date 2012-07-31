/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
 *
 */

#include "config.h"
#include "ephy-encoding.h"

#include <string.h>

G_DEFINE_TYPE (EphyEncoding, ephy_encoding, G_TYPE_OBJECT)

#define EPHY_ENCODING_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_ENCODING, EphyEncodingPrivate))

enum {
  PROP_0,

  PROP_TITLE,
  PROP_TITLE_ELIDED,
  PROP_COLLATION_KEY,
  PROP_ENCODING,
  PROP_LANGUAGE_GROUPS
};

struct _EphyEncodingPrivate {
  char *title;
  char *title_elided;
  char *collation_key;
  char *encoding;
  int language_groups;
};

static void
ephy_encoding_finalize (GObject *object)
{
  EphyEncodingPrivate *priv = EPHY_ENCODING (object)->priv;

  g_free (priv->title);
  g_free (priv->title_elided);
  g_free (priv->collation_key);
  g_free (priv->encoding);

  G_OBJECT_CLASS (ephy_encoding_parent_class)->finalize (object);
}

static void
ephy_encoding_get_property (GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  EphyEncodingPrivate *priv = EPHY_ENCODING (object)->priv;

  switch (prop_id) {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_TITLE_ELIDED:
      g_value_set_string (value, priv->title_elided);
      break;
    case PROP_COLLATION_KEY:
      g_value_set_string (value, priv->collation_key);
      break;
    case PROP_ENCODING:
      g_value_set_string (value, priv->encoding);
      break;
    case PROP_LANGUAGE_GROUPS:
      g_value_set_int (value, priv->language_groups);
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
    }
    else {
      last_underscore = FALSE;
      *q++ = *p;
    }
  }

  *q = '\0';

  return result;
}

static void
ephy_encoding_set_property (GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  EphyEncodingPrivate *priv = EPHY_ENCODING (object)->priv;

  switch (prop_id) {
  case PROP_TITLE: {
    char *elided, *collate_key, *normalised;

    g_free (priv->title);
    priv->title = g_strdup (g_value_get_string (value));

    elided = elide_underscores (priv->title);
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
    g_free (priv->title_elided);
    priv->title_elided = g_strdup (g_value_get_string (value));
    break;
  case PROP_COLLATION_KEY:
    g_free (priv->collation_key);
    priv->collation_key = g_strdup (g_value_get_string (value));
    break;
  case PROP_ENCODING:
    g_free (priv->encoding);
    priv->encoding = g_strdup (g_value_get_string (value));
    break;
  case PROP_LANGUAGE_GROUPS:
    priv->language_groups = g_value_get_int (value);
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

  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "Title",
                                                        "The encoding's title",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_TITLE_ELIDED,
                                   g_param_spec_string ("title-elided",
                                                        "Title Elided",
                                                        "The encoding's elided title",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_COLLATION_KEY,
                                   g_param_spec_string ("collation-key",
                                                        "Collation Key",
                                                        "The encoding's collation key",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_ENCODING,
                                   g_param_spec_string ("encoding",
                                                        "Encoding",
                                                        "The encoding's encoding",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_LANGUAGE_GROUPS,
                                   g_param_spec_int ("language-groups",
                                                     "Language Groups",
                                                     "The encoding's language groups",
                                                     LG_NONE, LG_ALL,
                                                     LG_NONE,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (gobject_class, sizeof (EphyEncodingPrivate));
}

static void
ephy_encoding_init (EphyEncoding *encoding)
{
  encoding->priv = EPHY_ENCODING_GET_PRIVATE (encoding);
}

const char *
ephy_encoding_get_title (EphyEncoding *encoding)
{
  g_return_val_if_fail (EPHY_IS_ENCODING (encoding), NULL);

  return encoding->priv->title;
}

const char *
ephy_encoding_get_title_elided (EphyEncoding *encoding)
{
  g_return_val_if_fail (EPHY_IS_ENCODING (encoding), NULL);

  return encoding->priv->title_elided;
}

const char *
ephy_encoding_get_collation_key (EphyEncoding *encoding)
{
  g_return_val_if_fail (EPHY_IS_ENCODING (encoding), NULL);

  return encoding->priv->collation_key;
}

const char *
ephy_encoding_get_encoding (EphyEncoding *encoding)
{
  g_return_val_if_fail (EPHY_IS_ENCODING (encoding), NULL);

  return encoding->priv->encoding;
}

int
ephy_encoding_get_language_groups (EphyEncoding *encoding)
{
  g_return_val_if_fail (EPHY_IS_ENCODING (encoding), LG_NONE);

  return encoding->priv->language_groups;
}

EphyEncoding *
ephy_encoding_new (const char *encoding, const char *title,
                   int language_groups)
{
  return g_object_new (EPHY_TYPE_ENCODING,
                       "encoding", encoding,
                       "title", title,
                       "language-groups", language_groups,
                       NULL);
}

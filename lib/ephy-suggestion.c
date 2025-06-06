/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2017 Igalia S.L.
 * Copyright © 2018 Jan-Michael Brummer
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
#include "ephy-suggestion.h"

#include "ephy-uri-helpers.h"

#include <glib.h>

struct _EphySuggestion {
  DzlSuggestion parent;

  char *unescaped_title;
  gboolean is_completion;
  cairo_surface_t *favicon;
};

G_DEFINE_FINAL_TYPE (EphySuggestion, ephy_suggestion, DZL_TYPE_SUGGESTION)

enum {
  PROP_0,
  PROP_UNESCAPED_TITLE,
  PROP_IS_COMPLETION,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_suggestion_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EphySuggestion *self = EPHY_SUGGESTION (object);

  switch (prop_id) {
    case PROP_UNESCAPED_TITLE:
      self->unescaped_title = g_strdup (g_value_get_string (value));
      break;
    case PROP_IS_COMPLETION:
      self->is_completion = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_suggestion_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  EphySuggestion *self = EPHY_SUGGESTION (object);

  switch (prop_id) {
    case PROP_UNESCAPED_TITLE:
      g_value_set_string (value, self->unescaped_title);
      break;
    case PROP_IS_COMPLETION:
      g_value_set_boolean (value, self->is_completion);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_suggestion_finalize (GObject *object)
{
  EphySuggestion *self = EPHY_SUGGESTION (object);

  g_free (self->unescaped_title);
  g_clear_pointer (&self->favicon, cairo_surface_destroy);

  G_OBJECT_CLASS (ephy_suggestion_parent_class)->finalize (object);
}

char *
ephy_suggestion_replace_typed_text (DzlSuggestion *self,
                                    const char    *typed_text)
{
  const char *url;

  g_assert (EPHY_IS_SUGGESTION (self));

  url = ephy_suggestion_get_uri (EPHY_SUGGESTION (self));

  return g_strdup (url);
}

cairo_surface_t *
ephy_suggestion_get_icon_surface (DzlSuggestion *self,
                                  GtkWidget     *widget)
{
  EphySuggestion *suggestion = EPHY_SUGGESTION (self);

  return suggestion->favicon;
}

static void
ephy_suggestion_class_init (EphySuggestionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DzlSuggestionClass *dzl_suggestion_class = DZL_SUGGESTION_CLASS (klass);

  object_class->get_property = ephy_suggestion_get_property;
  object_class->set_property = ephy_suggestion_set_property;
  object_class->finalize = ephy_suggestion_finalize;

  dzl_suggestion_class->replace_typed_text = ephy_suggestion_replace_typed_text;
  dzl_suggestion_class->get_icon_surface = ephy_suggestion_get_icon_surface;

  obj_properties[PROP_UNESCAPED_TITLE] =
    g_param_spec_string ("unescaped-title",
                         NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_IS_COMPLETION] =
    g_param_spec_boolean ("is-completion",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_suggestion_init (EphySuggestion *self)
{
}

EphySuggestion *
ephy_suggestion_new (const char *title_markup,
                     const char *unescaped_title,
                     const char *uri,
                     gboolean    is_completion)
{
  EphySuggestion *suggestion;
  char *decoded_uri = ephy_uri_decode (uri);
  char *escaped_uri = g_markup_escape_text (decoded_uri, -1);

  suggestion = g_object_new (EPHY_TYPE_SUGGESTION,
                             "icon-name", "ephy-webpage-symbolic",
                             "id", uri,
                             "subtitle", escaped_uri,
                             "title", title_markup,
                             "unescaped-title", unescaped_title,
                             "is-completion", is_completion,
                             NULL);

  g_free (decoded_uri);
  g_free (escaped_uri);

  return suggestion;
}

EphySuggestion *
ephy_suggestion_new_with_custom_subtitle (const char *title_markup,
                                          const char *unescaped_title,
                                          const char *subtitle,
                                          const char *uri)
{
  EphySuggestion *suggestion;
  char *decoded_uri = ephy_uri_decode (uri);
  char *escaped_uri = g_markup_escape_text (decoded_uri, -1);

  suggestion = g_object_new (EPHY_TYPE_SUGGESTION,
                             "icon-name", "ephy-webpage-symbolic",
                             "id", uri,
                             "subtitle", subtitle,
                             "title", title_markup,
                             "unescaped-title", unescaped_title,
                             NULL);

  g_free (decoded_uri);
  g_free (escaped_uri);

  return suggestion;
}

EphySuggestion *
ephy_suggestion_new_without_subtitle (const char *title_markup,
                                      const char *unescaped_title,
                                      const char *uri)
{
  EphySuggestion *suggestion;

  suggestion = g_object_new (EPHY_TYPE_SUGGESTION,
                             "icon-name", "ephy-webpage-symbolic",
                             "id", uri,
                             "title", title_markup,
                             "unescaped-title", unescaped_title,
                             NULL);

  return suggestion;
}

const char *
ephy_suggestion_get_unescaped_title (EphySuggestion *self)
{
  g_assert (EPHY_IS_SUGGESTION (self));

  return self->unescaped_title;
}

const char *
ephy_suggestion_get_subtitle (EphySuggestion *self)
{
  g_assert (EPHY_IS_SUGGESTION (self));

  return dzl_suggestion_get_subtitle (DZL_SUGGESTION (self));
}

const char *
ephy_suggestion_get_uri (EphySuggestion *self)
{
  g_assert (EPHY_IS_SUGGESTION (self));

  return dzl_suggestion_get_id (DZL_SUGGESTION (self));
}

void
ephy_suggestion_set_favicon (EphySuggestion  *self,
                             cairo_surface_t *favicon)
{
  g_clear_pointer (&self->favicon, cairo_surface_destroy);
  self->favicon = favicon;
  g_object_notify (G_OBJECT (self), "icon");
}

void
ephy_suggestion_set_secondary_icon (EphySuggestion *self,
                                    const char     *icon_name)
{
  dzl_suggestion_set_secondary_icon_name (DZL_SUGGESTION (self), icon_name);
}

gboolean
ephy_suggestion_is_completion (EphySuggestion *self)
{
  return self->is_completion;
}

void
ephy_suggestion_set_icon (EphySuggestion *self,
                          const char     *icon_name)
{
  dzl_suggestion_set_icon_name (DZL_SUGGESTION (self), icon_name);
}

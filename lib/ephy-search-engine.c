/* ephy-search-engine.c
 *
 * Copyright 2021 vanadiae <vanadiae35@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "ephy-search-engine.h"

#include <libsoup/soup.h>

struct _EphySearchEngine {
  GObject parent_instance;

  char *name;
  char *url;
  char *bang;
};

G_DEFINE_FINAL_TYPE (EphySearchEngine, ephy_search_engine, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  PROP_URL,
  PROP_BANG,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

const char *
ephy_search_engine_get_name (EphySearchEngine *self)
{
  return self->name;
}

void
ephy_search_engine_set_name (EphySearchEngine *self,
                             const char       *name)
{
  g_assert (name);

  if (g_strcmp0 (name, self->name) == 0)
    return;

  g_free (self->name);
  self->name = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

const char *
ephy_search_engine_get_url (EphySearchEngine *self)
{
  return self->url;
}

void
ephy_search_engine_set_url (EphySearchEngine *self,
                            const char       *url)
{
  g_assert (url);

  if (g_strcmp0 (url, self->url) == 0)
    return;

  g_free (self->url);
  self->url = g_strdup (url);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URL]);
}

const char *
ephy_search_engine_get_bang (EphySearchEngine *self)
{
  return self->bang;
}

void
ephy_search_engine_set_bang (EphySearchEngine *self,
                             const char       *bang)
{
  g_assert (bang);

  if (g_strcmp0 (bang, self->bang) == 0)
    return;

  g_free (self->bang);
  self->bang = g_strdup (bang);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BANG]);
}

static void
ephy_search_engine_finalize (GObject *object)
{
  EphySearchEngine *self = (EphySearchEngine *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->url, g_free);
  g_clear_pointer (&self->bang, g_free);

  G_OBJECT_CLASS (ephy_search_engine_parent_class)->finalize (object);
}

static void
ephy_search_engine_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  EphySearchEngine *self = EPHY_SEARCH_ENGINE (object);

  switch (prop_id) {
    case PROP_NAME:
      g_value_set_string (value, ephy_search_engine_get_name (self));
      break;
    case PROP_URL:
      g_value_set_string (value, ephy_search_engine_get_url (self));
      break;
    case PROP_BANG:
      g_value_set_string (value, ephy_search_engine_get_bang (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_search_engine_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  EphySearchEngine *self = EPHY_SEARCH_ENGINE (object);

  switch (prop_id) {
    case PROP_NAME:
      ephy_search_engine_set_name (self, g_value_get_string (value));
      break;
    case PROP_URL:
      ephy_search_engine_set_url (self, g_value_get_string (value));
      break;
    case PROP_BANG:
      ephy_search_engine_set_bang (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_search_engine_class_init (EphySearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_search_engine_finalize;
  object_class->get_property = ephy_search_engine_get_property;
  object_class->set_property = ephy_search_engine_set_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));
  properties [PROP_URL] =
    g_param_spec_string ("url",
                         "Url",
                         "The search URL with %s placeholder for this search engine.",
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));
  properties [PROP_BANG] =
    g_param_spec_string ("bang",
                         "Bang",
                         "The search shortcut (bang) for this search engine.",
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ephy_search_engine_init (EphySearchEngine *self)
{
  /* Default values set with the GParamSpec aren't actually set at the end
   * of the GObject construction process, so we must ensure all properties
   * we expect to be non NULL to be kept that way, as we want to allow
   * safely omitting properties when using g_object_new().
   */
  self->name = g_strdup ("");
  self->url = g_strdup ("");
  self->bang = g_strdup ("");
}

static char *
replace_placeholder (const char *url,
                     const char *search_query)
{
  GString *s = g_string_new (url);
  g_autofree char *encoded_query = soup_form_encode ("q", search_query, NULL);

  /* libsoup requires us to pass a field name to get the HTML-form encoded
   * search query. But since we don't require that the search URL has the
   * q= before the placeholder, just skip q= and use the encoded query
   * directly.
   */
  g_string_replace (s, "%s", encoded_query + strlen ("q="), 0);

  return g_string_free (s, FALSE);
}

/**
 * ephy_search_engine_build_search_address:
 * @self: an #EphySearchEngine
 * @search_query: The search query to be used in the search URL.
 *
 * Returns: (transfer full): @self's search URL with all the %s placeholders
 * replaced with @search_query.
 */
char *
ephy_search_engine_build_search_address (EphySearchEngine *self,
                                         const char       *search_query)
{
  return replace_placeholder (self->url, search_query);
}

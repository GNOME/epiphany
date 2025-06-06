/* ephy-opensearch-autodiscovery-link.c
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


#include "ephy-opensearch-autodiscovery-link.h"

struct _EphyOpensearchAutodiscoveryLink {
  GObject parent_instance;

  char *name;
  char *url;
};

G_DEFINE_FINAL_TYPE (EphyOpensearchAutodiscoveryLink, ephy_opensearch_autodiscovery_link, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  PROP_URL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

EphyOpensearchAutodiscoveryLink *
ephy_opensearch_autodiscovery_link_new (const char *name,
                                        const char *url)
{
  return g_object_new (EPHY_TYPE_OPENSEARCH_AUTODISCOVERY_LINK,
                       "name", name,
                       "url", url,
                       NULL);
}

/**
 * ephy_opensearch_autodiscovery_link_get_name:
 * @self: an #EphyOpensearchAutodiscoveryLink
 *
 * Returns: (transfer none): The name of this autodiscovery link.
 */
const char *
ephy_opensearch_autodiscovery_link_get_name (EphyOpensearchAutodiscoveryLink *self)
{
  g_assert (EPHY_IS_OPENSEARCH_AUTODISCOVERY_LINK (self));

  return self->name;
}

/**
 * ephy_opensearch_autodiscovery_link_get_url:
 * @self: an #EphyOpensearchAutodiscoveryLink
 *
 * Returns: (transfer none): The URL of the OpenSearch description file corresponding to this autodiscovered link.
 */
const char *
ephy_opensearch_autodiscovery_link_get_url (EphyOpensearchAutodiscoveryLink *self)
{
  g_assert (EPHY_IS_OPENSEARCH_AUTODISCOVERY_LINK (self));

  return self->url;
}

static void
ephy_opensearch_autodiscovery_link_finalize (GObject *object)
{
  EphyOpensearchAutodiscoveryLink *self = (EphyOpensearchAutodiscoveryLink *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->url, g_free);

  G_OBJECT_CLASS (ephy_opensearch_autodiscovery_link_parent_class)->finalize (object);
}

static void
ephy_opensearch_autodiscovery_link_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  EphyOpensearchAutodiscoveryLink *self = EPHY_OPENSEARCH_AUTODISCOVERY_LINK (object);

  switch (prop_id) {
    case PROP_NAME: g_value_set_string (value, self->name);
      break;
    case PROP_URL: g_value_set_string (value, self->url);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_opensearch_autodiscovery_link_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  EphyOpensearchAutodiscoveryLink *self = EPHY_OPENSEARCH_AUTODISCOVERY_LINK (object);

  switch (prop_id) {
    /* Only set at construction time, so no need to make separate setter functions. */
    case PROP_NAME:
      g_clear_pointer (&self->name, g_free);
      self->name = g_value_dup_string (value);
      break;
    case PROP_URL:
      g_clear_pointer (&self->url, g_free);
      self->url = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_opensearch_autodiscovery_link_class_init (EphyOpensearchAutodiscoveryLinkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_opensearch_autodiscovery_link_finalize;
  object_class->get_property = ephy_opensearch_autodiscovery_link_get_property;
  object_class->set_property = ephy_opensearch_autodiscovery_link_set_property;

  properties[PROP_NAME] = g_param_spec_string ("name",
                                               "name",
                                               "The name of the autodiscovery link.",
                                               "",
                                               G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_URL] = g_param_spec_string ("url",
                                              "url",
                                              "The URL of the opensearch description file that was autodiscovered.",
                                              "",
                                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ephy_opensearch_autodiscovery_link_init (EphyOpensearchAutodiscoveryLink *self)
{
}

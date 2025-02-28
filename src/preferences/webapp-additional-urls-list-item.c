/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Igalia S.L.
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
#include "webapp-additional-urls-list-item.h"

struct _EphyWebappAdditionalURLsListItem {
  GObject parent_instance;

  gchar *url;
};

G_DEFINE_FINAL_TYPE (EphyWebappAdditionalURLsListItem, ephy_webapp_additional_urls_list_item, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_URL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
ephy_webapp_additional_urls_list_item_finalize (GObject *object)
{
  EphyWebappAdditionalURLsListItem *self = EPHY_WEBAPP_ADDITIONAL_URLS_LIST_ITEM (object);

  g_clear_pointer (&self->url, g_free);

  G_OBJECT_CLASS (ephy_webapp_additional_urls_list_item_parent_class)->finalize (object);
}

static void
ephy_webapp_additional_urls_list_item_get_property (GObject    *object,
                                                    guint       prop_id,
                                                    GValue     *value,
                                                    GParamSpec *pspec)
{
  EphyWebappAdditionalURLsListItem *self = EPHY_WEBAPP_ADDITIONAL_URLS_LIST_ITEM (object);

  switch (prop_id) {
    case PROP_URL:
      g_value_set_string (value, ephy_webapp_additional_urls_list_item_get_url (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_webapp_additional_urls_list_item_set_property (GObject      *object,
                                                    guint         prop_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec)
{
  EphyWebappAdditionalURLsListItem *self = EPHY_WEBAPP_ADDITIONAL_URLS_LIST_ITEM (object);

  switch (prop_id) {
    case PROP_URL:
      ephy_webapp_additional_urls_list_item_set_url (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_webapp_additional_urls_list_item_class_init (EphyWebappAdditionalURLsListItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_webapp_additional_urls_list_item_finalize;
  object_class->get_property = ephy_webapp_additional_urls_list_item_get_property;
  object_class->set_property = ephy_webapp_additional_urls_list_item_set_property;

  properties [PROP_URL] =
    g_param_spec_string ("url",
                         NULL, NULL,
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ephy_webapp_additional_urls_list_item_init (EphyWebappAdditionalURLsListItem *list_item)
{
}

EphyWebappAdditionalURLsListItem *
ephy_webapp_additional_urls_list_item_new (const gchar *url)
{
  return g_object_new (EPHY_TYPE_WEBAPP_ADDITIONAL_URLS_LIST_ITEM,
                       "url", url,
                       NULL);
}

const gchar *
ephy_webapp_additional_urls_list_item_get_url (EphyWebappAdditionalURLsListItem *self)
{
  return self->url;
}

void
ephy_webapp_additional_urls_list_item_set_url (EphyWebappAdditionalURLsListItem *self,
                                               const gchar                      *url)
{
  g_assert (url);

  if (g_strcmp0 (url, self->url) == 0)
    return;

  g_free (self->url);
  self->url = g_strdup (url);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URL]);
}

gboolean
ephy_webapp_additional_urls_list_item_add_to_builder (EphyWebappAdditionalURLsListItem *item,
                                                      GVariantBuilder                  *builder)
{
  const gchar *url = ephy_webapp_additional_urls_list_item_get_url (item);

  if (url && url[0] != '\0')
    g_variant_builder_add (builder, "s", url);

  return FALSE;
}

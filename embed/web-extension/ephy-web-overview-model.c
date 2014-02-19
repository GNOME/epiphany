/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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
#include "ephy-web-overview-model.h"

#include <libsoup/soup.h>

struct _EphyWebOverviewModelPrivate
{
  GList *items;
  GHashTable *thumbnails;
};

enum
{
  URLS_CHANGED,
  THUMBNAIL_CHANGED,
  TITLE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyWebOverviewModel, ephy_web_overview_model, G_TYPE_OBJECT)

static void
ephy_web_overview_model_dispose (GObject *object)
{
  EphyWebOverviewModel *model = EPHY_WEB_OVERVIEW_MODEL (object);

  if (model->priv->items) {
    g_list_free_full (model->priv->items, (GDestroyNotify)ephy_web_overview_model_item_free);
    model->priv->items = NULL;
  }

  if (model->priv->thumbnails) {
    g_hash_table_destroy (model->priv->thumbnails);
    model->priv->thumbnails = NULL;
  }

  G_OBJECT_CLASS (ephy_web_overview_model_parent_class)->dispose (object);
}

static void
ephy_web_overview_model_class_init (EphyWebOverviewModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_web_overview_model_dispose;

  signals[URLS_CHANGED] =
    g_signal_new ("urls-changed",
                  EPHY_TYPE_WEB_OVERVIEW_MODEL,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[THUMBNAIL_CHANGED] =
    g_signal_new ("thumbnail-changed",
                  EPHY_TYPE_WEB_OVERVIEW_MODEL,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  signals[TITLE_CHANGED] =
    g_signal_new ("title-changed",
                  EPHY_TYPE_WEB_OVERVIEW_MODEL,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  g_type_class_add_private (object_class, sizeof(EphyWebOverviewModelPrivate));
}

static void
ephy_web_overview_model_init (EphyWebOverviewModel *model)
{
  model->priv = G_TYPE_INSTANCE_GET_PRIVATE (model, EPHY_TYPE_WEB_OVERVIEW_MODEL, EphyWebOverviewModelPrivate);
  model->priv->thumbnails = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   (GDestroyNotify)g_free,
                                                   (GDestroyNotify)g_free);
}

EphyWebOverviewModel *
ephy_web_overview_model_new (void)
{
  return g_object_new (EPHY_TYPE_WEB_OVERVIEW_MODEL, NULL);
}

void
ephy_web_overview_model_set_urls (EphyWebOverviewModel *model,
                                  GList *urls)
{
  g_return_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  g_list_free_full (model->priv->items, (GDestroyNotify)ephy_web_overview_model_item_free);
  model->priv->items = urls;
  g_signal_emit (model, signals[URLS_CHANGED], 0);
}

GList *
ephy_web_overview_model_get_urls (EphyWebOverviewModel *model)
{
  g_return_val_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model), NULL);

  return model->priv->items;
}

void
ephy_web_overview_model_set_url_thumbnail (EphyWebOverviewModel *model,
                                           const char *url,
                                           const char *path)
{
  const char *thumbnail_path;

  g_return_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  thumbnail_path = ephy_web_overview_model_get_url_thumbnail (model, url);
  if (g_strcmp0 (thumbnail_path, path) == 0)
    return;

  g_hash_table_insert (model->priv->thumbnails, g_strdup (url), g_strdup (path));
  g_signal_emit (model, signals[THUMBNAIL_CHANGED], 0, url, path);
}

const char *
ephy_web_overview_model_get_url_thumbnail (EphyWebOverviewModel *model,
                                           const char *url)
{
  g_return_val_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model), NULL);

  return g_hash_table_lookup (model->priv->thumbnails, url);
}

void
ephy_web_overview_model_set_url_title (EphyWebOverviewModel *model,
                                       const char *url,
                                       const char *title)
{
  GList *l;
  gboolean changed = FALSE;

  g_return_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  for (l = model->priv->items; l; l = g_list_next (l)) {
    EphyWebOverviewModelItem *item = (EphyWebOverviewModelItem *)l->data;

    if (g_strcmp0 (item->url, url) != 0)
      continue;

    if (g_strcmp0 (item->title, title) != 0) {
      changed = TRUE;

      g_free (item->title);
      item->title = g_strdup (title);
    }
  }

  if (changed)
    g_signal_emit (model, signals[TITLE_CHANGED], 0, url, title);
}

void
ephy_web_overview_model_delete_url (EphyWebOverviewModel *model,
                                    const char *url)
{
  GList *l;
  gboolean changed = FALSE;

  g_return_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  l = model->priv->items;
  while (l) {
    EphyWebOverviewModelItem *item = (EphyWebOverviewModelItem *)l->data;
    GList *next = l->next;

    if (g_strcmp0 (item->url, url) == 0) {
      changed = TRUE;

      ephy_web_overview_model_item_free (item);
      model->priv->items = g_list_delete_link (model->priv->items, l);
    }

    l = next;
  }

  if (changed)
    g_signal_emit (model, signals[URLS_CHANGED], 0);
}

void
ephy_web_overview_model_delete_host (EphyWebOverviewModel *model,
                                     const char *host)
{
  GList *l;
  gboolean changed = FALSE;

  g_return_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  l = model->priv->items;
  while (l) {
    EphyWebOverviewModelItem *item = (EphyWebOverviewModelItem *)l->data;
    SoupURI *uri = soup_uri_new (item->url);
    GList *next = l->next;

    if (g_strcmp0 (soup_uri_get_host (uri), host) == 0) {
      changed = TRUE;

      ephy_web_overview_model_item_free (item);
      model->priv->items = g_list_delete_link (model->priv->items, l);
    }

    soup_uri_free (uri);
    l = next;
  }

  if (changed)
    g_signal_emit (model, signals[URLS_CHANGED], 0);
}

void
ephy_web_overview_model_clear (EphyWebOverviewModel *model)
{
  g_return_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  if (!model->priv->items)
    return;

  g_list_free_full (model->priv->items, (GDestroyNotify)ephy_web_overview_model_item_free);
  model->priv->items = NULL;
  g_signal_emit (model, signals[URLS_CHANGED], 0);
}

EphyWebOverviewModelItem *
ephy_web_overview_model_item_new (const char *url,
                                  const char *title)
{
  EphyWebOverviewModelItem *item;

  item = g_slice_new0 (EphyWebOverviewModelItem);
  item->url = g_strdup (url);
  item->title = g_strdup (title);

  return item;
}

void
ephy_web_overview_model_item_free (EphyWebOverviewModelItem *item)
{
  if (G_UNLIKELY (!item))
    return;

  g_free (item->url);
  g_free (item->title);

  g_slice_free (EphyWebOverviewModelItem, item);
}

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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
#include "ephy-web-overview-model.h"

struct _EphyWebOverviewModel {
  GObject parent_instance;

  GList *items;
  GHashTable *thumbnails;

  GHashTable *urls_listeners;
  GHashTable *thumbnail_listeners;
  GHashTable *title_listeners;
};

G_DEFINE_FINAL_TYPE (EphyWebOverviewModel, ephy_web_overview_model, G_TYPE_OBJECT)

static void
ephy_web_overview_model_dispose (GObject *object)
{
  EphyWebOverviewModel *model = EPHY_WEB_OVERVIEW_MODEL (object);

  if (model->items) {
    g_list_free_full (model->items, (GDestroyNotify)ephy_web_overview_model_item_free);
    model->items = NULL;
  }

  if (model->thumbnails) {
    g_hash_table_destroy (model->thumbnails);
    model->thumbnails = NULL;
  }

  g_clear_pointer (&model->urls_listeners, g_hash_table_destroy);
  g_clear_pointer (&model->thumbnail_listeners, g_hash_table_destroy);
  g_clear_pointer (&model->title_listeners, g_hash_table_destroy);

  G_OBJECT_CLASS (ephy_web_overview_model_parent_class)->dispose (object);
}

static void
ephy_web_overview_model_class_init (EphyWebOverviewModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_web_overview_model_dispose;
}

static void
ephy_web_overview_model_init (EphyWebOverviewModel *model)
{
  model->thumbnails = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             (GDestroyNotify)g_free,
                                             (GDestroyNotify)g_free);
  model->urls_listeners = g_hash_table_new_full (g_direct_hash,
                                                 g_direct_equal,
                                                 g_object_unref,
                                                 NULL);
  model->thumbnail_listeners = g_hash_table_new_full (g_direct_hash,
                                                      g_direct_equal,
                                                      g_object_unref,
                                                      NULL);
  model->title_listeners = g_hash_table_new_full (g_direct_hash,
                                                  g_direct_equal,
                                                  g_object_unref,
                                                  NULL);
}

static GPtrArray *
ephy_web_overview_model_urls_to_js_value (EphyWebOverviewModel *model,
                                          JSCContext           *js_context)
{
  GPtrArray *urls;
  GList *l;

  urls = g_ptr_array_new_with_free_func (g_object_unref);
  for (l = model->items; l; l = g_list_next (l)) {
    EphyWebOverviewModelItem *item = (EphyWebOverviewModelItem *)l->data;
    g_autoptr (JSCValue) js_item = NULL;
    g_autoptr (JSCValue) value = NULL;

    js_item = jsc_value_new_object (js_context, NULL, NULL);
    value = jsc_value_new_string (js_context, item->url);
    jsc_value_object_set_property (js_item, "url", value);

    g_clear_object (&value);
    value = jsc_value_new_string (js_context, item->title);
    jsc_value_object_set_property (js_item, "title", value);

    g_ptr_array_add (urls, g_steal_pointer (&js_item));
  }

  return urls;
}

static void
ephy_web_overview_model_notify_urls_changed (EphyWebOverviewModel *model)
{
  GHashTableIter iter;
  gpointer key;
  g_autoptr (GPtrArray) urls = NULL;

  g_hash_table_iter_init (&iter, model->urls_listeners);
  while (g_hash_table_iter_next (&iter, &key, NULL)) {
    g_autoptr (JSCValue) value = NULL;
    g_autoptr (JSCValue) ret = NULL;

    value = jsc_weak_value_get_value (JSC_WEAK_VALUE (key));
    if (value && jsc_value_is_function (value)) {
      if (!urls)
        urls = ephy_web_overview_model_urls_to_js_value (model, jsc_value_get_context (value));
      ret = jsc_value_function_call (value, G_TYPE_PTR_ARRAY, urls, G_TYPE_NONE);
      (void)ret;
    }
  }
}

static void
ephy_web_overview_model_notify_thumbnail_changed (EphyWebOverviewModel *model,
                                                  const char           *url,
                                                  const char           *path)
{
  GHashTableIter iter;
  gpointer key;

  g_hash_table_iter_init (&iter, model->thumbnail_listeners);
  while (g_hash_table_iter_next (&iter, &key, NULL)) {
    g_autoptr (JSCValue) value = NULL;
    g_autoptr (JSCValue) ret = NULL;

    value = jsc_weak_value_get_value (JSC_WEAK_VALUE (key));
    if (value) {
      if (jsc_value_is_function (value)) {
        ret = jsc_value_function_call (value, G_TYPE_STRING, url, G_TYPE_STRING, path, G_TYPE_NONE);
        (void)ret;
      }
    }
  }
}

static void
ephy_web_overview_model_notify_title_changed (EphyWebOverviewModel *model,
                                              const char           *url,
                                              const char           *title)
{
  GHashTableIter iter;
  gpointer key;

  g_hash_table_iter_init (&iter, model->title_listeners);
  while (g_hash_table_iter_next (&iter, &key, NULL)) {
    g_autoptr (JSCValue) value = NULL;
    g_autoptr (JSCValue) ret = NULL;

    value = jsc_weak_value_get_value (JSC_WEAK_VALUE (key));
    if (value) {
      if (jsc_value_is_function (value)) {
        ret = jsc_value_function_call (value, G_TYPE_STRING, url, G_TYPE_STRING, title, G_TYPE_NONE);
        (void)ret;
      }
    }
  }
}

EphyWebOverviewModel *
ephy_web_overview_model_new (void)
{
  return g_object_new (EPHY_TYPE_WEB_OVERVIEW_MODEL, NULL);
}

void
ephy_web_overview_model_set_urls (EphyWebOverviewModel *model,
                                  GList                *urls)
{
  g_assert (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  g_list_free_full (model->items, (GDestroyNotify)ephy_web_overview_model_item_free);
  model->items = urls;
  ephy_web_overview_model_notify_urls_changed (model);
}

void
ephy_web_overview_model_set_url_thumbnail (EphyWebOverviewModel *model,
                                           const char           *url,
                                           const char           *path,
                                           gboolean              notify)
{
  const char *thumbnail_path;

  g_assert (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  thumbnail_path = g_hash_table_lookup (model->thumbnails, url);
  if (g_strcmp0 (thumbnail_path, path) == 0)
    return;

  g_hash_table_insert (model->thumbnails, g_strdup (url), g_strdup (path));
  if (notify)
    ephy_web_overview_model_notify_thumbnail_changed (model, url, path);
}

void
ephy_web_overview_model_set_url_title (EphyWebOverviewModel *model,
                                       const char           *url,
                                       const char           *title)
{
  GList *l;
  gboolean changed = FALSE;

  g_assert (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  for (l = model->items; l; l = g_list_next (l)) {
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
    ephy_web_overview_model_notify_title_changed (model, url, title);
}

void
ephy_web_overview_model_delete_url (EphyWebOverviewModel *model,
                                    const char           *url)
{
  GList *l;
  gboolean changed = FALSE;

  g_assert (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  l = model->items;
  while (l) {
    EphyWebOverviewModelItem *item = (EphyWebOverviewModelItem *)l->data;
    GList *next = l->next;

    if (g_strcmp0 (item->url, url) == 0) {
      changed = TRUE;

      ephy_web_overview_model_item_free (item);
      model->items = g_list_delete_link (model->items, l);
    }

    l = next;
  }

  if (changed)
    ephy_web_overview_model_notify_urls_changed (model);
}

void
ephy_web_overview_model_delete_host (EphyWebOverviewModel *model,
                                     const char           *host)
{
  GList *l;
  gboolean changed = FALSE;

  g_assert (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  l = model->items;
  while (l) {
    EphyWebOverviewModelItem *item = (EphyWebOverviewModelItem *)l->data;
    g_autoptr (GUri) uri = NULL;
    GList *next = l->next;

    uri = g_uri_parse (item->url, G_URI_FLAGS_PARSE_RELAXED, NULL);
    if (g_strcmp0 (g_uri_get_host (uri), host) == 0) {
      changed = TRUE;

      ephy_web_overview_model_item_free (item);
      model->items = g_list_delete_link (model->items, l);
    }

    l = next;
  }

  if (changed)
    ephy_web_overview_model_notify_urls_changed (model);
}

void
ephy_web_overview_model_clear (EphyWebOverviewModel *model)
{
  g_assert (EPHY_IS_WEB_OVERVIEW_MODEL (model));

  if (!model->items)
    return;

  g_list_free_full (model->items, (GDestroyNotify)ephy_web_overview_model_item_free);
  model->items = NULL;
  ephy_web_overview_model_notify_urls_changed (model);
}

EphyWebOverviewModelItem *
ephy_web_overview_model_item_new (const char *url,
                                  const char *title)
{
  EphyWebOverviewModelItem *item;

  item = g_new0 (EphyWebOverviewModelItem, 1);
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

  g_free (item);
}

static void
js_web_overview_model_set_thumbnail (EphyWebOverviewModel *model,
                                     const char           *url,
                                     const char           *path)
{
  ephy_web_overview_model_set_url_thumbnail (model, url, path, FALSE);
}

static char *
js_web_overview_model_get_thumbnail (EphyWebOverviewModel *model,
                                     const char           *url)
{
  return g_strdup (g_hash_table_lookup (model->thumbnails, url));
}

static GPtrArray *
js_web_overview_model_get_urls (EphyWebOverviewModel *model)
{
  return ephy_web_overview_model_urls_to_js_value (model, jsc_context_get_current ());
}

static void
js_event_listener_destroyed (JSCWeakValue *weak_value,
                             GHashTable   *listeners)
{
  g_hash_table_remove (listeners, weak_value);
}

static void
js_web_overview_model_add_urls_changed_event_listener (EphyWebOverviewModel *model,
                                                       JSCValue             *js_function)
{
  JSCWeakValue *weak_value;

  if (!jsc_value_is_function (js_function)) {
    jsc_context_throw (jsc_context_get_current (), "Invalid type passed to onurlschanged");
    return;
  }

  weak_value = jsc_weak_value_new (js_function);
  g_signal_connect (weak_value, "cleared",
                    G_CALLBACK (js_event_listener_destroyed),
                    model->urls_listeners);
  g_hash_table_add (model->urls_listeners, weak_value);
}

static void
js_web_overview_model_add_thumbnail_changed_event_listener (EphyWebOverviewModel *model,
                                                            JSCValue             *js_function)
{
  JSCWeakValue *weak_value;

  if (!jsc_value_is_function (js_function)) {
    jsc_context_throw (jsc_context_get_current (), "Invalid type passed to onthumbnailchanged");
    return;
  }

  weak_value = jsc_weak_value_new (js_function);
  g_signal_connect (weak_value, "cleared",
                    G_CALLBACK (js_event_listener_destroyed),
                    model->thumbnail_listeners);
  g_hash_table_add (model->thumbnail_listeners, weak_value);
}

static void
js_web_overview_model_add_title_changed_event_listener (EphyWebOverviewModel *model,
                                                        JSCValue             *js_function)
{
  JSCWeakValue *weak_value;

  if (!jsc_value_is_function (js_function)) {
    jsc_context_throw (jsc_context_get_current (), "Invalid type passed to ontitlechanged");
    return;
  }

  weak_value = jsc_weak_value_new (js_function);
  g_signal_connect (weak_value, "cleared",
                    G_CALLBACK (js_event_listener_destroyed),
                    model->title_listeners);
  g_hash_table_add (model->title_listeners, weak_value);
}

JSCValue *
ephy_web_overview_model_export_to_js_context (EphyWebOverviewModel *model,
                                              JSCContext           *js_context)
{
  JSCClass *js_class;

  js_class = jsc_context_register_class (js_context, "OverviewModel", NULL, NULL, NULL);
  jsc_class_add_method (js_class,
                        "setThumbnail",
                        G_CALLBACK (js_web_overview_model_set_thumbnail), NULL, NULL,
                        G_TYPE_NONE, 2,
                        G_TYPE_STRING, G_TYPE_STRING);
  jsc_class_add_method (js_class,
                        "getThumbnail",
                        G_CALLBACK (js_web_overview_model_get_thumbnail), NULL, NULL,
                        G_TYPE_STRING, 1,
                        G_TYPE_STRING);
  jsc_class_add_property (js_class,
                          "urls",
                          G_TYPE_PTR_ARRAY,
                          G_CALLBACK (js_web_overview_model_get_urls),
                          NULL,
                          NULL, NULL);
  jsc_class_add_property (js_class,
                          "onurlschanged",
                          JSC_TYPE_VALUE,
                          NULL,
                          G_CALLBACK (js_web_overview_model_add_urls_changed_event_listener),
                          NULL, NULL);
  jsc_class_add_property (js_class,
                          "onthumbnailchanged",
                          JSC_TYPE_VALUE,
                          NULL,
                          G_CALLBACK (js_web_overview_model_add_thumbnail_changed_event_listener),
                          NULL, NULL);
  jsc_class_add_property (js_class,
                          "ontitlechanged",
                          JSC_TYPE_VALUE,
                          NULL,
                          G_CALLBACK (js_web_overview_model_add_title_changed_event_listener),
                          NULL, NULL);

  return jsc_value_new_object (js_context, model, js_class);
}

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
#include "ephy-web-overview.h"

#include <string.h>

struct _EphyWebOverviewPrivate
{
  WebKitWebPage *web_page;
  EphyWebOverviewModel *model;
  GList *items;
};

enum
{
  PROP_0,
  PROP_WEB_PAGE,
  PROP_MODEL,
};

enum
{
  ITEM_REMOVED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyWebOverview, ephy_web_overview, G_TYPE_OBJECT)

typedef struct {
  char *url;

  WebKitDOMElement *anchor;
  WebKitDOMElement *thumbnail;
  WebKitDOMElement *title;
} OverviewItem;

static OverviewItem *
overview_item_new (WebKitDOMElement *anchor)
{
  OverviewItem *item;
  WebKitDOMNodeList *nodes;
  int i, n_nodes;

  item = g_slice_new0 (OverviewItem);
  item->anchor = g_object_ref (anchor);
  item->url = webkit_dom_element_get_attribute (anchor, "href");

  nodes = webkit_dom_node_get_child_nodes (WEBKIT_DOM_NODE (anchor));
  n_nodes = webkit_dom_node_list_get_length (nodes);
  for (i = 0; i < n_nodes; i++) {
    WebKitDOMNode* node = webkit_dom_node_list_item (nodes, i);
    WebKitDOMElement *element;
    char *tag;

    if (!WEBKIT_DOM_IS_ELEMENT (node))
      continue;

    element = WEBKIT_DOM_ELEMENT (node);
    tag = webkit_dom_element_get_tag_name (element);
    if (g_strcmp0 (tag, "SPAN") == 0) {
      char *class;

      class = webkit_dom_element_get_class_name (element);
      if (g_strcmp0 (class, "thumbnail") == 0)
        item->thumbnail = g_object_ref (element);
      else if (g_strcmp0 (class, "title") == 0)
        item->title = g_object_ref (element);

      g_free (class);
    }

    g_free (tag);
  }
  g_object_unref (nodes);

  return item;
}

static void
overview_item_free (OverviewItem *item)
{
  g_free (item->url);
  g_clear_object (&item->anchor);
  g_clear_object (&item->thumbnail);
  g_clear_object (&item->title);

  g_slice_free (OverviewItem, item);
}

static void
ephy_web_overview_web_page_uri_changed (WebKitWebPage *web_page,
                                        GParamSpec *spec,
                                        EphyWebOverview *overview)
{
  if (g_strcmp0 (webkit_web_page_get_uri (web_page), "ephy-about:overview") != 0)
    g_object_unref (overview);
}

static void
ephy_web_overview_model_urls_changed (EphyWebOverviewModel *model,
                                      EphyWebOverview *overview)
{
  GList *urls;
  GList *l;
  GList *items;
  OverviewItem* item;

  urls = ephy_web_overview_model_get_urls (model);

  items = overview->priv->items;
  for (l = urls; l; l = g_list_next (l)) {
    EphyWebOverviewModelItem *url = (EphyWebOverviewModelItem *)l->data;
    const char *thumbnail_path;

    thumbnail_path = ephy_web_overview_model_get_url_thumbnail (model, url->url);

    if (items) {
      WebKitDOMDOMTokenList *class_list;

      item = (OverviewItem *)items->data;

      g_free (item->url);
      item->url = g_strdup (url->url);

      class_list = webkit_dom_element_get_class_list (webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (item->anchor)));
      if (class_list && webkit_dom_dom_token_list_contains (class_list, "removed", NULL))
        webkit_dom_dom_token_list_remove (class_list, "removed", NULL);

      webkit_dom_element_set_attribute (item->anchor, "href", url->url, NULL);
      webkit_dom_element_set_attribute (item->anchor, "title", url->title, NULL);
      webkit_dom_node_set_text_content (WEBKIT_DOM_NODE (item->title), url->title, NULL);

      if (thumbnail_path) {
        char *style;

        style = g_strdup_printf ("background: url(ephy-about:/thumbnail-frame.png), url(file://%s) no-repeat 10px 9px;", thumbnail_path);
        webkit_dom_element_set_attribute (item->thumbnail, "style", style, NULL);
        g_free (style);
      } else {
        webkit_dom_element_remove_attribute (item->thumbnail, "style");
      }
    } else {
      WebKitDOMDocument *document;
      WebKitDOMElement *item_list, *anchor;
      WebKitDOMNode *new_node;

      item = g_slice_new0 (OverviewItem);
      item->url = g_strdup (url->url);

      document = webkit_web_page_get_dom_document (overview->priv->web_page);
      item_list = webkit_dom_document_get_element_by_id (document, "item-list");

      new_node = WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "LI", NULL));
      webkit_dom_node_append_child (WEBKIT_DOM_NODE (item_list), WEBKIT_DOM_NODE (new_node), NULL);

      anchor = webkit_dom_document_create_element (document, "A", NULL);
      item->anchor = g_object_ref (anchor);
      webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (anchor), "overview-item");
      webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (anchor), "href", url->url, NULL);
      webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (anchor), "title", url->title, NULL);
      webkit_dom_node_append_child (WEBKIT_DOM_NODE (new_node), WEBKIT_DOM_NODE (anchor), NULL);

      new_node = WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "DIV", NULL));
      webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (new_node), "close-button");
      webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (new_node), "onclick", "removeFromOverview(this.parentNode,event)", NULL);
      webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (new_node), "title", url->title, NULL);
      webkit_dom_node_set_text_content (new_node, "✖", NULL);
      webkit_dom_node_append_child (WEBKIT_DOM_NODE (anchor), new_node, NULL);

      new_node = WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "SPAN", NULL));
      item->thumbnail = g_object_ref (new_node);
      webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (new_node), "thumbnail");
      if (thumbnail_path) {
        char *style;

        style = g_strdup_printf ("background: url(ephy-about:/thumbnail-frame.png), url(file://%s) no-repeat 10px 9px;", thumbnail_path);
        webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (new_node), "style", style, NULL);
        g_free (style);
      }
      webkit_dom_node_append_child (WEBKIT_DOM_NODE (anchor), new_node, NULL);

      new_node = WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "SPAN", NULL));
      item->title = g_object_ref (new_node);
      webkit_dom_element_set_class_name (WEBKIT_DOM_ELEMENT (new_node), "title");
      webkit_dom_node_set_text_content (new_node, url->title, NULL);
      webkit_dom_node_append_child (WEBKIT_DOM_NODE (anchor), new_node, NULL);

      overview->priv->items = g_list_append (overview->priv->items, item);
    }

    items = g_list_next (items);
  }

  while (items) {
    WebKitDOMNode *anchor;
    GList *next = items->next;

    item = (OverviewItem *)items->data;

    anchor = WEBKIT_DOM_NODE (item->anchor);
    webkit_dom_node_remove_child (webkit_dom_node_get_parent_node (anchor), anchor, NULL);

    overview_item_free (item);
    overview->priv->items = g_list_delete_link (overview->priv->items, items);

    items = next;
  }
}

static void
ephy_web_overview_model_thumbnail_changed (EphyWebOverviewModel *model,
                                           const char *url,
                                           const char *path,
                                           EphyWebOverview *overview)
{
  GList *l;

  for (l = overview->priv->items; l; l = g_list_next (l)) {
    OverviewItem *item = (OverviewItem *)l->data;
    char *style;

    if (g_strcmp0 (item->url, url) != 0)
      continue;

    style = g_strdup_printf ("background: url(ephy-about:/thumbnail-frame.png), url(file://%s) no-repeat 10px 9px;", path);
    webkit_dom_element_set_attribute (item->thumbnail, "style", style, NULL);
    g_free (style);
  }
}

static void
ephy_web_overview_model_title_changed (EphyWebOverviewModel *model,
                                       const char *url,
                                       const char *title,
                                       EphyWebOverview *overview)
{
  GList *l;

  for (l = overview->priv->items; l; l = g_list_next (l)) {
    OverviewItem *item = (OverviewItem *)l->data;
    char *style;

    if (g_strcmp0 (item->url, url) != 0)
      continue;

    webkit_dom_element_set_attribute (item->anchor, "title", title, NULL);
    webkit_dom_node_set_text_content (WEBKIT_DOM_NODE (item->title), title, NULL);
  }
}

static void
ephy_web_overview_update_thumbnail_in_model_from_element (EphyWebOverview *overview,
                                                          const char *url,
                                                          WebKitDOMElement *thumbnail)
{
  WebKitDOMCSSStyleDeclaration *style;
  char *background;
  char *thumbnail_path;

  style = webkit_dom_element_get_style (thumbnail);
  if (webkit_dom_css_style_declaration_is_property_implicit (style, "background"))
    return;

  background = webkit_dom_css_style_declaration_get_property_value (style, "background");
  if (!background)
    return;

  thumbnail_path = g_strrstr (background, "file://");
  if (thumbnail_path) {
    char *p;

    thumbnail_path += strlen ("file://");
    p = g_strrstr (thumbnail_path, ")");
    if (p) {
      thumbnail_path = g_strndup (thumbnail_path, strlen (thumbnail_path) - strlen (p));
      g_signal_handlers_block_by_func (overview->priv->model, G_CALLBACK (ephy_web_overview_model_thumbnail_changed), overview);
      ephy_web_overview_model_set_url_thumbnail (overview->priv->model, url, thumbnail_path);
      g_signal_handlers_unblock_by_func (overview->priv->model, G_CALLBACK (ephy_web_overview_model_thumbnail_changed), overview);
      g_free (thumbnail_path);
    }

  }
  g_free (background);
}

static void
ephy_web_overview_document_loaded (WebKitWebPage *web_page,
                                   EphyWebOverview *overview)
{
  WebKitDOMDocument *document;
  WebKitDOMNodeList *nodes;
  int i, n_nodes;

  document = webkit_web_page_get_dom_document (web_page);
  nodes = webkit_dom_document_get_elements_by_tag_name (document, "a");
  n_nodes = webkit_dom_node_list_get_length (nodes);
  for (i = 0; i < n_nodes; i++) {
    WebKitDOMElement* element = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (nodes, i));
    char *class;

    class = webkit_dom_element_get_class_name (element);
    if (g_strcmp0 (class, "overview-item") == 0) {
      OverviewItem *item = overview_item_new (element);

      /* URLs and titles are always sent from the UI process, but thumbnails don't,
       * so update the model with the thumbnail of there's one.
       */
      ephy_web_overview_update_thumbnail_in_model_from_element (overview, item->url, item->thumbnail);

      overview->priv->items = g_list_prepend (overview->priv->items, item);
    }
    g_free (class);
  }
  g_object_unref (nodes);
  overview->priv->items = g_list_reverse (overview->priv->items);
}

static void
ephy_web_overview_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  EphyWebOverview *overview = EPHY_WEB_OVERVIEW (object);

  switch (prop_id)
  {
  case PROP_WEB_PAGE:
    overview->priv->web_page = g_value_get_object (value);
    break;
  case PROP_MODEL:
    overview->priv->model = g_value_get_object (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_web_overview_dispose (GObject *object)
{
  EphyWebOverview *overview = EPHY_WEB_OVERVIEW (object);

  if (overview->priv->items) {
    g_list_free_full (overview->priv->items, (GDestroyNotify)overview_item_free);
    overview->priv->items = NULL;
  }

  G_OBJECT_CLASS (ephy_web_overview_parent_class)->dispose (object);
}

static void
ephy_web_overview_web_view_destroyed (EphyWebOverview *overview,
                                      GObject *destroyed_web_view)
{
  overview->priv->web_page = NULL;
  g_object_unref (overview);
}

void
ephy_web_overview_finalize (GObject *object)
{
  EphyWebOverview *overview = EPHY_WEB_OVERVIEW (object);

  if (overview->priv->web_page) {
    g_object_weak_unref (G_OBJECT (overview->priv->web_page),
                         (GWeakNotify)ephy_web_overview_web_view_destroyed,
                         overview);
  }

  G_OBJECT_CLASS (ephy_web_overview_parent_class)->finalize (object);
}

static void
ephy_web_overview_constructed (GObject *object)
{
  EphyWebOverview *overview = EPHY_WEB_OVERVIEW (object);

  G_OBJECT_CLASS (ephy_web_overview_parent_class)->constructed (object);

  g_object_weak_ref (G_OBJECT (overview->priv->web_page),
                     (GWeakNotify)ephy_web_overview_web_view_destroyed,
                     overview);

  g_signal_connect_object (overview->priv->web_page, "notify::uri",
                           G_CALLBACK (ephy_web_overview_web_page_uri_changed),
                           overview, 0);
  g_signal_connect_object (overview->priv->web_page, "document-loaded",
                           G_CALLBACK (ephy_web_overview_document_loaded),
                           overview, 0);

  g_signal_connect_object (overview->priv->model, "urls-changed",
                           G_CALLBACK (ephy_web_overview_model_urls_changed),
                           overview, 0);
  g_signal_connect_object (overview->priv->model, "thumbnail-changed",
                           G_CALLBACK (ephy_web_overview_model_thumbnail_changed),
                           overview, 0);
  g_signal_connect_object (overview->priv->model, "title-changed",
                           G_CALLBACK (ephy_web_overview_model_title_changed),
                           overview, 0);
}

static void
ephy_web_overview_class_init (EphyWebOverviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_web_overview_dispose;
  object_class->finalize = ephy_web_overview_finalize;
  object_class->constructed = ephy_web_overview_constructed;
  object_class->set_property = ephy_web_overview_set_property;

  g_object_class_install_property (object_class,
                                   PROP_WEB_PAGE,
                                   g_param_spec_object ("web-page",
                                                        "WebPage",
                                                        "The overview WebPage",
                                                        WEBKIT_TYPE_WEB_PAGE,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        "Model",
                                                        "The overview model",
                                                        EPHY_TYPE_WEB_OVERVIEW_MODEL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  signals[ITEM_REMOVED] =
    g_signal_new ("item-removed",
                  EPHY_TYPE_WEB_OVERVIEW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  g_type_class_add_private (object_class, sizeof(EphyWebOverviewPrivate));
}

static void
ephy_web_overview_init (EphyWebOverview *overview)
{
  overview->priv = G_TYPE_INSTANCE_GET_PRIVATE (overview, EPHY_TYPE_WEB_OVERVIEW, EphyWebOverviewPrivate);
}

EphyWebOverview *
ephy_web_overview_new (WebKitWebPage *web_page,
                       EphyWebOverviewModel *model)
{
  g_return_val_if_fail (WEBKIT_IS_WEB_PAGE (web_page), NULL);
  g_return_val_if_fail (EPHY_IS_WEB_OVERVIEW_MODEL (model), NULL);

  return g_object_new (EPHY_TYPE_WEB_OVERVIEW,
                       "web-page", web_page,
                       "model", model,
                       NULL);
}

static JSValueRef
remove_item_from_overview_cb (JSContextRef context,
                              JSObjectRef function,
                              JSObjectRef this_object,
                              size_t argument_count,
                              const JSValueRef arguments[],
                              JSValueRef *exception)
{
  EphyWebOverview *overview;
  JSStringRef result_string_js;
  size_t max_size;
  char *result_string;

  overview = EPHY_WEB_OVERVIEW (JSObjectGetPrivate (this_object));

  result_string_js = JSValueToStringCopy (context, arguments[0], NULL);
  max_size = JSStringGetMaximumUTF8CStringSize (result_string_js);

  result_string = g_malloc (max_size);
  JSStringGetUTF8CString (result_string_js, result_string, max_size);
  g_signal_emit (overview, signals[ITEM_REMOVED], 0, result_string);
  JSStringRelease (result_string_js);
  g_free (result_string);

  return JSValueMakeUndefined (context);
}

static const JSStaticFunction overview_static_funcs[] =
{
  { "removeItemFromOverview", remove_item_from_overview_cb, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
  { NULL, NULL, 0 }
};

static const JSClassDefinition overview_class_def =
{
  0,                     /* version */
  kJSClassAttributeNone, /* attributes */
  "Overview",            /* className */
  NULL,                  /* parentClass */
  NULL,                  /* staticValues */
  overview_static_funcs, /* staticFunctions */
  NULL,                  /* initialize */
  NULL,                  /* finalize */
  NULL,                  /* hasProperty */
  NULL,                  /* getProperty */
  NULL,                  /* setProperty */
  NULL,                  /* deleteProperty */
  NULL,                  /* getPropertyNames */
  NULL,                  /* callAsFunction */
  NULL,                  /* callAsConstructor */
  NULL,                  /* hasInstance */
  NULL                   /* convertToType */
};

void
ephy_web_overview_init_js (EphyWebOverview *overview,
                           JSGlobalContextRef context)
{
  JSObjectRef global_object;
  JSClassRef class_def;
  JSObjectRef class_object;
  JSStringRef str;

  global_object = JSContextGetGlobalObject (context);

  class_def = JSClassCreate (&overview_class_def);
  class_object = JSObjectMake (context, class_def, overview);
  str = JSStringCreateWithUTF8CString ("Overview");
  JSObjectSetProperty (context, global_object, str, class_object, kJSPropertyAttributeNone, NULL);
  JSStringRelease (str);

  /* FIXME: check leaks here */
}

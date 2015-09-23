/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2013 Igalia S.L.
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
#include "ephy-web-dom-utils.h"

#include <libsoup/soup.h>

/**
 * ephy_web_dom_utils_has_modified_forms:
 * @document: the DOM document to check if there are or not modified forms.
 *
 * A small heuristic is used here. If there's only one input element modified
 * and it does not have a lot of text the user is likely not very interested in
 * saving this work, so it returns %FALSE in this case (eg, google search
 * input).
 *
 * Returns %TRUE if the user has modified &lt;input&gt; or &lt;textarea&gt;
 * values in the @document.
 **/
gboolean
ephy_web_dom_utils_has_modified_forms (WebKitDOMDocument *document)
{
  WebKitDOMHTMLCollection *forms;
  gulong forms_n;
  int i;

  forms = webkit_dom_document_get_forms (document);
  forms_n = webkit_dom_html_collection_get_length (forms);

  for (i = 0; i < forms_n; i++) {
    WebKitDOMHTMLCollection *elements;
    WebKitDOMNode *form_element = webkit_dom_html_collection_item (forms, i);
    gulong elements_n;
    int j;
    gboolean modified_input_element = FALSE;

    elements = webkit_dom_html_form_element_get_elements (WEBKIT_DOM_HTML_FORM_ELEMENT (form_element));
    elements_n = webkit_dom_html_collection_get_length (elements);

    for (j = 0; j < elements_n; j++) {
      WebKitDOMNode *element;

      element = webkit_dom_html_collection_item (elements, j);

      if (WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT (element))
        if (webkit_dom_html_text_area_element_is_edited (WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (element))) {
          char *text;
          gboolean has_data;

          text = webkit_dom_html_text_area_element_get_value (WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (element));
          has_data = text && *text;
          g_free (text);

          if (has_data)
            return TRUE;
        }

      if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (element))
        if (webkit_dom_html_input_element_is_edited (WEBKIT_DOM_HTML_INPUT_ELEMENT (element))) {
          glong length;
          char *text;

          /* A small heuristic here. If there's only one input element
           * modified and it does not have a lot of text the user is
           * likely not very interested in saving this work, so do
           * nothing (eg, google search input). */
          if (modified_input_element)
            return TRUE;

          modified_input_element = TRUE;

          text = webkit_dom_html_input_element_get_value (WEBKIT_DOM_HTML_INPUT_ELEMENT (element));
          length = g_utf8_strlen (text, -1);
          g_free (text);

          if (length > 50)
            return TRUE;
        }
    }
  }

  return FALSE;
}

/**
 * ephy_web_dom_utils_get_application_title:
 * @document: the DOM document.
 *
 * Returns web application title if it is defined in &lt;meta&gt; elements of
 * @document.
 **/
char *
ephy_web_dom_utils_get_application_title (WebKitDOMDocument *document)
{
  WebKitDOMNodeList *metas;
  char *title = NULL;
  gulong length, i;

  metas = webkit_dom_document_get_elements_by_tag_name (document, "meta");
  length = webkit_dom_node_list_get_length (metas);

  for (i = 0; i < length && title == NULL; i++) {
    char *name;
    char *property;
    WebKitDOMNode *node = webkit_dom_node_list_item (metas, i);

    name = webkit_dom_html_meta_element_get_name (WEBKIT_DOM_HTML_META_ELEMENT (node));
    property = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "property");
    if (g_strcmp0 (name, "application-name") == 0
        || g_strcmp0 (property, "og:site_name") == 0) {
      title = webkit_dom_html_meta_element_get_content (WEBKIT_DOM_HTML_META_ELEMENT (node));
    }
    g_free (property);
    g_free (name);
  }

  return title;
}

static char *
resolve_uri (const char *base_uri,
             const char *uri)
{
  SoupURI *base;
  SoupURI *new;
  char *ret;

  if (uri == NULL)
    return NULL;

  if (base_uri == NULL)
    return NULL;

  base = soup_uri_new (base_uri);
  new = soup_uri_new_with_base (base, uri);
  soup_uri_free (base);
  ret = soup_uri_to_string (new, FALSE);
  soup_uri_free (new);

  return ret;
}

static gboolean
get_icon_from_mstile (WebKitDOMDocument *document,
                      char              **uri_out,
                      char              **color_out)
{
  gboolean ret;
  WebKitDOMNodeList *metas;
  gulong length, i;
  char *image = NULL;
  char *color = NULL;

  metas = webkit_dom_document_get_elements_by_tag_name (document, "meta");
  length = webkit_dom_node_list_get_length (metas);

  for (i = 0; i < length; i++) {
    WebKitDOMNode *node = webkit_dom_node_list_item (metas, i);
    char *name;

    name = webkit_dom_html_meta_element_get_name (WEBKIT_DOM_HTML_META_ELEMENT (node));
    if (g_strcmp0 (name, "msapplication-TileImage") == 0) {
      if (image == NULL)
        image = webkit_dom_html_meta_element_get_content (WEBKIT_DOM_HTML_META_ELEMENT (node));
    } else if (g_strcmp0 (name, "msapplication-TileColor") == 0) {
      if (color == NULL)
        color = webkit_dom_html_meta_element_get_content (WEBKIT_DOM_HTML_META_ELEMENT (node));
    }
  }

  ret = (image != NULL && *image != '\0');

  if (uri_out != NULL)
    *uri_out = g_strdup (image);
  if (color_out != NULL)
    *color_out = g_strdup (color);

  g_free (image);
  g_free (color);

  return ret;
}

static gboolean
get_icon_from_ogp (WebKitDOMDocument *document,
                   char         **uri_out,
                   char         **color_out)
{
  gboolean ret;
  WebKitDOMNodeList *metas;
  gulong length, i;
  char *image = NULL;
  char *color = NULL;

  metas = webkit_dom_document_get_elements_by_tag_name (document, "meta");
  length = webkit_dom_node_list_get_length (metas);

  for (i = 0; i < length && image == NULL; i++) {
    WebKitDOMNode *node = webkit_dom_node_list_item (metas, i);
    char *property;
    char *itemprop;

    property = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "property");
    itemprop = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "itemprop");
    if (g_strcmp0 (property, "og:image") == 0 ||
        g_strcmp0 (itemprop, "image") == 0) {
      image = webkit_dom_html_meta_element_get_content (WEBKIT_DOM_HTML_META_ELEMENT (node));
    }
    g_free (property);
    g_free (itemprop);
  }

  ret = (image != NULL && *image != '\0');

  if (uri_out != NULL)
    *uri_out = g_strdup (image);
  if (color_out != NULL)
    *color_out = g_strdup (color);

  return ret;
}

static gboolean
get_icon_from_touch_icon (WebKitDOMDocument *document,
                          char         **uri_out,
                          char         **color_out)
{
  gboolean ret;
  WebKitDOMNodeList *links;
  gulong length, i;
  char *image = NULL;
  char *color = NULL;

  links = webkit_dom_document_get_elements_by_tag_name (document, "link");
  length = webkit_dom_node_list_get_length (links);

  for (i = 0; i < length && image == NULL; i++) {
    char *rel;
    WebKitDOMNode *node = webkit_dom_node_list_item (links, i);

    rel = webkit_dom_html_link_element_get_rel (WEBKIT_DOM_HTML_LINK_ELEMENT (node));
    /* TODO: support more than one possible icon. */
    if (g_strcmp0 (rel, "apple-touch-icon") == 0 ||
        g_strcmp0 (rel, "apple-touch-icon-precomposed") == 0) {
      image = webkit_dom_html_link_element_get_href (WEBKIT_DOM_HTML_LINK_ELEMENT (node));
    }
    g_free (rel);
  }

  ret = (image != NULL && *image != '\0');

  if (uri_out != NULL)
    *uri_out = g_strdup (image);
  if (color_out != NULL)
    *color_out = g_strdup (color);

  return ret;
}

static gboolean
get_icon_from_favicon (WebKitDOMDocument *document,
                       char         **uri_out,
                       char         **color_out)
{
  gboolean ret;
  WebKitDOMNodeList *links;
  gulong length, i;
  char *image = NULL;
  char *color = NULL;

  links = webkit_dom_document_get_elements_by_tag_name (document, "link");
  length = webkit_dom_node_list_get_length (links);

  for (i = 0; i < length; i++) {
    char *rel;
    WebKitDOMNode *node = webkit_dom_node_list_item (links, i);

    rel = webkit_dom_html_link_element_get_rel (WEBKIT_DOM_HTML_LINK_ELEMENT (node));
    if (g_strcmp0 (rel, "shortcut-icon") == 0 ||
        g_strcmp0 (rel, "shortcut icon") == 0 ||
        g_strcmp0 (rel, "SHORTCUT ICON") == 0 ||
        g_strcmp0 (rel, "Shortcut Icon") == 0 ||
        g_strcmp0 (rel, "icon shortcut") == 0 ||
        g_strcmp0 (rel, "icon") == 0) {
      image = webkit_dom_html_link_element_get_href (WEBKIT_DOM_HTML_LINK_ELEMENT (node));
    }
    g_free (rel);
  }

  ret = (image != NULL && *image != '\0');

  if (uri_out != NULL)
    *uri_out = g_strdup (image);
  if (color_out != NULL)
    *color_out = g_strdup (color);

  return ret;
}

/**
 * ephy_web_dom_utils_get_best_icon:
 * @document: the DOM document.
 * @base_uri: base URI of the #WebKitWebView.
 * @uri_out: Icon URI.
 * @color_out: Icon background color.
 *
 * Tries to get the icon (and its background color if any) for a web application
 * from the @document meta data. First try to get a mstile, then OGP, then touch
 * icon and finally favicon.
 *
 * Returns %TRUE if it finds an icon in the @document.
 **/
gboolean
ephy_web_dom_utils_get_best_icon (WebKitDOMDocument *document,
                                  const char        *base_uri,
                                  char             **uri_out,
                                  char             **color_out)
{
  gboolean ret = FALSE;
  char *image = NULL;
  char *color = NULL;

  ret = get_icon_from_mstile (document, &image, &color);
  if (! ret)
    ret = get_icon_from_ogp (document, &image, &color);
  if (! ret)
    ret = get_icon_from_touch_icon (document, &image, &color);
  if (! ret)
    ret = get_icon_from_favicon (document, &image, &color);

  if (uri_out != NULL)
    *uri_out = resolve_uri (base_uri, image);
  if (color_out != NULL)
    *color_out = g_strdup (color);

  g_free (image);
  g_free (color);

  return ret;
}

gboolean
ephy_web_dom_utils_find_form_auth_elements (WebKitDOMHTMLFormElement *form,
                                            WebKitDOMNode **username,
                                            WebKitDOMNode **password)
{
  WebKitDOMHTMLCollection *elements;
  WebKitDOMNode *username_node = NULL;
  WebKitDOMNode *password_node = NULL;
  guint i, n_elements;
  gboolean found_auth_elements = FALSE;
  gboolean found_auth_no_username_elements = FALSE;

  elements = webkit_dom_html_form_element_get_elements (form);
  n_elements = webkit_dom_html_collection_get_length (elements);

  for (i = 0; i < n_elements; i++) {
    WebKitDOMNode *element;
    char *element_type;
    char *element_name;

    element = webkit_dom_html_collection_item (elements, i);
    if (!WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (element))
      continue;

    g_object_get (element, "type", &element_type, "name", &element_name, NULL);

    if (g_strcmp0 (element_type, "text") == 0 ||
        g_strcmp0 (element_type, "email") == 0 ||
        g_strcmp0 (element_type, "tel") == 0) {
      /* We found more than one inputs of type text; we won't be saving here. */
      if (username_node) {
        g_free (element_type);
        found_auth_elements = FALSE;
        break;
      }

      username_node = g_object_ref (element);
      found_auth_elements = TRUE;
    } else if (g_strcmp0 (element_type, "password") == 0) {
      /* We found more than one inputs of type password; we won't be saving here. */
      if (password_node) {
        g_free (element_type);
        found_auth_elements = FALSE;
        break;
      }

      password_node = g_object_ref (element);
      found_auth_elements = TRUE;

      /* We found an input that usually doesn't require a separate login
       * adminpw is used by mailman admin pages */
      if (g_strcmp0 (element_name, "adminpw") == 0)
        found_auth_no_username_elements = TRUE;
    }

    g_free (element_type);
    g_free (element_name);
  }

  g_object_unref(elements);

  if (found_auth_no_username_elements && password_node) {
    g_clear_object (&username_node);
    *username = NULL;
    *password = password_node;

    return TRUE;
  }

  if (found_auth_elements && username_node && password_node) {
    *username = username_node;
    *password = password_node;

    return TRUE;
  }

  if (username_node)
    g_object_unref (username_node);
  if (password_node)
    g_object_unref (password_node);

  return FALSE;
}

/**
 * ephy_web_dom_utils_get_absolute_position_for_element:
 * @element: the #WebKitDOMElement.
 * @x: return address for the x coordinate.
 * @y: return address for the y coordinate.
 *
 * Obtains the coordinate for the top-left of the #WebKitDOMElement, relative
 * to the origin of the page.
 **/
void
ephy_web_dom_utils_get_absolute_position_for_element (WebKitDOMElement *element,
                                                      double *x,
                                                      double *y)
{
  WebKitDOMElement *parent;
  double offset_top, offset_left;
  double parent_x, parent_y;

  offset_top = webkit_dom_element_get_offset_top (element);
  offset_left = webkit_dom_element_get_offset_left (element);
  parent = webkit_dom_element_get_offset_parent (element);

  *x = offset_left;
  *y = offset_top;

  if (!parent)
    return;

  /* If there's a parent, we keep going. */
  ephy_web_dom_utils_get_absolute_position_for_element (parent, &parent_x, &parent_y);

  *x += parent_x;
  *y += parent_y;
}

/**
 * ephy_web_dom_utils_get_absolute_bottom_for_element:
 * @element: the #WebKitDOMElement.
 * @x: return address for the x coordinate.
 * @y: return address for the y coordinate.
 *
 * Obtains the coordinate for the bottom-left of the #WebKitDOMElement, relative
 * to the origin of the page.
 **/
void
ephy_web_dom_utils_get_absolute_bottom_for_element (WebKitDOMElement *element,
                                                    double *x,
                                                    double *y)
{
  double offset_height;

  ephy_web_dom_utils_get_absolute_position_for_element (element, x, y);

  offset_height = webkit_dom_element_get_offset_height (element);

  *y += offset_height;
}

/**
 * ephy_web_dom_utils_get_selection_as_string:
 * @selection: the #WebKitDOMDOMSelection.
 *
 * Gets the contents of the selection as a string.
 *
 * Returns: a newly allocated string with the selection or %NULL.
 **/
char *
ephy_web_dom_utils_get_selection_as_string   (WebKitDOMDOMSelection *selection)
{
  char *string;
  WebKitDOMRange *range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
  if (!range)
    return NULL;

  string = webkit_dom_range_to_string (range, NULL);
  g_object_unref (range);

  return string;
}

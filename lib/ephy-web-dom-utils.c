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

#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#include <webkit2/webkit-web-extension.h>
#else
#include <webkit/webkit.h>
#endif

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
        if (webkit_dom_html_text_area_element_is_edited (WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (element)))
          return TRUE;

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

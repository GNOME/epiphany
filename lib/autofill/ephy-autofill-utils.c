/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2018 Abdullah Alansari
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
#include "ephy-autofill-utils.h"

#include <glib-object.h>
#include <libsoup/soup.h>
#include <string.h>

#define MAX_ELEMENT_KEY_LENGTH 64

/**
 * ephy_autofill_utils_is_element_visible:
 * @element: a #WebKitDOMElement
 *
 * Returns: %TRUE if @key is trivially visible
 **/
bool
ephy_autofill_utils_is_element_visible (WebKitDOMElement *element)
{
  double height = webkit_dom_element_get_offset_height (element);
  double width = webkit_dom_element_get_offset_width (element);

  return (height > 1 && width > 1);
}

/**
 * ephy_autofill_utils_is_valid_element_key:
 * @key: (allow-none) an element key this can be:
 *  - a placeholder attribute
 *  - a name attribute
 *  - a label
 *
 * @key is valid iff:
 *  - Not too long. this guarantees good performance and system availability
 *  - Not an empty string
 *  - Not %NULL
 *
 * Returns: %TRUE if @key is valid
 **/
bool
ephy_autofill_utils_is_valid_element_key (const char *key)
{
  size_t length;

  if (key == NULL)
    return FALSE;

  length = strlen (key);
  return length > 0 && length <= MAX_ELEMENT_KEY_LENGTH;
}

/**
 * ephy_autofill_utils_is_empty_value:
 * @value: (allow-none): any value
 *
 * Returns: %TRUE if @value equals %NULL or has a length of %0
 **/
bool
ephy_autofill_utils_is_empty_value (const char *value)
{
  return (value == NULL || strlen (value) == 0);
}

/**
 * ephy_autofill_utils_get_element_label:
 * @element: a #WebKitDOMElement
 *
 * Gets the label element corresponding to @element and returns its text
 *
 * Returns: (transfer full): text value of @element label or %NULL
 **/
char *
ephy_autofill_utils_get_element_label (WebKitDOMElement *element)
{
  char *id = webkit_dom_element_get_id (element);
  char *label;

  if (ephy_autofill_utils_is_empty_value (id))
    label = NULL;
  else {
    char *label_selector = g_strconcat ("label[for=", id, "]", NULL);
    WebKitDOMDocument *document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
    WebKitDOMElement* label_element = webkit_dom_document_query_selector (document,
                                                                          label_selector,
                                                                          NULL);

    if (label_element == NULL)
      label = NULL;
    else
      label = webkit_dom_html_element_get_inner_text (WEBKIT_DOM_HTML_ELEMENT (label_element));

    g_free (label_selector);
  }

  g_free (id);
  return label;
}

/**
 * ephy_autofill_utils_is_https:
 * @node: a #WebKitDOMNode
 *
 * Returns: whether the page associated with @node is using HTTPS
 **/
bool
ephy_autofill_utils_is_https (WebKitDOMNode *node)
{
  WebKitDOMDocument *dom_document = webkit_dom_node_get_owner_document (node);
  char *url = webkit_dom_document_get_url (dom_document);
  SoupURI *uri = soup_uri_new (url);
  const char *scheme = soup_uri_get_scheme (uri);

  soup_uri_free (uri);
  g_free (url);

  return scheme == SOUP_URI_SCHEME_HTTPS;
}

/**
 * ephy_autofill_utils_dispatch_event:
 * @node: a #WebKitDOMNode
 * @event_type: the event type
 * @event_name: the event name
 * @bubbles: whether the event bubbles up through the DOM or not
 * @cancellable: whether the event is cancelable
 *
 * Provides a more convenient way to dispatch DOM events.
 **/
void
ephy_autofill_utils_dispatch_event (WebKitDOMNode *node,
                                    const char *event_type,
                                    const char *event_name,
                                    bool bubbles,
                                    bool cancellable)
{
  WebKitDOMDocument *document = webkit_dom_node_get_owner_document (node);
  WebKitDOMEvent *event = webkit_dom_document_create_event (document, event_type, NULL);

  webkit_dom_event_init_event (event, event_name, bubbles, cancellable);
  webkit_dom_event_target_dispatch_event (WEBKIT_DOM_EVENT_TARGET (node), event, NULL);

  g_object_unref (event);
}

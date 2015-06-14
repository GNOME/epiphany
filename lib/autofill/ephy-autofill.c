/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2019 Abdullah Alansari
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
#include "ephy-autofill.h"

#include "ephy-autofill-field.h"
#include "ephy-autofill-form-element.h"
#include "ephy-autofill-input-element.h"
#include "ephy-autofill-storage.h"
#include "ephy-autofill-utils.h"

/*
 * This file is the only file needed to be included to use Autofill
 * functionality. All other files (ephy-autofill-*.{c,h}) are used
 * to group common functionality together, simplify code and
 * break cycles in dependencies.
 *
 * Long story short, unless you are fixing/improving Autofill
 * You shouldn't concern yourself with other Autofill files
 */

static char *
get_element_nth_of_type (WebKitDOMElement *element)
{
  char *name = webkit_dom_node_get_node_name (WEBKIT_DOM_NODE (element));
  char *name_downcased = g_ascii_strdown (name, -1);
  char *nth_of_type;
  char *nth;

  WebKitDOMNode *sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
  int prev_siblings = 0;

  while (sibling)
  {
    char *sibling_name = webkit_dom_node_get_node_name (sibling);

    if (g_ascii_strcasecmp (sibling_name, name) == 0)
      prev_siblings++;

    sibling = webkit_dom_node_get_previous_sibling (sibling);
    g_free (sibling_name);
  }

  nth = g_strdup_printf ("%d", prev_siblings + 1);
  nth_of_type = g_strconcat (name_downcased, ":nth-of-type(", nth, ")", NULL);

  g_free (name_downcased);
  g_free (name);
  g_free (nth);

  return nth_of_type;
}

static GList *
get_element_css_path (WebKitDOMElement *element)
{
  WebKitDOMElement *el = element;
  GList *path = NULL;

  while (el && WEBKIT_DOM_IS_ELEMENT (el))
  {
    char *name = webkit_dom_node_get_node_name (WEBKIT_DOM_NODE (el));
    char *id = webkit_dom_element_get_id (el);

    if (g_ascii_strcasecmp (name, "body") == 0) {
      g_free (name);
      g_free (id);
      break;
    }
    else if (!ephy_autofill_utils_is_empty_value (id)) {
      char *selector = g_strconcat ("#", id, NULL);
      path = g_list_prepend (path, selector);
      break;
    }
    else {
      char *selector = get_element_nth_of_type (el);
      path = g_list_prepend (path, selector);
      el = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (el));
    }

    g_free (name);
    g_free (id);
  }

  return path;
}

static char *
join_css_path (GList *path)
{
  char *selector = g_strdup ("");
  GList *l;

  for (l = path; l; l = l->next)
  {
    char *old_selector = selector;
    char *x = (char *)l->data;

    if (l->next)
      selector = g_strconcat (old_selector, x, " > ", NULL);
    else
      selector = g_strconcat (old_selector, x, NULL);

    g_free (old_selector);
  }

  return selector;
}

static char *
get_element_css_selector (WebKitDOMElement *element)
{
  GList *css_path = get_element_css_path (element);
  char *css_selector = join_css_path (css_path);

  g_list_free_full (css_path, g_free);
  return css_selector;
}

static void
get_absolute_position_for_element (WebKitDOMElement *element,
                                   double *x,
                                   double *y)
{
  WebKitDOMElement *parent = webkit_dom_element_get_offset_parent (element);
  double offset_top = webkit_dom_element_get_offset_top (element);
  double offset_left = webkit_dom_element_get_offset_left (element);
  double parent_x, parent_y;

  *x = offset_left;
  *y = offset_top;

  if (parent) {
    get_absolute_position_for_element (parent, &parent_x, &parent_y);
    *x += parent_x;
    *y += parent_y;
  }
}

typedef struct
{
  const char *ephy_web_extension_object_path;
  const char *ephy_web_extension_interface;

  GDBusConnection *dbus_connection;
  unsigned long page_id;
} Message;

static bool
input_element_mouseup_cb (WebKitDOMEventTarget *event_source,
                          WebKitDOMMouseEvent *dom_event,
                          gpointer user_data);

static bool
input_element_mouseup_cb (WebKitDOMEventTarget *event_source,
                          WebKitDOMMouseEvent *dom_event,
                          gpointer user_data)
{
  Message *message = user_data;
  GDBusConnection *dbus_connection = message->dbus_connection;
  unsigned long page_id = message->page_id;
  WebKitDOMEventTarget *event_target = webkit_dom_event_get_target (WEBKIT_DOM_EVENT (dom_event));

  webkit_dom_event_target_remove_event_listener (event_target, "mouseup", G_CALLBACK (input_element_mouseup_cb), FALSE);

  if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (event_target)) {
    WebKitDOMDocument *dom_document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (event_target));
    WebKitDOMDOMWindow *dom_window = webkit_dom_document_get_default_view (dom_document);
    WebKitDOMHTMLInputElement *input_element = WEBKIT_DOM_HTML_INPUT_ELEMENT (event_target);
    WebKitDOMHTMLFormElement *form = webkit_dom_html_input_element_get_form (input_element);

    double element_height = webkit_dom_element_get_offset_height (WEBKIT_DOM_ELEMENT (event_target));
    double element_width = webkit_dom_element_get_offset_width (WEBKIT_DOM_ELEMENT (event_target));
    double element_x, element_y;
    long scroll_x, scroll_y;
    long x, y;

    GError *error = NULL;
    EphyAutofillField form_field = ephy_autofill_form_element_get_field (form, TRUE, TRUE);
    char *selector = get_element_css_selector (WEBKIT_DOM_ELEMENT (event_target));
    bool is_fillable_element = ephy_autofill_input_element_get_field (input_element, TRUE, TRUE) != EPHY_AUTOFILL_FIELD_UNKNOWN;
    bool has_personal_fields = (form_field & EPHY_AUTOFILL_FIELD_PERSONAL) != 0;
    bool has_card_fields = (form_field & EPHY_AUTOFILL_FIELD_CARD) != 0;

    g_object_get (dom_window,
                  "scroll-x", &scroll_x,
                  "scroll-y", &scroll_y,
                  NULL);

    get_absolute_position_for_element (WEBKIT_DOM_ELEMENT (event_target), &element_x, &element_y);

    x = element_x - scroll_x;
    y = element_y - scroll_y;

    if (ephy_autofill_utils_is_empty_value (webkit_dom_html_input_element_get_value (input_element)) &&
        (is_fillable_element || has_personal_fields || has_card_fields)) {
      g_dbus_connection_emit_signal (dbus_connection,
                                     NULL,
                                     message->ephy_web_extension_object_path,
                                     message->ephy_web_extension_interface,
                                     "Autofill",
                                     g_variant_new ("(tsbbbtttt)", page_id, selector, is_fillable_element, has_personal_fields, has_card_fields,
                                                    (unsigned long)x, (unsigned long)y, (unsigned long)element_width, (unsigned long)element_height),
                                     &error);
    }

    if (error) {
      g_warning ("Error emitting signal Autofill: %s\n", error->message);
      g_error_free (error);
    }

    g_object_unref (dom_window);
    g_free (selector);
  }

  return FALSE;
}

static bool
document_mousedown_cb (WebKitDOMEventTarget *event_source,
                       WebKitDOMMouseEvent *dom_event,
                       gpointer user_data)
{
  Message *message = user_data;
  WebKitDOMEventTarget *event_target = webkit_dom_event_get_target (WEBKIT_DOM_EVENT (dom_event));
  WebKitDOMDocument *dom_document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (event_target));
  WebKitDOMElement *focused_element = webkit_dom_document_get_active_element (dom_document);
  bool is_element_focused = (unsigned long)event_target == (unsigned long)focused_element;

  if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (event_target) && is_element_focused) {
    webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (event_target), "mouseup",
                                                G_CALLBACK (input_element_mouseup_cb), FALSE,
                                                message);
  }

  return FALSE;
}

static void
web_page_destroyed_cb (gpointer data,
                       GObject *where_the_object_was)
{
  Message *message = data;
  g_slice_free (Message, message);
}

void
ephy_autofill_web_page_document_loaded (WebKitWebPage *web_page,
                                        GDBusConnection *dbus_connection,
                                        const char *ephy_web_extension_object_path,
                                        const char *ephy_web_extension_interface)
{
  WebKitDOMDocument *document = webkit_web_page_get_dom_document (web_page);
  Message *message = g_slice_new (Message);

  message->ephy_web_extension_object_path = ephy_web_extension_object_path;
  message->ephy_web_extension_interface = ephy_web_extension_interface;
  message->dbus_connection = dbus_connection;
  message->page_id = webkit_web_page_get_id (web_page);

  g_object_weak_ref (G_OBJECT (web_page), web_page_destroyed_cb, message);

  // NOTE: We can't use "click" event here, since we want to only work on focused elements that are clicked,
  // and when the element receives the "click" event it's always focused.
  // In addition, we can't use "focus" and "click" events, since "focus" is always followed by a "click" event.
  webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (document), "mousedown",
                                              G_CALLBACK (document_mousedown_cb), TRUE,
                                              message);
}

void
ephy_autofill_fill (WebKitWebPage *web_page,
                    const char *selector,
                    EphyAutofillFillChoice fill_choice)
{
  bool is_valid_fill_choice = (fill_choice |
                               EPHY_AUTOFILL_FILL_CHOICE_FORM_PERSONAL |
                               EPHY_AUTOFILL_FILL_CHOICE_FORM_ALL |
                               EPHY_AUTOFILL_FILL_CHOICE_ELEMENT) != 0;
  bool is_valid_selector = !ephy_autofill_utils_is_empty_value (selector);

  WebKitDOMHTMLInputElement *input_element;
  WebKitDOMHTMLFormElement *form;
  WebKitDOMDocument *document;
  WebKitDOMElement *element;

  if (!is_valid_fill_choice || !is_valid_selector)
    return;

  document = webkit_web_page_get_dom_document (web_page);
  element = webkit_dom_document_query_selector (document, selector, NULL);

  if (!WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (element))
    return;

  input_element = WEBKIT_DOM_HTML_INPUT_ELEMENT (element);
  form = webkit_dom_html_input_element_get_form (input_element);

  switch (fill_choice)
  {
    case EPHY_AUTOFILL_FILL_CHOICE_FORM_PERSONAL:
      ephy_autofill_form_element_fill (form, TRUE, FALSE);
      break;
    case EPHY_AUTOFILL_FILL_CHOICE_FORM_ALL:
      ephy_autofill_form_element_fill (form, TRUE, TRUE);
      break;
    case EPHY_AUTOFILL_FILL_CHOICE_ELEMENT:
      ephy_autofill_input_element_fill (input_element, TRUE, TRUE);
    default: return;
  }
}

/**
 * ephy_autofill_delete:
 * @field: an #EphyAutofillField
 * @callback: a #GAsyncReadyCallback
 * @user_data: a #gpointer
 *
 * A simple function that calls ephy_autofill_storage_delete()
 **/
void
ephy_autofill_delete (EphyAutofillField field,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
  ephy_autofill_storage_delete (field, callback, user_data);
}

/**
 * ephy_autofill_get:
 * @field: an #EphyAutofillField
 * @callback: a #GAsyncReadyCallback
 * @user_data: a #gpointer
 *
 * A simple function that calls ephy_autofill_storage_get()
 **/
void
ephy_autofill_get (EphyAutofillField field,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
  ephy_autofill_storage_get (field, callback, user_data);
}

/**
 * ephy_autofill_set:
 * @field: an #EphyAutofillField
 * @value: (allow-none): value
 * @callback: a #GAsyncReadyCallback
 * @user_data: a #gpointer
 *
 * A simple function that calls ephy_autofill_storage_set()
 **/
void
ephy_autofill_set (EphyAutofillField field,
                   const char *value,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
  ephy_autofill_storage_set (field, value, callback, user_data);
}

/**
 * ephy_autofill_delete_finish:
 * @res: a #GAsyncResult
 *
 * Returns: ephy_autofill_storage_delete_finish()
 **/
bool
ephy_autofill_delete_finish (GAsyncResult *res)
{
  return ephy_autofill_storage_delete_finish (res);
}

/**
 * ephy_autofill_get_finish:
 * @res: a #GAsyncResult
 *
 * Returns: (transfer full): ephy_autofill_storage_get_finish()
 **/
char *
ephy_autofill_get_finish (GAsyncResult *res)
{
  return ephy_autofill_storage_get_finish (res);
}

/**
 * ephy_autofill_set_finish:
 * @res: a #GAsyncResult
 *
 * Returns: ephy_autofill_storage_set_finish()
 **/
bool
ephy_autofill_set_finish (GAsyncResult *res)
{
  return ephy_autofill_storage_set_finish (res);
}

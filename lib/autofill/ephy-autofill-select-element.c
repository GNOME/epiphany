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
#include "ephy-autofill-select-element.h"

#include "ephy-autofill-matchers.h"
#include "ephy-autofill-storage.h"
#include "ephy-autofill-utils.h"

#include <glib-object.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#define MAX_SELECT_ELEMENT_LENGTH 256

static EphyAutofillField
get_credit_card_field (WebKitDOMHTMLSelectElement *select_element,
                       const char *element_key)
{
  if (ephy_autofill_matchers_is_card_expdate_month (element_key) &&
      ephy_autofill_matchers_is_card_expdate_year (element_key)) {

    if (ephy_autofill_matchers_is_month (element_key))
      return EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH;
    if (ephy_autofill_matchers_is_year (element_key))
      return EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR;

    return EPHY_AUTOFILL_FIELD_UNKNOWN;
  }
  if (ephy_autofill_matchers_is_card_expdate_month (element_key))
    return EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH;
  if (ephy_autofill_matchers_is_card_expdate_year (element_key))
    return EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR;
  if (ephy_autofill_matchers_is_card_type (element_key))
    return EPHY_AUTOFILL_FIELD_CARD_TYPE;

  return EPHY_AUTOFILL_FIELD_UNKNOWN;
}

static EphyAutofillField
get_personal_field (WebKitDOMHTMLSelectElement *select_element,
                    const char *element_key)
{
  if (ephy_autofill_matchers_is_country (element_key))
    return EPHY_AUTOFILL_FIELD_COUNTRY_NAME;
  if (ephy_autofill_matchers_is_state (element_key))
    return EPHY_AUTOFILL_FIELD_STATE;
  if (ephy_autofill_matchers_is_city (element_key))
    return EPHY_AUTOFILL_FIELD_CITY;

  return EPHY_AUTOFILL_FIELD_UNKNOWN;
}

static EphyAutofillField
get_field_helper (WebKitDOMHTMLSelectElement *select_element,
                  const char *element_key,
                  bool fill_personal_info,
                  bool fill_credit_card_info)
{
  EphyAutofillField field = EPHY_AUTOFILL_FIELD_UNKNOWN;

  if (!ephy_autofill_utils_is_valid_element_key (element_key))
    return field;
  if (fill_personal_info)
    field = get_personal_field (select_element, element_key);
  if (fill_credit_card_info && field == EPHY_AUTOFILL_FIELD_UNKNOWN)
    field = get_credit_card_field (select_element, element_key);

  return field;
}

/**
 * ephy_autofill_select_element_get_field:
 * @select_element: a #WebKitDOMHTMLSelectElement
 * @fill_personal_info: whether personal info should be used
 * @fill_credit_card_info: whether credit card info should be used
 *
 * Returns: type of @select_element field (e.g: Year, Country)
 **/
EphyAutofillField
ephy_autofill_select_element_get_field (WebKitDOMHTMLSelectElement *select_element,
                                        bool fill_personal_info,
                                        bool fill_card_info)
{
  EphyAutofillField field;
  char *label, *name, *id;
  bool is_https;

  if (!ephy_autofill_utils_is_element_visible (WEBKIT_DOM_ELEMENT (select_element)))
    return EPHY_AUTOFILL_FIELD_UNKNOWN;

  is_https = ephy_autofill_utils_is_https (WEBKIT_DOM_NODE (select_element));
  label = ephy_autofill_utils_get_element_label (WEBKIT_DOM_ELEMENT (select_element));
  name = webkit_dom_html_select_element_get_name (select_element);
  id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (select_element));

  field = get_field_helper (select_element, name, fill_personal_info, fill_card_info && is_https);

  if (field == EPHY_AUTOFILL_FIELD_UNKNOWN)
    field = get_field_helper (select_element, id, fill_personal_info, fill_card_info && is_https);

  if (field == EPHY_AUTOFILL_FIELD_UNKNOWN)
    field = get_field_helper (select_element, label, fill_personal_info, fill_card_info && is_https);

  g_free (label);
  g_free (name);
  g_free (id);

  return field;
}

static long
get_named_index (WebKitDOMHTMLSelectElement *select_element,
                 const char *name)
{
  long select_element_length = webkit_dom_html_select_element_get_length (select_element);
  long named_item_index;
  long i;
  bool has_named_item;

  if (select_element_length > MAX_SELECT_ELEMENT_LENGTH || name == NULL)
    return -1;

  has_named_item = FALSE;
  named_item_index = -1;

  for (i = 0; i < select_element_length && !has_named_item; i++) {
    WebKitDOMNode *option_node = webkit_dom_html_select_element_item (select_element, i);
    WebKitDOMHTMLOptionElement *option_element = WEBKIT_DOM_HTML_OPTION_ELEMENT (option_node);
    char *label = webkit_dom_html_option_element_get_label (option_element);
    char *value = webkit_dom_html_option_element_get_value (option_element);
    char *text = webkit_dom_html_option_element_get_text (option_element);

    if ((label != NULL && g_ascii_strcasecmp (label, name) == 0) ||
        (value != NULL && g_ascii_strcasecmp (value, name) == 0) ||
        (text != NULL && g_ascii_strcasecmp (text, name) == 0)) {
      named_item_index = i;
      has_named_item = TRUE;
    }

    g_free (label);
    g_free (value);
    g_free (text);
  }

  return named_item_index;
}

static void
fill_cb (GObject *source_object,
         GAsyncResult *res,
         gpointer user_data)
{
  WebKitDOMHTMLSelectElement *select_element = user_data;
  char *autofill_value = ephy_autofill_storage_get_finish (res);
  long index = get_named_index (select_element, autofill_value);

  if (index >= 0) {
    webkit_dom_html_select_element_set_selected_index (select_element, index);
    ephy_autofill_utils_dispatch_event (WEBKIT_DOM_NODE (select_element),
                                        "HTMLEvents", "change",
                                        FALSE, TRUE);
  }

  g_free (autofill_value);
}

/**
 * ephy_autofill_select_element_fill:
 * @select_element: a #WebKitDOMHTMLInputElement
 * @fill_personal_info: whether personal info should be filled
 * @fill_credit_card_info: whether credit card info should be filled
 *
 * Fills @select_element asynchronously iff:
 * - the type of the value to be filled is recognized
 * - @select_element does not have too many option elements
 **/
void
ephy_autofill_select_element_fill (WebKitDOMHTMLSelectElement *select_element,
                                   bool fill_personal_info,
                                   bool fill_credit_card_info)
{
  EphyAutofillField field = ephy_autofill_select_element_get_field (select_element, fill_personal_info, fill_credit_card_info);

  if (field != EPHY_AUTOFILL_FIELD_UNKNOWN)
    ephy_autofill_storage_get (field, fill_cb, select_element);
}

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
#include "ephy-autofill-input-element.h"

#include "ephy-autofill-matchers.h"
#include "ephy-autofill-storage.h"
#include "ephy-autofill-utils.h"

#include <glib-object.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

static bool
is_valid_input_type (const char *type)
{
  return ephy_autofill_utils_is_empty_value (type) ||
         g_ascii_strcasecmp (type, "search") == 0 ||
         g_ascii_strcasecmp (type, "email") == 0 ||
         g_ascii_strcasecmp (type, "text") == 0 ||
         g_ascii_strcasecmp (type, "tel") == 0;
}

static EphyAutofillField
get_personal_field (const char *element_key)
{
  if (ephy_autofill_matchers_is_fullname (element_key))
    return EPHY_AUTOFILL_FIELD_FULLNAME;
  if (ephy_autofill_matchers_is_firstname (element_key))
    return EPHY_AUTOFILL_FIELD_FIRSTNAME;
  if (ephy_autofill_matchers_is_lastname (element_key))
    return EPHY_AUTOFILL_FIELD_LASTNAME;
  if (ephy_autofill_matchers_is_username (element_key))
    return EPHY_AUTOFILL_FIELD_USERNAME;
  if (ephy_autofill_matchers_is_email (element_key))
    return EPHY_AUTOFILL_FIELD_EMAIL;
  if (ephy_autofill_matchers_is_phone (element_key))
    return EPHY_AUTOFILL_FIELD_PHONE;
  if (ephy_autofill_matchers_is_organization (element_key))
    return EPHY_AUTOFILL_FIELD_ORGANIZATION;
  if (ephy_autofill_matchers_is_postal_code (element_key))
    return EPHY_AUTOFILL_FIELD_POSTAL_CODE;
  if (ephy_autofill_matchers_is_country (element_key))
    return EPHY_AUTOFILL_FIELD_COUNTRY_NAME;
  if (ephy_autofill_matchers_is_state (element_key))
    return EPHY_AUTOFILL_FIELD_STATE;
  if (ephy_autofill_matchers_is_city (element_key))
    return EPHY_AUTOFILL_FIELD_CITY;
  if (ephy_autofill_matchers_is_street_address (element_key))
    return EPHY_AUTOFILL_FIELD_STREET_ADDRESS;

  return EPHY_AUTOFILL_FIELD_UNKNOWN;
}

static EphyAutofillField
get_credit_card_field (WebKitDOMHTMLInputElement *input_element,
                       const char *element_key)
{
  if (ephy_autofill_matchers_is_card_expdate_month (element_key) &&
      ephy_autofill_matchers_is_card_expdate_year (element_key)) {

    if (ephy_autofill_matchers_is_month (element_key))
      return EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH;
    if (ephy_autofill_matchers_is_year (element_key))
      return EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR;

    return EPHY_AUTOFILL_FIELD_CARD_EXPDATE;
  }
  if (ephy_autofill_matchers_is_card_expdate_month (element_key))
    return EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH;
  if (ephy_autofill_matchers_is_card_expdate_year (element_key))
    return EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR;
  if (ephy_autofill_matchers_is_card_expdate (element_key))
    return EPHY_AUTOFILL_FIELD_CARD_EXPDATE;
  if (ephy_autofill_matchers_is_name_on_card (element_key))
    return EPHY_AUTOFILL_FIELD_NAME_ON_CARD;
  if (ephy_autofill_matchers_is_card_number (element_key))
    return EPHY_AUTOFILL_FIELD_CARD_NUMBER;
  if (ephy_autofill_matchers_is_card_type (element_key))
    return EPHY_AUTOFILL_FIELD_CARD_TYPE;

  return EPHY_AUTOFILL_FIELD_UNKNOWN;
}

static EphyAutofillField
get_field_helper (WebKitDOMHTMLInputElement *input_element,
                  const char *element_key,
                  bool fill_personal_info,
                  bool fill_credit_card_info)
{
  EphyAutofillField field = EPHY_AUTOFILL_FIELD_UNKNOWN;

  if (!ephy_autofill_utils_is_valid_element_key (element_key))
    field = EPHY_AUTOFILL_FIELD_UNKNOWN;
  else {
    if (fill_personal_info)
      field = get_personal_field (element_key);

    if (fill_credit_card_info && field == EPHY_AUTOFILL_FIELD_UNKNOWN)
      field = get_credit_card_field (input_element, element_key);
  }

  return field;
}

static EphyAutofillField
get_field_by_type (const char *type)
{
  if (ephy_autofill_utils_is_empty_value (type))
    return EPHY_AUTOFILL_FIELD_UNKNOWN;
  if (g_ascii_strcasecmp (type, "email") == 0)
    return EPHY_AUTOFILL_FIELD_EMAIL;
  if (g_ascii_strcasecmp (type, "tel") == 0)
    return EPHY_AUTOFILL_FIELD_PHONE;

  return EPHY_AUTOFILL_FIELD_UNKNOWN;
}

/**
 * ephy_autofill_input_element_get_field:
 * @input_element: a #WebKitDOMHTMLInputElement
 * @fill_personal_info: whether personal info should be used
 * @fill_credit_card_info: whether credit card info should be used
 *
 * Returns: type of @input_element field (e.g: First name, Email)
 **/
EphyAutofillField
ephy_autofill_input_element_get_field (WebKitDOMHTMLInputElement *input_element,
                                       bool fill_personal_info,
                                       bool fill_credit_card_info)
{
  char *type = webkit_dom_html_input_element_get_input_type (input_element);
  EphyAutofillField field = EPHY_AUTOFILL_FIELD_UNKNOWN;

  if (is_valid_input_type (type) && ephy_autofill_utils_is_element_visible (WEBKIT_DOM_ELEMENT (input_element))) {
    bool is_https = ephy_autofill_utils_is_https (WEBKIT_DOM_NODE (input_element));
    char *label = ephy_autofill_utils_get_element_label (WEBKIT_DOM_ELEMENT (input_element));
    char *name = webkit_dom_html_input_element_get_name (input_element);
    char *id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (input_element));

    if (field == EPHY_AUTOFILL_FIELD_UNKNOWN)
      field = get_field_helper (input_element, name, fill_personal_info, fill_credit_card_info && is_https);

    if (field == EPHY_AUTOFILL_FIELD_UNKNOWN)
      field = get_field_helper (input_element, id, fill_personal_info, fill_credit_card_info && is_https);

    if (field == EPHY_AUTOFILL_FIELD_UNKNOWN)
      field = get_field_helper (input_element, label, fill_personal_info, fill_credit_card_info && is_https);

    if (field == EPHY_AUTOFILL_FIELD_UNKNOWN && fill_personal_info)
      field = get_field_by_type (type);

    g_free (label);
    g_free (name);
    g_free (id);
  }

  g_free (type);
  return field;
}

static void
fill_input_element_cb (GObject *source_object,
                       GAsyncResult *res,
                       gpointer user_data)
{
  WebKitDOMHTMLInputElement *input_element = user_data;
  char *autofill_value = ephy_autofill_storage_get_finish (res);

  if (!ephy_autofill_utils_is_empty_value(autofill_value)) {
    webkit_dom_html_input_element_set_value (input_element, autofill_value);
    ephy_autofill_utils_dispatch_event (WEBKIT_DOM_NODE (input_element),
                                        "HTMLEvents", "change",
                                        FALSE, TRUE);

    webkit_dom_html_input_element_set_auto_filled (input_element, TRUE);
  }

  g_free (autofill_value);
}

/**
 * ephy_autofill_input_element_fill:
 * @input_element: a #WebKitDOMHTMLInputElement
 * @fill_personal_info: whether personal info should be filled
 * @fill_credit_card_info: whether credit card info should be filled
 *
 * Fills @input_element asynchronously iff:
 * - @input_element type attribute is valid (e.g: text, email, %NULL, %"")
 * - @input_element keys (e.g: name, placeholder) are not too long
 * - The type of the value to be filled is recognized
 * - @input_element value is empty
 **/
void
ephy_autofill_input_element_fill (WebKitDOMHTMLInputElement *input_element,
                                  bool fill_personal_info,
                                  bool fill_credit_card_info)
{
  char *value = webkit_dom_html_input_element_get_value (WEBKIT_DOM_HTML_INPUT_ELEMENT (input_element));

  if (ephy_autofill_utils_is_empty_value (value)) {
    EphyAutofillField field = ephy_autofill_input_element_get_field (input_element, fill_personal_info, fill_credit_card_info);

    if (field != EPHY_AUTOFILL_FIELD_UNKNOWN)
      ephy_autofill_storage_get (field, fill_input_element_cb, input_element);
  }

  g_free (value);
}

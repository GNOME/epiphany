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
#include "ephy-autofill-form-element.h"

#include "ephy-autofill-input-element.h"
#include "ephy-autofill-select-element.h"

#include <glib-object.h>

#define MAX_FORM_LENGTH 128

static bool
is_valid_form (WebKitDOMHTMLFormElement *form)
{
  return form != NULL && webkit_dom_html_form_element_get_length (form) < MAX_FORM_LENGTH;
}

/*
 * ephy_autofill_form_element_fill:
 * @form: (allow-none): a #WebKitDOMHTMLFormElement
 * @fill_personal_info:
 * @fill_card_info:
 *  This is different from GSettings preferences.
 *  For example it is now used to indicate that the user accepted to
 *  fill credit card info from the Autofill popup.
 *
 * Goes through all input and select elements in @form and fills them
 * using the appropriate function:
 * - Select element: ephy_autofill_select_element_fill()
 * - Input element: ephy_autofill_input_element_fill()
 */
void
ephy_autofill_form_element_fill (WebKitDOMHTMLFormElement *form,
                                 bool fill_personal_info,
                                 bool fill_card_info)
{
  WebKitDOMHTMLCollection *elements;
  long length;
  long i;

  if (!is_valid_form (form))
    return;

  elements = webkit_dom_html_form_element_get_elements (form);
  length = webkit_dom_html_collection_get_length (elements);

  for (i = 0; i < length; i++) {
    WebKitDOMNode *element = webkit_dom_html_collection_item (elements, i);

    if (WEBKIT_DOM_IS_HTML_SELECT_ELEMENT (element))
      ephy_autofill_select_element_fill (WEBKIT_DOM_HTML_SELECT_ELEMENT (element), fill_personal_info, fill_card_info);
    else if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (element))
      ephy_autofill_input_element_fill (WEBKIT_DOM_HTML_INPUT_ELEMENT (element), fill_personal_info, fill_card_info);
  }

  g_object_unref (elements);
}

/**
 * ephy_autofill_form_element_get_field:
 * @form: (allow-none): a #WebKitDOMHTMLFormElement
 * @personal_info_enabled:
 * @card_info_enabled:
 *
 * @form is considered valid if:
 *  - It is not %NULL
 *  - Has a reasonable number of items, for the moment %128.
 *    The limit is used to guarantee good performance
 *    and insure system availability.
 *
 * Returns: a field representing the @form
 **/
EphyAutofillField
ephy_autofill_form_element_get_field (WebKitDOMHTMLFormElement *form,
                                      bool personal_info_enabled,
                                      bool card_info_enabled)
{
  EphyAutofillField form_fields = EPHY_AUTOFILL_FIELD_UNKNOWN;
  WebKitDOMHTMLCollection *elements;
  long collection_length;
  long i;

  if (!is_valid_form (form))
    return EPHY_AUTOFILL_FIELD_UNKNOWN;

  elements = webkit_dom_html_form_element_get_elements (form);
  collection_length = webkit_dom_html_collection_get_length (elements);

  for (i = 0; i < collection_length; i++) {
    WebKitDOMNode *element = webkit_dom_html_collection_item (elements, i);

    if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (element)) {
      form_fields |= ephy_autofill_input_element_get_field (WEBKIT_DOM_HTML_INPUT_ELEMENT (element),
                                                            personal_info_enabled, card_info_enabled);
    }
    else if (WEBKIT_DOM_IS_HTML_SELECT_ELEMENT (element)) {
      form_fields |= ephy_autofill_select_element_get_field (WEBKIT_DOM_HTML_SELECT_ELEMENT (element),
                                                             personal_info_enabled, card_info_enabled);
    }
  }

  g_object_unref (elements);
  return form_fields;
}

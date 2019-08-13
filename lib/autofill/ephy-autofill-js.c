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
#include "ephy-autofill-js.h"

#include "ephy-autofill-storage.h"

static void
get_field_value_cb (GObject *source_object,
                    GAsyncResult *res,
                    gpointer user_data)
{
  JSCValue *cb = jsc_weak_value_get_value (user_data);
  char *autofill_value = ephy_autofill_storage_get_finish (res);
  g_autoptr(JSCValue) ret = NULL;

  if (cb == NULL) {
    g_free (autofill_value);
    return;
  }

  ret = jsc_value_function_call (cb,
                                 G_TYPE_STRING,
                                 autofill_value,
                                 G_TYPE_NONE);

  g_free (autofill_value);
}

void
ephy_autofill_js_change_value (JSCValue *js_input_element, const char *value)
{
  WebKitDOMNode *node = webkit_dom_node_for_js_value (js_input_element);
  WebKitDOMElement *element = WEBKIT_DOM_ELEMENT (node);

  webkit_dom_element_html_input_element_set_auto_filled (element, TRUE);
  webkit_dom_element_html_input_element_set_editing_value (element, value);
}

void ephy_autofill_js_emit_signal (const char       *object_path,
                                   const char       *interface,
                                   GDBusConnection  *dbus_connection,
                                   unsigned long     page_id,
                                   EphyAutofillField field,
                                   char             *selector,
                                   bool              is_fillable_element,
                                   bool              has_personal_fields,
                                   bool              has_card_fields,
                                   double            x,
                                   double            y,
                                   double            element_width,
                                   double            element_height)
{
  GError *error = NULL;
  GVariant *variant = g_variant_new ("(tsbbbtttt)",
                                     page_id,
                                     selector,
                                     is_fillable_element,
                                     has_personal_fields,
                                     has_card_fields,
                                     (unsigned long)x,
                                     (unsigned long)y,
                                     (unsigned long)element_width,
                                     (unsigned long)element_height);

  g_dbus_connection_emit_signal (dbus_connection,
                                  NULL,
                                  object_path,
                                  interface,
                                  "Autofill",
                                  variant,
                                  &error);

  if (error) {
    g_warning ("Error emitting signal Autofill: %s\n", error->message);
    g_error_free (error);
  }
}

void ephy_autofill_js_get_field_value (EphyAutofillField field, JSCValue *cb) {
  ephy_autofill_storage_get (field,
                             get_field_value_cb,
                             jsc_weak_value_new (cb));
}

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
 */

#include "config.h"
#include "ephy-web-extension.h"

#include <gio/gio.h>
#include <webkit2/webkit-web-extension.h>

static const char introspection_xml[] =
  "<node>"
  " <interface name='org.gnome.Epiphany.WebExtension'>"
  "  <method name='HasModifiedForms'>"
  "   <arg type='t' name='page_id' direction='in'/>"
  "   <arg type='b' name='has_modified_forms' direction='out'/>"
  "  </method>"
  " </interface>"
  "</node>";

#define MIN_INPUT_LENGTH 50

static gboolean
ephy_web_extension_page_has_modified_forms (WebKitWebPage *web_page)
{
  WebKitDOMHTMLCollection *forms = NULL;
  WebKitDOMDocument *document = NULL;
  gulong forms_n;
  int i;

  document = webkit_web_page_get_dom_document (web_page);
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

          if (length > MIN_INPUT_LENGTH)
            return TRUE;
        }
    }
  }

  return FALSE;
}

static void
handle_method_call (GDBusConnection *connection,
                    const char *sender,
                    const char *object_path,
                    const char *interface_name,
                    const char *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer user_data)
{
  WebKitWebExtension *web_extension = WEBKIT_WEB_EXTENSION (user_data);
  WebKitWebPage *web_page;
  guint64 page_id;

  if (g_strcmp0 (interface_name, EPHY_WEB_EXTENSION_INTERFACE) != 0)
    return;

  g_variant_get(parameters, "(t)", &page_id);
  web_page = webkit_web_extension_get_page (web_extension, page_id);
  if (!web_page) {
    g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                           "Invalid page ID: %"G_GUINT64_FORMAT, page_id);
    return;
  }

  if (g_strcmp0 (method_name, "HasModifiedForms") == 0) {
    gboolean has_modifed_forms;

    has_modifed_forms = ephy_web_extension_page_has_modified_forms (web_page);
    g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", has_modifed_forms));
  }
}

static const GDBusInterfaceVTable interface_vtable = {
  handle_method_call,
  NULL,
  NULL
};

static void
bus_acquired_cb (GDBusConnection *connection,
                 const char *name,
                 gpointer user_data)
{
  guint registration_id;
  GError *error = NULL;
  static GDBusNodeInfo *introspection_data = NULL;
  if (!introspection_data)
    introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

  registration_id = g_dbus_connection_register_object (connection,
                                                       EPHY_WEB_EXTENSION_OBJECT_PATH,
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       g_object_ref (user_data),
                                                       (GDestroyNotify)g_object_unref,
                                                       &error);
  if (!registration_id) {
    g_warning ("Failed to register object: %s\n", error->message);
    g_error_free (error);
  }
}

G_MODULE_EXPORT void
webkit_web_extension_initialize (WebKitWebExtension *extension)
{
  char *service_name;

  service_name = g_strdup_printf ("%s-%s", EPHY_WEB_EXTENSION_SERVICE_NAME, g_getenv ("EPHY_WEB_EXTENSION_ID"));
  g_bus_own_name (G_BUS_TYPE_SESSION,
                  service_name,
                  G_BUS_NAME_OWNER_FLAGS_NONE,
                  bus_acquired_cb,
                  NULL, NULL,
                  g_object_ref (extension),
                  (GDestroyNotify)g_object_unref);
  g_free (service_name);
}

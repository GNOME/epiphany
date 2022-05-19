/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-web-process-extension.h"
#include "ephy-webextension-api.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-permissions-manager.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-uri-helpers.h"
#include "ephy-web-overview-model.h"
#include "ephy-webextension-common.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <jsc/jsc.h>
#include <libsoup/soup.h>
#include <string.h>
#include <webkit2/webkit-web-extension.h>
#include <JavaScriptCore/JavaScript.h>

struct _EphyWebProcessExtension {
  GObject parent_instance;

  WebKitWebExtension *extension;
  gboolean initialized;

  GCancellable *cancellable;

  EphyWebOverviewModel *overview_model;
  EphyPermissionsManager *permissions_manager;

  WebKitScriptWorld *script_world;
  GHashTable *content_script_worlds;

  gboolean should_remember_passwords;
  gboolean is_private_profile;

  GHashTable *frames_map;
  GHashTable *translation_table;
};

G_DEFINE_TYPE (EphyWebProcessExtension, ephy_web_process_extension, G_TYPE_OBJECT)

static void
web_page_will_submit_form (WebKitWebPage            *web_page,
                           WebKitDOMHTMLFormElement *dom_form,
                           WebKitFormSubmissionStep  step,
                           WebKitFrame              *source_frame,
                           WebKitFrame              *target_frame,
                           GPtrArray                *text_field_names,
                           GPtrArray                *text_field_values)
{
  EphyWebProcessExtension *extension;
  gboolean form_submit_handled;
  g_autoptr (JSCContext) js_context = NULL;
  g_autoptr (JSCValue) js_ephy = NULL;
  g_autoptr (JSCValue) js_form = NULL;
  g_autoptr (JSCValue) js_result = NULL;

  form_submit_handled =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dom_form),
                                        "ephy-form-submit-handled"));
  if (form_submit_handled)
    return;

  g_object_set_data (G_OBJECT (dom_form),
                     "ephy-form-submit-handled",
                     GINT_TO_POINTER (TRUE));

  extension = ephy_web_process_extension_get ();
  js_context = webkit_frame_get_js_context_for_script_world (source_frame, extension->script_world);
  js_ephy = jsc_context_get_value (js_context, "Ephy");
  js_form = webkit_frame_get_js_value_for_dom_object_in_script_world (source_frame, WEBKIT_DOM_OBJECT (dom_form), extension->script_world);
  js_result = jsc_value_object_invoke_method (js_ephy,
                                              "handleFormSubmission",
                                              G_TYPE_UINT64, webkit_web_page_get_id (web_page),
                                              G_TYPE_UINT64, webkit_frame_get_id (source_frame),
                                              JSC_TYPE_VALUE, js_form,
                                              G_TYPE_NONE);
  (void)js_result;
}

static char *
password_form_message_serializer (guint64  page_id,
                                  gboolean is_insecure_action)
{
  GVariant *variant;
  char *message;

  variant = g_variant_new ("(tb)", page_id, is_insecure_action);
  message = g_variant_print (variant, FALSE);
  g_variant_unref (variant);

  return message;
}

static gboolean
remove_if_value_matches_user_data (gpointer key,
                                   gpointer value,
                                   gpointer user_data)
{
  return value == user_data;
}

static void
frame_destroyed_notify (EphyWebProcessExtension *extension,
                        GObject                 *where_the_object_was)
{
  /* This is awkward. We can't just remove the frame from the table
   * directly since we don't have any way to get its ID except by
   * checking every entry in the map....
   */
  g_hash_table_foreach_remove (extension->frames_map,
                               remove_if_value_matches_user_data,
                               where_the_object_was);
}

static void
web_page_form_controls_associated (WebKitWebPage           *web_page,
                                   GPtrArray               *elements,
                                   WebKitFrame             *frame,
                                   EphyWebProcessExtension *extension)
{
  g_autoptr (GPtrArray) form_controls = NULL;
  g_autoptr (JSCContext) js_context = NULL;
  g_autoptr (JSCValue) js_ephy = NULL;
  g_autoptr (JSCValue) js_serializer = NULL;
  g_autoptr (JSCValue) js_result = NULL;
  guint64 frame_id;
  guint64 *frame_id_copy;
  guint i;

  js_context = webkit_frame_get_js_context_for_script_world (frame, extension->script_world);

  form_controls = g_ptr_array_new_with_free_func (g_object_unref);
  for (i = 0; i < elements->len; ++i) {
    WebKitDOMObject *element = WEBKIT_DOM_OBJECT (g_ptr_array_index (elements, i));

    g_ptr_array_add (form_controls, webkit_frame_get_js_value_for_dom_object_in_script_world (frame, element, extension->script_world));
  }

  js_ephy = jsc_context_get_value (js_context, "Ephy");
  js_serializer = jsc_value_new_function (js_context,
                                          "passwordFormMessageSerializer",
                                          G_CALLBACK (password_form_message_serializer), NULL, NULL,
                                          G_TYPE_STRING, 2,
                                          G_TYPE_UINT64, G_TYPE_BOOLEAN);
  js_result = jsc_value_object_invoke_method (js_ephy,
                                              "formControlsAssociated",
                                              G_TYPE_UINT64, webkit_web_page_get_id (web_page),
                                              G_TYPE_UINT64, webkit_frame_get_id (frame),
                                              G_TYPE_PTR_ARRAY, form_controls,
                                              JSC_TYPE_VALUE, js_serializer,
                                              G_TYPE_NONE);
  (void)js_result;

  frame_id = webkit_frame_get_id (frame);
  if (!g_hash_table_contains (extension->frames_map, &frame_id)) {
    frame_id_copy = g_malloc (sizeof (guint64));
    *frame_id_copy = frame_id;
    g_hash_table_insert (extension->frames_map, g_steal_pointer (&frame_id_copy), frame);
    g_object_weak_ref (G_OBJECT (frame), (GWeakNotify)frame_destroyed_notify, extension);
  }
}

static gboolean
web_page_context_menu (WebKitWebPage          *web_page,
                       WebKitContextMenu      *context_menu,
                       WebKitWebHitTestResult *hit_test_result,
                       gpointer                user_data)
{
  EphyWebProcessExtension *extension;
  g_autofree char *string = NULL;
  GVariantBuilder builder;
  WebKitFrame *frame;
  g_autoptr (JSCContext) js_context = NULL;
  g_autoptr (JSCValue) js_value = NULL;

  extension = ephy_web_process_extension_get ();
  /* FIXME: this is wrong, see https://gitlab.gnome.org/GNOME/epiphany/issues/442
   * We need a way to get the right frame to use here.
   */
  frame = webkit_web_page_get_main_frame (web_page);
  js_context = webkit_frame_get_js_context_for_script_world (frame, extension->script_world);

  js_value = jsc_context_evaluate (js_context, "window.getSelection().toString();", -1);
  if (!jsc_value_is_null (js_value) && !jsc_value_is_undefined (js_value))
    string = jsc_value_to_string (js_value);

  if (!string || *string == '\0')
    return FALSE;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&builder, "{sv}", "SelectedText", g_variant_new_string (g_strstrip (string)));
  webkit_context_menu_set_user_data (context_menu,
                                     g_variant_builder_end (&builder));

  return TRUE;
}

static void
content_script_window_object_cleared_cb (WebKitScriptWorld *world,
                                         WebKitWebPage     *page,
                                         WebKitFrame       *frame,
                                         gpointer           user_data)
{
  EphyWebProcessExtension *extension = user_data;
  g_autoptr (JSCContext) js_context = NULL;
  g_autoptr (JSCValue) js_browser = NULL;
  g_autoptr (JSCValue) result = NULL;
  g_autoptr (GBytes) bytes = NULL;
  JsonObject *translations;
  const char *guid;
  const char *data;
  gsize data_size;

  guid = webkit_script_world_get_name (world);
  js_context = webkit_frame_get_js_context_for_script_world (frame, world);
  translations = g_hash_table_lookup (extension->translation_table, guid);

  js_browser = jsc_context_get_value (js_context, "browser");
  g_assert (!jsc_value_is_object (js_browser));

  bytes = g_resources_lookup_data ("/org/gnome/epiphany-web-extension/js/webextensions-common.js", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  data = g_bytes_get_data (bytes, &data_size);
  result = jsc_context_evaluate_with_source_uri (js_context, data, data_size, "resource:///org/gnome/epiphany-web-extension/js/webextensions-common.js", 1);
  g_clear_object (&result);

  ephy_webextension_install_common_apis (js_context, guid, translations);
}

static void
create_content_script_world (EphyWebProcessExtension *extension,
                             const char              *guid)
{
  WebKitScriptWorld *world = webkit_script_world_new_with_name (guid);

  g_hash_table_insert (extension->content_script_worlds, g_strdup (guid), world);

  g_signal_connect (world,
                    "window-object-cleared",
                    G_CALLBACK (content_script_window_object_cleared_cb),
                    extension);
}

static gboolean
web_page_received_message (WebKitWebPage     *web_page,
                           WebKitUserMessage *message,
                           gpointer           user_data)
{
  EphyWebProcessExtension *extension = user_data;
  const char *name = webkit_user_message_get_name (message);

  if (g_strcmp0 (name, "WebExtension.Initialize") == 0) {
    GVariant *parameters;
    const char *guid;

    parameters = webkit_user_message_get_parameters (message);
    if (!parameters)
      return FALSE;

    g_variant_get (parameters, "&s", &guid);
    create_content_script_world (extension, guid);
  } else {
    g_warning ("Unhandled page message: %s", name);
    return FALSE;
  }

  return TRUE;
}

static void
ephy_web_process_extension_page_created_cb (EphyWebProcessExtension *extension,
                                            WebKitWebPage           *web_page)
{
  g_autoptr (JSCContext) js_context = NULL;

  g_signal_connect (web_page, "context-menu",
                    G_CALLBACK (web_page_context_menu),
                    extension);
  g_signal_connect (web_page, "will-submit-form",
                    G_CALLBACK (web_page_will_submit_form),
                    extension);
  g_signal_connect (web_page, "form-controls-associated-for-frame",
                    G_CALLBACK (web_page_form_controls_associated),
                    extension);
  g_signal_connect (web_page, "user-message-received",
                    G_CALLBACK (web_page_received_message),
                    extension);
}

static void
ephy_web_process_extension_add_translations (GHashTable *translation_table,
                                             const char *guid,
                                             const char *data)
{
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *root;
  JsonObject *root_object;
  g_autoptr (GError) error = NULL;

  g_hash_table_remove (translation_table, guid);

  if (!data || !*data)
    return;

  parser = json_parser_new ();
  if (json_parser_load_from_data (parser, data, -1, &error)) {
    root = json_parser_get_root (parser);
    g_assert (root);
    root_object = json_node_get_object (root);
    g_assert (root_object);

    g_hash_table_insert (translation_table, g_strdup (guid), json_object_ref (root_object));
  } else {
    g_warning ("Could not read translation json data: %s. '%s'", error->message, data);
  }
}

static void
ephy_web_process_extension_user_message_received_cb (EphyWebProcessExtension *extension,
                                                     WebKitUserMessage       *message)
{
  const char *name = webkit_user_message_get_name (message);

  if (g_strcmp0 (name, "History.SetURLs") == 0) {
    if (extension->overview_model) {
      GVariant *parameters;
      GVariantIter iter;
      const char *url;
      const char *title;
      GList *items = NULL;
      g_autoptr (GVariant) array = NULL;

      parameters = webkit_user_message_get_parameters (message);
      if (!parameters)
        return;

      g_variant_get (parameters, "@a(ss)", &array);
      g_variant_iter_init (&iter, array);

      while (g_variant_iter_loop (&iter, "(&s&s)", &url, &title))
        items = g_list_prepend (items, ephy_web_overview_model_item_new (url, title));

      ephy_web_overview_model_set_urls (extension->overview_model, g_list_reverse (items));
    }
  } else if (g_strcmp0 (name, "History.SetURLThumbnail") == 0) {
    if (extension->overview_model) {
      GVariant *parameters;
      const char *url;
      const char *path;

      parameters = webkit_user_message_get_parameters (message);
      if (!parameters)
        return;

      g_variant_get (parameters, "(&s&s)", &url, &path);
      ephy_web_overview_model_set_url_thumbnail (extension->overview_model, url, path, TRUE);
    }
  } else if (g_strcmp0 (name, "History.SetURLTitle") == 0) {
    if (extension->overview_model) {
      GVariant *parameters;
      const char *url;
      const char *title;

      parameters = webkit_user_message_get_parameters (message);
      if (!parameters)
        return;

      g_variant_get (parameters, "(&s&s)", &url, &title);
      ephy_web_overview_model_set_url_title (extension->overview_model, url, title);
    }
  } else if (g_strcmp0 (name, "History.DeleteURL") == 0) {
    if (extension->overview_model) {
      GVariant *parameters;
      const char *url;

      parameters = webkit_user_message_get_parameters (message);
      if (!parameters)
        return;

      g_variant_get (parameters, "&s", &url);
      ephy_web_overview_model_delete_url (extension->overview_model, url);
    }
  } else if (g_strcmp0 (name, "History.DeleteHost") == 0) {
    if (extension->overview_model) {
      GVariant *parameters;
      const char *host;

      parameters = webkit_user_message_get_parameters (message);
      if (!parameters)
        return;

      g_variant_get (parameters, "&s", &host);
      ephy_web_overview_model_delete_host (extension->overview_model, host);
    }
  } else if (g_strcmp0 (name, "History.Clear") == 0) {
    if (extension->overview_model)
      ephy_web_overview_model_clear (extension->overview_model);
  } else if (g_strcmp0 (name, "PasswordManager.SetShouldRememberPasswords") == 0) {
    GVariant *parameters;

    parameters = webkit_user_message_get_parameters (message);
    if (!parameters)
      return;

    g_variant_get (parameters, "b", &extension->should_remember_passwords);
  } else if (g_strcmp0 (name, "WebExtension.UpdateTranslations") == 0) {
    GVariant *parameters;
    const char *guid;
    const char *data;

    parameters = webkit_user_message_get_parameters (message);
    if (!parameters)
      return;

    g_variant_get (parameters, "(&s&s)", &guid, &data);
    ephy_web_process_extension_add_translations (extension->translation_table, guid, data);
  }
}

static JSCValue *
get_password_manager (EphyWebProcessExtension *self,
                      guint64                  frame_id)
{
  WebKitFrame *frame;
  g_autoptr (JSCContext) js_context = NULL;
  g_autoptr (JSCValue) js_ephy = NULL;

  frame = g_hash_table_lookup (self->frames_map, &frame_id);
  if (!frame)
    return NULL;

  js_context = webkit_frame_get_js_context_for_script_world (frame, self->script_world);
  js_ephy = jsc_context_get_value (js_context, "Ephy");

  return jsc_value_object_get_property (js_ephy, "passwordManager");
}

static void
drop_frame_weak_ref (gpointer key,
                     gpointer value,
                     gpointer extension)
{
  g_object_weak_unref (G_OBJECT (value), (GWeakNotify)frame_destroyed_notify, extension);
}

static void
ephy_web_process_extension_dispose (GObject *object)
{
  EphyWebProcessExtension *extension = EPHY_WEB_PROCESS_EXTENSION (object);

  if (extension->cancellable) {
    g_cancellable_cancel (extension->cancellable);
    g_clear_object (&extension->cancellable);
  }

  g_clear_object (&extension->overview_model);
  g_clear_object (&extension->permissions_manager);

  g_clear_object (&extension->script_world);
  g_clear_object (&extension->extension);

  if (extension->frames_map) {
    g_hash_table_foreach (extension->frames_map, drop_frame_weak_ref, extension);
    g_clear_pointer (&extension->frames_map, g_hash_table_unref);
  }

  g_clear_pointer (&extension->translation_table, g_hash_table_destroy);
  g_clear_pointer (&extension->content_script_worlds, g_hash_table_destroy);

  G_OBJECT_CLASS (ephy_web_process_extension_parent_class)->dispose (object);
}

static void
ephy_web_process_extension_class_init (EphyWebProcessExtensionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_web_process_extension_dispose;
}

static void
ephy_web_process_extension_init (EphyWebProcessExtension *extension)
{
  extension->cancellable = g_cancellable_new ();
  extension->overview_model = ephy_web_overview_model_new ();
}

static gpointer
ephy_web_process_extension_create_instance (gpointer data)
{
  return g_object_new (EPHY_TYPE_WEB_PROCESS_EXTENSION, NULL);
}

EphyWebProcessExtension *
ephy_web_process_extension_get (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return EPHY_WEB_PROCESS_EXTENSION (g_once (&once_init, ephy_web_process_extension_create_instance, NULL));
}

static void
js_log (const char *message)
{
  LOG ("%s", message);
}

static char *
js_gettext (const char *message)
{
  return g_strdup (g_dgettext (GETTEXT_PACKAGE, message));
}

static void
js_auto_fill (JSCValue   *js_element,
              const char *value)
{
  WebKitDOMNode *node;
  WebKitDOMElement *element;

  node = webkit_dom_node_for_js_value (js_element);
  element = WEBKIT_DOM_ELEMENT (node);

  webkit_dom_element_html_input_element_set_auto_filled (element, TRUE);
  webkit_dom_element_html_input_element_set_editing_value (element, value);
}

typedef struct {
  EphyWebProcessExtension *extension;
  guint64 promise_id;
  guint64 frame_id;
} PasswordManagerQueryData;

static void
web_view_query_usernames_ready_cb (WebKitWebPage            *web_page,
                                   GAsyncResult             *result,
                                   PasswordManagerQueryData *data)
{
  WebKitUserMessage *reply;
  GVariant *parameters;
  const char **usernames;
  g_autoptr (JSCValue) password_manager = NULL;
  g_autoptr (GError) error = NULL;

  reply = webkit_web_page_send_message_to_view_finish (web_page, result, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error getting usernames from WebView: %s\n", error->message);
    g_free (data);
    return;
  }

  parameters = webkit_user_message_get_parameters (reply);
  if (!parameters) {
    g_free (data);
    return;
  }

  usernames = g_variant_get_strv (parameters, NULL);
  password_manager = get_password_manager (data->extension, data->frame_id);
  if (password_manager) {
    g_autoptr (JSCValue) ret = NULL;

    ret = jsc_value_object_invoke_method (password_manager, "_onQueryUsernamesResponse",
                                          G_TYPE_STRV, usernames,
                                          G_TYPE_UINT64, data->promise_id,
                                          G_TYPE_NONE);
    (void)ret;
  }

  g_free (usernames);
  g_free (data);
}

static void
js_query_usernames (const char              *origin,
                    guint64                  promise_id,
                    guint64                  page_id,
                    guint64                  frame_id,
                    EphyWebProcessExtension *extension)
{
  WebKitWebPage *web_page;
  WebKitUserMessage *message;
  PasswordManagerQueryData *data;

  if (!origin)
    return;

  web_page = webkit_web_extension_get_page (extension->extension, page_id);
  if (!web_page)
    return;

  data = g_new0 (PasswordManagerQueryData, 1);
  data->extension = extension;
  data->promise_id = promise_id;
  data->frame_id = frame_id;
  message = webkit_user_message_new ("PasswordManager.QueryUsernames",
                                     g_variant_new ("s", origin));
  webkit_web_page_send_message_to_view (web_page, message,
                                        extension->cancellable,
                                        (GAsyncReadyCallback)web_view_query_usernames_ready_cb,
                                        data);
}

static void
web_view_query_password_ready_cb (WebKitWebPage            *web_page,
                                  GAsyncResult             *result,
                                  PasswordManagerQueryData *data)
{
  WebKitUserMessage *reply;
  GVariant *parameters;
  const char *username;
  const char *password;
  g_autoptr (JSCValue) password_manager = NULL;
  g_autoptr (GError) error = NULL;

  reply = webkit_web_page_send_message_to_view_finish (web_page, result, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error getting password from WebView: %s\n", error->message);
    g_free (data);
    return;
  }

  parameters = webkit_user_message_get_parameters (reply);
  if (!parameters) {
    g_free (data);
    return;
  }

  g_variant_get (parameters, "(m&sm&s)", &username, &password);
  password_manager = get_password_manager (data->extension, data->frame_id);
  if (password_manager) {
    g_autoptr (JSCValue) ret = NULL;

    ret = jsc_value_object_invoke_method (password_manager, "_onQueryResponse",
                                          G_TYPE_STRING, username,
                                          G_TYPE_STRING, password,
                                          G_TYPE_UINT64, data->promise_id,
                                          G_TYPE_NONE);
    (void)ret;
  }

  g_free (data);
}

static void
js_query_password (const char              *origin,
                   const char              *target_origin,
                   const char              *username,
                   const char              *username_field,
                   const char              *password_field,
                   guint64                  promise_id,
                   guint64                  page_id,
                   guint64                  frame_id,
                   EphyWebProcessExtension *extension)
{
  WebKitWebPage *web_page;
  WebKitUserMessage *message;
  PasswordManagerQueryData *data;

  if (!origin || !target_origin || !password_field)
    return;

  web_page = webkit_web_extension_get_page (extension->extension, page_id);
  if (!web_page)
    return;

  data = g_new0 (PasswordManagerQueryData, 1);
  data->extension = extension;
  data->promise_id = promise_id;
  data->frame_id = frame_id;
  message = webkit_user_message_new ("PasswordManager.QueryPassword",
                                     g_variant_new ("(ssmsmss)", origin, target_origin, username, username_field, password_field));
  webkit_web_page_send_message_to_view (web_page, message,
                                        extension->cancellable,
                                        (GAsyncReadyCallback)web_view_query_password_ready_cb,
                                        data);
}

static gboolean
js_is_web_application (void)
{
  return ephy_profile_dir_is_web_application ();
}

static gboolean
js_is_edited (JSCValue *js_element)
{
  WebKitDOMNode *node = webkit_dom_node_for_js_value (js_element);

  return webkit_dom_element_html_input_element_is_user_edited (WEBKIT_DOM_ELEMENT (node));
}

static gboolean
js_should_remember_passwords (EphyWebProcessExtension *extension)
{
  g_assert (EPHY_IS_WEB_PROCESS_EXTENSION (extension));

  /* We currently don't remember passwords in private profiles. But there is
   * no good reason for this and we should probably change this.
   */
  return extension->should_remember_passwords && !extension->is_private_profile;
}

static void
js_exception_handler (JSCContext   *context,
                      JSCException *exception)
{
  g_autoptr (JSCValue) js_console = NULL;
  g_autoptr (JSCValue) js_result = NULL;
  g_autofree char *report = NULL;

  js_console = jsc_context_get_value (context, "console");
  js_result = jsc_value_object_invoke_method (js_console, "error", JSC_TYPE_EXCEPTION, exception, G_TYPE_NONE);
  (void)js_result;
  report = jsc_exception_report (exception);
  g_warning ("%s", report);

  jsc_context_throw_exception (context, exception);
}

static void
window_object_cleared_cb (WebKitScriptWorld       *world,
                          WebKitWebPage           *page,
                          WebKitFrame             *frame,
                          EphyWebProcessExtension *extension)
{
  g_autoptr (JSCContext) js_context = NULL;
  g_autoptr (GBytes) bytes = NULL;
  const char *data;
  gsize data_size;
  g_autoptr (JSCValue) js_ephy = NULL;
  g_autoptr (JSCValue) js_function = NULL;
  g_autoptr (JSCValue) result = NULL;

  js_context = webkit_frame_get_js_context_for_script_world (frame, world);
  jsc_context_push_exception_handler (js_context, (JSCExceptionHandler)js_exception_handler, NULL, NULL);

  result = jsc_context_get_value (js_context, "Ephy");
  g_assert (jsc_value_is_undefined (result));
  g_clear_object (&result);

  bytes = g_resources_lookup_data ("/org/gnome/epiphany-web-process-extension/js/ephy.js", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  data = g_bytes_get_data (bytes, &data_size);
  result = jsc_context_evaluate_with_source_uri (js_context, data, data_size, "resource:///org/gnome/epiphany-web-process-extension/js/ephy.js", 1);
  g_clear_pointer (&bytes, g_bytes_unref);
  g_clear_object (&result);

  js_ephy = jsc_context_get_value (js_context, "Ephy");

  js_function = jsc_value_new_function (js_context,
                                        "log",
                                        G_CALLBACK (js_log), NULL, NULL,
                                        G_TYPE_NONE, 1,
                                        G_TYPE_STRING);
  jsc_value_object_set_property (js_ephy, "log", js_function);
  g_clear_object (&js_function);

  js_function = jsc_value_new_function (js_context,
                                        "gettext",
                                        G_CALLBACK (js_gettext), NULL, NULL,
                                        G_TYPE_STRING, 1,
                                        G_TYPE_STRING);
  jsc_value_object_set_property (js_ephy, "_", js_function);
  g_clear_object (&js_function);

  if (g_strcmp0 (webkit_web_page_get_uri (page), "ephy-about:overview") == 0) {
    g_autoptr (JSCValue) js_overview = NULL;
    g_autoptr (JSCValue) js_overview_ctor = NULL;
    g_autoptr (JSCValue) jsc_overview_model = NULL;

    bytes = g_resources_lookup_data ("/org/gnome/epiphany-web-process-extension/js/overview.js", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    data = g_bytes_get_data (bytes, &data_size);
    result = jsc_context_evaluate_with_source_uri (js_context, data, data_size, "resource:///org/gnome/epiphany-web-process-extension/js/overview.js", 1);
    g_clear_pointer (&bytes, g_bytes_unref);
    g_clear_object (&result);

    jsc_overview_model = ephy_web_overview_model_export_to_js_context (extension->overview_model,
                                                                       js_context);

    js_overview_ctor = jsc_value_object_get_property (js_ephy, "Overview");
    js_overview = jsc_value_constructor_call (js_overview_ctor,
                                              JSC_TYPE_VALUE, jsc_overview_model,
                                              G_TYPE_NONE);
    jsc_value_object_set_property (js_ephy, "overview", js_overview);
  }

  ephy_permissions_manager_export_to_js_context (extension->permissions_manager,
                                                 js_context,
                                                 js_ephy);

  if (!extension->is_private_profile) {
    g_autoptr (JSCValue) js_password_manager_ctor = jsc_value_object_get_property (js_ephy, "PasswordManager");
    g_autoptr (JSCValue) js_password_manager = jsc_value_constructor_call (js_password_manager_ctor,
                                                                           G_TYPE_UINT64, webkit_web_page_get_id (page),
                                                                           G_TYPE_UINT64, webkit_frame_get_id (frame),
                                                                           G_TYPE_NONE);
    jsc_value_object_set_property (js_ephy, "passwordManager", js_password_manager);

    js_function = jsc_value_new_function (js_context,
                                          "autoFill",
                                          G_CALLBACK (js_auto_fill), NULL, NULL,
                                          G_TYPE_NONE, 2,
                                          JSC_TYPE_VALUE, G_TYPE_STRING);
    jsc_value_object_set_property (js_ephy, "autoFill", js_function);
    g_clear_object (&js_function);

    js_function = jsc_value_new_function (js_context,
                                          "queryUsernames",
                                          G_CALLBACK (js_query_usernames),
                                          extension, NULL,
                                          G_TYPE_NONE, 4,
                                          G_TYPE_STRING, G_TYPE_UINT64,
                                          G_TYPE_UINT64, G_TYPE_UINT64);
    jsc_value_object_set_property (js_ephy, "queryUsernames", js_function);
    g_clear_object (&js_function);

    js_function = jsc_value_new_function (js_context,
                                          "queryPassword",
                                          G_CALLBACK (js_query_password),
                                          extension, NULL,
                                          G_TYPE_NONE, 8,
                                          G_TYPE_STRING, G_TYPE_STRING,
                                          G_TYPE_STRING, G_TYPE_STRING,
                                          G_TYPE_STRING, G_TYPE_UINT64,
                                          G_TYPE_UINT64, G_TYPE_UINT64);
    jsc_value_object_set_property (js_ephy, "queryPassword", js_function);
    g_clear_object (&js_function);
  }

  js_function = jsc_value_new_function (js_context,
                                        "isWebApplication",
                                        G_CALLBACK (js_is_web_application), NULL, NULL,
                                        G_TYPE_BOOLEAN, 0);
  jsc_value_object_set_property (js_ephy, "isWebApplication", js_function);
  g_clear_object (&js_function);

  js_function = jsc_value_new_function (js_context,
                                        "isEdited",
                                        G_CALLBACK (js_is_edited), NULL, NULL,
                                        G_TYPE_BOOLEAN, 1,
                                        JSC_TYPE_VALUE);
  jsc_value_object_set_property (js_ephy, "isEdited", js_function);
  g_clear_object (&js_function);

  js_function = jsc_value_new_function (js_context,
                                        "shouldRememberPasswords",
                                        G_CALLBACK (js_should_remember_passwords),
                                        g_object_ref (extension), g_object_unref,
                                        G_TYPE_BOOLEAN, 0);
  jsc_value_object_set_property (js_ephy, "shouldRememberPasswords", js_function);
  g_clear_object (&js_function);
}

void
ephy_web_process_extension_initialize (EphyWebProcessExtension *extension,
                                       WebKitWebExtension      *wk_extension,
                                       const char              *guid,
                                       gboolean                 should_remember_passwords,
                                       gboolean                 is_private_profile)
{
  g_assert (EPHY_IS_WEB_PROCESS_EXTENSION (extension));

  if (extension->initialized)
    return;

  extension->initialized = TRUE;

  g_assert (guid && *guid);

  extension->script_world = webkit_script_world_new_with_name (guid);

  g_signal_connect (extension->script_world,
                    "window-object-cleared",
                    G_CALLBACK (window_object_cleared_cb),
                    extension);

  extension->extension = g_object_ref (wk_extension);

  extension->should_remember_passwords = should_remember_passwords;
  extension->is_private_profile = is_private_profile;

  extension->permissions_manager = ephy_permissions_manager_new ();

  g_signal_connect_swapped (extension->extension, "user-message-received",
                            G_CALLBACK (ephy_web_process_extension_user_message_received_cb),
                            extension);
  g_signal_connect_swapped (extension->extension, "page-created",
                            G_CALLBACK (ephy_web_process_extension_page_created_cb),
                            extension);

  extension->frames_map = g_hash_table_new_full (g_int64_hash, g_int64_equal,
                                                 g_free, NULL);

  extension->translation_table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                        g_free, (GDestroyNotify)json_object_unref);

  extension->content_script_worlds = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                            g_free, g_object_unref);
}

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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
#include "ephy-reader-handler.h"

#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-output-encoding.h"
#include "ephy-settings.h"
#include "ephy-web-view.h"

#include <adwaita.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

struct _EphyReaderHandler {
  GObject parent_instance;

  GList *outstanding_requests;
};

G_DEFINE_FINAL_TYPE (EphyReaderHandler, ephy_reader_handler, G_TYPE_OBJECT)

typedef struct {
  EphyReaderHandler *source_handler;
  WebKitURISchemeRequest *scheme_request;
  WebKitWebView *web_view;
  GCancellable *cancellable;
  guint load_changed_id;
} EphyReaderRequest;

static EphyReaderRequest *
ephy_reader_request_new (EphyReaderHandler      *handler,
                         WebKitURISchemeRequest *request)
{
  EphyReaderRequest *reader_request;

  reader_request = g_new (EphyReaderRequest, 1);
  reader_request->source_handler = g_object_ref (handler);
  reader_request->scheme_request = g_object_ref (request);
  reader_request->web_view = NULL; /* created only if required */
  reader_request->cancellable = g_cancellable_new ();
  reader_request->load_changed_id = 0;

  return reader_request;
}

static void
ephy_reader_request_free (EphyReaderRequest *request)
{
  if (request->load_changed_id > 0)
    g_signal_handler_disconnect (request->web_view, request->load_changed_id);

  g_object_unref (request->source_handler);
  g_object_unref (request->scheme_request);
  g_clear_object (&request->web_view);

  g_cancellable_cancel (request->cancellable);
  g_object_unref (request->cancellable);

  g_free (request);
}

static void
finish_uri_scheme_request (EphyReaderRequest *request,
                           gchar             *data,
                           GError            *error)
{
  GInputStream *stream;
  gssize data_length;

  g_assert ((data && !error) || (!data && error));

  if (error) {
    webkit_uri_scheme_request_finish_error (request->scheme_request, error);
  } else {
    data_length = MIN (strlen (data), G_MAXSSIZE);
    stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
    webkit_uri_scheme_request_finish (request->scheme_request, stream, data_length, "text/html");
    g_object_unref (stream);
  }

  request->source_handler->outstanding_requests =
    g_list_remove (request->source_handler->outstanding_requests,
                   request);

  ephy_reader_request_free (request);
}

static const char *
enum_nick (GType enum_type,
           int   value)
{
  GEnumClass *enum_class;
  const GEnumValue *enum_value;
  const char *nick = NULL;

  enum_class = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value (enum_class, value);
  if (enum_value)
    nick = enum_value->value_nick;

  g_type_class_unref (enum_class);
  return nick;
}

static char *
readability_get_property_string (JSCValue *jsc_value,
                                 char     *property)
{
  char *result = NULL;

  if (!jsc_value_is_object (jsc_value))
    return NULL;

  if (jsc_value_object_has_property (jsc_value, property)) {
    g_autoptr (JSCValue) jsc_content = jsc_value_object_get_property (jsc_value, property);

    result = jsc_value_to_string (jsc_content);

    if (result && strcmp (result, "null") == 0)
      g_clear_pointer (&result, g_free);
  }

  return result;
}

static void
readability_js_finish_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (object);
  EphyReaderRequest *request = user_data;
  g_autoptr (JSCValue) value = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *byline = NULL;
  g_autofree gchar *encoded_byline = NULL;
  g_autofree gchar *content = NULL;
  g_autofree gchar *reading_time = NULL;
  g_autofree gchar *encoded_title = NULL;
  g_autoptr (GString) html = NULL;
  g_autoptr (GBytes) style_css = NULL;
  const gchar *title;
  const gchar *font_style;
  const gchar *color_scheme;
  AdwStyleManager *style_manager;

  value = webkit_web_view_evaluate_javascript_finish (web_view, result, &error);
  if (!value) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error running javascript: %s", error->message);
    return;
  }

  byline = readability_get_property_string (value, "byline");
  content = readability_get_property_string (value, "content");
  reading_time = readability_get_property_string (value, "reading_time");
  title = webkit_web_view_get_title (web_view);

  encoded_byline = byline ? ephy_encode_for_html_entity (byline) : g_strdup ("");
  encoded_title = ephy_encode_for_html_entity (title);

  html = g_string_new (NULL);
  style_css = g_resources_lookup_data ("/org/gnome/epiphany/readability/reader.css", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);

  font_style = enum_nick (EPHY_TYPE_PREFS_READER_FONT_STYLE,
                          g_settings_get_enum (EPHY_SETTINGS_READER,
                                               EPHY_PREFS_READER_FONT_STYLE));

  style_manager = adw_style_manager_get_default ();

  if (adw_style_manager_get_system_supports_color_schemes (style_manager))
    color_scheme = adw_style_manager_get_dark (style_manager) ? "dark" : "light";
  else
    color_scheme = enum_nick (EPHY_TYPE_PREFS_READER_COLOR_SCHEME,
                              g_settings_get_enum (EPHY_SETTINGS_READER,
                                                   EPHY_PREFS_READER_COLOR_SCHEME));

  g_string_append_printf (html, "<style>%s</style>"
                          "<title>%s</title>"
                          "<meta http-equiv='Content-Type' content='text/html;' charset='UTF-8'>" \
                          "<meta http-equiv='Content-Security-Policy' content=\"script-src 'none'\">" \
                          "<body class='%s %s'>"
                          "<article>"
                          "<h2>"
                          "%s"
                          "</h2>"
                          "<i>"
                          "%s"
                          "</i>"
                          "<br>"
                          "<br>",
                          (gchar *)g_bytes_get_data (style_css, NULL),
                          encoded_title,
                          font_style,
                          color_scheme,
                          encoded_title,
                          encoded_byline);

  g_string_append (html, reading_time ? reading_time : "");
  g_string_append (html, "<br/><hr/>");

  /* We cannot encode the page content because it contains HTML tags inserted by
   * Readability.js. Upstream recommends that we use an XSS sanitizer like
   * DOMPurify plus Content-Security-Policy, but I'm not keen on adding more
   * bundled JS dependencies, and we have an advantage over Firefox in that we
   * don't need scripts to work at this point. So instead the above CSP
   * completely blocks all scripts, which should hopefully obviate the need for
   * a DOM purifier.
   *
   * Note the encoding for page title and byline is still required, as they're
   * not supposed to contain markup, and Readability.js unescapes them before
   * returning them to us.
   */
  g_string_append (html, content ? content : "");
  g_string_append (html, "</article>");
  g_string_append (html, "</body>");

  finish_uri_scheme_request (request, g_strdup (html->str), NULL);
}

static void
ephy_reader_request_begin_get_source_from_web_view (EphyReaderRequest *request,
                                                    WebKitWebView     *web_view)
{
  gsize length;
  const char *script;
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) bytes = g_resources_lookup_data ("/org/gnome/epiphany/readability/Readability.js",
                                                      G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

  if (!bytes) {
    g_critical ("Failed to get Readability.js from resources: %s", error->message);
    return;
  }

  script = (const char *)g_bytes_get_data (bytes, &length);
  webkit_web_view_evaluate_javascript (web_view, script, length, NULL,
                                       "resource:///org/gnome/epiphany/readability/Readability.js",
                                       request->cancellable,
                                       readability_js_finish_cb,
                                       request);
}

static void
load_changed_cb (WebKitWebView     *web_view,
                 WebKitLoadEvent    load_event,
                 EphyReaderRequest *request)
{
  if (load_event == WEBKIT_LOAD_FINISHED) {
    g_signal_handler_disconnect (request->web_view, request->load_changed_id);
    request->load_changed_id = 0;

    ephy_reader_request_begin_get_source_from_web_view (request, web_view);
  }
}

static void
ephy_reader_request_begin_get_source_from_uri (EphyReaderRequest *request,
                                               const char        *uri)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitWebContext *context = ephy_embed_shell_get_web_context (shell);
  WebKitNetworkSession *network_session = ephy_embed_shell_get_network_session (shell);

  g_assert (!request->web_view);
  request->web_view = WEBKIT_WEB_VIEW (g_object_ref_sink (g_object_new (WEBKIT_TYPE_WEB_VIEW,
                                                                        "web-context", context,
                                                                        "network-session", network_session,
                                                                        NULL)));

  g_assert (request->load_changed_id == 0);
  request->load_changed_id = g_signal_connect (request->web_view, "load-changed",
                                               G_CALLBACK (load_changed_cb),
                                               request);

  webkit_web_view_load_uri (request->web_view, uri);
}

static void
ephy_reader_request_start (EphyReaderRequest *request)
{
  g_autoptr (GUri) uri = NULL;
  const char *original_uri;
  WebKitWebView *web_view;

  original_uri = webkit_uri_scheme_request_get_uri (request->scheme_request);
  uri = g_uri_parse (original_uri, G_URI_FLAGS_PARSE_RELAXED, NULL);

  if (!uri) {
    /* Can't assert because user could theoretically input something weird */
    GError *error = g_error_new (WEBKIT_NETWORK_ERROR,
                                 WEBKIT_NETWORK_ERROR_FAILED,
                                 _("%s is not a valid URI"),
                                 original_uri);
    finish_uri_scheme_request (request, NULL, error);
    return;
  }

  web_view = webkit_uri_scheme_request_get_web_view (request->scheme_request);
  if (web_view) {
    gboolean entering_reader_mode;

    g_object_get (G_OBJECT (web_view), "entering-reader-mode", &entering_reader_mode, NULL);
    if (!entering_reader_mode)
      web_view = NULL;
  }

  if (web_view) {
    ephy_reader_request_begin_get_source_from_web_view (request, web_view);
  } else {
    /* Extract URI:
     * ephy-reader:https://example.com/whatever?xyz into https://example.com/whatever?xyz
     */
    g_assert (g_str_has_prefix (original_uri, "ephy-reader:"));
    ephy_reader_request_begin_get_source_from_uri (request, original_uri + strlen ("ephy-reader:"));
  }

  request->source_handler->outstanding_requests =
    g_list_prepend (request->source_handler->outstanding_requests, request);
}

static void
cancel_outstanding_request (EphyReaderRequest *request)
{
  g_cancellable_cancel (request->cancellable);
}

static void
ephy_reader_handler_dispose (GObject *object)
{
  EphyReaderHandler *handler = EPHY_READER_HANDLER (object);

  if (handler->outstanding_requests) {
    g_list_foreach (handler->outstanding_requests, (GFunc)cancel_outstanding_request, NULL);
    g_list_free (handler->outstanding_requests);
    handler->outstanding_requests = NULL;
  }

  G_OBJECT_CLASS (ephy_reader_handler_parent_class)->dispose (object);
}

static void
ephy_reader_handler_init (EphyReaderHandler *handler)
{
}

static void
ephy_reader_handler_class_init (EphyReaderHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_reader_handler_dispose;
}

EphyReaderHandler *
ephy_reader_handler_new (void)
{
  return EPHY_READER_HANDLER (g_object_new (EPHY_TYPE_READER_HANDLER, NULL));
}

void
ephy_reader_handler_handle_request (EphyReaderHandler      *handler,
                                    WebKitURISchemeRequest *scheme_request)
{
  EphyReaderRequest *reader_request;

  reader_request = ephy_reader_request_new (handler, scheme_request);
  ephy_reader_request_start (reader_request);
}

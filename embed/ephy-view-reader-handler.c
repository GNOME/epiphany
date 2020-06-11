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
#include "ephy-view-reader-handler.h"

#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-web-view.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

struct _EphyViewReaderHandler {
  EphySourceTransformHandler parent_instance;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (EphyViewReaderHandler, ephy_view_reader_handler, EPHY_TYPE_SOURCE_TRANSFORM_HANDLER)

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

static
char *readability_get_property_string (WebKitJavascriptResult *js_result,
                                       char                   *property)
{
  JSCValue *jsc_value;
  char *result = NULL;

  jsc_value = webkit_javascript_result_get_js_value (js_result);

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
  EphySourceTransformRequest *request = user_data;
  g_autoptr (WebKitJavascriptResult) js_result = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *byline = NULL;
  g_autofree gchar *content = NULL;
  g_autoptr (GString) html = NULL;
  g_autoptr (GBytes) style_css = NULL;
  const gchar *title;
  const gchar *font_style;
  const gchar *color_scheme;

  js_result = webkit_web_view_run_javascript_finish (web_view, result, &error);
  if (!js_result) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Error running javascript: %s", error->message);
    g_error_free (error);
    return;
  }

  byline = readability_get_property_string (js_result, "byline");
  content = readability_get_property_string (js_result, "content");

  html = g_string_new ("");
  style_css = g_resources_lookup_data ("/org/gnome/epiphany/readability/reader.css", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  title = webkit_web_view_get_title (web_view);
  font_style = enum_nick (EPHY_TYPE_PREFS_READER_FONT_STYLE,
                          g_settings_get_enum (EPHY_SETTINGS_READER,
                                               EPHY_PREFS_READER_FONT_STYLE));
  color_scheme = enum_nick (EPHY_TYPE_PREFS_READER_COLOR_SCHEME,
                            g_settings_get_enum (EPHY_SETTINGS_READER,
                                                 EPHY_PREFS_READER_COLOR_SCHEME));

  g_string_append_printf (html, "<style>%s</style>"
                          "<title>%s</title>"
                          "<body class='%s %s'>"
                          "<article>"
                          "<h2>"
                          "%s"
                          "</h2>"
                          "<i>"
                          "%s"
                          "</i>"
                          "<hr>",
                          (gchar *)g_bytes_get_data (style_css, NULL),
                          title,
                          font_style,
                          color_scheme,
                          title,
                          byline != NULL ? byline : "");
  g_string_append (html, content);
  g_string_append (html, "</article>");


  ephy_source_transform_handler_finish_request (request, g_strdup (html->str));
}

static guchar *
ephy_view_reader_handler_transform_source (EphySourceTransformHandler *handler,
                                           EphySourceTransformRequest *request,
                                           const guchar               *data,
                                           gsize                       length)
{
  EphyViewReaderHandler *reader_handler = EPHY_VIEW_READER_HANDLER (handler);
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (request->web_view);

  webkit_web_view_run_javascript_from_gresource (web_view,
                                                 "/org/gnome/epiphany/readability/Readability.js",
                                                 reader_handler->cancellable,
                                                 readability_js_finish_cb,
                                                 request);

  return NULL;
}

static void
ephy_view_reader_handler_init (EphyViewReaderHandler *handler)
{
}

static void
ephy_view_reader_handler_class_init (EphyViewReaderHandlerClass *klass)
{
  EphySourceTransformHandlerClass *handler_class = EPHY_SOURCE_TRANSFORM_HANDLER_CLASS (klass);

  handler_class->transform_source = ephy_view_reader_handler_transform_source;
}

EphyViewReaderHandler *
ephy_view_reader_handler_new (void)
{
  return EPHY_VIEW_READER_HANDLER (g_object_new (EPHY_TYPE_VIEW_READER_HANDLER, NULL));
}

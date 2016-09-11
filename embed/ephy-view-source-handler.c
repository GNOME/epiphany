/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
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
#include "ephy-view-source-handler.h"

#include "ephy-embed-shell.h"

#include <gio/gio.h>
#include <string.h>

struct _EphyViewSourceHandler {
  GObject parent_instance;

  GList *outstanding_requests;
};

G_DEFINE_TYPE (EphyViewSourceHandler, ephy_view_source_handler, G_TYPE_OBJECT)

typedef struct {
  EphyViewSourceHandler *source_handler;
  WebKitURISchemeRequest *scheme_request;
  WebKitWebView *web_view;
  GCancellable *cancellable;
  guint load_changed_id;
} EphyViewSourceRequest;

static EphyViewSourceRequest *
ephy_view_source_request_new (EphyViewSourceHandler  *handler,
                              WebKitURISchemeRequest *request)
{
  EphyViewSourceRequest *view_source_request;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitWebContext *context = ephy_embed_shell_get_web_context (shell);

  view_source_request = g_slice_new (EphyViewSourceRequest);
  view_source_request->source_handler = handler;
  view_source_request->scheme_request = g_object_ref (request);
  view_source_request->web_view = g_object_ref_sink (webkit_web_view_new_with_context (context));
  view_source_request->cancellable = g_cancellable_new ();
  view_source_request->load_changed_id = 0;

  return view_source_request;
}

static void
ephy_view_source_request_free (EphyViewSourceRequest *request)
{
  if (request->load_changed_id > 0)
    g_signal_handler_disconnect (request->web_view, request->load_changed_id);

  g_object_unref (request->scheme_request);
  g_object_unref (request->web_view);

  g_cancellable_cancel (request->cancellable);
  g_object_unref (request->cancellable);

  g_slice_free (EphyViewSourceRequest, request);
}

static void
finish_uri_scheme_request (EphyViewSourceRequest *request,
                           gchar                 *data)
{
  GInputStream *stream;
  gssize data_length;

  data_length = MIN (strlen (data), G_MAXSSIZE);
  stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
  webkit_uri_scheme_request_finish (request->scheme_request, stream, data_length, "text/html");

  request->source_handler->outstanding_requests =
      g_list_remove (request->source_handler->outstanding_requests,
                     request);

  ephy_view_source_request_free (request);
  g_object_unref (stream);
}

static void
web_resource_data_cb (WebKitWebResource     *resource,
                      GAsyncResult          *result,
                      EphyViewSourceRequest *request)
{
  guchar *data;
  char *data_str;
  char *escaped_str;
  char *html;
  gsize length;
  GError *error = NULL;

  data = webkit_web_resource_get_data_finish (resource, result, &length, &error);
  if (error) {
    html = g_strdup (error->message);
    length = strlen (html);
    g_error_free (error);
    finish_uri_scheme_request (request, html);
    return;
  }

  data_str = g_malloc (length + 1);
  strncpy (data_str, (const char *)data, length);
  data_str[length] = '\0';
  g_free (data);

  escaped_str = g_markup_escape_text (data_str, -1);
  g_free (data_str);

  html = g_strdup_printf ("<head>"
                            "<link href=\"ephy-resource:///org/gnome/epiphany/prism.css\" rel=\"stylesheet\"/>"
                          "</head>"
                          "<body>"
                            "<script src=\"ephy-resource:///org/gnome/epiphany/prism.js\"></script>"
                            /* http://prismjs.com/plugins/line-numbers/ */
                            "<pre class=\"line-numbers\">"
                              "<code class=\"language-markup\">%s</code>"
                            "</pre>"
                          "</body>",
                          escaped_str);
  g_free (escaped_str);

  finish_uri_scheme_request (request, html);
}

static void
load_changed_cb (WebKitWebView         *web_view,
                 WebKitLoadEvent        load_event,
                 EphyViewSourceRequest *request)
{
  if (load_event == WEBKIT_LOAD_FINISHED) {
    WebKitWebResource *resource = webkit_web_view_get_main_resource (web_view);
    webkit_web_resource_get_data (resource,
                                  request->cancellable,
                                  (GAsyncReadyCallback)(web_resource_data_cb),
                                  request);
  }
}

static void
ephy_view_source_request_start (EphyViewSourceRequest *request)
{
  SoupURI *soup_uri;
  char *modified_uri;
  char *decoded_fragment;
  const char *original_uri;

  request->source_handler->outstanding_requests =
      g_list_prepend (request->source_handler->outstanding_requests, request);

  original_uri = webkit_uri_scheme_request_get_uri (request->scheme_request);
  soup_uri = soup_uri_new (original_uri);

  if (!soup_uri) {
    g_critical ("Failed to construct SoupURI for %s", original_uri);
    finish_uri_scheme_request (request, g_strdup (""));
    return;
  }

  /* Convert e.g. ephy-source://gnome.org#https to https://gnome.org */
  g_assert (soup_uri->fragment);
  decoded_fragment = soup_uri_decode (soup_uri->fragment);
  soup_uri_set_scheme (soup_uri, decoded_fragment);
  soup_uri_set_fragment (soup_uri, NULL);
  modified_uri = soup_uri_to_string (soup_uri, FALSE);

  g_assert(request->load_changed_id == 0);
  request->load_changed_id = g_signal_connect (request->web_view, "load-changed",
                                               G_CALLBACK (load_changed_cb),
                                               request);

  webkit_web_view_load_uri (request->web_view, modified_uri);

  g_free (decoded_fragment);
  g_free (modified_uri);
  soup_uri_free (soup_uri);
}

static void
ephy_view_source_handler_dispose (GObject *object)
{
  EphyViewSourceHandler *handler = EPHY_VIEW_SOURCE_HANDLER (object);

  if (handler->outstanding_requests) {
    g_list_free_full (handler->outstanding_requests, (GDestroyNotify)ephy_view_source_request_free);
    handler->outstanding_requests = NULL;
  }

  G_OBJECT_CLASS (ephy_view_source_handler_parent_class)->dispose (object);
}

static void
ephy_view_source_handler_init (EphyViewSourceHandler *handler)
{
}

static void
ephy_view_source_handler_class_init (EphyViewSourceHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_view_source_handler_dispose;
}

EphyViewSourceHandler *
ephy_view_source_handler_new (void)
{
  return EPHY_VIEW_SOURCE_HANDLER (g_object_new (EPHY_TYPE_VIEW_SOURCE_HANDLER, NULL));
}

void
ephy_view_source_handler_handle_request (EphyViewSourceHandler  *handler,
                                         WebKitURISchemeRequest *scheme_request)
{
  EphyViewSourceRequest *view_source_request;

  view_source_request = ephy_view_source_request_new (handler, scheme_request);
  ephy_view_source_request_start (view_source_request);
}

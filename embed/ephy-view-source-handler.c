/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Igalia S.L.
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
#include "ephy-view-source-handler.h"

#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-output-encoding.h"
#include "ephy-web-view.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

struct _EphyViewSourceHandler {
  GObject parent_instance;

  GList *outstanding_requests;
};

G_DEFINE_FINAL_TYPE (EphyViewSourceHandler, ephy_view_source_handler, G_TYPE_OBJECT)

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

  view_source_request = g_new (EphyViewSourceRequest, 1);
  view_source_request->source_handler = g_object_ref (handler);
  view_source_request->scheme_request = g_object_ref (request);
  view_source_request->web_view = NULL; /* created only if required */
  view_source_request->cancellable = g_cancellable_new ();
  view_source_request->load_changed_id = 0;

  return view_source_request;
}

static void
ephy_view_source_request_free (EphyViewSourceRequest *request)
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
finish_uri_scheme_request (EphyViewSourceRequest *request,
                           gchar                 *data,
                           GError                *error)
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

  ephy_view_source_request_free (request);
}

static void
web_resource_data_cb (WebKitWebResource     *resource,
                      GAsyncResult          *result,
                      EphyViewSourceRequest *request)
{
  g_autofree guchar *data = NULL;
  g_autofree char *data_str = NULL;
  g_autofree char *encoded_str = NULL;
  g_autofree char *encoded_uri = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *html = NULL;
  gsize length;

  data = webkit_web_resource_get_data_finish (resource, result, &length, &error);
  if (!data) {
    finish_uri_scheme_request (request, NULL, error);
    return;
  }

  /* Convert data to a string */
  data_str = g_malloc (length + 1);
  memcpy (data_str, data, length);
  data_str[length] = '\0';

  encoded_str = ephy_encode_for_html_entity (data_str);
  encoded_uri = ephy_encode_for_html_entity (webkit_web_resource_get_uri (resource));

  html = g_strdup_printf ("<head>"
                          "  <link rel='stylesheet' href='ephy-resource:///org/gnome/epiphany/highlightjs/nnfx-light.css' media='(prefers-color-scheme: no-preference), (prefers-color-scheme: light)'>"
                          "  <link rel='stylesheet' href='ephy-resource:///org/gnome/epiphany/highlightjs/nnfx-dark.css' media='(prefers-color-scheme: dark)'>"
                          "  <link rel='stylesheet' href='ephy-resource:///org/gnome/epiphany/highlightjs/epiphany.css'>"
                          "  <title>%s</title>"
                          "</head>"
                          "<body class='hljs'>"
                          "  <script src='ephy-resource:///org/gnome/epiphany/highlightjs/highlight.js'></script>"
                          "  <script src='ephy-resource:///org/gnome/epiphany/highlightjs/highlightjs-line-numbers.js'></script>"
                          "  <script>hljs.highlightAll();"
                          "          hljs.initLineNumbersOnLoad();</script>"
                          "  <pre><code class='html'>%s</code></pre>"
                          "</body>",
                          encoded_uri,
                          encoded_str);

  finish_uri_scheme_request (request, g_steal_pointer (&html), NULL);
}

static void
ephy_view_source_request_begin_get_source_from_web_view (EphyViewSourceRequest *request,
                                                         WebKitWebView         *web_view)
{
  WebKitWebResource *resource = webkit_web_view_get_main_resource (web_view);
  g_assert (resource);
  webkit_web_resource_get_data (resource,
                                request->cancellable,
                                (GAsyncReadyCallback)(web_resource_data_cb),
                                request);
}

static void
load_changed_cb (WebKitWebView         *web_view,
                 WebKitLoadEvent        load_event,
                 EphyViewSourceRequest *request)
{
  if (load_event == WEBKIT_LOAD_FINISHED) {
    g_signal_handler_disconnect (request->web_view, request->load_changed_id);
    request->load_changed_id = 0;

    ephy_view_source_request_begin_get_source_from_web_view (request, web_view);
  }
}

static void
ephy_view_source_request_begin_get_source_from_uri (EphyViewSourceRequest *request,
                                                    const char            *uri)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitWebContext *context = ephy_embed_shell_get_web_context (shell);

  request->web_view = WEBKIT_WEB_VIEW (g_object_ref_sink (g_object_new (WEBKIT_TYPE_WEB_VIEW,
                                                                        "web-context", context,
                                                                        NULL)));

  g_assert (request->load_changed_id == 0);
  request->load_changed_id = g_signal_connect (request->web_view, "load-changed",
                                               G_CALLBACK (load_changed_cb),
                                               request);

  webkit_web_view_load_uri (request->web_view, uri);
}

static gint
embed_is_displaying_matching_uri (EphyEmbed *embed,
                                  char      *uri)
{
  EphyWebView *web_view;

  if (ephy_embed_has_load_pending (embed))
    return -1;

  web_view = ephy_embed_get_web_view (embed);
  if (ephy_web_view_is_loading (web_view))
    return -1;

  return g_strcmp0 (ephy_web_view_get_address (web_view), uri);
}

static WebKitWebView *
get_web_view_matching_uri (const char *uri)
{
  EphyEmbedShell *shell;
  GtkWindow *window;
  GList *embeds = NULL;
  GList *found;
  EphyEmbed *embed = NULL;

  shell = ephy_embed_shell_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (shell));

  if (!EPHY_IS_EMBED_CONTAINER (window))
    goto out;

  embeds = ephy_embed_container_get_children (EPHY_EMBED_CONTAINER (window));
  found = g_list_find_custom (embeds, uri, (GCompareFunc)embed_is_displaying_matching_uri);

  if (found)
    embed = found->data;

out:
  g_list_free (embeds);

  return embed ? WEBKIT_WEB_VIEW (ephy_embed_get_web_view (embed)) : NULL;
}

static void
ephy_view_source_request_start (EphyViewSourceRequest *request)
{
  const char *converted_uri;
  const char *original_uri;
  WebKitWebView *web_view;

  request->source_handler->outstanding_requests =
    g_list_prepend (request->source_handler->outstanding_requests, request);

  original_uri = webkit_uri_scheme_request_get_uri (request->scheme_request);

  /* Convert e.g. view-source:https://gnome.org to https://gnome.org */
  converted_uri = original_uri + strlen (EPHY_VIEW_SOURCE_SCHEME) + 1;

  web_view = get_web_view_matching_uri (converted_uri);
  if (web_view)
    ephy_view_source_request_begin_get_source_from_web_view (request, WEBKIT_WEB_VIEW (web_view));
  else {
    ephy_view_source_request_begin_get_source_from_uri (request, converted_uri);
  }
}

static void
cancel_outstanding_request (EphyViewSourceRequest *request)
{
  g_cancellable_cancel (request->cancellable);
}

static void
ephy_view_source_handler_dispose (GObject *object)
{
  EphyViewSourceHandler *handler = EPHY_VIEW_SOURCE_HANDLER (object);

  if (handler->outstanding_requests) {
    g_list_foreach (handler->outstanding_requests, (GFunc)cancel_outstanding_request, NULL);
    g_list_free (handler->outstanding_requests);
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

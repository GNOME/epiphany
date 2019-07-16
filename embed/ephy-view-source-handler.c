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
#include "ephy-web-view.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
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
  guchar *data;
  char *data_str;
  char *escaped_str;
  char *html;
  gsize length;
  GError *error = NULL;

  data = webkit_web_resource_get_data_finish (resource, result, &length, &error);
  if (error) {
    finish_uri_scheme_request (request, NULL, error);
    g_error_free (error);
    return;
  }

  data_str = g_malloc (length + 1);
  strncpy (data_str, (const char *)data, length);
  data_str[length] = '\0';
  g_free (data);

  escaped_str = g_markup_escape_text (data_str, -1);
  g_free (data_str);

  html = g_strdup_printf ("<body>"
                          "  <pre>"
                          "    <code class=\"language-html\">%s</code>"
                          "  </pre>"
                          "</body>",
                          escaped_str);
  g_free (escaped_str);

  finish_uri_scheme_request (request, html, NULL);
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
  if (load_event == WEBKIT_LOAD_FINISHED)
    ephy_view_source_request_begin_get_source_from_web_view (request, web_view);
}

static void
ephy_view_source_request_begin_get_source_from_uri (EphyViewSourceRequest *request,
                                                    const char            *uri)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitWebContext *context = ephy_embed_shell_get_web_context (shell);

  request->web_view = WEBKIT_WEB_VIEW (g_object_ref_sink (webkit_web_view_new_with_context (context)));

  g_assert (request->load_changed_id == 0);
  request->load_changed_id = g_signal_connect (request->web_view, "load-changed",
                                               G_CALLBACK (load_changed_cb),
                                               request);

  webkit_web_view_load_uri (request->web_view, uri);
}

static gint
embed_is_displaying_matching_uri (EphyEmbed *embed,
                                  SoupURI   *uri)
{
  EphyWebView *web_view;
  SoupURI *view_uri;
  gint ret = -1;

  if (ephy_embed_has_load_pending (embed))
    return -1;

  web_view = ephy_embed_get_web_view (embed);
  if (ephy_web_view_is_loading (web_view))
    return -1;

  view_uri = soup_uri_new (ephy_web_view_get_address (web_view));
  if (!view_uri)
    return -1;

  soup_uri_set_fragment (view_uri, NULL);
  ret = soup_uri_equal (view_uri, uri) ? 0 : -1;

  soup_uri_free (view_uri);

  return ret;
}

static WebKitWebView *
get_web_view_matching_uri (SoupURI *uri)
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
  SoupURI *soup_uri;
  char *modified_uri;
  char *decoded_fragment;
  const char *original_uri;
  WebKitWebView *web_view;

  request->source_handler->outstanding_requests =
    g_list_prepend (request->source_handler->outstanding_requests, request);

  original_uri = webkit_uri_scheme_request_get_uri (request->scheme_request);
  soup_uri = soup_uri_new (original_uri);

  if (!soup_uri || !soup_uri->fragment) {
    /* Can't assert because user could theoretically input something weird */
    GError *error = g_error_new (WEBKIT_NETWORK_ERROR,
                                 WEBKIT_NETWORK_ERROR_FAILED,
                                 _("%s is not a valid URI"),
                                 original_uri);
    finish_uri_scheme_request (request, NULL, error);
    g_error_free (error);
    return;
  }

  /* Convert e.g. ephy-source://gnome.org#https to https://gnome.org */
  decoded_fragment = soup_uri_decode (soup_uri->fragment);
  soup_uri_set_scheme (soup_uri, decoded_fragment);
  soup_uri_set_fragment (soup_uri, NULL);
  modified_uri = soup_uri_to_string (soup_uri, FALSE);
  g_assert (modified_uri);

  web_view = get_web_view_matching_uri (soup_uri);
  if (web_view)
    ephy_view_source_request_begin_get_source_from_web_view (request, WEBKIT_WEB_VIEW (web_view));
  else
    ephy_view_source_request_begin_get_source_from_uri (request, modified_uri);

  g_free (decoded_fragment);
  g_free (modified_uri);
  soup_uri_free (soup_uri);
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

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016, 2018 Igalia S.L.
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
#include "ephy-source-transform-handler.h"

#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-web-view.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

typedef struct {
  GObject parent_instance;

  GList *outstanding_requests;
} EphySourceTransformHandlerPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EphySourceTransformHandler, ephy_source_transform_handler, G_TYPE_OBJECT)

typedef struct {
  EphySourceTransformHandler *transform_handler;
  WebKitURISchemeRequest *scheme_request;
  WebKitWebView *web_view;
  GCancellable *cancellable;
  guint load_changed_id;
} EphySourceTransformRequest;

static EphySourceTransformRequest *
ephy_source_transform_request_new (EphySourceTransformHandler *handler,
                                   WebKitURISchemeRequest     *request)
{
  EphySourceTransformRequest *source_transform_request;

  source_transform_request = g_slice_new (EphySourceTransformRequest);
  source_transform_request->transform_handler = g_object_ref (handler);
  source_transform_request->scheme_request = g_object_ref (request);
  source_transform_request->web_view = NULL; /* created only if required */
  source_transform_request->cancellable = g_cancellable_new ();
  source_transform_request->load_changed_id = 0;

  return source_transform_request;
}

static void
ephy_source_transform_request_free (EphySourceTransformRequest *request)
{
  if (request->load_changed_id > 0)
    g_signal_handler_disconnect (request->web_view, request->load_changed_id);

  g_object_unref (request->transform_handler);
  g_object_unref (request->scheme_request);
  g_clear_object (&request->web_view);

  g_cancellable_cancel (request->cancellable);
  g_object_unref (request->cancellable);

  g_slice_free (EphySourceTransformRequest, request);
}

static void
finish_uri_scheme_request (EphySourceTransformRequest *request,
                           gchar                      *data,
                           GError                     *error)
{
  EphySourceTransformHandlerPrivate *priv = ephy_source_transform_handler_get_instance_private (request->transform_handler);
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

  priv->outstanding_requests = g_list_remove (priv->outstanding_requests, request);

  ephy_source_transform_request_free (request);
}

static void
web_resource_data_cb (WebKitWebResource          *resource,
                      GAsyncResult               *result,
                      EphySourceTransformRequest *request)
{
  EphySourceTransformHandler *handler = request->transform_handler;
  guchar *source;
  char *html;
  gsize length;
  GError *error = NULL;

  source = webkit_web_resource_get_data_finish (resource, result, &length, &error);
  if (error) {
    finish_uri_scheme_request (request, NULL, error);
    g_error_free (error);
    return;
  }

  g_assert (EPHY_SOURCE_TRANSFORM_HANDLER_GET_CLASS (handler)->transform_source != NULL);
  /* FIXME: Encoding schenanigans here */
  html = (char *)EPHY_SOURCE_TRANSFORM_HANDLER_GET_CLASS (handler)->transform_source (handler, source, length);

  finish_uri_scheme_request (request, html, NULL);

  g_free (source);
}

static void
ephy_source_transform_request_begin_get_source_from_web_view (EphySourceTransformRequest *request,
                                                              WebKitWebView              *web_view)
{
  WebKitWebResource *resource = webkit_web_view_get_main_resource (web_view);
  g_assert (resource);
  webkit_web_resource_get_data (resource,
                                request->cancellable,
                                (GAsyncReadyCallback)(web_resource_data_cb),
                                request);
}

static void
load_changed_cb (WebKitWebView              *web_view,
                 WebKitLoadEvent             load_event,
                 EphySourceTransformRequest *request)
{
  if (load_event == WEBKIT_LOAD_FINISHED)
    ephy_source_transform_request_begin_get_source_from_web_view (request, web_view);
}

static void
ephy_source_transform_request_begin_get_source_from_uri (EphySourceTransformRequest *request,
                                                         const char                 *uri)
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
ephy_source_transform_request_start (EphySourceTransformRequest *request)
{
  EphySourceTransformHandlerPrivate *priv = ephy_source_transform_handler_get_instance_private (request->transform_handler);
  SoupURI *soup_uri;
  char *modified_uri;
  char *decoded_fragment;
  const char *original_uri;
  WebKitWebView *web_view;

  priv->outstanding_requests = g_list_prepend (priv->outstanding_requests, request);

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

  /* FIXME: Switch to ephy-source:https://gnome.org? */
  /* Convert e.g. ephy-source://gnome.org#https to https://gnome.org */
  decoded_fragment = soup_uri_decode (soup_uri->fragment);
  soup_uri_set_scheme (soup_uri, decoded_fragment);
  soup_uri_set_fragment (soup_uri, NULL);
  modified_uri = soup_uri_to_string (soup_uri, FALSE);
  g_assert (modified_uri);

  web_view = get_web_view_matching_uri (soup_uri);
  if (web_view)
    ephy_source_transform_request_begin_get_source_from_web_view (request, WEBKIT_WEB_VIEW (web_view));
  else
    ephy_source_transform_request_begin_get_source_from_uri (request, modified_uri);

  g_free (decoded_fragment);
  g_free (modified_uri);
  soup_uri_free (soup_uri);
}

static void
cancel_outstanding_request (EphySourceTransformRequest *request)
{
  g_cancellable_cancel (request->cancellable);
}

static void
ephy_source_transform_handler_dispose (GObject *object)
{
  EphySourceTransformHandlerPrivate *priv = ephy_source_transform_handler_get_instance_private (EPHY_SOURCE_TRANSFORM_HANDLER (object));

  if (priv->outstanding_requests) {
    g_list_foreach (priv->outstanding_requests, (GFunc)cancel_outstanding_request, NULL);
    g_list_free (priv->outstanding_requests);
    priv->outstanding_requests = NULL;
  }

  G_OBJECT_CLASS (ephy_source_transform_handler_parent_class)->dispose (object);
}

static void
ephy_source_transform_handler_init (EphySourceTransformHandler *handler)
{
}

static void
ephy_source_transform_handler_class_init (EphySourceTransformHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_source_transform_handler_dispose;
}

void
ephy_source_transform_handler_handle_request (EphySourceTransformHandler *handler,
                                              WebKitURISchemeRequest     *scheme_request)
{
  EphySourceTransformRequest *source_transform_request;

  source_transform_request = ephy_source_transform_request_new (handler, scheme_request);
  ephy_source_transform_request_start (source_transform_request);
}

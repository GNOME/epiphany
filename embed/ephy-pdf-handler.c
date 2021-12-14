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
#include "ephy-pdf-handler.h"

#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-output-encoding.h"
#include "ephy-web-view.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

struct _EphyPDFHandler {
  GObject parent_instance;

  GList *outstanding_requests;
};

G_DEFINE_TYPE (EphyPDFHandler, ephy_pdf_handler, G_TYPE_OBJECT)

typedef struct {
  EphyPDFHandler *source_handler;
  WebKitURISchemeRequest *scheme_request;
  GCancellable *cancellable;
  EphyDownload *download;
  char *file_name;
} EphyPdfRequest;

static EphyPdfRequest *
ephy_pdf_request_new (EphyPDFHandler         *handler,
                      WebKitURISchemeRequest *request)
{
  EphyPdfRequest *pdf_request;

  pdf_request = g_new0 (EphyPdfRequest, 1);
  pdf_request->source_handler = g_object_ref (handler);
  pdf_request->scheme_request = g_object_ref (request);
  pdf_request->cancellable = g_cancellable_new ();

  return pdf_request;
}

static void
ephy_pdf_request_free (EphyPdfRequest *request)
{
  if (request->download) {
    g_signal_handlers_disconnect_by_data (request->download, request);

    if (ephy_download_is_active (request->download))
      ephy_download_cancel (request->download);
  }

  g_object_unref (request->source_handler);
  g_object_unref (request->scheme_request);
  g_clear_pointer (&request->file_name, g_free);

  g_cancellable_cancel (request->cancellable);
  g_object_unref (request->cancellable);

  g_free (request);
}

static void
finish_uri_scheme_request (EphyPdfRequest *request,
                           gchar          *data,
                           GError         *error)
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

  ephy_pdf_request_free (request);
}

static void
pdf_file_deleted (GObject      *source,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  if (!g_file_delete_finish (G_FILE (source), res, &error))
    g_warning ("Could not delete temporary PDF file: %s", error->message);
}

static void
pdf_file_loaded (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  EphyPdfRequest *self = user_data;
  g_autoptr (GBytes) html_file = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GString) html = NULL;
  g_autofree char *file_data = NULL;
  g_autofree char *encoded_file_data = NULL;
  g_autofree char *encoded_filename = NULL;
  gsize len = 0;

  if (!g_file_load_contents_finish (G_FILE (source), res, &file_data, &len, NULL, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Could not read PDF file content: %s", error->message);
    return;
  }

  g_file_delete_async (G_FILE (source), G_PRIORITY_DEFAULT, NULL, pdf_file_deleted, NULL);

  html = g_string_new (NULL);
  html_file = g_resources_lookup_data ("/org/gnome/epiphany/pdfjs/web/viewer.html", 0, NULL);
  encoded_file_data = g_base64_encode ((const guchar *)file_data, len);
  encoded_filename = self->file_name ? ephy_encode_for_html_attribute (self->file_name) : g_strdup ("");
  g_string_printf (html, g_bytes_get_data (html_file, NULL), encoded_file_data, encoded_filename);

  finish_uri_scheme_request (self, g_strdup (html->str), NULL);
}

static void
download_completed_cb (EphyDownload   *download,
                       EphyPdfRequest *self)
{
  g_assert (download);
  g_assert (self);

  g_signal_handlers_disconnect_by_data (download, self);

  if (g_strcmp0 ("application/pdf", ephy_download_get_content_type (download)) == 0) {
    g_autoptr (GFile) file = NULL;
    const char *document_uri = webkit_download_get_destination (ephy_download_get_webkit_download (download));

    file = g_file_new_for_uri (document_uri);

    g_file_load_contents_async (file, self->cancellable, pdf_file_loaded, self);
  } else {
    g_warning ("PDF %s has invalid MIME type: %s",
               ephy_download_get_destination_uri (download),
               ephy_download_get_content_type (download));
  }

  g_clear_object (&self->download);
}

static void
download_errored_cb (EphyDownload   *download,
                     GError         *error,
                     EphyPdfRequest *self)
{
  g_assert (download);
  g_assert (error);
  g_assert (self);

  g_signal_handlers_disconnect_by_data (download, self);

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    WebKitURIRequest *request = webkit_download_get_request (ephy_download_get_webkit_download (download));

    g_warning ("Cannot fetch pdf from <%s>: %s",
               webkit_uri_request_get_uri (request),
               error ? error->message : "Unknown error");
  }

  g_clear_object (&self->download);
}

static gboolean
decide_destination_cb (WebKitDownload *wk_download,
                       const gchar    *suggested_filename,
                       gpointer        user_data)
{
  EphyPdfRequest *request = user_data;
  g_autofree gchar *tmp_file = NULL;
  g_autofree gchar *file_uri = NULL;

  tmp_file = g_strdup_printf ("%s/%s", g_get_tmp_dir (), g_path_get_basename (suggested_filename));
  file_uri = g_filename_to_uri (tmp_file, NULL, NULL);
  ephy_download_set_destination_uri (request->download, file_uri);

  g_clear_pointer (&request->file_name, g_free);
  request->file_name = g_path_get_basename (suggested_filename);

  return TRUE;
}

static void
ephy_pdf_request_start (EphyPdfRequest *request)
{
  const char *modified_uri;
  const char *original_uri;

  request->source_handler->outstanding_requests =
    g_list_prepend (request->source_handler->outstanding_requests, request);

  original_uri = webkit_uri_scheme_request_get_uri (request->scheme_request);
  g_assert (g_str_has_prefix (original_uri, "ephy-pdf:"));
  modified_uri = original_uri + strlen ("ephy-pdf:");

  request->download = ephy_download_new_for_uri_internal (modified_uri);
  ephy_download_disable_desktop_notification (request->download);
  webkit_download_set_allow_overwrite (ephy_download_get_webkit_download (request->download), TRUE);

  g_signal_connect (request->download, "completed", G_CALLBACK (download_completed_cb), request);
  g_signal_connect (request->download, "error", G_CALLBACK (download_errored_cb), request);
  g_signal_connect (ephy_download_get_webkit_download (request->download), "decide-destination", G_CALLBACK (decide_destination_cb), request);
}

static void
cancel_outstanding_request (EphyPdfRequest *request)
{
  g_cancellable_cancel (request->cancellable);
}

static void
ephy_pdf_handler_dispose (GObject *object)
{
  EphyPDFHandler *handler = EPHY_PDF_HANDLER (object);

  if (handler->outstanding_requests) {
    g_list_foreach (handler->outstanding_requests, (GFunc)cancel_outstanding_request, NULL);
    g_list_free (handler->outstanding_requests);
    handler->outstanding_requests = NULL;
  }

  G_OBJECT_CLASS (ephy_pdf_handler_parent_class)->dispose (object);
}

static void
ephy_pdf_handler_init (EphyPDFHandler *handler)
{
}

static void
ephy_pdf_handler_class_init (EphyPDFHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_pdf_handler_dispose;
}

EphyPDFHandler *
ephy_pdf_handler_new (void)
{
  return EPHY_PDF_HANDLER (g_object_new (EPHY_TYPE_PDF_HANDLER, NULL));
}

void
ephy_pdf_handler_handle_request (EphyPDFHandler         *handler,
                                 WebKitURISchemeRequest *scheme_request)
{
  EphyPdfRequest *pdf_request;

  pdf_request = ephy_pdf_request_new (handler, scheme_request);
  ephy_pdf_request_start (pdf_request);
}

void
ephy_pdf_handler_stop (EphyPDFHandler *handler,
                       WebKitWebView  *web_view)
{
  GList *list;

  for (list = handler->outstanding_requests; list; list = list->next) {
    EphyPdfRequest *request = list->data;
    WebKitWebView *request_web_view = webkit_uri_scheme_request_get_web_view (request->scheme_request);

    if (request_web_view == web_view) {
      ephy_pdf_request_free (request);
      return;
    }
  }
}

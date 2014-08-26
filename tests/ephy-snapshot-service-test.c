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
 *
 */

#include "config.h"
#include "ephy-debug.h"
#include "ephy-snapshot-service.h"

#include <libsoup/soup.h>
#include <string.h>

#define TEST_SERVER_URI "http://127.0.0.1:45716"
static time_t mtime;
SoupServer *server;

static gboolean
quit_when_test_done (GtkWidget *w,
                     GdkEvent *event,
                     gint *tests)
{
  if (--(*tests) == 0)
    gtk_main_quit ();

  return FALSE;
}

static void
on_snapshot_ready (GObject *source,
                   GAsyncResult *res,
                   gint *tests)
{
  GdkPixbuf *pixbuf;
#if 0
  GtkWidget *w,*i;
#endif
  GError *error = NULL;

  pixbuf = ephy_snapshot_service_get_snapshot_finish (EPHY_SNAPSHOT_SERVICE (source),
                                                      res, NULL, &error);
  g_assert (GDK_IS_PIXBUF (pixbuf) || error != NULL);

  if (error) {
    g_print ("Error loading pixbuf: %s\n", error->message);
    g_error_free (error);
    quit_when_test_done (NULL, NULL, tests);
    return;
  } else

#if 0
  w = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  i = gtk_image_new_from_pixbuf (pixbuf);
  gtk_container_add (GTK_CONTAINER (w), i);
  gtk_widget_show_all (w);
  g_signal_connect (w, "delete-event",
                    G_CALLBACK (quit_when_test_done), tests);
#else
  quit_when_test_done (NULL, NULL, tests);
#endif
}

static void
test_snapshot (void)
{
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  WebKitWebView *webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  gint tests = 1;

  webkit_web_view_load_uri (webview, TEST_SERVER_URI);
  ephy_snapshot_service_get_snapshot_async (service,
                                            webview,
                                            mtime,
                                            NULL,
                                            (GAsyncReadyCallback)on_snapshot_ready,
                                            &tests);
  gtk_main ();
}

static void
test_cached_snapshot (void)
{
  gint tests = 1;
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  WebKitWebView *webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());

  webkit_web_view_load_uri (webview, TEST_SERVER_URI);
  ephy_snapshot_service_get_snapshot_async (service,
                                            webview,
                                            mtime,
                                            NULL,
                                            (GAsyncReadyCallback)on_snapshot_ready,
                                            &tests);
  gtk_main ();
}

static void
test_many_snapshots (void)
{
  WebKitWebView *webview;
  gint tests = 3;
  EphySnapshotService *service = ephy_snapshot_service_get_default ();

  webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  webkit_web_view_load_uri (webview, TEST_SERVER_URI "/some");
  ephy_snapshot_service_get_snapshot_async (service,
                                            webview,
                                            mtime,
                                            NULL,
                                            (GAsyncReadyCallback)on_snapshot_ready,
                                            &tests);

  webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  webkit_web_view_load_uri (webview, TEST_SERVER_URI "/other");
  ephy_snapshot_service_get_snapshot_async (service,
                                            webview,
                                            mtime,
                                            NULL,
                                            (GAsyncReadyCallback)on_snapshot_ready,
                                            &tests);

  webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  webkit_web_view_load_uri (webview, TEST_SERVER_URI "/place");
  ephy_snapshot_service_get_snapshot_async (service,
                                            webview,
                                            mtime,
                                            NULL,
                                            (GAsyncReadyCallback)on_snapshot_ready,
                                            &tests);
  gtk_main ();
}

static void
test_snapshot_with_cancellable (void)
{
  gint tests = 1;
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  WebKitWebView *webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  GCancellable *cancellable = g_cancellable_new ();

  webkit_web_view_load_uri (webview, TEST_SERVER_URI "/and");
  ephy_snapshot_service_get_snapshot_async (service,
                                            webview,
                                            mtime,
                                            cancellable,
                                            (GAsyncReadyCallback)on_snapshot_ready,
                                            &tests);
  gtk_main ();
}

static void
test_already_cancelled_snapshot (void)
{
  gint tests = 1;
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  GCancellable *cancellable = g_cancellable_new ();
  WebKitWebView *webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  webkit_web_view_load_uri (webview, TEST_SERVER_URI "/so");

  g_cancellable_cancel (cancellable);
  ephy_snapshot_service_get_snapshot_async (service,
                                            webview,
                                            mtime,
                                            cancellable,
                                            (GAsyncReadyCallback)on_snapshot_ready,
                                            &tests);
  gtk_main ();
}

static gboolean
cancel (GCancellable *cancellable)
{
  g_cancellable_cancel (cancellable);
  return FALSE;
}

static void
test_snapshot_and_timed_cancellation (void)
{
  gint tests = 1;
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  GCancellable *cancellable = g_cancellable_new ();
  WebKitWebView *webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  webkit_web_view_load_uri (webview, TEST_SERVER_URI "/on");

  ephy_snapshot_service_get_snapshot_async (service,
                                            webview,
                                            mtime,
                                            cancellable,
                                            (GAsyncReadyCallback)on_snapshot_ready,
                                            &tests);
  g_timeout_add (15, (GSourceFunc)cancel, cancellable);
  gtk_main ();
}

static void
server_callback (SoupServer *server, SoupMessage *msg,
                 const char *path, GHashTable *query,
                 SoupClientContext *context, gpointer data)
{
  const char *response = "<html><h1>This is a header</h1></html>";

  if (msg->method == SOUP_METHOD_GET) {
		soup_message_set_status (msg, SOUP_STATUS_OK);
		soup_message_body_append (msg->response_body, SOUP_MEMORY_STATIC,
                              response, strlen (response));
		soup_message_body_complete (msg->response_body);
  }
  else
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);
  ephy_debug_init ();

  server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "snapshot-service-test-server",
                            NULL);
	soup_server_add_handler (server, NULL,
                           server_callback, NULL, NULL);
  soup_server_listen_local (server, 45716,
                            SOUP_SERVER_LISTEN_IPV4_ONLY,
                            NULL);
  mtime = time(NULL);

  g_test_add_func ("/lib/ephy-snapshot-service/test_snapshot",
                   test_snapshot);
  g_test_add_func ("/lib/ephy-snapshot-service/test_cached_snapshot",
                   test_cached_snapshot);
  g_test_add_func ("/lib/ephy-snapshot-service/test_many_snapshots",
                   test_many_snapshots);
  g_test_add_func ("/lib/ephy-snapshot-service/test_snapshot_with_cancellable",
                   test_snapshot_with_cancellable);
  g_test_add_func ("/lib/ephy-snapshot-service/test_already_cancelled_snapshot",
                   test_already_cancelled_snapshot);
  g_test_add_func ("/lib/ephy-snapshot-service/test_snapshot_and_timed_cancellation",
                   test_snapshot_and_timed_cancellation);
  return g_test_run ();
}

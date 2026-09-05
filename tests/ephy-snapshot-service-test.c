/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#include <glib.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <webkit/webkit.h>

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-snapshot-service.h"
#include "ephy-test-utils.h"

static char *base_uri;

typedef struct {
  GMainLoop *loop;
  char *snapshot_path;
  GError *error;
  guint timeout_id;
} SnapshotTestData;

static void
on_watchdog_timeout (gpointer user_data)
{
  g_error ("Test timed out waiting for snapshot operation");
}

static void
snapshot_test_data_init (SnapshotTestData *data)
{
  data->loop = g_main_loop_new (NULL, FALSE);
  data->snapshot_path = NULL;
  data->error = NULL;
  data->timeout_id = g_timeout_add_seconds_once (10, on_watchdog_timeout, NULL);
}

static void
snapshot_test_data_clear (SnapshotTestData *data)
{
  g_clear_handle_id (&data->timeout_id, g_source_remove);
  g_clear_pointer (&data->loop, g_main_loop_unref);
  g_clear_pointer (&data->snapshot_path, g_free);
  g_clear_error (&data->error);
}

static void
on_snapshot_path_ready (GObject      *source,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  SnapshotTestData *data = user_data;

  data->snapshot_path = ephy_snapshot_service_get_snapshot_path_finish (EPHY_SNAPSHOT_SERVICE (source),
                                                                        res,
                                                                        &data->error);
  g_main_loop_quit (data->loop);
}

static WebKitWebView *
create_web_view (void)
{
  return WEBKIT_WEB_VIEW (g_object_ref_sink (G_OBJECT (webkit_web_view_new ())));
}

static void
server_callback (SoupServer        *soup_server,
                 SoupServerMessage *msg,
                 const char        *path,
                 GHashTable        *query,
                 gpointer           user_data)
{
  const char *html = "<html><body style='background-color: green;'><h1>Snapshot Test</h1></body></html>";
  SoupMessageBody *body = soup_server_message_get_response_body (msg);

  soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
  soup_message_body_append (body, SOUP_MEMORY_STATIC, html, strlen (html));
  soup_message_body_complete (body);
}

static void
test_snapshot (void)
{
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  g_autoptr (WebKitWebView) view = create_web_view ();
  g_autofree char *url = g_strconcat (base_uri, "/snapshot", NULL);
  SnapshotTestData data;

  snapshot_test_data_init (&data);
  webkit_web_view_load_uri (view, url);

  ephy_snapshot_service_get_snapshot_path_async (service,
                                                 view,
                                                 NULL,
                                                 on_snapshot_path_ready,
                                                 &data);
  g_main_loop_run (data.loop);

  g_assert_no_error (data.error);
  g_assert_nonnull (data.snapshot_path);
  g_assert_true (g_file_test (data.snapshot_path, G_FILE_TEST_EXISTS));

  snapshot_test_data_clear (&data);
}

static void
test_cached_snapshot (void)
{
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  g_autoptr (WebKitWebView) view = create_web_view ();
  g_autofree char *url = g_strconcat (base_uri, "/snapshot", NULL);
  const char *cached_path;
  SnapshotTestData data;

  cached_path = ephy_snapshot_service_lookup_cached_snapshot_path (service, url);
  g_assert_nonnull (cached_path);
  g_assert_true (g_file_test (cached_path, G_FILE_TEST_EXISTS));

  snapshot_test_data_init (&data);
  webkit_web_view_load_uri (view, url);

  ephy_snapshot_service_get_snapshot_path_async (service,
                                                 view,
                                                 NULL,
                                                 on_snapshot_path_ready,
                                                 &data);
  g_main_loop_run (data.loop);

  g_assert_no_error (data.error);
  g_assert_cmpstr (data.snapshot_path, ==, cached_path);

  snapshot_test_data_clear (&data);
}

typedef struct {
  GMainLoop *loop;
  guint remaining;
  guint timeout_id;
} MultiSnapshotData;

static void
on_multi_snapshot_ready (GObject      *source,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  MultiSnapshotData *mdata = user_data;
  g_autoptr (GError) error = NULL;
  g_autofree char *path = NULL;

  path = ephy_snapshot_service_get_snapshot_path_finish (EPHY_SNAPSHOT_SERVICE (source),
                                                         res,
                                                         &error);
  g_assert_no_error (error);
  g_assert_nonnull (path);
  g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));

  mdata->remaining--;
  if (mdata->remaining == 0)
    g_main_loop_quit (mdata->loop);
}

static void
test_many_snapshots (void)
{
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  g_autoptr (GPtrArray) views = g_ptr_array_new_full (3, g_object_unref);
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  MultiSnapshotData mdata;
  const char *paths[3] = { "/page1", "/page2", "/page3" };

  mdata.loop = loop;
  mdata.remaining = 3;
  mdata.timeout_id = g_timeout_add_seconds_once (15, on_watchdog_timeout, NULL);

  for (guint i = 0; i < 3; i++) {
    g_autofree char *url = g_strconcat (base_uri, paths[i], NULL);
    WebKitWebView *view = create_web_view ();

    g_ptr_array_add (views, view);
    webkit_web_view_load_uri (view, url);
    ephy_snapshot_service_get_snapshot_path_async (service,
                                                   view,
                                                   NULL,
                                                   on_multi_snapshot_ready,
                                                   &mdata);
  }

  g_main_loop_run (mdata.loop);

  g_clear_handle_id (&mdata.timeout_id, g_source_remove);
}

static void
test_snapshot_with_cancellable (void)
{
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  g_autoptr (WebKitWebView) view = create_web_view ();
  g_autofree char *url = g_strconcat (base_uri, "/with-cancellable", NULL);
  g_autoptr (GCancellable) cancellable = g_cancellable_new ();
  SnapshotTestData data;

  snapshot_test_data_init (&data);
  webkit_web_view_load_uri (view, url);

  ephy_snapshot_service_get_snapshot_path_async (service,
                                                 view,
                                                 cancellable,
                                                 on_snapshot_path_ready,
                                                 &data);
  g_cancellable_cancel (cancellable);
  g_main_loop_run (data.loop);

  g_assert_null (data.snapshot_path);
  g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  snapshot_test_data_clear (&data);
}

static void
test_already_cancelled_snapshot (void)
{
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  g_autoptr (WebKitWebView) view = create_web_view ();
  g_autofree char *url = g_strconcat (base_uri, "/already-cancelled", NULL);
  g_autoptr (GCancellable) cancellable = g_cancellable_new ();
  SnapshotTestData data;

  snapshot_test_data_init (&data);
  webkit_web_view_load_uri (view, url);

  g_cancellable_cancel (cancellable);
  ephy_snapshot_service_get_snapshot_path_async (service,
                                                 view,
                                                 cancellable,
                                                 on_snapshot_path_ready,
                                                 &data);
  g_main_loop_run (data.loop);

  g_assert_null (data.snapshot_path);
  g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  snapshot_test_data_clear (&data);
}

static void
test_delete_snapshot (void)
{
  EphySnapshotService *service = ephy_snapshot_service_get_default ();
  g_autoptr (WebKitWebView) view = create_web_view ();
  g_autofree char *url = g_strconcat (base_uri, "/to-delete", NULL);
  SnapshotTestData data;
  g_autofree char *saved_path = NULL;

  snapshot_test_data_init (&data);
  webkit_web_view_load_uri (view, url);

  ephy_snapshot_service_get_snapshot_path_async (service,
                                                 view,
                                                 NULL,
                                                 on_snapshot_path_ready,
                                                 &data);
  g_main_loop_run (data.loop);

  g_assert_no_error (data.error);
  g_assert_nonnull (data.snapshot_path);
  g_assert_true (g_file_test (data.snapshot_path, G_FILE_TEST_EXISTS));
  saved_path = g_strdup (data.snapshot_path);

  ephy_snapshot_service_delete_snapshot_for_url (service, url);

  /* Allow async delete callback to run */
  while (g_file_test (saved_path, G_FILE_TEST_EXISTS))
    g_main_context_iteration (NULL, TRUE);

  g_assert_false (g_file_test (saved_path, G_FILE_TEST_EXISTS));

  snapshot_test_data_clear (&data);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr (SoupServer) server = NULL;
  g_autoptr (GError) error = NULL;
  GSList *uris;
  int ret;

  /* Disable accelerated compositing for headless CI environments */
  g_setenv ("WEBKIT_DISABLE_COMPOSITING_MODE", "1", FALSE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("NO_AT_BRIDGE", "1", TRUE);
  g_setenv ("GTK_A11Y", "none", TRUE);

  g_test_init (&argc, &argv, NULL);
  g_test_log_set_fatal_handler (ephy_test_utils_log_fatal_func, NULL);
  gtk_init ();

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL, EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS, &error))
    g_error ("Something wrong happened with ephy_file_helpers_init(): %s", error->message);

  server = soup_server_new (NULL, NULL);
  soup_server_add_handler (server, NULL, server_callback, NULL, NULL);
  soup_server_listen_local (server, 0, 0, &error);
  g_assert_no_error (error);

  uris = soup_server_get_uris (server);
  g_assert_nonnull (uris);
  base_uri = g_uri_to_string ((GUri *)uris->data);
  g_slist_free_full (uris, (GDestroyNotify)g_uri_unref);

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
  g_test_add_func ("/lib/ephy-snapshot-service/test_delete_snapshot",
                   test_delete_snapshot);

  ret = g_test_run ();

  soup_server_disconnect (server);
  g_free (base_uri);
  ephy_file_helpers_shutdown ();

  return ret;
}

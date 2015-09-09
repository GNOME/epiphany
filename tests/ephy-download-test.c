/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-download-test.c
 * This file is part of Epiphany
 *
 * Copyright © 2011 - Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ephy-debug.h"
#include "ephy-download.h"
#include "ephy-embed-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"
#include "ephy-shell.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <string.h>

#define HTML_STRING "testing-ephy-download"
SoupURI *base_uri;

static char *
get_uri_for_path (const char *path)
{
  SoupURI *uri;
  char *uri_string;

  uri = soup_uri_new_with_base (base_uri, path);
  uri_string = soup_uri_to_string (uri, FALSE);
  soup_uri_free (uri);

  return uri_string;
}

static void
server_callback (SoupServer *server,
                 SoupMessage *msg,
                 const char *path,
                 GHashTable *query,
                 SoupClientContext *context,
                 gpointer data)
{
  soup_message_set_status (msg, SOUP_STATUS_OK);

  if (g_str_equal (path, "/cancelled"))
    soup_message_set_status (msg, SOUP_STATUS_CANT_CONNECT);

  soup_message_body_append (msg->response_body, SOUP_MEMORY_STATIC,
                            HTML_STRING, strlen (HTML_STRING));

  soup_message_body_complete (msg->response_body);
}

typedef struct {
  GMainLoop *loop;
  EphyDownload *download;
  char *destination;
  char *source;
} Fixture;

static void
fixture_setup (Fixture *fixture, gconstpointer data)
{
  char *tmp_filename;
  char *dest_file;

  tmp_filename = ephy_file_tmp_filename (".ephy-download-XXXXXX", NULL);
  dest_file = g_build_filename (ephy_file_tmp_dir (), tmp_filename, NULL);

  fixture->source = get_uri_for_path ("/default");
  fixture->download = ephy_download_new_for_uri (fixture->source, NULL);
  fixture->destination = g_filename_to_uri (dest_file, NULL, NULL);
  fixture->loop = g_main_loop_new (NULL, TRUE);

  ephy_download_set_destination_uri (fixture->download, fixture->destination);

  g_free (tmp_filename);
  g_free (dest_file);
}

static void
fixture_teardown (Fixture *fixture, gconstpointer data)
{
  g_free (fixture->destination);
  g_free (fixture->source);

  g_object_unref (fixture->download);

  g_main_loop_unref (fixture->loop);
}

static gboolean
test_file_was_downloaded (EphyDownload *download)
{
  char *filename;
  gboolean ret;

  filename = g_filename_from_uri (ephy_download_get_destination_uri (download),
                                  NULL, NULL);

  ret = g_file_test (filename, G_FILE_TEST_EXISTS);
  g_free (filename);

  return ret;
}

static void
completed_cb (EphyDownload *download,
              Fixture *fixture)
{
  g_assert (test_file_was_downloaded (download));
  g_main_loop_quit (fixture->loop);
}

static void
test_ephy_download_new (Fixture *fixture, gconstpointer data)
{
  g_assert (EPHY_IS_DOWNLOAD (fixture->download));
}

static void
test_ephy_download_new_for_uri (Fixture *fixture, gconstpointer data)
{
  WebKitDownload *download = ephy_download_get_webkit_download (fixture->download);
  WebKitURIRequest *request = webkit_download_get_request (download);

  g_assert(request);
  g_assert_cmpstr (fixture->source, ==, webkit_uri_request_get_uri (request));
}

static void
test_ephy_download_start (Fixture *fixture, gconstpointer data)
{
  g_signal_connect (G_OBJECT (fixture->download), "completed",
                    G_CALLBACK (completed_cb), fixture);

  ephy_download_set_action (fixture->download, EPHY_DOWNLOAD_ACTION_DO_NOTHING);
  g_main_loop_run (fixture->loop);
}

int
main (int argc, char *argv[])
{
  int ret;
  GSList* uris;
  SoupServer *server;

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);
  g_application_register (G_APPLICATION (ephy_shell_get_default ()), NULL, NULL);

  server = soup_server_new (NULL, NULL);
  soup_server_listen_local (server, 0,
                            SOUP_SERVER_LISTEN_IPV4_ONLY,
                            NULL);
  uris = soup_server_get_uris (server);
  base_uri = (SoupURI*)uris->data;
  g_slist_free (uris);

  soup_server_add_handler (server, NULL, server_callback, NULL, NULL);

  g_test_add ("/embed/ephy-download/new",
              Fixture, NULL, fixture_setup,
              test_ephy_download_new, fixture_teardown);
  g_test_add ("/embed/ephy-download/new_for_uri",
              Fixture, NULL, fixture_setup,
              test_ephy_download_new_for_uri, fixture_teardown);
  g_test_add ("/embed/ephy-download/start",
              Fixture, NULL, fixture_setup,
              test_ephy_download_start, fixture_teardown);

  ret = g_test_run ();

  g_object_unref (ephy_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}

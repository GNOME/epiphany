/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * testephyembedpersist.c
 * This file is part of Epiphany
 *
 * Copyright © 2009, 2010 - Gustavo Noronha Silva
 * Copyright © 2010 - Igalia S.L.
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
#include "ephy-embed-persist.h"
#include "ephy-embed-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-stock-icons.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <string.h>

#define HTML_STRING "testing-embed-persist"
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
  EphyEmbedPersist *embed;
  char *destination;
} PersistFixture;

static void
persist_fixture_setup (PersistFixture *fixture,
                       gconstpointer data)
{
  char *tmp_filename;
  char *uri_string;

  tmp_filename = ephy_file_tmp_filename ("embed-persist-save-XXXXXX", NULL);
  fixture->destination = g_build_filename (ephy_file_tmp_dir (), tmp_filename, NULL);

  fixture->loop = g_main_loop_new (NULL, TRUE);
  fixture->embed = EPHY_EMBED_PERSIST (g_object_new (EPHY_TYPE_EMBED_PERSIST, NULL));

  uri_string = get_uri_for_path ("/default");

  ephy_embed_persist_set_source (fixture->embed, uri_string);
  ephy_embed_persist_set_dest (fixture->embed, fixture->destination);

  g_free (tmp_filename);
  g_free (uri_string);
}

static void
persist_fixture_teardown (PersistFixture *fixture,
                          gconstpointer data)
{
  g_unlink (fixture->destination);
  g_free (fixture->destination);

  if (fixture->embed != NULL)
    g_object_unref (fixture->embed);

  g_main_loop_unref (fixture->loop);
}

static void
test_embed_persist_new (PersistFixture *fixture,
                        gconstpointer data)
{
  g_assert (EPHY_IS_EMBED_PERSIST (fixture->embed));
}

static void
test_embed_persist_set_dest (PersistFixture *fixture,
                             gconstpointer data)
{
  const char *dest_value = NULL;
  char *read_value;

  ephy_embed_persist_set_dest (fixture->embed, dest_value);
  g_object_get (G_OBJECT (fixture->embed), "dest", &read_value, NULL);

  g_assert_cmpstr (dest_value, ==, read_value);

  g_free (read_value);
}

static void
test_embed_persist_set_embed (PersistFixture *fixture,
                              gconstpointer data)
{
  EphyEmbed *orig_value;
  EphyEmbed *fail_value;
  EphyEmbed *read_value;

  orig_value = EPHY_EMBED (g_object_new (EPHY_TYPE_EMBED, NULL));
  fail_value = EPHY_EMBED (g_object_new (EPHY_TYPE_EMBED, NULL));

  ephy_embed_persist_set_embed (fixture->embed, orig_value);
  g_object_get (G_OBJECT (fixture->embed), "embed", &read_value, NULL);

  g_assert (read_value == orig_value);
  g_assert (read_value != fail_value);

  g_object_unref (read_value);
  g_object_ref_sink (fail_value);
  g_object_unref (fail_value);
}

static void
test_embed_persist_save_empty_dest (PersistFixture *fixture,
                                    gconstpointer data)
{
  ephy_embed_persist_set_source (fixture->embed, "ficticious-source");

  /* No dest is set and no EPHY_EMBED_PERSIST_ASK_DESTINATION flag,
     so the destination will be downloads folder with the suggested filename. */
  g_assert (ephy_embed_persist_save (fixture->embed) == TRUE);

  /* Otherwise the reference from ephy_embed_persist_save () is never unref'd */
  ephy_embed_persist_cancel (fixture->embed);
}

static void
test_embed_persist_save (PersistFixture *fixture,
                         gconstpointer data)
{
  /* Source and dest set, should return TRUE */
  g_assert (ephy_embed_persist_save (fixture->embed) == TRUE);

  /* Otherwise the reference from ephy_embed_persist_save () is never unref'd */
  ephy_embed_persist_cancel (fixture->embed);
}

static void
test_embed_persist_cancel (PersistFixture *fixture,
                           gconstpointer data)
{
  ephy_embed_persist_cancel (fixture->embed);
  /* This is the only case where the embed unrefs itself */
  fixture->embed = NULL;
}

static void
completed_cb (EphyEmbedPersist *persist,
              PersistFixture *fixture)
{
  g_main_loop_quit (fixture->loop);
}

static void
test_embed_persist_save_completed (PersistFixture *fixture,
                                   gconstpointer userdata)
{
  g_signal_connect (G_OBJECT (fixture->embed), "completed",
                    G_CALLBACK (completed_cb), fixture);

  ephy_embed_persist_save (fixture->embed);

  g_main_loop_run (fixture->loop);

  g_assert (g_file_test (fixture->destination, G_FILE_TEST_EXISTS));
}

static void
cancelled_cb (EphyEmbedPersist *persist,
              PersistFixture *fixture)
{
  g_main_loop_quit (fixture->loop);
}

static void
test_embed_persist_cancelled (PersistFixture *fixture,
                              gconstpointer userdata)
{
  char *uri_string;

  g_signal_connect (G_OBJECT (fixture->embed), "cancelled",
                    G_CALLBACK (cancelled_cb), fixture);

  uri_string = get_uri_for_path ("/cancelled");
  ephy_embed_persist_set_source (fixture->embed, uri_string);
  g_free (uri_string);

  g_assert (ephy_embed_persist_save (fixture->embed));

  g_main_loop_run (fixture->loop);
}

int
main (int argc, char *argv[])
{
  int ret;
  SoupServer *server;

  gtk_test_init (&argc, &argv);
  g_thread_init (NULL);

  ephy_debug_init ();
  ephy_embed_prefs_init ();
  _ephy_shell_create_instance ();

  if (!ephy_file_helpers_init (NULL, TRUE, FALSE, NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  server = soup_server_new (SOUP_SERVER_PORT, 0, NULL);
  soup_server_run_async (server);

  base_uri = soup_uri_new ("http://127.0.0.1/");
  soup_uri_set_port (base_uri, soup_server_get_port (server));

  soup_server_add_handler (server, NULL, server_callback, NULL, NULL);

  g_test_add ("/embed/ephy-embed-persist/new",
              PersistFixture, NULL,
              persist_fixture_setup,
              test_embed_persist_new,
              persist_fixture_teardown);
  g_test_add ("/embed/ephy-embed-persist/set_dest",
              PersistFixture, NULL,
              persist_fixture_setup,
              test_embed_persist_set_dest,
              persist_fixture_teardown);
  g_test_add ("/embed/ephy-embed-persist/set_embed",
              PersistFixture, NULL,
              persist_fixture_setup,
              test_embed_persist_set_embed,
              persist_fixture_teardown);
  g_test_add ("/embed/ephy-embed-persist/save_empty_dest",
              PersistFixture, NULL,
              persist_fixture_setup,
              test_embed_persist_save_empty_dest,
              persist_fixture_teardown);

  g_test_add ("/embed/ephy-embed-persist/save",
              PersistFixture, NULL,
              persist_fixture_setup,
              test_embed_persist_save,
              persist_fixture_teardown);
  g_test_add ("/embed/ephy-embed-persist/cancel",
              PersistFixture, NULL,
              persist_fixture_setup,
              test_embed_persist_cancel,
              persist_fixture_teardown);

  g_test_add ("/embed/ephy-embed-persist/save_completed",
              PersistFixture, NULL,
              persist_fixture_setup,
              test_embed_persist_save_completed,
              persist_fixture_teardown);
  g_test_add ("/embed/ephy-embed-persist/cancelled",
              PersistFixture, NULL,
              persist_fixture_setup,
              test_embed_persist_cancelled,
              persist_fixture_teardown);

  ret = g_test_run ();

  g_object_unref (ephy_shell);
  ephy_file_helpers_shutdown ();

  return ret;
}

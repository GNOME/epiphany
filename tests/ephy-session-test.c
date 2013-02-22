/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2012 - Igalia S.L.
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
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-private.h"
#include "ephy-file-helpers.h"
#include "ephy-private.h"
#include "ephy-shell.h"
#include "ephy-session.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <string.h>

const char *session_data = 
"<?xml version=\"1.0\"?>"
"<session>"
	 "<window x=\"94\" y=\"48\" width=\"1132\" height=\"684\" active-tab=\"0\" role=\"epiphany-window-67c6e8a5\">"
	 	 "<embed url=\"about:memory\" title=\"Memory usage\"/>"
	 "</window>"
"</session>";

static gboolean load_stream_retval;

static void
load_from_stream_cb (GObject *object,
                     GAsyncResult *result,
                     gpointer user_data)
{
  GMainLoop *loop = (GMainLoop *)user_data;

  load_stream_retval = ephy_session_load_from_stream_finish (EPHY_SESSION (object), result, NULL);
  g_main_loop_quit (loop);
}

static gboolean
load_session_from_string (EphySession *session,
                          const char *data)
{
  GMainLoop *loop;
  GInputStream *stream;

  loop = g_main_loop_new (NULL, FALSE);
  stream = g_memory_input_stream_new_from_data (data, -1, NULL);
  ephy_session_load_from_stream (session, stream, 0, NULL, load_from_stream_cb, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  return load_stream_retval;
}

static void
test_ephy_session_load (void)
{
    EphySession *session;
    gboolean ret;
    GList *l;
    EphyEmbed *embed;
    EphyWebView *view;

    session = ephy_shell_get_session (ephy_shell_get_default ());
    g_assert (session);

    ret = load_session_from_string (session, session_data);
    g_assert (ret);

    l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 1);

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
    g_assert (embed);
    view = ephy_embed_get_web_view (embed);
    g_assert (view);
    g_assert_cmpstr (ephy_web_view_get_address (view), ==, "about:memory");

    ephy_session_clear (session);
}

const char *session_data_many_windows =
"<?xml version=\"1.0\"?>"
"<session>"
	 "<window x=\"100\" y=\"26\" width=\"1067\" height=\"740\" active-tab=\"0\" role=\"epiphany-window-7da420dd\">"
	   "<embed url=\"about:epiphany\" title=\"Epiphany\"/>"
	 "</window>"
	 "<window x=\"73\" y=\"26\" width=\"1067\" height=\"740\" active-tab=\"0\" role=\"epiphany-window-1261c786\">"
	   "<embed url=\"about:epiphany\" title=\"Epiphany\"/>"
	 "</window>"
"</session>";

static void
test_ephy_session_clear (void)
{
  EphySession *session;
  GList *l;

  session = EPHY_SESSION (ephy_shell_get_session (ephy_shell_get_default ()));
  load_session_from_string (session, session_data_many_windows);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  gtk_widget_destroy (GTK_WIDGET (l->data));

  ephy_session_clear (session);

  g_assert (gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ())) == NULL);
  g_assert (ephy_session_get_can_undo_tab_closed (session) == FALSE);
}

const char *session_data_empty = 
"";

static void
test_ephy_session_load_empty_session (void)
{
    EphySession *session;
    gboolean ret;
    GList *l;
    EphyEmbed *embed;
    EphyWebView *view;

    session = ephy_shell_get_session (ephy_shell_get_default ());
    g_assert (session);

    ret = load_session_from_string (session, session_data_empty);
    g_assert (ret == FALSE);

    /* Loading the session should have failed, but we should still get
     * the default empty window. Got to spin the mainloop though,
     * since the fallback is done by queueing another session
     * command. */
    while (g_main_context_pending (NULL))
      g_main_context_iteration (NULL, FALSE);

    l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 1);

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
    g_assert (embed);
    view = ephy_embed_get_web_view (embed);
    g_assert (view);
    g_assert_cmpstr (ephy_web_view_get_address (view), ==, "ephy-about:overview");

    ephy_session_clear (session);
}

static void
test_ephy_session_load_many_windows (void)
{
    EphySession *session;
    gboolean ret;
    GList *l, *p;
    EphyEmbed *embed;
    EphyWebView *view;

    session = ephy_shell_get_session (ephy_shell_get_default ());
    g_assert (session);

    ret = load_session_from_string (session, session_data_many_windows);
    g_assert (ret);

    l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 2);

    for (p = l; p; p = p->next) {
      embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (p->data));
      g_assert (embed);
      view = ephy_embed_get_web_view (embed);
      g_assert (view);
      g_assert_cmpstr (ephy_web_view_get_address (view), ==, "about:epiphany");
    }

    ephy_session_clear (session);
}

static void
open_uris_after_loading_session (const char** uris, int final_num_windows)
{
    EphySession *session;
    gboolean ret;
    GList *l, *p;
    EphyEmbed *embed;
    EphyWebView *view;
    guint32 user_time;

    session = ephy_shell_get_session (ephy_shell_get_default ());
    g_assert (session);

    user_time = gdk_x11_display_get_user_time (gdk_display_get_default ());

    ret = load_session_from_string (session, session_data_many_windows);
    g_assert (ret);

    l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));

    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 2);

    for (p = l; p; p = p->next) {
      embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (p->data));
      g_assert (embed);
      view = ephy_embed_get_web_view (embed);
      g_assert (view);
      g_assert_cmpstr (ephy_web_view_get_address (view), ==, "about:epiphany");
    }

    /* Causing a session load here should not create new windows, since we
     * already have some.
     */
    ephy_session_save (session, "type:session_state");

    ephy_session_resume (session, user_time, NULL, NULL, NULL);

    /* Ensure the queue is processed. */
    while (gtk_events_pending ())
        gtk_main_iteration_do (FALSE);

    l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 2);

    /* We should still have only 2 windows after the session load
     * command - it should bail after noticing there are windows
     * already.
     */
    ephy_shell_open_uris (ephy_shell_get_default (), uris, 0, user_time);

    while (gtk_events_pending ())
        gtk_main_iteration_do (FALSE);

    /* We should still have 2 windows here, since the new URI should be
     * in a new tab of an existing window.
     */
    l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, final_num_windows);

    ephy_session_clear (session);
}

static void
test_ephy_session_open_uri_after_loading_session (void)
{
    const char* uris[] = { "ephy-about:epiphany", NULL };

    open_uris_after_loading_session (uris, 2);
}

static void
test_ephy_session_open_empty_uri_forces_new_window (void)
{
    const char* uris[] = { "", NULL };

    open_uris_after_loading_session (uris, 3);
}

static void
test_ephy_session_restore_tabs (void)
{
  EphySession *session = EPHY_SESSION (ephy_shell_get_session (ephy_shell_get_default ()));
  const char* uris[] = { "ephy-about:epiphany", "ephy-about:config", NULL };
  guint32 user_time = gdk_x11_display_get_user_time (gdk_display_get_default ());
  gboolean ret;
  GList *l;
  gchar *url;
  int n_windows;
  EphyEmbed *embed;

  /* Nothing to restore. */
  g_assert (ephy_session_get_can_undo_tab_closed (session) == FALSE);

  ephy_shell_open_uris (ephy_shell_get_default(), uris, 0, user_time);
  while (gtk_events_pending ())
    gtk_main_iteration_do (FALSE);

  /* Nothing to restore, again. */
  g_assert (ephy_session_get_can_undo_tab_closed (session) == FALSE);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
  url = g_strdup (ephy_web_view_get_address (ephy_embed_get_web_view (embed)));
  gtk_widget_destroy (GTK_WIDGET (embed));

  /* There should be now at least one tab that can be restored. */
  g_assert (ephy_session_get_can_undo_tab_closed (session) == TRUE);

  ephy_session_undo_close_tab (session);
  while (gtk_events_pending ())
    gtk_main_iteration_do (FALSE);

  /* Nothing to restore, again. */
  g_assert (ephy_session_get_can_undo_tab_closed (session) == FALSE);

  /* The active child should now be pointing to the restored tab,
     whose address is the one we copied previously. */
  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
  g_assert_cmpstr (ephy_web_view_get_address (ephy_embed_get_web_view (embed)),
                   ==, url);
  g_free (url);

  ephy_session_clear (session);

  ret = load_session_from_string (session, session_data_many_windows);
  g_assert (ret);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  n_windows = g_list_length (l);
  /* We need more than one window for the next test to make sense. */
  g_assert_cmpint (n_windows, >, 1);
  gtk_widget_destroy (GTK_WIDGET (l->data));
  /* One window is gone. */
  g_assert_cmpint (n_windows, ==, g_list_length (gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default()))) + 1);
  g_assert (ephy_session_get_can_undo_tab_closed (session) == TRUE);
  ephy_session_undo_close_tab (session);
  while (gtk_events_pending ())
    gtk_main_iteration_do (FALSE);
  /* We have the same amount of windows than before destroying one. */
  g_assert_cmpint (n_windows, ==, g_list_length (gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default()))));

  ephy_session_clear (session);
}

int
main (int argc, char *argv[])
{
  int ret;

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();
  ephy_embed_prefs_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_PRIVATE_PROFILE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);
  g_assert (ephy_shell_get_default ());

  g_application_register (G_APPLICATION (ephy_shell_get_default ()), NULL, NULL);

  g_test_add_func ("/src/ephy-session/load",
                   test_ephy_session_load);

  g_test_add_func ("/src/ephy-session/clear",
                   test_ephy_session_clear);

  g_test_add_func ("/src/ephy-session/load-empty-session",
                   test_ephy_session_load_empty_session);

  g_test_add_func ("/src/ephy-session/load-many-windows",
                   test_ephy_session_load_many_windows);

  g_test_add_func ("/src/ephy-session/open-uri-after-loading_session",
                   test_ephy_session_open_uri_after_loading_session);

  g_test_add_func ("/src/ephy-session/open-empty-uri-forces-new-window",
                   test_ephy_session_open_empty_uri_forces_new_window);

  g_test_add_func("/src/ephy-session/restore-tabs",
                  test_ephy_session_restore_tabs);

  ret = g_test_run ();

  g_object_unref (ephy_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}

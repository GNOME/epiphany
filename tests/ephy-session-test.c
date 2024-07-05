/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 - Igalia S.L.
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
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-session.h"
#include "ephy-test-utils.h"

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
load_from_stream_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GMainLoop *loop = (GMainLoop *)user_data;

  load_stream_retval = ephy_session_load_from_stream_finish (EPHY_SESSION (object), result, NULL);
  g_main_loop_quit (loop);
}

static gboolean
load_session_from_string (EphySession *session,
                          const char  *data)
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
enable_delayed_loading (void)
{
  g_settings_set_boolean (EPHY_SETTINGS_MAIN,
                          EPHY_PREFS_RESTORE_SESSION_DELAYING_LOADS,
                          TRUE);
}

static void
disable_delayed_loading (void)
{
  g_settings_set_boolean (EPHY_SETTINGS_MAIN,
                          EPHY_PREFS_RESTORE_SESSION_DELAYING_LOADS,
                          FALSE);
}

static void
test_ephy_session_load (void)
{
  EphySession *session;
  gboolean ret;
  GList *l;
  EphyEmbed *embed;
  EphyWebView *view;
  GMainLoop *loop;

  disable_delayed_loading ();

  session = ephy_shell_get_session (ephy_shell_get_default ());
  g_assert_nonnull (session);

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  ret = load_session_from_string (session, session_data);
  g_assert_true (ret);

  ephy_test_utils_ensure_web_views_are_loaded (loop);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  g_assert_nonnull (l);
  g_assert_cmpint (g_list_length (l), ==, 1);

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
  g_assert_nonnull (embed);
  view = ephy_embed_get_web_view (embed);
  g_assert_nonnull (view);
  ephy_test_utils_check_ephy_web_view_address (view, "ephy-about:memory");

  ephy_session_clear (session);

  enable_delayed_loading ();
}

const char *session_data_many_windows =
  "<?xml version=\"1.0\"?>"
  "<session>"
  "<window x=\"100\" y=\"26\" width=\"1067\" height=\"740\" active-tab=\"0\" role=\"epiphany-window-7da420dd\">"
  "<embed url=\"about:epiphany\" title=\"Epiphany\"/>"
  "</window>"
  "<window x=\"73\" y=\"26\" width=\"1067\" height=\"740\" active-tab=\"0\" role=\"epiphany-window-1261c786\">"
  "<embed url=\"about:config\" title=\"Epiphany\"/>"
  "</window>"
  "</session>";

static void
test_ephy_session_clear (void)
{
  EphySession *session;
  GList *l;
  GMainLoop *loop;

  disable_delayed_loading ();

  session = EPHY_SESSION (ephy_shell_get_session (ephy_shell_get_default ()));

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  load_session_from_string (session, session_data_many_windows);

  ephy_test_utils_ensure_web_views_are_loaded (loop);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  gtk_widget_destroy (GTK_WIDGET (l->data));

  ephy_session_clear (session);

  g_assert_null (gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ())));
  g_assert_false (ephy_session_get_can_undo_tab_closed (session));
}

const char *session_data_empty =
  "";

#if 0
static void
test_ephy_session_load_empty_session (void)
{
  EphySession *session;
  gboolean ret;
  GList *l;
  EphyEmbed *embed;
  EphyWebView *view;
  GMainLoop *loop;

  disable_delayed_loading ();

  session = ephy_shell_get_session (ephy_shell_get_default ());
  g_assert_nonnull (session);

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  ret = load_session_from_string (session, session_data_empty);
  g_assert_false (ret);

  /* Loading the session should have failed, but we should still get
   * the default empty window. Got to spin the mainloop though,
   * since the fallback is done by queueing another session
   * command. */
  ephy_test_utils_ensure_web_views_are_loaded (loop);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  g_assert_nonnull (l);
  g_assert_cmpint (g_list_length (l), ==, 1);

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
  g_assert_nonnull (embed);
  view = ephy_embed_get_web_view (embed);
  g_assert_nonnull (view);
  ephy_test_utils_check_ephy_web_view_address (view, "ephy-about:overview");

  enable_delayed_loading ();
  ephy_session_clear (session);
}
#endif

static void
test_ephy_session_load_many_windows (void)
{
  EphySession *session;
  gboolean ret;
  GList *l, *p;
  EphyEmbed *embed;
  EphyWebView *view;
  GMainLoop *loop;

  disable_delayed_loading ();

  session = ephy_shell_get_session (ephy_shell_get_default ());
  g_assert_nonnull (session);

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  ret = load_session_from_string (session, session_data_many_windows);
  g_assert_true (ret);
  g_assert_cmpint (ephy_test_utils_get_web_view_ready_counter (), >=, 0);
  g_assert_cmpint (ephy_test_utils_get_web_view_ready_counter (), <=, 2);

  ephy_test_utils_ensure_web_views_are_loaded (loop);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  g_assert_nonnull (l);
  g_assert_cmpint (g_list_length (l), ==, 2);

  for (p = l; p; p = p->next) {
    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (p->data));
    g_assert_nonnull (embed);
    view = ephy_embed_get_web_view (embed);
    g_assert_nonnull (view);
  }

  enable_delayed_loading ();
  ephy_session_clear (session);
}

static void
open_uris_after_loading_session (const char **uris,
                                 int          final_num_windows)
{
  EphySession *session;
  gboolean ret;
  GList *l, *p;
  EphyEmbed *embed;
  EphyWebView *view;
  guint32 user_time;
  GMainLoop *loop;

  disable_delayed_loading ();

  session = ephy_shell_get_session (ephy_shell_get_default ());
  g_assert_nonnull (session);

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  user_time = gdk_x11_display_get_user_time (gdk_display_get_default ());

  ret = load_session_from_string (session, session_data_many_windows);
  g_assert_true (ret);
  g_assert_cmpint (ephy_test_utils_get_web_view_ready_counter (), >=, 0);
  g_assert_cmpint (ephy_test_utils_get_web_view_ready_counter (), <=, 2);

  ephy_test_utils_ensure_web_views_are_loaded (loop);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));

  g_assert_nonnull (l);
  g_assert_cmpint (g_list_length (l), ==, 2);

  for (p = l; p; p = p->next) {
    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (p->data));
    g_assert_nonnull (embed);
    view = ephy_embed_get_web_view (embed);
    g_assert_nonnull (view);
  }

  /* Causing a session load here should not create new windows, since we
   * already have some.
   */
  ephy_session_save (session);

  ephy_session_resume (session, user_time, NULL, NULL, NULL);

  /* Ensure the queue is processed. */
  while (gtk_events_pending ())
    gtk_main_iteration_do (FALSE);

  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  g_assert_nonnull (l);
  g_assert_cmpint (g_list_length (l), ==, 2);

  /* We should still have only 2 windows after the session load
   * command - it should bail after noticing there are windows
   * already.
   */
  ephy_shell_open_uris (ephy_shell_get_default (), uris, 0);

  while (gtk_events_pending ())
    gtk_main_iteration_do (FALSE);

  /* We should still have 2 windows here, since the new URI should be
   * in a new tab of an existing window.
   */
  l = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  g_assert_nonnull (l);
  g_assert_cmpint (g_list_length (l), ==, final_num_windows);

  enable_delayed_loading ();
  ephy_session_clear (session);
}

static void
test_ephy_session_open_uri_after_loading_session (void)
{
  const char *uris[] = { "ephy-about:epiphany", NULL };

  open_uris_after_loading_session (uris, 2);
}

static void
test_ephy_session_open_empty_uri_forces_new_window (void)
{
  const char *uris[] = { "", NULL };

  open_uris_after_loading_session (uris, 3);
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);
  g_assert_nonnull (ephy_shell_get_default ());

  g_application_register (G_APPLICATION (ephy_shell_get_default ()), NULL, NULL);

  g_test_add_func ("/src/ephy-session/load",
                   test_ephy_session_load);

  g_test_add_func ("/src/ephy-session/clear",
                   test_ephy_session_clear);

#if 0
  /* FIXME: This test needs fixing. See bug #707220. */
  g_test_add_func ("/src/ephy-session/load-empty-session",
                   test_ephy_session_load_empty_session);
#endif
  g_test_add_func ("/src/ephy-session/load-many-windows",
                   test_ephy_session_load_many_windows);

  g_test_add_func ("/src/ephy-session/open-uri-after-loading_session",
                   test_ephy_session_open_uri_after_loading_session);

  g_test_add_func ("/src/ephy-session/open-empty-uri-forces-new-window",
                   test_ephy_session_open_empty_uri_forces_new_window);

  ret = g_test_run ();

  g_object_unref (ephy_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}

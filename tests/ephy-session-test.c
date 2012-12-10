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

static void
test_ephy_session_load (void)
{
    EphySession *session;
    gboolean ret;
    GList *l;
    EphyEmbed *embed;
    EphyWebView *view;

    session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
    g_assert (session);

    ret = ephy_session_load_from_string (session, session_data, -1, 0);
    g_assert (ret);

    l = ephy_shell_get_windows (ephy_shell);
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 1);

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
    g_assert (embed);
    view = ephy_embed_get_web_view (embed);
    g_assert (view);
    g_assert_cmpstr (ephy_web_view_get_address (view), ==, "ephy-about:memory");

    /* FIXME: Destroy the window. I think ideally we'd like something
     * like 'ephy_session_clear ()' to reset everything to its initial
     * state here. That or allow EphyShell to be created more than
     * once and do it once per test. */
    gtk_widget_destroy (GTK_WIDGET (l->data));
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

    session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
    g_assert (session);

    ret = ephy_session_load_from_string (session, session_data_empty, -1, 0);
    g_assert (ret == FALSE);

    /* Loading the session should have failed, but we should still get
     * the default empty window. Got to spin the mainloop though,
     * since the fallback is done by queueing another session
     * command. */
    while (g_main_context_pending (NULL))
      g_main_context_iteration (NULL, FALSE);

    l = ephy_shell_get_windows (ephy_shell);
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 1);

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (l->data));
    g_assert (embed);
    view = ephy_embed_get_web_view (embed);
    g_assert (view);
    g_assert_cmpstr (ephy_web_view_get_address (view), ==, "ephy-about:overview");

    /* FIXME: Destroy the window. I think ideally we'd like something
     * like 'ephy_session_clear ()' to reset everything to its initial
     * state here. That or allow EphyShell to be created more than
     * once and do it once per test. */
    gtk_widget_destroy (GTK_WIDGET (l->data));
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
test_ephy_session_load_many_windows (void)
{
    EphySession *session;
    gboolean ret;
    GList *l, *p;
    EphyEmbed *embed;
    EphyWebView *view;

    session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
    g_assert (session);

    ret = ephy_session_load_from_string (session, session_data_many_windows, -1, 0);
    g_assert (ret);

    l = ephy_shell_get_windows (ephy_shell);
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 2);

    for (p = l; p; p = p->next) {
      embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (p->data));
      g_assert (embed);
      view = ephy_embed_get_web_view (embed);
      g_assert (view);
      g_assert_cmpstr (ephy_web_view_get_address (view), ==, "ephy-about:epiphany");
    }

    /* FIXME: See comments above. */
    for (p = l; p; p = p->next)
      gtk_widget_destroy (GTK_WIDGET (p->data));
}

static void
test_ephy_session_open_uri_after_loading_session (void)
{
    EphySession *session;
    gboolean ret;
    GList *l, *p;
    EphyEmbed *embed;
    EphyWebView *view;
    guint32 user_time;
    const char* uris[] = { "ephy-about:epiphany", NULL };

    session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
    g_assert (session);

    user_time = gdk_x11_display_get_user_time (gdk_display_get_default ());

    ret = ephy_session_load_from_string (session, session_data_many_windows, -1, 0);
    g_assert (ret);

    l = ephy_shell_get_windows (ephy_shell);
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 2);

    for (p = l; p; p = p->next) {
      embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (p->data));
      g_assert (embed);
      view = ephy_embed_get_web_view (embed);
      g_assert (view);
      g_assert_cmpstr (ephy_web_view_get_address (view), ==, "ephy-about:epiphany");
    }

    /* Causing a session load here should not create new windows, since we
     * already have some.
     */
    ephy_session_save (session, "type:session_state");

    ephy_session_queue_command (session,
                                EPHY_SESSION_CMD_RESUME_SESSION,
                                "type:session_state",
                                NULL,
                                user_time,
                                FALSE);

    /* Ensure the queue is processed. */
    while (gtk_events_pending ())
        gtk_main_iteration_do (FALSE);

    l = ephy_shell_get_windows (ephy_shell);
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 2);

    /* We should still have only 2 windows after the session load
     * command - it should bail after noticing there are windows
     * already.
     */
    ephy_session_queue_command (session,
                                EPHY_SESSION_CMD_OPEN_URIS,
                                NULL,
                                uris,
                                user_time,
                                FALSE);

    while (gtk_events_pending ())
        gtk_main_iteration_do (FALSE);

    /* We should still have 2 windows here, since the new URI should be
     * in a new tab of an existing window.
     */
    l = ephy_shell_get_windows (ephy_shell);
    g_assert (l);
    g_assert_cmpint (g_list_length (l), ==, 2);
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
  g_assert (ephy_shell);

  g_application_register (G_APPLICATION (ephy_shell), NULL, NULL);

  g_test_add_func ("/src/ephy-session/load",
                   test_ephy_session_load);

  g_test_add_func ("/src/ephy-session/load-empty-session",
                   test_ephy_session_load_empty_session);

  g_test_add_func ("/src/ephy-session/load-many-windows",
                   test_ephy_session_load_many_windows);

  g_test_add_func ("/src/ephy-session/open-uri-after-loading_session",
                   test_ephy_session_open_uri_after_loading_session);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();
  g_object_unref (ephy_shell);

  return ret;
}

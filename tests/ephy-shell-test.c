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
#include "ephy-embed.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-test-utils.h"
#include "ephy-window.h"

#include <glib.h>
#include <gtk/gtk.h>

static void
test_ephy_shell_basic_embeds (void)
{
  EphyShell *ephy_shell;
  EphyWindow *window;

  EphyEmbed *embed1;
  EphyEmbed *embed2;

  GList *children;

  ephy_shell = ephy_shell_get_default ();

  window = ephy_window_new ();
  g_assert_true (EPHY_IS_WINDOW (window));

  /* Embed should be created. */
  embed1 = ephy_shell_new_tab_full
             (ephy_shell,
             NULL,       /* title */
             NULL,       /* related view */
             window,
             NULL,       /* embed */
             EPHY_NEW_TAB_DONT_SHOW_WINDOW,       /* flags */
             gtk_get_current_event_time ());
  g_assert_true (EPHY_IS_EMBED (embed1));

  g_assert_true (gtk_widget_get_toplevel (GTK_WIDGET (embed1)) == GTK_WIDGET (window));

  children = ephy_embed_container_get_children (EPHY_EMBED_CONTAINER (window));
  g_assert_cmpint (g_list_length (children), ==, 1);
  g_list_free (children);

  /* Another embed should be created */
  embed2 = ephy_shell_new_tab_full
             (ephy_shell,
             NULL,       /* title */
             NULL,       /* related view */
             window,       /* window */
             NULL,       /* embed */
             EPHY_NEW_TAB_DONT_SHOW_WINDOW,       /* flags */
             gtk_get_current_event_time ());
  g_assert_true (EPHY_IS_EMBED (embed2));

  /* A second children should exist now. */
  children = ephy_embed_container_get_children (EPHY_EMBED_CONTAINER (window));
  g_assert_cmpint (g_list_length (children), ==, 2);
  g_list_free (children);

  gtk_widget_destroy (GTK_WIDGET (window));
}

static void
test_ephy_shell_parent_windows (void)
{
  EphyShell *ephy_shell;
  GtkWidget *window;
  GtkWidget *window2;
  EphyEmbed *embed;

  ephy_shell = ephy_shell_get_default ();
  window = GTK_WIDGET (ephy_window_new ());

  /* parent-window provided */
  embed = ephy_shell_new_tab
            (ephy_shell, EPHY_WINDOW (window), NULL,
            EPHY_NEW_TAB_DONT_SHOW_WINDOW);

  g_assert_true (EPHY_IS_EMBED (embed));
  g_assert_true (gtk_widget_get_toplevel (GTK_WIDGET (embed)) == window);
  g_object_ref_sink (embed);
  g_object_unref (embed);

  /* Another new-window */
  window2 = GTK_WIDGET (ephy_window_new ());
  embed = ephy_shell_new_tab
            (ephy_shell, EPHY_WINDOW (window2), NULL,
            EPHY_NEW_TAB_DONT_SHOW_WINDOW);

  /* The parent window should be a completely new one. */
  g_assert_true (EPHY_IS_EMBED (embed));
  g_assert_true (gtk_widget_get_toplevel (GTK_WIDGET (embed)) != window);
  g_assert_true (gtk_widget_get_toplevel (GTK_WIDGET (embed)) == window2);

  gtk_widget_destroy (window);
  gtk_widget_destroy (window2);
}

static void
test_ephy_shell_tab_load (void)
{
  EphyShell *ephy_shell;
  GtkWidget *window;
  EphyEmbed *embed;
  EphyWebView *view;
  GMainLoop *loop;

  ephy_shell = ephy_shell_get_default ();
  window = GTK_WIDGET (ephy_window_new ());

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  /* homepage is "about:blank" for now, see embed/ephy-web-view.c */
  embed = ephy_shell_new_tab
            (ephy_shell, EPHY_WINDOW (window), NULL,
            EPHY_NEW_TAB_DONT_SHOW_WINDOW);
  ephy_web_view_load_homepage (ephy_embed_get_web_view (embed));

  g_assert_true (EPHY_IS_EMBED (embed));

  ephy_test_utils_ensure_web_views_are_loaded (loop);

  view = ephy_embed_get_web_view (embed);
  ephy_test_utils_check_ephy_web_view_address (view, "ephy-about:overview");
  g_assert_cmpstr (ephy_web_view_get_typed_address (view), ==, NULL);

  g_object_ref_sink (embed);
  g_object_unref (embed);

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  /* open-page "about:epiphany" for testing. */
  embed = ephy_shell_new_tab
            (ephy_shell, EPHY_WINDOW (window), NULL,
            EPHY_NEW_TAB_DONT_SHOW_WINDOW);
  ephy_web_view_load_url (ephy_embed_get_web_view (embed), "about:epiphany");

  g_assert_true (EPHY_IS_EMBED (embed));

  ephy_test_utils_ensure_web_views_are_loaded (loop);

  ephy_test_utils_check_ephy_embed_address (embed, "ephy-about:epiphany");

  gtk_widget_destroy (window);
}

static void
test_ephy_shell_tab_append (void)
{
  EphyShell *ephy_shell;
  GtkWidget *window;
  EphyTabView *tab_view;

  EphyEmbed *embed1;
  EphyEmbed *embed2;
  EphyEmbed *embed3;
  EphyEmbed *embed4;
  EphyEmbed *embed5;

  ephy_shell = ephy_shell_get_default ();
  window = GTK_WIDGET (ephy_window_new ());
  tab_view = ephy_window_get_tab_view (EPHY_WINDOW (window));

  embed1 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), NULL,
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed1), ==, 0);

  embed2 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), embed1,
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed1), ==, 0);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed2), ==, 1);

  embed3 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), embed1,
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW | EPHY_NEW_TAB_APPEND_AFTER);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed1), ==, 0);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed3), ==, 1);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed2), ==, 2);

  embed4 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), embed1,
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW | EPHY_NEW_TAB_APPEND_LAST);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed1), ==, 0);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed3), ==, 1);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed2), ==, 2);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed4), ==, 3);

  embed5 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), embed3,
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW | EPHY_NEW_TAB_APPEND_AFTER);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed1), ==, 0);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed3), ==, 1);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed5), ==, 2);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed2), ==, 3);
  g_assert_cmpint (ephy_tab_view_get_page_index (tab_view, embed4), ==, 4);

  gtk_widget_destroy (window);
}

#if 0
static void
test_ephy_shell_tab_from_external (void)
{
  EphyShell *ephy_shell;
  GtkWidget *window;
  EphyTabView *tab_view;
  GMainLoop *loop;

  EphyEmbed *embed;
  EphyEmbed *embed2;
  EphyEmbed *embed3;
  EphyEmbed *embed4;
  EphyEmbed *embed5;

  ephy_shell = ephy_shell_get_default ();

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  embed = ephy_shell_new_tab (ephy_shell, NULL, NULL, "about:epiphany",
                              EPHY_NEW_TAB_DONT_SHOW_WINDOW | EPHY_NEW_TAB_OPEN_PAGE);
  window = gtk_widget_get_toplevel (GTK_WIDGET (embed));
  tab_view = ephy_window_get_tab_view (EPHY_WINDOW (window));

  /* This embed should be used in load-from-external. */
  embed2 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), NULL, NULL,
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW | EPHY_NEW_TAB_IN_EXISTING_WINDOW);
  g_assert_true (gtk_widget_get_toplevel (GTK_WIDGET (embed2)) == window);

  /* ephy_shell_new_tab_full uses ephy_web_view_is_loading() to know if
   * it can reuse an embed for EPHY_NEW_TAB_FROM_EXTERNAL. EphyWebView
   * will say that the view is still loading because there's no event
   * loop, fake one so we get a working test. */
  ephy_web_view_load_homepage (ephy_embed_get_web_view (embed2));

  embed3 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), NULL, "about:memory",
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW | EPHY_NEW_TAB_OPEN_PAGE | EPHY_NEW_TAB_IN_EXISTING_WINDOW);
  g_assert_true (gtk_widget_get_toplevel (GTK_WIDGET (embed3)) == window);

  ephy_test_utils_ensure_web_views_are_loaded (loop);

  /* This one should fail, because the active embed is not @embed2. */
  ephy_test_utils_check_ephy_embed_address (embed2, "ephy-about:overview");
  g_assert_cmpint (ephy_tab_view_get_selected_index (tab_view), ==, 0);

  loop = ephy_test_utils_setup_ensure_web_views_are_loaded ();

  embed4 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), NULL, "about:applications",
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW | EPHY_NEW_TAB_IN_EXISTING_WINDOW | EPHY_NEW_TAB_OPEN_PAGE | EPHY_NEW_TAB_FROM_EXTERNAL);
  g_assert_true (embed4 != embed2);

  ephy_test_utils_ensure_web_views_are_loaded (loop);

  ephy_test_utils_check_ephy_embed_address (embed2, "ephy-about:overview");
  ephy_test_utils_check_ephy_embed_address (embed4, "ephy-about:applications");

  ephy_tab_view_select_nth_page (tab_view, 1);

  /* This should work */
  ephy_test_utils_check_ephy_embed_address (embed2, "ephy-about:overview");
  g_assert_cmpint (ephy_tab_view_get_selected_index (tab_view), ==, 1);

  loop = ephy_test_utils_setup_wait_until_load_is_committed (ephy_embed_get_web_view (embed2));

  embed5 = ephy_shell_new_tab (ephy_shell, EPHY_WINDOW (window), NULL, "about:applications",
                               EPHY_NEW_TAB_DONT_SHOW_WINDOW | EPHY_NEW_TAB_IN_EXISTING_WINDOW | EPHY_NEW_TAB_OPEN_PAGE | EPHY_NEW_TAB_FROM_EXTERNAL);

  g_assert_true (embed5 == embed2);

  ephy_test_utils_wait_until_load_is_committed (loop);

  ephy_test_utils_check_ephy_embed_address (embed5, "ephy-about:applications");

  gtk_widget_destroy (window);
}
#endif

static void
test_ephy_shell_tab_no_history (void)
{
  /* TODO: BackForwardList */
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  /* This should affect only this test, we use this to safely change
   * settings. */
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL, EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS, NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);
  g_application_register (G_APPLICATION (ephy_shell_get_default ()), NULL, NULL);

  g_test_add_func ("/src/ephy-shell/basic_embeds",
                   test_ephy_shell_basic_embeds);

  g_test_add_func ("/src/ephy-shell/parent_windows",
                   test_ephy_shell_parent_windows);

  g_test_add_func ("/src/ephy-shell/tab_load",
                   test_ephy_shell_tab_load);

  g_test_add_func ("/src/ephy-shell/tab_append",
                   test_ephy_shell_tab_append);

#if 0
  /* FIXME: This test is broken. See bug #707217. */
  g_test_add_func ("/src/ephy-shell/tab_from_external",
                   test_ephy_shell_tab_from_external);
#endif

  g_test_add_func ("/src/ephy-shell/tab_no_history",
                   test_ephy_shell_tab_no_history);

  ret = g_test_run ();

  g_object_unref (ephy_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}

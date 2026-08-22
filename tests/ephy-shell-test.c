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

#include <glib.h>
#include <gtk/gtk.h>

#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-test-utils.h"
#include "ephy-window.h"

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
  g_test_add_func ("/src/ephy-shell/tab_append",
                   test_ephy_shell_tab_append);

  g_test_add_func ("/src/ephy-shell/tab_no_history",
                   test_ephy_shell_tab_no_history);

  ret = g_test_run ();

  g_object_unref (ephy_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}

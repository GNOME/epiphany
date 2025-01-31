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
#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-history-service.h"
#include "ephy-search-engine-manager.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-web-view.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <string.h>

#define HTML_STRING "testing-ephy-web-view"
#define SERVER_PORT 12321

static void
server_callback (SoupServer        *server,
                 SoupServerMessage *msg,
                 const char        *path,
                 GHashTable        *query,
                 gpointer           data)
{
  SoupMessageHeaders *response_headers;
  SoupMessageBody *response_body;

  response_headers = soup_server_message_get_response_headers (msg);
  response_body = soup_server_message_get_response_body (msg);

  if (!strcmp (path, "/redirect")) {
    soup_server_message_set_status (msg, SOUP_STATUS_MOVED_PERMANENTLY, NULL);
    soup_message_headers_append (response_headers, "Location", "/redirect-result");
  } else {
    soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
  }


  soup_message_body_append (response_body, SOUP_MEMORY_STATIC,
                            HTML_STRING, strlen (HTML_STRING));

  soup_message_body_complete (response_body);
}

static void
load_changed_cb (WebKitWebView   *view,
                 WebKitLoadEvent  load_event,
                 GMainLoop       *loop)
{
  char *expected_url;
  const char *loaded_url;

  if (load_event != WEBKIT_LOAD_FINISHED)
    return;

  expected_url = g_object_get_data (G_OBJECT (view), "test.expected_url");
  g_assert_nonnull (expected_url);

  loaded_url = webkit_web_view_get_uri (view);
  g_assert_cmpstr (loaded_url, ==, expected_url);

  g_signal_handlers_disconnect_by_func (view, load_changed_cb, loop);

  g_free (expected_url);
  g_main_loop_quit (loop);
}

typedef struct {
  const char *url;
  const char *expected_url;
} URLTest;

static const URLTest test_load_url[] = {
  /* This will load the server unavailable error page unless you have a
   * local server in port 80 */
  /* { "localhost", "https://localhost/" }, */
  /* { "127.0.0.1", "https://127.0.0.1/" }, */

  /* Require internet */

  {
    "http://127.0.0.1:12321",
    "http://127.0.0.1:12321/"
  },
  {
    "http://127.0.0.1:12321/path",
    "http://127.0.0.1:12321/path"
  },

  /* port is SERVER_PORT */
  {
    "http://localhost:12321",
    "http://localhost:12321/"
  },

#if 0
  /* FAIL */
  {
    "gnome.org:80",
    "https://www.gnome.org/"
  },
#endif

  /* Queries */
  {
    "http://localhost:12321/?key=value",
    "http://localhost:12321/?key=value"
  },
  {
    "http://localhost:12321/?key=value:sub-value",
    "http://localhost:12321/?key=value:sub-value"
  },
  {
    "http://localhost:12321/?key=value&key2=value2",
    "http://localhost:12321/?key=value&key2=value2"
  },
  {
    "http://localhost:12321/?key=value&key2=",
    "http://localhost:12321/?key=value&key2="
  },

  /* Other HTTP status */
  {
    "http://localhost:12321/redirect",
    "http://localhost:12321/redirect-result"
  },

  /* { "about:epiphany", "ephy-about:epiphany" }, */
  /* { "about:applications", "ephy-about:applications" }, */
  /* { "about:memory", "ephy-about:memory" }, */
};

/* Tests that EphyWebView is successfully loading the given URL. */
static void
test_ephy_web_view_load_url (void)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (test_load_url); i++) {
    URLTest test;
    GMainLoop *loop;
    EphyWebView *view;
    GtkWidget *window = gtk_window_new ();

    view = EPHY_WEB_VIEW (ephy_web_view_new ());
    gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (view));
    test = test_load_url[i];
    loop = g_main_loop_new (NULL, FALSE);

    ephy_web_view_load_url (view, test.url);

    g_object_set_data (G_OBJECT (view),
                       "test.expected_url", g_strdup (test.expected_url));

    g_test_message ("[%s] \t-> %s", test.url, test.expected_url);

    g_signal_connect (view, "load-changed",
                      G_CALLBACK (load_changed_cb), loop);

    g_main_loop_run (loop);
    g_main_loop_unref (loop);
    g_object_unref (window);
  }
}

typedef struct {
  const char *url;
  gboolean match;
} RegexTest;

/* Formal, correct, URLs should be match=TRUE */
static const RegexTest test_non_search_regex[] = {
  /* Searches */
  { "localhost localdomain:8080/home/", FALSE },

  /* Relative paths should be searched */
  { "./", FALSE },

  { "localhost", TRUE },
  { "localhost.localdomain", TRUE },
  { "localhost.localdomain:8080", TRUE },
  { "localhost.localdomain:8080/home/", TRUE },

  { "gnome.org", TRUE },
  { "www.gnome.org", TRUE },
  { "https://www.gnome.org", TRUE },

  /* Ip */
  { "192.168.1.1", TRUE },
  { "192.168.1.1:80", TRUE },
  { "f80e::2a1:f85f:fc8f:a0d1", TRUE },

#if 0
  /* FAIL */
  { "192.168.1.1:80 fails to load", FALSE },
  { "localhost.localdomain 8080/home/", FALSE },
  { "org.gnome.Epiphany: failed to start", FALSE },
  { "ephy-web-view.c:130 error something", FALSE },

  { "[f80e::2a1:f85f:fc8f:a0d1]:80", TRUE },
#endif
};

static void
test_ephy_web_view_non_search_regex (void)
{
  GRegex *regex_non_search, *regex_domain;
  GError *error = NULL;
  guint i;

  regex_non_search = g_regex_new (EPHY_WEB_VIEW_NON_SEARCH_REGEX,
                                  0, G_REGEX_MATCH_NOTEMPTY, &error);

  if (error) {
    g_test_message ("Regex failed: %s", error->message);
    g_error_free (error);
  }
  g_assert_nonnull (regex_non_search);

  regex_domain = g_regex_new (EPHY_WEB_VIEW_DOMAIN_REGEX,
                              0, G_REGEX_MATCH_NOTEMPTY, &error);

  if (error) {
    g_test_message ("Regex failed: %s", error->message);
    g_error_free (error);
  }
  g_assert_nonnull (regex_domain);

  for (i = 0; i < G_N_ELEMENTS (test_non_search_regex); i++) {
    RegexTest test;

    test = test_non_search_regex[i];

    g_test_message ("%s\t\t%s",
                    test.match ? "NO SEARCH" : "SEARCH",
                    test.url);

    g_assert_true (g_regex_match (regex_non_search, test.url, 0, NULL) == test.match ||
                   g_regex_match (regex_domain, test.url, 0, NULL) == test.match);
  }

  g_regex_unref (regex_non_search);
  g_regex_unref (regex_domain);
}

typedef struct {
  const char *url;
  const char *expected;
} normalize_or_autosearch_t;

normalize_or_autosearch_t normalize_or_autosearch_test_ddg[] = {
  { "google.com", "https://google.com" },
  { "https://google.com", "https://google.com" },
  { "https://google.com/this/is/a/path", "https://google.com/this/is/a/path" },
  { "search", "https://duckduckgo.com/?q=search&t=epiphany" },
  { "search.me", "https://search.me" },
  { "lala.lala", "https://duckduckgo.com/?q=lala.lala&t=epiphany" },
  { "lala/lala", "https://duckduckgo.com/?q=lala%2Flala&t=epiphany" },
  { "127.0.0.1", "https://127.0.0.1" },
  { "https://127.0.0.1", "https://127.0.0.1" },
  { "totalgarbage0xdeadbeef", "https://duckduckgo.com/?q=totalgarbage0xdeadbeef&t=epiphany" },
  { "planet.gnome.org", "https://planet.gnome.org" },
  { "search separated words please", "https://duckduckgo.com/?q=search+separated+words+please&t=epiphany" },
  { "\"a quoted string should be searched\"", "https://duckduckgo.com/?q=%22a+quoted+string+should+be+searched%22&t=epiphany" }
};

normalize_or_autosearch_t normalize_or_autosearch_test_google[] = {
  { "search", "https://www.google.com/?q=search" },
  { "lala/lala", "https://www.google.com/?q=lala%2Flala" },
};

static void
verify_normalize_or_autosearch_urls (EphyWebView               *view,
                                     normalize_or_autosearch_t *test,
                                     gint                       n_tests)
{
  int i;

  for (i = 0; i < n_tests; i++) {
    const char *url;
    char *result;

    url = test[i].url;

    result = ephy_embed_utils_normalize_or_autosearch_address (url);
    g_assert_cmpstr (result, ==, test[i].expected);

    g_free (result);
  }
}

static void
test_ephy_web_view_normalize_or_autosearch (void)
{
  EphyWebView *view;
  EphySearchEngineManager *manager;
  EphyEmbedShell *shell;
  EphySearchEngine *default_engine;
  g_autoptr (EphySearchEngine) test_engine = NULL;

  view = EPHY_WEB_VIEW (ephy_web_view_new ());

  shell = ephy_embed_shell_get_default ();
  manager = ephy_embed_shell_get_search_engine_manager (shell);

  default_engine = ephy_search_engine_manager_get_default_engine (manager);
  test_engine = g_object_new (EPHY_TYPE_SEARCH_ENGINE,
                              "name", "org.gnome.Epiphany.EphyWebViewTest",
                              "url", "https://duckduckgo.com/?q=%s&t=epiphany",
                              NULL);
  ephy_search_engine_manager_add_engine (manager, test_engine);
  ephy_search_engine_manager_set_default_engine (manager, test_engine);
  g_assert_true (ephy_search_engine_manager_get_default_engine (manager) == test_engine);
  verify_normalize_or_autosearch_urls (view, normalize_or_autosearch_test_ddg, G_N_ELEMENTS (normalize_or_autosearch_test_ddg));

  ephy_search_engine_set_url (test_engine, "https://www.google.com/?q=%s");

  verify_normalize_or_autosearch_urls (view, normalize_or_autosearch_test_google, G_N_ELEMENTS (normalize_or_autosearch_test_google));

  ephy_search_engine_manager_delete_engine (manager, test_engine);

  ephy_search_engine_manager_set_default_engine (manager, default_engine);
  g_assert_true (ephy_search_engine_manager_get_default_engine (manager) == default_engine);
  g_object_unref (g_object_ref_sink (view));
}

static void
quit_main_loop_when_load_finished (WebKitWebView   *view,
                                   WebKitLoadEvent  load_event,
                                   GMainLoop       *loop)
{
  if (load_event != WEBKIT_LOAD_FINISHED)
    return;

  g_main_loop_quit (loop);
  g_signal_handlers_disconnect_by_func (view, quit_main_loop_when_load_finished, NULL);
}

static guint back_forward_list_counter = 0;

static void
back_forward_list_changed (WebKitBackForwardList     *list,
                           WebKitBackForwardListItem *added_item,
                           GList                     *removed_items,
                           GMainLoop                 *loop)
{
  back_forward_list_counter--;

  if (back_forward_list_counter == 0)
    g_main_loop_quit (loop);
}

static void
wait_until_back_forward_list_changes (WebKitWebView *view,
                                      GMainLoop     *loop)
{
  WebKitBackForwardList *back_forward_list = webkit_web_view_get_back_forward_list (view);
  g_signal_connect (back_forward_list, "changed", G_CALLBACK (back_forward_list_changed), loop);
}

static GMainLoop *
setup_ensure_back_forward_list_changes (EphyWebView *view)
{
  GMainLoop *loop;

  back_forward_list_counter = 1;

  loop = g_main_loop_new (NULL, FALSE);
  wait_until_back_forward_list_changes (WEBKIT_WEB_VIEW (view), loop);

  return loop;
}

static void
ensure_back_forward_list_changes (GMainLoop *loop)
{
  if (back_forward_list_counter != 0)
    g_main_loop_run (loop);

  g_assert_cmpint (back_forward_list_counter, ==, 0);
  g_main_loop_unref (loop);
}

static void
test_ephy_web_view_provisional_load_failure_updates_back_forward_list (void)
{
  GMainLoop *loop;
  EphyWebView *view;
  const char *bad_url;

  view = EPHY_WEB_VIEW (ephy_web_view_new ());

  loop = setup_ensure_back_forward_list_changes (view);
  bad_url = "https://localhost:2984375930/";

  ephy_web_view_load_url (view, bad_url);

  ensure_back_forward_list_changes (loop);

  g_assert_nonnull (webkit_back_forward_list_get_current_item (webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW (view))));

  g_assert_cmpstr (bad_url, ==, webkit_back_forward_list_item_get_uri (
                     webkit_back_forward_list_get_current_item (
                       webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW (view)))));

  g_object_unref (g_object_ref_sink (view));
}

static void
visit_url_cb (EphyHistoryService *service,
              EphyHistoryURL     *url,
              gpointer            user_data)
{
  if (strcmp (url->url, "https://localhost:2984375932/") != 0)
    return;

  /* We are only loading an error page, this code should never be
   * reached. */
  g_assert_not_reached ();
}

static void
test_ephy_web_view_error_pages_not_stored_in_history (void)
{
  GMainLoop *loop;
  EphyWebView *view;
  const char *bad_url;
  EphyHistoryService *history_service;
  EphyEmbedShell *embed_shell = ephy_embed_shell_get_default ();

  view = EPHY_WEB_VIEW (ephy_web_view_new ());
  loop = g_main_loop_new (NULL, FALSE);
  bad_url = "https://localhost:2984375932/";

  history_service = ephy_embed_shell_get_global_history_service (embed_shell);
  g_assert_nonnull (history_service);
  g_signal_connect (history_service, "visit-url",
                    G_CALLBACK (visit_url_cb), NULL);

  ephy_web_view_load_url (view, bad_url);

  g_signal_connect (view, "load-changed",
                    G_CALLBACK (quit_main_loop_when_load_finished), loop);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);
  g_signal_handlers_disconnect_by_func (history_service, G_CALLBACK (visit_url_cb), NULL);

  g_object_unref (g_object_ref_sink (view));
}

int
main (int   argc,
      char *argv[])
{
  int ret;
  SoupServer *server;

  /* FIXME: disable AC mode for now for WebView tests because CI doesn't support it. */
  g_setenv ("WEBKIT_DISABLE_COMPOSITING_MODE", "1", FALSE);

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  _ephy_shell_create_instance (EPHY_EMBED_SHELL_MODE_TEST);

  server = soup_server_new (NULL, NULL);
  soup_server_add_handler (server, NULL, server_callback, NULL, NULL);
  soup_server_listen_local (server, SERVER_PORT,
                            SOUP_SERVER_LISTEN_IPV4_ONLY, NULL);

  g_test_add_func ("/embed/ephy-web-view/non_search_regex",
                   test_ephy_web_view_non_search_regex);

  g_test_add_func ("/embed/ephy-web-view/normalize_or_autosearch",
                   test_ephy_web_view_normalize_or_autosearch);

  g_test_add_func ("/embed/ephy-web-view/load_url",
                   test_ephy_web_view_load_url);

  g_test_add_func ("/embed/ephy-web-view/provisional_load_failure_updates_back_forward_list",
                   test_ephy_web_view_provisional_load_failure_updates_back_forward_list);

  g_test_add_func ("/embed/ephy-web-view/error-pages-not-stored-in-history",
                   test_ephy_web_view_error_pages_not_stored_in_history);

  ret = g_test_run ();

  g_object_unref (server);
  g_object_unref (ephy_shell_get_default ());
  ephy_file_helpers_shutdown ();

  return ret;
}

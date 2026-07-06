/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2010, 2011, 2012 Igalia S.L.
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

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-history-service.h"

static const char *
test_db_filename (void)
{
  static char *filename = NULL;
  if (!filename)
    filename = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  return filename;
}

static EphyHistoryService *
ensure_empty_history (const char *filename)
{
  if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    g_unlink (filename);

  return ephy_history_service_new (filename, EPHY_SQLITE_CONNECTION_MODE_READWRITE);
}

static void
test_create_history_service (void)
{
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());

  g_object_unref (service);
}

static void
destroy_history_service_and_end_main_loop (EphyHistoryService *service)
{
  GMainLoop *loop = g_object_steal_data (G_OBJECT (service), "main-loop");

  g_object_unref (service);
  g_main_loop_quit (loop);
}

static void
test_create_history_service_and_destroy_later (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());

  g_timeout_add_once (100, (GSourceOnceFunc)destroy_history_service_and_end_main_loop, service);

  g_object_set_data (G_OBJECT (service), "main-loop", loop);

  g_main_loop_run (loop);
}

static void
page_vist_created (EphyHistoryService *service,
                   GAsyncResult       *result,
                   gpointer            user_data)
{
  GMainLoop *loop = g_object_steal_data (G_OBJECT (service), "main-loop");
  g_autoptr (GError) error = NULL;
  gboolean success;

  if (user_data) {
    g_assert_true (EPHY_IS_HISTORY_SERVICE (user_data));
    g_object_unref (user_data);
  }

  success = ephy_history_service_add_visit_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  g_object_unref (service);
  g_main_loop_quit (loop);
}

static void
test_create_history_entry (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());

  EphyHistoryPageVisit *visit = ephy_history_page_visit_new ("http://www.gnome.org", 0, EPHY_PAGE_VISIT_TYPED);
  ephy_history_service_add_visit (service, visit, NULL, (GAsyncReadyCallback)page_vist_created, NULL);
  ephy_history_page_visit_free (visit);

  g_object_set_data (G_OBJECT (service), "main-loop", loop);
  g_main_loop_run (loop);
}

static void
test_readonly_mode (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());
  EphyHistoryService *readonly_service = ephy_history_service_new (test_db_filename (), EPHY_SQLITE_CONNECTION_MODE_MEMORY);

  /* Having the database open read-only should not break normal connections.
   * https://bugzilla.gnome.org/show_bug.cgi?id=778649 */
  EphyHistoryPageVisit *visit = ephy_history_page_visit_new ("http://www.gnome.org", 0, EPHY_PAGE_VISIT_TYPED);
  ephy_history_service_add_visit (service, visit, NULL, (GAsyncReadyCallback)page_vist_created, readonly_service);
  ephy_history_page_visit_free (visit);

  g_object_set_data (G_OBJECT (service), "main-loop", loop);
  g_main_loop_run (loop);
}

static GList *
create_test_page_visit_list (void)
{
  GList *visits = NULL;
  int i;
  for (i = 0; i < 100; i++) {
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.gnome.org", 3, EPHY_PAGE_VISIT_TYPED));
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.gnome.org", 5, EPHY_PAGE_VISIT_TYPED));
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.cuteoverload.com", 7, EPHY_PAGE_VISIT_TYPED));
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.cuteoverload.com", 8, EPHY_PAGE_VISIT_TYPED));
  }
  return visits;
}

static void
verify_create_history_entry_cb (EphyHistoryService *service,
                                GAsyncResult       *result,
                                gpointer            user_data)
{
  GMainLoop *loop = g_object_steal_data (G_OBJECT (service), "main-loop");
  g_autoptr (GError) error = NULL;
  GList *visits;
  GList *baseline_visits = create_test_page_visit_list ();
  GList *current;
  GList *current_baseline = baseline_visits;

  g_assert_null (user_data);
  visits = ephy_history_service_query_visits_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (visits);
  g_assert_cmpint (g_list_length (visits), ==, g_list_length (baseline_visits));

  current = visits;
  while (current_baseline) {
    EphyHistoryPageVisit *visit, *baseline_visit;

    g_assert_nonnull (current);
    visit = (EphyHistoryPageVisit *)current->data;
    baseline_visit = (EphyHistoryPageVisit *)current_baseline->data;

    g_assert_cmpstr (visit->url->url, ==, baseline_visit->url->url);
    g_assert_cmpstr (visit->url->title, ==, baseline_visit->url->title);
    g_assert_cmpint (visit->visit_time, ==, baseline_visit->visit_time);
    g_assert_cmpint (visit->visit_type, ==, baseline_visit->visit_type);

    current = current->next;
    current_baseline = current_baseline->next;
  }

  ephy_history_page_visit_list_free (baseline_visits);
  g_list_free_full (visits, (GDestroyNotify)ephy_history_page_visit_free);

  g_object_unref (service);
  g_main_loop_quit (loop);
}

static void
verify_create_history_entry (EphyHistoryService *service,
                             GAsyncResult       *result,
                             gpointer            user_data)
{
  g_autoptr (GError) error = NULL;
  gboolean success;

  g_assert_cmpint (42, ==, GPOINTER_TO_INT (user_data));
  success = ephy_history_service_add_visits_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  ephy_history_service_find_visits_in_time (service, 0, 8, NULL, (GAsyncReadyCallback)verify_create_history_entry_cb, NULL);
}

static void
test_create_history_entries (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());

  GList *visits = create_test_page_visit_list ();

  /* We use 42 here just to verify that user_data is passed properly to the callback */
  ephy_history_service_add_visits (service, visits, NULL, (GAsyncReadyCallback)verify_create_history_entry, GINT_TO_POINTER (42));
  ephy_history_page_visit_list_free (visits);

  g_object_set_data (G_OBJECT (service), "main-loop", loop);
  g_main_loop_run (loop);
}

static void
get_url (EphyHistoryService *service,
         GAsyncResult       *result,
         gpointer            user_data)
{
  g_autoptr (GError) error = NULL;
  EphyHistoryURL *url;

  url = ephy_history_service_get_url_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (url);
  g_assert_cmpstr (url->title, ==, "GNOME");

  ephy_history_url_free (url);
  g_object_unref (service);
  g_main_loop_quit (user_data);
}

static void
set_url_title (EphyHistoryService *service,
               GAsyncResult       *result,
               gpointer            user_data)
{
  GMainLoop *loop = g_object_steal_data (G_OBJECT (service), "main-loop");
  gboolean test_result = GPOINTER_TO_INT (user_data);
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = ephy_history_service_set_url_title_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  if (!test_result) {
    g_object_unref (service);
    g_main_loop_quit (loop);
  } else
    ephy_history_service_get_url (service, "http://www.gnome.org", NULL, (GAsyncReadyCallback)get_url, loop);
}

static void
set_url_title_visit_created (EphyHistoryService *service,
                             GAsyncResult       *result,
                             gpointer            user_data)
{
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = ephy_history_service_add_visit_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  ephy_history_service_set_url_title (service, "http://www.gnome.org", "GNOME", NULL, (GAsyncReadyCallback)set_url_title, user_data);
}

static void
test_set_url_title_helper (gboolean test_results)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());

  EphyHistoryPageVisit *visit = ephy_history_page_visit_new ("http://www.gnome.org", 0, EPHY_PAGE_VISIT_TYPED);
  ephy_history_service_add_visit (service, visit, NULL, (GAsyncReadyCallback)set_url_title_visit_created, GINT_TO_POINTER (test_results));
  ephy_history_page_visit_free (visit);

  g_object_set_data (G_OBJECT (service), "main-loop", loop);
  g_main_loop_run (loop);
}

static void
test_set_url_title (void)
{
  test_set_url_title_helper (FALSE);
}

static void
test_set_url_title_is_correct (void)
{
  test_set_url_title_helper (TRUE);
}

static void
set_url_title_url_not_existent (EphyHistoryService *service,
                                GAsyncResult       *result,
                                gpointer            user_data)
{
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = ephy_history_service_set_url_title_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_false (success);
  g_object_unref (service);
  g_main_loop_quit (user_data);
}

static void
test_set_url_title_url_not_existent (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());

  ephy_history_service_set_url_title (service, "http://www.gnome.org", "GNOME", NULL, (GAsyncReadyCallback)set_url_title_url_not_existent, loop);

  g_main_loop_run (loop);
}

static void
test_get_url_done (EphyHistoryService *service,
                   GAsyncResult       *result,
                   gpointer            user_data)
{
  GMainLoop *loop = g_object_steal_data (G_OBJECT (service), "main-loop");
  EphyHistoryURL *url;
  gboolean expected_success = GPOINTER_TO_INT (user_data);
  g_autoptr (GError) error = NULL;

  url = ephy_history_service_get_url_finish (service, result, &error);
  if (expected_success) {
    g_assert_no_error (error);
    g_assert_nonnull (url);
    g_assert_cmpstr (url->url, ==, "http://www.gnome.org");
    g_assert_cmpint (url->id, !=, -1);
  } else {
    g_assert_null (url);
  }

  if (url)
    ephy_history_url_free (url);

  g_object_unref (service);
  g_main_loop_quit (loop);
}

static void
test_get_url_visit_added (EphyHistoryService *service,
                          GAsyncResult       *result,
                          gpointer            user_data)
{
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = ephy_history_service_add_visit_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  ephy_history_service_get_url (service, "http://www.gnome.org", NULL, (GAsyncReadyCallback)test_get_url_done, user_data);
}

static void
test_get_url_helper (gboolean add_entry)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());

  if (add_entry) {
    EphyHistoryPageVisit *visit = ephy_history_page_visit_new ("http://www.gnome.org", 0, EPHY_PAGE_VISIT_TYPED);
    ephy_history_service_add_visit (service, visit, NULL, (GAsyncReadyCallback)test_get_url_visit_added, GINT_TO_POINTER (add_entry));
    ephy_history_page_visit_free (visit);
  } else {
    ephy_history_service_get_url (service, "http://www.gnome.org", NULL, (GAsyncReadyCallback)test_get_url_done, GINT_TO_POINTER (add_entry));
  }

  g_object_set_data (G_OBJECT (service), "main-loop", loop);
  g_main_loop_run (loop);
}

static void
test_get_url (void)
{
  test_get_url_helper (TRUE);
}

static void
test_get_url_not_existent (void)
{
  test_get_url_helper (FALSE);
}

static GList *
create_visits_for_complex_tests (void)
{
  int i;
  GList *visits = NULL;

  for (i = 0; i < 10; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.gnome.org", 10 * i, EPHY_PAGE_VISIT_TYPED));
  for (i = 0; i < 30; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.wikipedia.org", 10 * i, EPHY_PAGE_VISIT_TYPED));
  for (i = 0; i < 20; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.freedesktop.org", 10 * i, EPHY_PAGE_VISIT_TYPED));
  for (i = 0; i < 5; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.musicbrainz.org", 10 * i, EPHY_PAGE_VISIT_TYPED));
  for (i = 0; i < 2; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.webkitgtk.org", 1000 * i, EPHY_PAGE_VISIT_TYPED));

  return visits;
}

static void
verify_complex_url_query (EphyHistoryService *service,
                          GAsyncResult       *result,
                          gpointer            user_data)
{
  GMainLoop *loop = g_object_steal_data (G_OBJECT (service), "main-loop");
  EphyHistoryURL *url, *baseline;
  g_autoptr (GError) error = NULL;
  GList *urls;

  urls = ephy_history_service_query_urls_finish (service, result, &error);
  g_assert_no_error (error);

  /* Only one result expected. */
  g_assert_cmpint (g_list_length (urls), ==, 1);

  url = (EphyHistoryURL *)urls->data;
  baseline = (EphyHistoryURL *)user_data;

  g_assert_cmpstr (url->url, ==, baseline->url);
  g_assert_cmpuint (url->visit_count, ==, baseline->visit_count);

  ephy_history_url_free (baseline);
  g_list_free_full (urls, (GDestroyNotify)ephy_history_url_free);
  g_object_unref (service);

  g_main_loop_quit (loop);
}

static void
perform_complex_url_query (EphyHistoryService *service,
                           GAsyncResult       *result,
                           gpointer            user_data)
{
  EphyHistoryQuery *query;
  EphyHistoryURL *url;
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = ephy_history_service_add_visits_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  /* Get the most visited site that contains 'k'. */
  query = ephy_history_query_new ();
  query->substring_list = g_list_prepend (query->substring_list, g_strdup ("k"));
  query->limit = 1;
  query->sort_type = EPHY_HISTORY_SORT_MOST_VISITED;

  /* The expected result. */
  url = ephy_history_url_new ("http://www.wikipedia.org",
                              "Wikipedia",
                              30, 30, 0);

  ephy_history_service_query_urls (service, query, NULL, (GAsyncReadyCallback)verify_complex_url_query, url);
  ephy_history_query_free (query);
}

static void
test_complex_url_query (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());
  GList *visits;

  visits = create_visits_for_complex_tests ();
  ephy_history_service_add_visits (service, visits, NULL, (GAsyncReadyCallback)perform_complex_url_query, NULL);
  ephy_history_page_visit_list_free (visits);

  g_object_set_data (G_OBJECT (service), "main-loop", loop);
  g_main_loop_run (loop);
}

static void
perform_complex_url_query_with_time_range (EphyHistoryService *service,
                                           GAsyncResult       *result,
                                           gpointer            user_data)
{
  EphyHistoryQuery *query;
  EphyHistoryURL *url;
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = ephy_history_service_add_visits_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  /* Get the most visited site that contains 'k' that was visited since timestamp 500. */
  query = ephy_history_query_new ();
  query->substring_list = g_list_prepend (query->substring_list, g_strdup ("k"));
  query->limit = 1;
  query->sort_type = EPHY_HISTORY_SORT_MOST_VISITED;
  query->from = 500;

  /* The expected result. */
  url = ephy_history_url_new ("http://www.webkitgtk.org",
                              "WebKitGTK",
                              2, 2, 0);

  ephy_history_service_query_urls (service, query, NULL, (GAsyncReadyCallback)verify_complex_url_query, url);
  ephy_history_query_free (query);
}

static void
test_complex_url_query_with_time_range (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());
  GList *visits;

  visits = create_visits_for_complex_tests ();
  ephy_history_service_add_visits (service, visits, NULL, (GAsyncReadyCallback)perform_complex_url_query_with_time_range, NULL);
  ephy_history_page_visit_list_free (visits);

  g_object_set_data (G_OBJECT (service), "main-loop", loop);
  g_main_loop_run (loop);
}

static void
verify_query_after_clear (EphyHistoryService *service,
                          GAsyncResult       *result,
                          gpointer            user_data)
{
  GList *urls;
  g_autoptr (GError) error = NULL;

  urls = ephy_history_service_query_urls_finish (service, result, &error);
  g_assert_no_error (error);

  /* No results expected. */
  g_assert_cmpint (g_list_length (urls), ==, 0);

  g_list_free_full (urls, (GDestroyNotify)ephy_history_url_free);
  g_object_unref (service);

  g_main_loop_quit (user_data);
}

static void
perform_query_after_clear (EphyHistoryService *service,
                           GAsyncResult       *result,
                           gpointer            user_data)
{
  EphyHistoryQuery *query;
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = ephy_history_service_clear_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (success);

  /* Get 10 random sites, the query should fail. */
  query = ephy_history_query_new ();
  query->substring_list = g_list_prepend (query->substring_list, g_strdup ("gnome"));
  query->limit = 10;
  query->sort_type = EPHY_HISTORY_SORT_MOST_VISITED;

  ephy_history_service_query_urls (service, query, NULL, (GAsyncReadyCallback)verify_query_after_clear, user_data);
  ephy_history_query_free (query);
}

static void
test_clear (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());
  GList *visits = create_test_page_visit_list ();

  ephy_history_service_add_visits (service, visits, NULL, NULL, NULL);
  ephy_history_page_visit_list_free (visits);

  ephy_history_service_clear (service, NULL, (GAsyncReadyCallback)perform_query_after_clear, loop);

  g_main_loop_run (loop);
}

static void
test_cancellation_cb (EphyHistoryService *service,
                      GAsyncResult       *result,
                      gpointer            user_data)
{
  GMainLoop *loop = user_data;
  g_autoptr (GError) error = NULL;
  GList *urls;

  urls = ephy_history_service_find_urls_finish (service, result, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_assert_null (urls);

  g_object_unref (service);
  g_main_loop_quit (loop);
}

static void
test_cancellation (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  EphyHistoryService *service = ensure_empty_history (test_db_filename ());
  g_autoptr (GCancellable) cancellable = g_cancellable_new ();

  ephy_history_service_find_urls (service, 0, 0, -1, 0, NULL,
                                  EPHY_HISTORY_SORT_MOST_RECENTLY_VISITED,
                                  cancellable,
                                  (GAsyncReadyCallback)test_cancellation_cb,
                                  loop);

  g_cancellable_cancel (cancellable);
  g_main_loop_run (loop);
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  gtk_test_init (&argc, &argv);

  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  g_test_add_func ("/embed/history/test_create_history_service", test_create_history_service);
  g_test_add_func ("/embed/history/test_create_history_service_and_destroy_later", test_create_history_service_and_destroy_later);
  g_test_add_func ("/embed/history/test_create_history_entry", test_create_history_entry);
  g_test_add_func ("/embed/history/test_readonly_mode", test_readonly_mode);
  g_test_add_func ("/embed/history/test_create_history_entries", test_create_history_entries);
  g_test_add_func ("/embed/history/test_set_url_title", test_set_url_title);
  g_test_add_func ("/embed/history/test_set_url_title_is_correct", test_set_url_title_is_correct);
  g_test_add_func ("/embed/history/test_set_url_title_url_not_existent", test_set_url_title_url_not_existent);
  g_test_add_func ("/embed/history/test_get_url", test_get_url);
  g_test_add_func ("/embed/history/test_get_url_not_existent", test_get_url_not_existent);
  g_test_add_func ("/embed/history/test_complex_url_query", test_complex_url_query);
  g_test_add_func ("/embed/history/test_complex_url_query_with_time_range", test_complex_url_query_with_time_range);
  g_test_add_func ("/embed/history/test_clear", test_clear);
  g_test_add_func ("/embed/history/test_cancellation", test_cancellation);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();

  if (g_file_test (test_db_filename (), G_FILE_TEST_IS_REGULAR))
    g_assert_cmpint (g_unlink (test_db_filename ()), ==, 0);

  return ret;
}

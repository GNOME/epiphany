/* vim: set sw=2 ts=2 sts=2 et: */
/*
 * ephy-sqlite-statement.c
 * This file is part of Epiphany
 *
 * Copyright © 2010 Igalia S.L.
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

#include "ephy-history-service.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

static EphyHistoryService *
ensure_empty_history (const char* filename)
{
  if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    g_unlink (filename);

  return ephy_history_service_new (filename);
}

static void
test_create_history_service (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  EphyHistoryService *service = ensure_empty_history (temporary_file);

  g_free (temporary_file);
  g_object_unref (service);
}

static gboolean
destroy_history_service_and_end_main_loop (EphyHistoryService *service)
{
  g_object_unref (service);
  g_assert (TRUE);
  gtk_main_quit ();

  return FALSE;
}

static void
test_create_history_service_and_destroy_later (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  EphyHistoryService *service = ensure_empty_history (temporary_file);
  g_free (temporary_file);
  g_timeout_add (100, (GSourceFunc) destroy_history_service_and_end_main_loop, service);

  gtk_main ();
}

static void
page_vist_created (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  g_object_unref (service);
  g_assert (result_data == NULL);
  g_assert (user_data == NULL);
  g_assert (success);
  gtk_main_quit ();
}

static void
test_create_history_entry (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  EphyHistoryService *service = ensure_empty_history(temporary_file);

  EphyHistoryPageVisit *visit = ephy_history_page_visit_new ("http://www.gnome.org", 0, EPHY_PAGE_VISIT_TYPED);
  ephy_history_service_add_visit (service, visit, page_vist_created, NULL);
  ephy_history_page_visit_free (visit);
  g_free (temporary_file);

  gtk_main ();
}

static GList *
create_test_page_visit_list ()
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
verify_create_history_entry_cb (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  GList *visits = (GList *) result_data;
  GList *baseline_visits = create_test_page_visit_list ();
  GList *current = visits;
  GList *current_baseline = baseline_visits;

  g_assert (user_data == NULL);
  g_assert (success);
  g_assert (visits != NULL);
  g_assert_cmpint (g_list_length (visits), ==, g_list_length (baseline_visits));

  while (current_baseline) {
    EphyHistoryPageVisit *visit, *baseline_visit;

    g_assert (current);
    visit = (EphyHistoryPageVisit *) current->data;
    baseline_visit = (EphyHistoryPageVisit *) current_baseline->data;

    g_assert_cmpstr (visit->url->url, ==, baseline_visit->url->url);
    g_assert_cmpstr (visit->url->title, ==, baseline_visit->url->title);
    g_assert_cmpint (visit->visit_time, ==, baseline_visit->visit_time);
    g_assert_cmpint (visit->visit_type, ==, baseline_visit->visit_type);

    current = current->next;
    current_baseline = current_baseline->next;
  }

  ephy_history_page_visit_list_free (visits);
  ephy_history_page_visit_list_free (baseline_visits);

  g_object_unref (service);
  gtk_main_quit ();
}

static void
verify_create_history_entry (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  g_assert (result_data == NULL);
  g_assert_cmpint (42, ==, GPOINTER_TO_INT(user_data)); 
  g_assert (success);
  ephy_history_service_find_visits_in_time (service, 0, 8, verify_create_history_entry_cb, NULL);
}

static void
test_create_history_entries (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  EphyHistoryService *service = ensure_empty_history(temporary_file);

  GList *visits = create_test_page_visit_list ();

  /* We use 42 here just to verify that user_data is passed properly to the callback */
  ephy_history_service_add_visits (service, visits, verify_create_history_entry, GINT_TO_POINTER(42));
  ephy_history_page_visit_list_free (visits);
  g_free (temporary_file);

  gtk_main ();
}

static void
get_url (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  EphyHistoryURL *url = (EphyHistoryURL *) result_data;

  g_assert (success == TRUE);
  g_assert (url != NULL);
  g_assert_cmpstr (url->title, ==, "GNOME");

  ephy_history_url_free (url);
  g_object_unref (service);
  gtk_main_quit();
}

static void
set_url_title (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  gboolean test_result = GPOINTER_TO_INT (user_data);
  g_assert (success == TRUE);

  if (test_result == FALSE) {
    g_object_unref (service);
    gtk_main_quit ();
  } else
    ephy_history_service_get_url (service, "http://www.gnome.org", get_url, NULL);
}

static void
set_url_title_visit_created (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  ephy_history_service_set_url_title (service, "http://www.gnome.org", "GNOME", set_url_title, user_data);
}

static void
test_set_url_title_helper (gboolean test_results)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  EphyHistoryService *service = ensure_empty_history(temporary_file);

  EphyHistoryPageVisit *visit = ephy_history_page_visit_new ("http://www.gnome.org", 0, EPHY_PAGE_VISIT_TYPED);
  ephy_history_service_add_visit (service, visit, set_url_title_visit_created, GINT_TO_POINTER (test_results));
  ephy_history_page_visit_free (visit);
  g_free (temporary_file);

  gtk_main ();
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
set_url_title_url_not_existent (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  g_assert (success == FALSE);
  g_object_unref (service);
  gtk_main_quit ();
}

static void
test_set_url_title_url_not_existent (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  EphyHistoryService *service = ensure_empty_history(temporary_file);
  g_free (temporary_file);

  ephy_history_service_set_url_title (service, "http://www.gnome.org", "GNOME", set_url_title_url_not_existent, NULL);

  gtk_main();
}

static void
test_get_url_done (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  EphyHistoryURL *url;
  gboolean expected_success = GPOINTER_TO_INT (user_data);

  url = (EphyHistoryURL *)result_data;

  g_assert (success == expected_success);

  if (expected_success == TRUE) {
    g_assert (url != NULL);
    g_assert_cmpstr (url->url, ==, "http://www.gnome.org");
    g_assert_cmpint (url->id, !=, -1);
    ephy_history_url_free (url);
  } else
    g_assert (url == NULL);

  g_object_unref (service);
  gtk_main_quit ();
}

static void
test_get_url_visit_added (EphyHistoryService *service, gboolean success, gpointer result_data, gpointer user_data)
{
  g_assert (success == TRUE);

  ephy_history_service_get_url (service, "http://www.gnome.org", test_get_url_done, user_data);
}

static void
test_get_url_helper (gboolean add_entry)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  EphyHistoryService *service = ensure_empty_history(temporary_file);
  g_free (temporary_file);

  if (add_entry == TRUE) {
    EphyHistoryPageVisit *visit = ephy_history_page_visit_new ("http://www.gnome.org", 0, EPHY_PAGE_VISIT_TYPED);
    ephy_history_service_add_visit (service, visit, test_get_url_visit_added, GINT_TO_POINTER (add_entry));
    ephy_history_page_visit_free (visit);
  } else
    ephy_history_service_get_url (service, "http://www.gnome.org", test_get_url_done, GINT_TO_POINTER (add_entry));

  gtk_main();
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

static void
verify_complex_url_query (EphyHistoryService *service,
                          gboolean success,
                          gpointer result_data,
                          gpointer user_data)
{
  EphyHistoryURL *url, *baseline;
  GList *urls = (GList*) result_data;

  /* Only one result expected. */
  g_assert_cmpint (g_list_length (urls), ==, 1);

  url = (EphyHistoryURL *) urls->data;
  baseline = (EphyHistoryURL *) user_data;

  g_assert_cmpstr (url->url, ==, baseline->url);
  g_assert_cmpuint (url->visit_count, ==, baseline->visit_count);

  g_object_unref (service);

  gtk_main_quit();
}

static void
perform_complex_url_query (EphyHistoryService *service,
                           gboolean success,
                           gpointer result_data,
                           gpointer user_data)
{
  EphyHistoryQuery *query;
  EphyHistoryURL *url;

  g_assert (success == TRUE);

  /* Get the most visited site that contains 'k'. */
  query = ephy_history_query_new ();
  query->substring_list = g_list_prepend (query->substring_list, "k");
  query->limit = 1;
  query->sort_type = EPHY_HISTORY_SORT_MV;

  /* The expected result. */
  url = ephy_history_url_new ("http://www.wikipedia.org",
                              "Wikipedia",
                              30, 30, 0, 1);

  ephy_history_service_query_urls (service, query, verify_complex_url_query, url);
}

static void
test_complex_url_query (void)
{
  gchar *temporary_file = g_build_filename (g_get_tmp_dir (), "epiphany-history-test.db", NULL);
  EphyHistoryService *service = ensure_empty_history(temporary_file);
  GList *visits = NULL;
  int i;

  for (i = 0; i < 10; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.gnome.org", 10 * i, EPHY_PAGE_VISIT_TYPED));
  for (i = 0; i < 30; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.wikipedia.org", 10 * i, EPHY_PAGE_VISIT_TYPED));
  for (i = 0; i < 20; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.freedesktop.org", 10 * i, EPHY_PAGE_VISIT_TYPED));
  for (i = 0; i < 5; i++)
    visits = g_list_append (visits, ephy_history_page_visit_new ("http://www.musicbrainz.org", 10 * i, EPHY_PAGE_VISIT_TYPED));

  ephy_history_service_add_visits (service, visits, perform_complex_url_query, NULL);

  gtk_main ();
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/embed/history/test_create_history_service", test_create_history_service);
  g_test_add_func ("/embed/history/test_create_history_service_and_destroy_later", test_create_history_service_and_destroy_later);
  g_test_add_func ("/embed/history/test_create_history_entry", test_create_history_entry);
  g_test_add_func ("/embed/history/test_create_history_entries", test_create_history_entries);
  g_test_add_func ("/embed/history/test_set_url_title", test_set_url_title);
  g_test_add_func ("/embed/history/test_set_url_title_is_correct", test_set_url_title_is_correct);
  g_test_add_func ("/embed/history/test_set_url_title_url_not_existent", test_set_url_title_url_not_existent);
  g_test_add_func ("/embed/history/test_get_url", test_get_url);
  g_test_add_func ("/embed/history/test_get_url_not_existent", test_get_url_not_existent);
  g_test_add_func ("/embed/history/test_complex_url_query", test_complex_url_query);

  return g_test_run ();
}

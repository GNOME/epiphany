/* ephy-search-engine-manager-tests.c
 *
 * Copyright 2021 vanadiae <vanadiae35@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-search-engine-manager.h"
#include "ephy-settings.h"

static void
test_search_bang_for_name (void)
{
  struct {
    char *name;
    char *expected_bang;
  } test_results[] = {
    {"", ""},
    {"  (  ( ", ""},
    {"  DuckDuckGo   ", "!ddg"},
    {"DuckDuck go", "!ddg"},
    {"DuckDuck Go", "!ddg"},
    {"duck duck go", "!ddg"},
    {"duckduckgo", "!d"},
    {"Wikipedia (en)", "!we"},
    {"Wikipedia(en)", "!we"},
  };

  for (guint i = 0; i < G_N_ELEMENTS (test_results); i++) {
    g_autofree char *built_bang = ephy_search_engine_build_bang_for_name (test_results[i].name);

    g_message ("Testing bang %s for name %s in %s", test_results[i].expected_bang,
               test_results[i].name, __func__);
    g_assert_cmpstr (test_results[i].expected_bang, ==, built_bang);
  }
}

static void
test_search_engine_manager (void)
{
  g_autoptr (EphySearchEngineManager) manager = ephy_search_engine_manager_new ();
  g_autoptr (EphySearchEngine) wikipedia = NULL;
  g_autoptr (EphySearchEngine) ddg = NULL;
  g_autoptr (EphySearchEngine) google = NULL;
  EphySearchEngine *expected_engines[3];
  g_autofree char *built_search_address = NULL;

  g_assert_true (EPHY_IS_SEARCH_ENGINE_MANAGER (manager));

  /* NULL check for properties, as we expect those to kept to an empty "" string
   * at the bare minimum.
   */
  wikipedia = g_object_new (EPHY_TYPE_SEARCH_ENGINE, NULL);
  g_assert_true (EPHY_IS_SEARCH_ENGINE (wikipedia));
  g_assert_nonnull (ephy_search_engine_get_name (wikipedia));
  g_assert_nonnull (ephy_search_engine_get_url (wikipedia));
  g_assert_nonnull (ephy_search_engine_get_bang (wikipedia));

  ephy_search_engine_set_name (wikipedia, "Wikipedia TEST");
  g_assert_cmpstr (ephy_search_engine_get_name (wikipedia), ==, "Wikipedia TEST");
  ephy_search_engine_set_url (wikipedia, "https://wikipedia.org/%s");
  g_assert_cmpstr (ephy_search_engine_get_url (wikipedia), ==, "https://wikipedia.org/%s");
  ephy_search_engine_set_bang (wikipedia, "!w");
  g_assert_cmpstr (ephy_search_engine_get_bang (wikipedia), ==, "!w");

  built_search_address = ephy_search_engine_build_search_address (wikipedia, "EPHY TEST SEARCH QUERY");
  g_assert_cmpstr (built_search_address, ==, "https://wikipedia.org/EPHY+TEST+SEARCH+QUERY");
  g_assert_false (ephy_search_engine_manager_has_bang (manager, "!w"));
  ephy_search_engine_manager_add_engine (manager, wikipedia);
  g_assert_true (ephy_search_engine_manager_has_bang (manager, "!w"));
  g_assert_true (ephy_search_engine_manager_find_engine_by_name (manager, ephy_search_engine_get_name (wikipedia)) == wikipedia);
  g_assert_true (ephy_search_engine_manager_get_default_engine (manager) != wikipedia);

  /* Ensure we have the default search engines coming from gschema. */
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (manager)), >=, 3 + 1);
  for (guint i = 0;;) {
    g_autoptr (EphySearchEngine) engine = g_list_model_get_item (G_LIST_MODEL (manager), i);

    if (!engine)
      break;

    g_assert_true (EPHY_IS_SEARCH_ENGINE (engine));
    g_assert_cmpstr (ephy_search_engine_get_name (engine), !=, "");
    g_assert_cmpstr (ephy_search_engine_get_url (engine), !=, "");
    g_assert_cmpstr (ephy_search_engine_get_bang (engine), !=, "");

    /* We'll want to start with a fresh start, so delete all engines except
     * our test one. We can't delete all default engines right at the beginning
     * because our logic in the EphySearchEngineManager asserts we always keep
     * a search engine available. So instead, hack around by using an index that
     * only skips our own search engine when we get it, but stays at the same index
     * otherwise (since iterating normally and removing items is not allowed like
     * in any array-like type in any programming language).
     */
    if (g_strcmp0 (ephy_search_engine_get_name (engine), ephy_search_engine_get_name (wikipedia)) != 0)
      ephy_search_engine_manager_delete_engine (manager, engine);
    else
      /* Skip our own search engine so we don't infinite loop. */
      i++;
  }
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (manager)), ==, 1);
  /* We expect the manager to set the first search engine as default when
   * we've removed the currently default one. Since we've deleted all engines
   * expect ours, just test it here.
   */
  g_assert_true (ephy_search_engine_manager_get_default_engine (manager) == wikipedia);

  ddg = g_object_new (EPHY_TYPE_SEARCH_ENGINE,
                      "name", "Duckduckgo TEST",
                      "url", "https://duckduckgo.com/search?q=%s",
                      "bang", "!ddg",
                      NULL);
  ephy_search_engine_manager_add_engine (manager, ddg);
  g_assert_true (ephy_search_engine_manager_find_engine_by_name (manager, "Duckduckgo TEST"));
  google = g_object_new (EPHY_TYPE_SEARCH_ENGINE,
                         "name", "Google TEST",
                         "url", "https://google.com/search?q=%s",
                         "bang", "!g",
                         NULL);
  ephy_search_engine_manager_add_engine (manager, google);
  g_assert_true (ephy_search_engine_manager_find_engine_by_name (manager, "Google TEST"));

  /* Make sure that for some reason the default engine didn't change when adding other engines. */
  g_assert_true (ephy_search_engine_manager_get_default_engine (manager) == wikipedia);

  /* Tests sort order. At that point we should have those in that order since
   * they are sorted alphabetically by name in the EphySearchEngineManager.
   */
  expected_engines[0] = ddg;
  expected_engines[1] = google;
  expected_engines[2] = wikipedia;

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (manager)), ==, G_N_ELEMENTS (expected_engines));
  for (guint i = 0; i < G_N_ELEMENTS (expected_engines); i++) {
    g_autoptr (EphySearchEngine) sort_engine = g_list_model_get_item (G_LIST_MODEL (manager), i);

    g_assert_true (EPHY_IS_SEARCH_ENGINE (sort_engine));
    g_assert_cmpstr (ephy_search_engine_get_name (sort_engine), ==, ephy_search_engine_get_name (expected_engines[i]));
    g_assert (sort_engine == expected_engines[i]);
  }

  /* Save before removing the default engine, so we can actually check
   * if we didn't introduce automatic saving in the EphySearchEngineManager code.
   */
  ephy_search_engine_manager_save_to_settings (manager);

  /* Test that keeping the fast-path bangs hash table works fine and picks up
   * changes when an engine changes bang.
   */
  g_assert_true (ephy_search_engine_manager_has_bang (manager, "!ddg"));
  g_assert_false (ephy_search_engine_manager_has_bang (manager, "#DDG"));
  ephy_search_engine_set_bang (ddg, "#DDG");
  g_assert_false (ephy_search_engine_manager_has_bang (manager, "!ddg"));
  g_assert_true (ephy_search_engine_manager_has_bang (manager, "#DDG"));
  ephy_search_engine_set_bang (ddg, "!ddg");
  /* Also check if the hash table is properly updated when deleting or adding an engine. */
  ephy_search_engine_manager_delete_engine (manager, ddg);
  g_assert_false (ephy_search_engine_manager_has_bang (manager, "!ddg"));
  ephy_search_engine_manager_add_engine (manager, ddg);
  g_assert_true (ephy_search_engine_manager_has_bang (manager, "!ddg"));

  /* Tests what happens when we remove the default search engine. Our defined
   * behavior is to set the first search engine in the sorted list, so make sure
   * that's working properly here. The order is as tested above.
   */
  ephy_search_engine_manager_set_default_engine (manager, wikipedia);
  g_assert_true (ephy_search_engine_manager_get_default_engine (manager) == wikipedia);
  ephy_search_engine_manager_delete_engine (manager, wikipedia);
  g_assert_true (ephy_search_engine_manager_get_default_engine (manager) == ddg);
  ephy_search_engine_manager_add_engine (manager, wikipedia);
  g_assert_true (ephy_search_engine_manager_get_default_engine (manager) == ddg);

  g_clear_object (&manager);
  g_settings_reset (EPHY_SETTINGS_MAIN, EPHY_PREFS_SEARCH_ENGINES);
}

static void
test_parse_bang_search (void)
{
  g_autoptr (EphySearchEngineManager) manager = ephy_search_engine_manager_new ();
  EphySearchEngine *engine;

  #define DDG_QUERY(x) ("https://duckduckgo.com/search?q=" x)
  #define GOOGLE_QUERY(x) ("https://google.com/search?q=" x)
  #define WIKIPEDIA_QUERY(x) ("https://wikipedia.org/" x)

  struct {
    char *name;
    char *url;
    char *bang;
  } engines[] = {
    {"Duckduckgo", DDG_QUERY ("%s"), "#ddg"},
    {"Google", GOOGLE_QUERY ("%s"), "/g"},
    {"Wikipedia", WIKIPEDIA_QUERY ("%s"), "!w"},
  };
  struct {
    char *bang_search;
    char *expected_url;
  } test_searches[] = {
    /* Empty bang searches. */
    {"", NULL},
    {"      ", NULL},
    /* Bang searches with only one word (i.e. can't be a bang search). */
    {"   eeee", NULL},
    {"eeee    ", NULL},
    {"     eeee    ", NULL},
    {"eeee", NULL},
    /* Bang search without any bang */
    {"This is not a bang search", NULL},
    /* Real bang searches but we check it did respect the order we prefer.
     * Last bang is preferred as it's the one which is more likely to
     * have been chosen or typed last.
     */
    {"     #ddg foobar    ", DDG_QUERY ("foobar")},
    {"#ddg foobar    ", DDG_QUERY ("foobar")},
    {"#ddg foobar", DDG_QUERY ("foobar")},
    {"     #ddg foobar !w    ", WIKIPEDIA_QUERY ("foobar")},
    {"     #ddg foo   bar baz !w    ", WIKIPEDIA_QUERY ("foo+++bar+baz")},
    {"foobar !w    ", WIKIPEDIA_QUERY ("foobar")},
    {"foobar /g", GOOGLE_QUERY ("foobar")},
  };
  g_autoptr (EphySearchEngine) placeholder_engine = g_object_new (EPHY_TYPE_SEARCH_ENGINE, NULL);
  guint i = 0;

  /* Workaround assert in EphySearchEngineManager that ensures we always
   * keep at least one search engine around.
   */
  ephy_search_engine_manager_add_engine (manager, placeholder_engine);
  /* Delete all default engines inherited from gschema. */
  while ((engine = g_list_model_get_item (G_LIST_MODEL (manager), i))) {
    /* Avoid infinite loop by skipping our workaround placeholder engine. */
    if (engine == placeholder_engine)
      i++;
    else
      ephy_search_engine_manager_delete_engine (manager, engine);

    g_clear_object (&engine);
  }

  for (guint i = 0; i < G_N_ELEMENTS (engines); i++) {
    g_autoptr (EphySearchEngine) test_engine =
      g_object_new (EPHY_TYPE_SEARCH_ENGINE,
                    "name", engines[i].name,
                    "url", engines[i].url,
                    "bang", engines[i].bang,
                    NULL);
    ephy_search_engine_manager_add_engine (manager, test_engine);
  }

  for (guint i = 0; i < G_N_ELEMENTS (test_searches); i++) {
    g_autofree char *parsed_search = NULL;

    g_message ("Testing bang search %s parsing in %s", test_searches[i].bang_search, __func__);
    parsed_search =
      ephy_search_engine_manager_parse_bang_search (manager,
                                                    test_searches[i].bang_search);
    g_assert_cmpstr (parsed_search, ==, test_searches[i].expected_url);
  }

  ephy_search_engine_manager_delete_engine (manager, placeholder_engine);
}

static void
test_opensearch (void)
{
  g_autoptr (EphySearchEngineManager) manager = ephy_search_engine_manager_new ();
  g_autoptr (EphySearchEngine) opensearch = NULL;
  g_autofree char *built_suggestions_address = NULL;

  g_assert_true (EPHY_IS_SEARCH_ENGINE_MANAGER (manager));

  opensearch = g_object_new (EPHY_TYPE_SEARCH_ENGINE, NULL);
  g_assert_true (EPHY_IS_SEARCH_ENGINE (opensearch));

  g_assert_null (ephy_search_engine_get_suggestions_url (opensearch));
  ephy_search_engine_set_suggestions_url (opensearch, "https://www.opensearch.test/s=%%s");
  g_assert_cmpstr (ephy_search_engine_get_suggestions_url (opensearch), ==, "https://www.opensearch.test/s=%%s");

  g_assert_null (ephy_search_engine_get_opensearch_url (opensearch));
  ephy_search_engine_set_opensearch_url (opensearch, "https://www.opensearch.test/url");
  g_assert_cmpstr (ephy_search_engine_get_opensearch_url (opensearch), ==, "https://www.opensearch.test/url");

  built_suggestions_address = ephy_search_engine_build_suggestions_address (opensearch, "test search");
  g_assert_cmpstr ("https://www.opensearch.test/s=%test+search", ==, built_suggestions_address);
}

int
main (int   argc,
      char *argv[])
{
  int ret;

  ephy_debug_init ();

  g_test_init (&argc, &argv, NULL);

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  g_test_add_func ("/lib/search-engine-manager/test_search_bang_for_name", test_search_bang_for_name);
  g_test_add_func ("/lib/search-engine-manager/test_search_engine_manager", test_search_engine_manager);
  g_test_add_func ("/lib/search-engine-manager/test_parse_bang_search", test_parse_bang_search);
  g_test_add_func ("/lib/search-engine-manager/test_opensearch", test_opensearch);

  ret = g_test_run ();

  ephy_file_helpers_shutdown ();

  return ret;
}

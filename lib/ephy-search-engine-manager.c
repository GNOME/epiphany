/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Cedric Le Moigne <cedlemo@gmx.com>
 *  Copyright 2021 vanadiae <vanadiae35@gmail.com>
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

#include "ephy-search-engine-manager.h"

#include "ephy-file-helpers.h"
#include "ephy-string.h"

#include "ephy-settings.h"
#include "ephy-prefs.h"

struct _EphySearchEngineManager {
  GObject parent_instance;

  GPtrArray *engines;

  EphySearchEngine *default_engine; /* unowned */
  EphySearchEngine *incognito_engine; /* unowned */

  /* This is just to speed things up. It updates based on each search engine's
   * notify::bang signal, so it is never out of sync because signal callbacks
   * are called synchronously. The key is the bang, and the value is the
   * corresponding EphySearchEngine.
   */
  GHashTable *bangs;
};

static void list_model_iface_init (GListModelInterface *iface,
                                   gpointer             iface_data);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphySearchEngineManager, ephy_search_engine_manager,
                               G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_DEFAULT_ENGINE,
  PROP_INCOGNITO_ENGINE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static int
search_engine_compare_func (EphySearchEngine **a,
                            EphySearchEngine **b)
{
  return g_strcmp0 (ephy_search_engine_get_name (*a),
                    ephy_search_engine_get_name (*b));
}

static void
on_search_engine_bang_changed_cb (EphySearchEngine        *engine,
                                  GParamSpec              *pspec,
                                  EphySearchEngineManager *manager)
{
  GHashTableIter iter;
  const char *bang;
  EphySearchEngine *old_bang_engine;

  g_hash_table_iter_init (&iter, manager->bangs);

  /* We have no way of knowing what bang @engine was previously using, so
   * we must iterate the whole bangs hash table to find @engine, remove its
   * bang-engine pair and finally insert it back with its new bang.
   */
  while (g_hash_table_iter_next (&iter, (gpointer *)&bang, (gpointer *)&old_bang_engine)) {
    if (old_bang_engine == engine) {
      /* We found the engine by its pointer (not bang), so we remove it from the hash table. */
      g_hash_table_iter_remove (&iter);
    }
  }

  bang = ephy_search_engine_get_bang (engine);

  /* Now that we've removed the engine from the hash table (with its old bang),
   * we can add it back with its new value, in case its bang isn't empty.
   */
  if (*bang != '\0')
    g_hash_table_insert (manager->bangs, (gpointer)bang, engine);
}

static void
load_search_engines_from_settings (EphySearchEngineManager *manager)
{
  g_autoptr (GVariantIter) iter = NULL;
  GVariant *variant;
  g_autofree char *default_engine_name = g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_DEFAULT_SEARCH_ENGINE);
  g_autofree char *incognito_engine_name = g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_INCOGNITO_SEARCH_ENGINE);

  g_settings_get (EPHY_SETTINGS_MAIN, EPHY_PREFS_SEARCH_ENGINES, "aa{sv}", &iter);

  while ((variant = g_variant_iter_next_value (iter))) {
    g_auto (GVariantDict) dict = {};
    /* Make sure we keep our state clean and respect the non-NULL expectations. */
    const char *name = "", *url = "", *bang = "";
    /* These need to be NULL when there isn't a corresponding key in the dict. */
    const char *suggestions_url = NULL;
    const char *opensearch_url = NULL;
    g_autoptr (EphySearchEngine) search_engine = NULL;

    g_variant_dict_init (&dict, variant);

    /* All of those checks are just to make sure we keep our state clean and
     * respect the non-NULL expectations.
     */
    g_variant_dict_lookup (&dict, "name", "&s", &name);
    g_variant_dict_lookup (&dict, "url", "&s", &url);
    g_variant_dict_lookup (&dict, "bang", "&s", &bang);
    g_variant_dict_lookup (&dict, "suggestions_url", "&s", &suggestions_url);
    g_variant_dict_lookup (&dict, "opensearch_url", "&s", &opensearch_url);

    search_engine = g_object_new (EPHY_TYPE_SEARCH_ENGINE,
                                  "name", name,
                                  "url", url,
                                  "bang", bang,
                                  "suggestions-url", suggestions_url,
                                  "opensearch-url", opensearch_url,
                                  NULL);
    g_assert (EPHY_IS_SEARCH_ENGINE (search_engine));

    /* Bangs are assumed to be unique, so this shouldn't happen unless GSettings
     * are wrongly modified or we messed up input validation in the UI.
     */
    if (g_hash_table_lookup (manager->bangs, bang)) {
      g_warning ("Found bang %s assigned to several search engines in GSettings."
                 "The bang for %s is hence reset to avoid collision.",
                 bang, name);
      ephy_search_engine_set_bang (search_engine, "");
    }

    ephy_search_engine_manager_add_engine (manager, search_engine);
    if (g_strcmp0 (ephy_search_engine_get_name (search_engine), default_engine_name) == 0)
      ephy_search_engine_manager_set_default_engine (manager, search_engine);
    if (g_strcmp0 (ephy_search_engine_get_name (search_engine), incognito_engine_name) == 0)
      ephy_search_engine_manager_set_incognito_engine (manager, search_engine);

    g_variant_unref (variant);
  }

  /* Both of these conditions should never actually be encountered, unless someone
   * messed up with GSettings manually or we did something wrong in the UI
   * (i.e. validation code has an issue in the prefs).
   */
  if (G_UNLIKELY (manager->engines->len == 0)) {
    g_settings_reset (EPHY_SETTINGS_MAIN, EPHY_PREFS_SEARCH_ENGINES);
    g_settings_reset (EPHY_SETTINGS_MAIN, EPHY_PREFS_DEFAULT_SEARCH_ENGINE);
    g_settings_reset (EPHY_SETTINGS_MAIN, EPHY_PREFS_INCOGNITO_SEARCH_ENGINE);
    load_search_engines_from_settings (manager);

    g_warning ("Having no search engine is forbidden. Resetting to default ones instead.");
  }
  g_assert (manager->engines->len > 0);

  if (G_UNLIKELY (!manager->default_engine)) {
    g_warning ("Could not find default search engine set in the gsettings within all available search engines! Setting the first one as fallback.");
    ephy_search_engine_manager_set_default_engine (manager, manager->engines->pdata[0]);
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_DEFAULT_SEARCH_ENGINE, ephy_search_engine_get_name (manager->engines->pdata[0]));
  }

  if (G_UNLIKELY (!manager->incognito_engine)) {
    g_warning ("Could not find incognito search engine set in the gsettings within all available search engines! Setting the first one as fallback.");
    ephy_search_engine_manager_set_incognito_engine (manager, manager->engines->pdata[0]);
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_INCOGNITO_SEARCH_ENGINE, ephy_search_engine_get_name (manager->engines->pdata[0]));
  }
}

static void
ephy_search_engine_manager_init (EphySearchEngineManager *manager)
{
  /* We don't use _new_full(), as we'll directly insert unowned bangs from
   * ephy_search_engine_get_bang(), and the value belongs to us anyway (as part
   * of the list store), so both don't need to be freed.
   */
  manager->bangs = g_hash_table_new (g_str_hash, g_str_equal);

  manager->engines = g_ptr_array_new_with_free_func (g_object_unref);

  load_search_engines_from_settings (manager);
}

static void
ephy_search_engine_manager_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  EphySearchEngineManager *self = EPHY_SEARCH_ENGINE_MANAGER (object);

  switch (prop_id) {
    case PROP_DEFAULT_ENGINE:
      g_value_take_object (value, ephy_search_engine_manager_get_default_engine (self));
      break;
    case PROP_INCOGNITO_ENGINE:
      g_value_take_object (value, ephy_search_engine_manager_get_incognito_engine (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_search_engine_manager_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  EphySearchEngineManager *self = EPHY_SEARCH_ENGINE_MANAGER (object);

  switch (prop_id) {
    case PROP_DEFAULT_ENGINE:
      ephy_search_engine_manager_set_default_engine (self, g_value_get_object (value));
      break;
    case PROP_INCOGNITO_ENGINE:
      ephy_search_engine_manager_set_incognito_engine (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_search_engine_manager_finalize (GObject *object)
{
  EphySearchEngineManager *manager = EPHY_SEARCH_ENGINE_MANAGER (object);

  g_clear_pointer (&manager->bangs, g_hash_table_destroy);
  g_clear_pointer (&manager->engines, g_ptr_array_unref);

  G_OBJECT_CLASS (ephy_search_engine_manager_parent_class)->finalize (object);
}

static void
ephy_search_engine_manager_class_init (EphySearchEngineManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_search_engine_manager_finalize;
  object_class->get_property = ephy_search_engine_manager_get_property;
  object_class->set_property = ephy_search_engine_manager_set_property;

  properties [PROP_DEFAULT_ENGINE] =
    g_param_spec_object ("default-engine",
                         NULL, NULL,
                         EPHY_TYPE_SEARCH_ENGINE,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));
  properties [PROP_INCOGNITO_ENGINE] =
    g_param_spec_object ("incognito-engine",
                         NULL, NULL,
                         EPHY_TYPE_SEARCH_ENGINE,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static GType
list_model_get_item_type (GListModel *list)
{
  return EPHY_TYPE_SEARCH_ENGINE;
}

static guint
list_model_get_n_items (GListModel *list)
{
  EphySearchEngineManager *manager = EPHY_SEARCH_ENGINE_MANAGER (list);

  return manager->engines->len;
}

static gpointer
list_model_get_item (GListModel *list,
                     guint       position)
{
  EphySearchEngineManager *manager = EPHY_SEARCH_ENGINE_MANAGER (list);

  if (position >= manager->engines->len)
    return NULL;
  else
    return g_object_ref (manager->engines->pdata[position]);
}

static void
list_model_iface_init (GListModelInterface *iface,
                       gpointer             iface_data)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items = list_model_get_n_items;
  iface->get_item = list_model_get_item;
}

EphySearchEngineManager *
ephy_search_engine_manager_new (void)
{
  return EPHY_SEARCH_ENGINE_MANAGER (g_object_new (EPHY_TYPE_SEARCH_ENGINE_MANAGER, NULL));
}

/**
 * ephy_search_engine_manager_get_default_engine:
 *
 * Returns: (transfer none): the default search engine for @manager.
 */
EphySearchEngine *
ephy_search_engine_manager_get_default_engine (EphySearchEngineManager *manager)
{
  g_assert (EPHY_IS_SEARCH_ENGINE (manager->default_engine));

  return manager->default_engine;
}

/**
 * ephy_search_engine_manager_set_default_engine:
 * @engine: (transfer none): the search engine to set as default for @manager.
 *   This search engine must already be added to the search engine manager.
 *
 * Note that you must call ephy_search_engine_manager_save_to_settings() when
 * appropriate to save it. It isn't done automatically because we don't save
 * the search engines themselves on every change, as that would be pretty expensive
 * when typing the information, so it's better if the default search engine and
 * the search engines themselves are always kept in sync, in case there's an issue
 * somewhere in the code where it doesn't save one part or another.
 */
void
ephy_search_engine_manager_set_default_engine (EphySearchEngineManager *manager,
                                               EphySearchEngine        *engine)
{
  g_assert (EPHY_IS_SEARCH_ENGINE (engine));
  /* Improper input validation if that happens in our code. */
  g_assert (g_ptr_array_find (manager->engines, engine, NULL));

  manager->default_engine = engine;
  g_object_notify_by_pspec (G_OBJECT (manager), properties[PROP_DEFAULT_ENGINE]);
}

/**
 * ephy_search_engine_manager_get_incognito_engine:
 *
 * Returns: (transfer none): the incognito search engine for @manager.
 */
EphySearchEngine *
ephy_search_engine_manager_get_incognito_engine (EphySearchEngineManager *manager)
{
  g_assert (EPHY_IS_SEARCH_ENGINE (manager->incognito_engine));

  return manager->incognito_engine;
}

/**
 * ephy_search_engine_manager_set_incognito_engine:
 * @engine: (transfer none): the search engine to set as incognito engine for @manager.
 *   This search engine must already be added to the search engine manager.
 *
 * Note that you must call ephy_search_engine_manager_save_to_settings() when
 * appropriate to save it. It isn't done automatically because we don't save
 * the search engines themselves on every change, as that would be pretty expensive
 * when typing the information, so it's better if the incognito search engine and
 * the search engines themselves are always kept in sync, in case there's an issue
 * somewhere in the code where it doesn't save one part or another.
 */
void
ephy_search_engine_manager_set_incognito_engine (EphySearchEngineManager *manager,
                                                 EphySearchEngine        *engine)
{
  g_assert (EPHY_IS_SEARCH_ENGINE (engine));
  /* Improper input validation if that happens in our code. */
  g_assert (g_ptr_array_find (manager->engines, engine, NULL));

  manager->incognito_engine = engine;
  g_object_notify_by_pspec (G_OBJECT (manager), properties[PROP_INCOGNITO_ENGINE]);
}

/**
 * ephy_search_engine_manager_add_engine:
 * @engine: The search engine to add to @manager. @manager will take a reference
 *   on it.
 *
 * Adds search engine @engine to @manager.
 */
void
ephy_search_engine_manager_add_engine (EphySearchEngineManager *manager,
                                       EphySearchEngine        *engine)
{
  gboolean bang_existed = FALSE;
  guint new_sorted_position;

  if (*ephy_search_engine_get_bang (engine) != '\0') {
    bang_existed = !g_hash_table_insert (manager->bangs,
                                         (gpointer)ephy_search_engine_get_bang (engine),
                                         engine);
  }
  /* Programmer/validation error that doesn't properly use ephy_search_engine_manager_has_bang(). */
  g_assert (!bang_existed);
  g_signal_connect (engine, "notify::bang", G_CALLBACK (on_search_engine_bang_changed_cb), manager);

  g_ptr_array_add (manager->engines, g_object_ref (engine));

  /* It's a pity there isn't a more efficient g_ptr_array_add_sorted() function.
   * Comparison should be fast anyway, but still.
   */
  g_ptr_array_sort (manager->engines, (GCompareFunc)search_engine_compare_func);

  /* The engine likely will have moved in the array so we need to make sure
   * to report the items-changed signal at the proper position.
   */
  g_assert (g_ptr_array_find (manager->engines, engine, &new_sorted_position));
  g_list_model_items_changed (G_LIST_MODEL (manager),
                              new_sorted_position,
                              0,
                              1);
}

/**
 * ephy_search_engine_manager_delete_engine:
 * @engine: The search engine to delete from @manager.
 *
 * Deletes search engine @engine from @manager.
 */
void
ephy_search_engine_manager_delete_engine (EphySearchEngineManager *manager,
                                          EphySearchEngine        *engine)
{
  guint pos;
  const char *bang;

  /* Never allow not having a search engine, as too much relies on having one
   * and it just doesn't make sense at all to not have one. We assert as the
   * validation should prevent this from happening, so if it crashes then it's
   * for a good reason and the code should be fixed.
   */
  g_assert (manager->engines->len > 1);

  /* Removing an engine not in the manager is a programmer error. */
  g_assert (g_ptr_array_find (manager->engines, engine, &pos));

  bang = ephy_search_engine_get_bang (engine);
  if (*bang != '\0')
    g_hash_table_remove (manager->bangs, bang);

  /* Temporary ref so that we can remove the engine, and be sure that
   * the engine at index 0 isn't already the same as this one when
   * setting back another engine as default one.
   */
  g_object_ref (engine);

  g_ptr_array_remove_index (manager->engines, pos);

  if (engine == manager->default_engine) {
    g_assert (manager->engines->len != 0);

    /* Just set the first search engine in the sorted array as new search engine
     * so we're sure we'll still have a valid default search engine at any time.
     */
    ephy_search_engine_manager_set_default_engine (manager, manager->engines->pdata[0]);
  }

  if (engine == manager->incognito_engine) {
    g_assert (manager->engines->len != 0);

    /* Just set the first search engine in the sorted array as new incognito search engine
     * so we're sure we'll still have a valid incognito search engine at any time.
     */
    ephy_search_engine_manager_set_incognito_engine (manager, manager->engines->pdata[0]);
  }

  /* Drop temporary ref. */
  g_object_unref (engine);

  g_list_model_items_changed (G_LIST_MODEL (manager), pos, 1, 0);
}

/**
 * ephy_search_engine_manager_find_engine_by_name:
 * @engine_name: The name of the search engine to look for.
 *
 * Iterates @manager and finds the first search engine with its name set to @engine_name.
 * This is just a helper function, it isn't more efficient than iterating @manager
 * yourself and making string comparison with the engine's name.
 *
 * Returns: (transfer none): The #EphySearchEngine with name @engine_name if found in @manager, or %NULL if not found.
 */
EphySearchEngine *
ephy_search_engine_manager_find_engine_by_name (EphySearchEngineManager *manager,
                                                const char              *engine_name)
{
  for (guint i = 0; i < manager->engines->len; i++) {
    EphySearchEngine *engine = manager->engines->pdata[i];

    if (g_strcmp0 (ephy_search_engine_get_name (engine), engine_name) == 0)
      return engine;
  }

  return NULL;
}

/**
 * ephy_search_engine_manager_has_bang:
 * @bang: the bang to look for
 *
 * Checks whether @manager has a search engine that uses @bang as shortcut bang.
 * This is easier and more efficient than iterating manually on @manager yourself
 * and check for the bang for each search engine, as @manager internally keeps
 * a hash table with all used bangs.
 *
 * Returns: Whether @manager already has a search engine with its bang set to @bang.
 */
gboolean
ephy_search_engine_manager_has_bang (EphySearchEngineManager *manager,
                                     const char              *bang)
{
  return !!g_hash_table_lookup (manager->bangs, bang);
}

/**
 * parse_bang_query:
 * @search: the search with bangs to perform
 * @choosen_bang_engine: (out): if this function returns a non %NULL value, this
 *   argument will be set to the search engine from @manager that should be used
 *   to perform the search using the search query this function returns.
 *
 * This is the implementation for ephy_search_engine_manager_parse_bang_search()
 * and ephy_search_engine_manager_parse_bang_suggestions(). See the doc of the
 * former for details on this function's behaviours.
 *
 * Returns: (transfer full): the search query without the bangs.
 */
static char *
parse_bang_query (EphySearchEngineManager  *manager,
                  const char               *search,
                  EphySearchEngine        **choosen_bang_engine)
{
  g_autofree char *first_word = NULL;
  g_autofree char *last_word = NULL;
  g_autofree char *query_without_bangs = NULL;

  /* i.e. the end of @last_word */
  const char *last_non_space_p;

  /* i.e. the start of @first_word */
  const char *first_non_space_p;

  /* Both of these are set appropriately when we discover each bang within @search. */
  const char *query_start, *query_end;

  /* This one is separate from query_{start,end} because e.g. if the last word isn't
   * a bang, then we'll want to include it so query_end will be last_non_space_p.
   * Otherwise query_end will be space_p. */
  const char *space_p;
  EphySearchEngine *final_bang_engine = NULL, *bang_engine = NULL;

  g_assert (search);
  if (*search == '\0')
    return NULL;

  last_non_space_p = search + strlen (search) - 1;
  while (last_non_space_p != search && *last_non_space_p == ' ')
    last_non_space_p = g_utf8_find_prev_char (search, last_non_space_p);

  first_non_space_p = search;
  while (*first_non_space_p == ' ')
    first_non_space_p = g_utf8_find_next_char (first_non_space_p, NULL);

  /* Means the search query is empty or is full of spaces. So not a bang search. */
  if (last_non_space_p <= first_non_space_p)
    return NULL;

  /* There's no strrnchr() available, so must backwards iterate ourselves to
   * find the space character between @last_non_space_p and @search's beginning
   */
  space_p = last_non_space_p;
  while (space_p != search && *space_p != ' ')
    space_p = g_utf8_find_prev_char (search, space_p);

  /* This is necessary here because @last_non_space_p will point _at_ the
   * last non space character, not _just after_ it, which is not how substring
   * lengths are usually calculated like (since g_strndup (first_p, last_p - first_p)
   * should work without having to use +1 all around).
   */
  last_non_space_p++;

  /* There is a word, but only one, so it can't be a proper bang search */
  if (space_p <= first_non_space_p)
    return NULL;

  /* +1 to skip the space. */
  last_word = g_strndup (space_p + 1, last_non_space_p - (space_p + 1));
  bang_engine = g_hash_table_lookup (manager->bangs, last_word);

  /* Don't include the last word in the query as it's a proper bang. */
  if (bang_engine) {
    query_end = space_p;
    final_bang_engine = bang_engine;
  }
  /* The last word isn't a bang, so include it in the query. */
  else {
    query_end = last_non_space_p;
  }

  space_p = strchr (first_non_space_p, ' ');
  first_word = g_strndup (first_non_space_p, space_p - first_non_space_p);
  bang_engine = g_hash_table_lookup (manager->bangs, first_word);
  if (bang_engine) {
    /* +1 to skip the space. */
    query_start = space_p + 1;

    /* We prefer using the last typed bang (the one at the end), so that's
     * what we'll prefer using here.
     */
    if (!final_bang_engine)
      final_bang_engine = bang_engine;
  } else {
    /* It's not a proper bang, so we need to include it in the search query. */
    query_start = first_non_space_p;
  }

  /* No valid bang was found for this search query, so it's not a bang search. */
  if (!final_bang_engine)
    return NULL;

  /* Now that we've placed query_start and query_end properly depending on
   * whether the first/last word is a valid bang, we can copy the part that
   * doesn't include all the bangs to search this query using the search engine
   * we found for the bang.
   */
  query_without_bangs = g_strndup (query_start, query_end - query_start);

  *choosen_bang_engine = final_bang_engine;

  return g_steal_pointer (&query_without_bangs);
}

/**
 * ephy_search_engine_manager_parse_bang_search:
 *
 * This function looks at the first and last word of @search, checks if
 * one of them is the bang of one of the search engines in @manager, and
 * returns the corresponding search URL as returned by ephy_search_engine_build_search_address().
 * The last word will be looked at first, so that when someone changes their
 * mind at the end of the line they can just type the new bang and it will
 * be used instead of the first one.
 *
 * What is called a "bang search" is a search of the form "!bang this is the
 * search query", or with the bang at the end or at both ends (in which case
 * the end bang will be preferred).
 *
 * Returns: (transfer full) (nullable): The search URL corresponding to @search, with
 *   the search engine picked using the bang available in @search, or %NULL if
 *   there wasn't any recognized bang engine in @search. As such this function can
 *   also be used as a way of detecting whether @search is a "bang search", to
 *   process the search using the default search engine in that case.
 */
char *
ephy_search_engine_manager_parse_bang_search (EphySearchEngineManager *manager,
                                              const char              *search)
{
  EphySearchEngine *engine = NULL;
  g_autofree char *no_bangs_query = parse_bang_query (manager, search, &engine);

  if (no_bangs_query) {
    return ephy_search_engine_build_search_address (engine, no_bangs_query);
  }

  return NULL;
}

/**
 * ephy_search_engine_manager_parse_bang_suggestions:
 *
 * Same as ephy_search_engine_manager_parse_bang_search() but for the suggestions
 * URL instead of the search URL.
 */
char *
ephy_search_engine_manager_parse_bang_suggestions (EphySearchEngineManager  *manager,
                                                   const char               *search,
                                                   EphySearchEngine        **out_engine)
{
  EphySearchEngine *engine = NULL;
  g_autofree char *no_bangs_query = parse_bang_query (manager, search, &engine);

  if (no_bangs_query) {
    if (out_engine)
      *out_engine = engine;
    return ephy_search_engine_build_suggestions_address (engine, no_bangs_query);
  } else {
    return NULL;
  }
}

/**
 * ephy_search_engine_manager_save_to_settings:
 *
 * Saves the search engines and the default search engine to the GSettings.
 *
 * You must call this function after having done the changes (e.g. when closing
 * the preferences window).
 */
void
ephy_search_engine_manager_save_to_settings (EphySearchEngineManager *manager)
{
  GVariantBuilder builder;
  GVariant *variant;
  gpointer item;
  guint i = 0;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

  while ((item = g_list_model_get_item (G_LIST_MODEL (manager), i++))) {
    g_autoptr (EphySearchEngine) engine = EPHY_SEARCH_ENGINE (item);
    const char *suggestions_url, *opensearch_url;
    GVariantDict dict;

    g_assert (EPHY_IS_SEARCH_ENGINE (engine));

    g_variant_dict_init (&dict, NULL);
    g_variant_dict_insert (&dict, "name", "s", ephy_search_engine_get_name (engine));
    g_variant_dict_insert (&dict, "url", "s", ephy_search_engine_get_url (engine));
    g_variant_dict_insert (&dict, "bang", "s", ephy_search_engine_get_bang (engine));

    /* Let's not insert the OpenSearch-specific fields when they are not actually used. */
    suggestions_url = ephy_search_engine_get_suggestions_url (engine);
    opensearch_url = ephy_search_engine_get_opensearch_url (engine);
    if (suggestions_url)
      g_variant_dict_insert (&dict, "suggestions_url", "s", suggestions_url);
    if (opensearch_url)
      g_variant_dict_insert (&dict, "opensearch_url", "s", opensearch_url);

    g_variant_builder_add_value (&builder, g_variant_dict_end (&dict));
  }
  variant = g_variant_builder_end (&builder);
  g_settings_set_value (EPHY_SETTINGS_MAIN, EPHY_PREFS_SEARCH_ENGINES, variant);
}

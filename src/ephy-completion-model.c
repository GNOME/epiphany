/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 *  Copyright © 2012 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-completion-model.h"

#include "ephy-embed-shell.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-history-service.h"
#include "ephy-shell.h"

#include <string.h>

G_DEFINE_TYPE (EphyCompletionModel, ephy_completion_model, GTK_TYPE_LIST_STORE)

#define EPHY_COMPLETION_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_COMPLETION_MODEL, EphyCompletionModelPrivate))

struct _EphyCompletionModelPrivate {
  EphyHistoryService *history_service;
  EphyHistory *legacy_history_service;
  EphyFaviconCache *favicon_cache;

  EphyNode *bookmarks;
  GSList *search_terms;
};

static void
ephy_completion_model_constructed (GObject *object)
{
  GType types[N_COL] = { G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                         G_TYPE_INT, G_TYPE_STRING, G_TYPE_BOOLEAN,
                         GDK_TYPE_PIXBUF };

  gtk_list_store_set_column_types (GTK_LIST_STORE (object),
                                   N_COL,
                                   types);
}

static void
free_search_terms (GSList *search_terms)
{
  GSList *iter;
  
  for (iter = search_terms; iter != NULL; iter = iter->next)
    g_regex_unref ((GRegex*)iter->data);
  
  g_slist_free (search_terms);
}

static void
ephy_completion_model_finalize (GObject *object)
{
  EphyCompletionModelPrivate *priv = EPHY_COMPLETION_MODEL (object)->priv;

  if (priv->search_terms) {
    free_search_terms (priv->search_terms);
    priv->search_terms = NULL;
  }

  G_OBJECT_CLASS (ephy_completion_model_parent_class)->finalize (object);
}

static void
ephy_completion_model_class_init (EphyCompletionModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_completion_model_constructed;
  object_class->finalize = ephy_completion_model_finalize;

  g_type_class_add_private (object_class, sizeof (EphyCompletionModelPrivate));
}

static void
ephy_completion_model_init (EphyCompletionModel *model)
{
  EphyCompletionModelPrivate *priv;
  EphyBookmarks *bookmarks_service;

  model->priv = priv = EPHY_COMPLETION_MODEL_GET_PRIVATE (model);

  priv->history_service = EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (embed_shell));
  priv->legacy_history_service = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));
  priv->favicon_cache = EPHY_FAVICON_CACHE (ephy_embed_shell_get_favicon_cache (embed_shell));

  bookmarks_service = ephy_shell_get_bookmarks (ephy_shell);
  priv->bookmarks = ephy_bookmarks_get_bookmarks (bookmarks_service);
}

static gboolean
is_base_address (const char *address)
{
  if (address == NULL)
    return FALSE;

  /* A base address is <scheme>://<host>/
   * Neither scheme nor host contain a slash, so we can use slashes
   * figure out if it's a base address.
   *
   * Note: previous code was using a GRegExp to do the same thing. 
   * While regexps are much nicer to read, they're also a lot
   * slower.
   */
  address = strchr (address, '/');
  if (address == NULL ||
      address[1] != '/')
    return FALSE;

  address += 2;
  address = strchr (address, '/');
  if (address == NULL ||
      address[1] != 0)
    return FALSE;

  return TRUE;
}

static int
get_relevance (const char *location,
               int visit_count,
               gboolean is_bookmark)
{
  /* FIXME: use frecency. */
  int relevance = 0;

  /* We have three ordered groups: history's base addresses,
     bookmarks, deep history addresses. */
  if (is_bookmark)
    relevance = 1 << 5;
  else {
    visit_count = MIN (visit_count, (1 << 5) - 1);

    if (is_base_address (location))
      relevance = visit_count << 10;
    else
      relevance = visit_count;
  }
  
  return relevance;
}

typedef struct {
  char *title;
  char *location;
  char *keywords;
  int relevance;
  gboolean is_bookmark;
} PotentialRow;

static void
set_row_in_model (EphyCompletionModel *model, int position, PotentialRow *row)
{
  const char *favicon_location = ephy_history_get_icon (model->priv->legacy_history_service,
                                                        row->location);

  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, position,
                                     EPHY_COMPLETION_TEXT_COL, row->title ? row->title : "",
                                     EPHY_COMPLETION_URL_COL, row->location,
                                     EPHY_COMPLETION_ACTION_COL, row->location,
                                     EPHY_COMPLETION_KEYWORDS_COL, row->keywords ? row->keywords : "",
                                     EPHY_COMPLETION_EXTRA_COL, row->is_bookmark,
                                     EPHY_COMPLETION_FAVICON_COL, ephy_favicon_cache_get (model->priv->favicon_cache,
                                                                                          favicon_location),
                                     EPHY_COMPLETION_RELEVANCE_COL, row->relevance,
                                     -1);
}

static void
replace_rows_in_model (EphyCompletionModel *model, GSList *new_rows)
{
  /* This is by far the simplest way of doing, and yet it gives
   * basically the same result than the other methods... */
  int i;

  gtk_list_store_clear (GTK_LIST_STORE (model));

  for (i = 0; new_rows != NULL; i++) {
    PotentialRow *row = (PotentialRow*)new_rows->data;
    
    set_row_in_model (model, i, row);
    new_rows = new_rows->next;
  }
}

static gboolean
should_add_bookmark_to_model (EphyCompletionModel *model,
                              const char *search_string,
                              const char *title,
                              const char *location,
                              const char *keywords)
{
  gboolean ret = TRUE;
  EphyCompletionModelPrivate *priv = model->priv;

  if (priv->search_terms) {
    GSList *iter;
    GRegex *current = NULL;

    for (iter = priv->search_terms; iter != NULL; iter = iter->next) {
      current = (GRegex*) iter->data;
      if (((title && !g_regex_match (current, title, G_REGEX_MATCH_NOTEMPTY, NULL))) &&
          ((location && !g_regex_match (current, location, G_REGEX_MATCH_NOTEMPTY, NULL))) &&
          ((keywords && !g_regex_match (current, keywords, G_REGEX_MATCH_NOTEMPTY, NULL)))) {
        ret = FALSE;
        break;
      }
    }
  }

  return ret;
}

typedef struct {
  EphyCompletionModel *model;
  char *search_string;
  EphyHistoryJobCallback callback;
  gpointer user_data;
} FindURLsData;

static int
find_url (gconstpointer a,
          gconstpointer b)
{
  return g_strcmp0 (((PotentialRow*)a)->location,
                    ((char *)b));
}

static PotentialRow *
potential_row_new (const char *title, const char *location,
                   const char *keywords, int visit_count,
                   gboolean is_bookmark)
{
  PotentialRow *row = g_slice_new0 (PotentialRow);

  row->title = g_strdup (title);
  row->location = g_strdup (location);
  row->keywords = g_strdup (keywords);
  row->relevance = get_relevance (location, visit_count, is_bookmark);
  row->is_bookmark = is_bookmark;

  return row;
}

static void
free_potential_row (PotentialRow *row)
{
  g_free (row->title);
  g_free (row->location);
  g_free (row->keywords);
  
  g_slice_free (PotentialRow, row);
}

static GSList *
add_to_potential_rows (GSList *rows,
                       const char *title,
                       const char *location,
                       const char *keywords,
                       int visit_count,
                       gboolean is_bookmark,
                       gboolean search_for_duplicates)
{
  gboolean found = FALSE;
  PotentialRow *row = potential_row_new (title, location, keywords, visit_count, is_bookmark);

  if (search_for_duplicates) {
    GSList *p;

    p = g_slist_find_custom (rows, location, find_url);
    if (p) {
      PotentialRow *match = (PotentialRow*)p->data;
      if (row->relevance > match->relevance)
        match->relevance = row->relevance;
      
      found = TRUE;
      free_potential_row (row);
    }
  }

  if (!found)
    rows = g_slist_prepend (rows, row);

  return rows;
}

static int
sort_by_relevance (gconstpointer a, gconstpointer b)
{
  PotentialRow *r1 = (PotentialRow*)a;
  PotentialRow *r2 = (PotentialRow*)b;

  if (r1->relevance < r2->relevance)
    return 1;
  else if (r1->relevance > r2->relevance)
    return -1;
  else
    return 0;
}

static void
query_completed_cb (EphyHistoryService *service,
                    gboolean success,
                    gpointer result_data,
                    FindURLsData *user_data)
{
  EphyCompletionModel *model = user_data->model;
  EphyCompletionModelPrivate *priv = model->priv;
  GList *p, *urls;
  GPtrArray *children;
  GSList *list = NULL;
  int i;

  /* Bookmarks */
  children = ephy_node_get_children (priv->bookmarks);

  /* FIXME: perhaps this could be done in a service thread? There
   * should never be a ton of bookmarks, but seems a bit cleaner and
   * consistent with what we do for the history. */
  for (i = 0; i < children->len; i++) {
    EphyNode *kid;
    const char *keywords, *location, *title;

    kid = g_ptr_array_index (children, i);
    location = ephy_node_get_property_string (kid, EPHY_NODE_BMK_PROP_LOCATION);
    title = ephy_node_get_property_string (kid, EPHY_NODE_BMK_PROP_TITLE);
    keywords = ephy_node_get_property_string (kid, EPHY_NODE_BMK_PROP_KEYWORDS);

    if (should_add_bookmark_to_model (model, user_data->search_string,
                                      title, location, keywords))
      list = add_to_potential_rows (list, title, location, keywords, 0, TRUE, FALSE);
  }

  /* History */
  urls = (GList*)result_data;

  for (p = urls; p != NULL; p = p->next) {
    EphyHistoryURL *url = (EphyHistoryURL*)p->data;

    list = add_to_potential_rows (list, url->title, url->url, NULL, url->visit_count, FALSE, TRUE);
  }

  /* Sort the rows by relevance. */
  list = g_slist_sort (list, sort_by_relevance);

  /* Now that we have all the rows we want to insert, replace the rows
   * in the current model one by one, sorted by relevance. */
  replace_rows_in_model (model, list);

  /* Notify */
  if (user_data->callback)
    user_data->callback (service, success, result_data, user_data->user_data);

  g_free (user_data->search_string);
  g_slice_free (FindURLsData, user_data);

  g_slist_free_full (list, (GDestroyNotify)free_potential_row);
}

static void
update_search_terms (EphyCompletionModel *model,
                     const char *text)
{
  const char *current;
  const char *ptr;
  char *tmp;
  char *term;
  GRegex *term_regex;
  GRegex *quote_regex;
  gint count;
  gboolean inside_quotes = FALSE;
  EphyCompletionModelPrivate *priv = model->priv;

  if (priv->search_terms) {
    free_search_terms (priv->search_terms);
    priv->search_terms = NULL;
  }

  quote_regex = g_regex_new ("\"", G_REGEX_OPTIMIZE,
                             G_REGEX_MATCH_NOTEMPTY, NULL);
    
  /*
   * This code loops through the string using pointer arythmetics.
   * Although the string we are handling may contain UTF-8 chars
   * this works because only ASCII chars affect what is actually
   * copied from the string as a search term.
   */
  for (count = 0, current = ptr = text; ptr[0] != '\0'; ptr++, count++) {
    /*
     * If we found a double quote character; we will 
     * consume bytes up until the next quote, or
     * end of line;
     */
    if (ptr[0] == '"')
      inside_quotes = !inside_quotes;

    /*
     * If we found a space, and we are not looking for a
     * closing double quote, or if the next char is the
     * end of the string, append what we have already as
     * a search term.
     */
    if (((ptr[0] == ' ') && (!inside_quotes)) || ptr[1] == '\0') {
      /*
       * We special-case the end of the line because
       * we would otherwise not copy the last character
       * of the search string, since the for loop will
       * stop before that.
       */
      if (ptr[1] == '\0')
        count++;
        
      /*
       * remove quotes, and quote any regex-sensitive
       * characters
       */
      tmp = g_regex_escape_string (current, count);
      term = g_regex_replace (quote_regex, tmp, -1, 0,
                              "", G_REGEX_MATCH_NOTEMPTY, NULL);
      g_strstrip (term);
      g_free (tmp);

      /* we don't want empty search terms */
      if (term[0] != '\0') {
        term_regex = g_regex_new (term,
                                  G_REGEX_CASELESS | G_REGEX_OPTIMIZE,
                                  G_REGEX_MATCH_NOTEMPTY, NULL);
        priv->search_terms = g_slist_append (priv->search_terms, term_regex);
      }
      g_free (term);

      /* count will be incremented by the for loop */
      count = -1;
      current = ptr + 1;
    }
  }

  g_regex_unref (quote_regex);
}

#define MAX_COMPLETION_HISTORY_URLS 8

void
ephy_completion_model_update_for_string (EphyCompletionModel *model,
                                         const char *search_string,
                                         EphyHistoryJobCallback callback,
                                         gpointer data)
{
  EphyCompletionModelPrivate *priv;
  char **strings;
  int i;
  GList *query = NULL;
  FindURLsData *user_data;

  g_return_if_fail (EPHY_IS_COMPLETION_MODEL (model));
  g_return_if_fail (search_string != NULL);

  priv = model->priv;

  /* Split the search string. */
  strings = g_strsplit (search_string, " ", -1);
  for (i = 0; strings[i]; i++)
    query = g_list_append (query, g_strdup (strings[i]));
  g_strfreev (strings);

  update_search_terms (model, search_string);

  user_data = g_slice_new (FindURLsData);
  user_data->model = model;
  user_data->search_string = g_strdup (search_string);
  user_data->callback = callback;
  user_data->user_data = data;

  ephy_history_service_find_urls (priv->history_service,
                                  0, 0,
                                  MAX_COMPLETION_HISTORY_URLS,
                                  query,
                                  (EphyHistoryJobCallback)query_completed_cb,
                                  user_data);
}

EphyCompletionModel *
ephy_completion_model_new (void)
{
  return g_object_new (EPHY_TYPE_COMPLETION_MODEL, NULL);
}

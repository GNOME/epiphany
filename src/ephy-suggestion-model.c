/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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
 */

#include "config.h"
#include "ephy-suggestion-model.h"

#include "ephy-embed-shell.h"
#include "ephy-search-engine-manager.h"
#include "ephy-suggestion.h"

#include <dazzle.h>
#include <glib/gi18n.h>

#define MAX_COMPLETION_HISTORY_URLS 8

struct _EphySuggestionModel {
  GObject               parent;
  EphyHistoryService   *history_service;
  EphyBookmarksManager *bookmarks_manager;
  GSequence            *items;
  GSList               *search_terms;
};

enum {
  PROP_0,
  PROP_BOOKMARKS_MANAGER,
  PROP_HISTORY_SERVICE,
  N_PROPS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphySuggestionModel, ephy_suggestion_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ephy_suggestion_model_finalize (GObject *object)
{
  EphySuggestionModel *self = (EphySuggestionModel *)object;

  g_clear_object (&self->bookmarks_manager);
  g_clear_object (&self->history_service);
  g_clear_pointer (&self->items, g_sequence_free);

  g_slist_free_full (self->search_terms, (GDestroyNotify)g_regex_unref);

  G_OBJECT_CLASS (ephy_suggestion_model_parent_class)->finalize (object);
}

static void
ephy_suggestion_model_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EphySuggestionModel *self = EPHY_SUGGESTION_MODEL (object);

  switch (prop_id) {
    case PROP_HISTORY_SERVICE:
      g_value_set_object (value, self->history_service);
      break;
    case PROP_BOOKMARKS_MANAGER:
      g_value_set_object (value, self->bookmarks_manager);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_suggestion_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphySuggestionModel *self = EPHY_SUGGESTION_MODEL (object);

  switch (prop_id) {
    case PROP_HISTORY_SERVICE:
      self->history_service = g_value_dup_object (value);
      break;
    case PROP_BOOKMARKS_MANAGER:
      self->bookmarks_manager = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_suggestion_model_class_init (EphySuggestionModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_suggestion_model_finalize;
  object_class->get_property = ephy_suggestion_model_get_property;
  object_class->set_property = ephy_suggestion_model_set_property;

  properties [PROP_BOOKMARKS_MANAGER] =
    g_param_spec_object ("bookmarks-manager",
                         "Bookmarks Manager",
                         "The bookmarks manager for suggestions",
                         EPHY_TYPE_BOOKMARKS_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HISTORY_SERVICE] =
    g_param_spec_object ("history-service",
                         "History Service",
                         "The history service for suggestions",
                         EPHY_TYPE_HISTORY_SERVICE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ephy_suggestion_model_init (EphySuggestionModel *self)
{
  self->items = g_sequence_new (g_object_unref);
}

static GType
ephy_suggestion_model_get_item_type (GListModel *model)
{
  return EPHY_TYPE_SUGGESTION;
}

static guint
ephy_suggestion_model_get_n_items (GListModel *model)
{
  EphySuggestionModel *self = EPHY_SUGGESTION_MODEL (model);

  return g_sequence_get_length (self->items);
}

static gpointer
ephy_suggestion_model_get_item (GListModel *model,
                                guint       position)
{
  EphySuggestionModel *self = EPHY_SUGGESTION_MODEL (model);
  GSequenceIter *iter;
  DzlSuggestion *suggestion;

  iter = g_sequence_get_iter_at_pos (self->items, position);
  suggestion = g_sequence_get (iter);

  return g_object_ref (suggestion);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ephy_suggestion_model_get_item_type;
  iface->get_item = ephy_suggestion_model_get_item;
  iface->get_n_items = ephy_suggestion_model_get_n_items;
}

EphySuggestionModel *
ephy_suggestion_model_new (EphyHistoryService   *history_service,
                           EphyBookmarksManager *bookmarks_manager)
{
  g_assert (EPHY_IS_HISTORY_SERVICE (history_service));
  g_assert (EPHY_IS_BOOKMARKS_MANAGER (bookmarks_manager));

  return g_object_new (EPHY_TYPE_SUGGESTION_MODEL,
                       "history-service", history_service,
                       "bookmarks-manager", bookmarks_manager,
                       NULL);
}

static void
update_search_terms (EphySuggestionModel *self,
                     const char          *text)
{
  const char *current;
  const char *ptr;
  char *tmp;
  char *term;
  GRegex *term_regex;
  GRegex *quote_regex;
  gint count;
  gboolean inside_quotes = FALSE;

  g_assert (EPHY_IS_SUGGESTION_MODEL (self));

  if (self->search_terms) {
    g_slist_free_full (self->search_terms, (GDestroyNotify)g_regex_unref);
    self->search_terms = NULL;
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
        self->search_terms = g_slist_append (self->search_terms, term_regex);
      }
      g_free (term);

      /* count will be incremented by the for loop */
      count = -1;
      current = ptr + 1;
    }
  }

  g_regex_unref (quote_regex);
}

static gboolean
should_add_bookmark_to_model (EphySuggestionModel *self,
                              const char          *search_string,
                              const char          *title,
                              const char          *location)
{
  gboolean ret = TRUE;

  if (self->search_terms) {
    GSList *iter;
    GRegex *current = NULL;

    for (iter = self->search_terms; iter != NULL; iter = iter->next) {
      current = (GRegex *)iter->data;
      if ((!g_regex_match (current, title ? title : "", G_REGEX_MATCH_NOTEMPTY, NULL)) &&
          (!g_regex_match (current, location ? location : "", G_REGEX_MATCH_NOTEMPTY, NULL))) {
        ret = FALSE;
        break;
      }
    }
  }

  return ret;
}

static guint
add_bookmarks (EphySuggestionModel *self,
               const char          *query)
{
  GSequence *bookmarks;
  guint added = 0;

  bookmarks = ephy_bookmarks_manager_get_bookmarks (self->bookmarks_manager);

  for (GSequenceIter *iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark;
    const char *url, *title;

    bookmark = g_sequence_get (iter);

    url = ephy_bookmark_get_url (bookmark);
    title = ephy_bookmark_get_title (bookmark);

    if (should_add_bookmark_to_model (self, query, title, url)) {
      EphySuggestion *suggestion;

      suggestion = ephy_suggestion_new (title, url);
      g_sequence_append (self->items, suggestion);
      added++;
    }
  }

  return added;
}

static guint
add_history (EphySuggestionModel *self,
             GList               *urls)
{
  guint added = 0;

  for (const GList *p = urls; p != NULL; p = p->next) {
    EphyHistoryURL *url = (EphyHistoryURL *)p->data;
    EphySuggestion *suggestion;

    suggestion = ephy_suggestion_new (url->title, url->url);
    g_sequence_append (self->items, suggestion);
    added++;
  }

  return added;
}

static guint
add_search_engines (EphySuggestionModel *self,
                    const char          *query)
{
  EphyEmbedShell *shell;
  EphySearchEngineManager *manager;
  char **engines;
  guint added = 0;

  shell = ephy_embed_shell_get_default ();
  manager = ephy_embed_shell_get_search_engine_manager (shell);
  engines = ephy_search_engine_manager_get_names (manager);

  for (guint i = 0; engines[i] != NULL; i++) {
    EphySuggestion *suggestion;
    char *address;

    address = ephy_search_engine_manager_build_search_address (manager, engines[i], query);
    suggestion = ephy_suggestion_new_without_subtitle (engines[i], address);

    g_sequence_append (self->items, suggestion);
    added++;

    g_free (address);
  }

  g_strfreev (engines);

  return added;
}

static void
query_completed_cb (EphyHistoryService *service,
                    gboolean            success,
                    gpointer            result_data,
                    gpointer            user_data)
{
  GTask *task = user_data;
  EphySuggestionModel *self;
  const gchar *query;
  GList *urls;
  guint removed;
  guint added;

  self = g_task_get_source_object (task);
  query = g_task_get_task_data (task);
  urls = (GList *)result_data;

  removed = g_sequence_get_length (self->items);

  g_clear_pointer (&self->items, g_sequence_free);
  self->items = g_sequence_new (g_object_unref);

  added = add_bookmarks (self, query);
  added += add_history (self, urls);
  added += add_search_engines (self, query);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

void
ephy_suggestion_model_query_async (EphySuggestionModel *self,
                                   const gchar         *query,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GTask *task = NULL;
  char **strings;
  GList *qlist = NULL;

  g_assert (EPHY_IS_SUGGESTION_MODEL (self));
  g_assert (query != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ephy_suggestion_model_query_async);
  g_task_set_task_data (task, g_strdup (query), g_free);

  /* Split the search string. */
  strings = g_strsplit (query, " ", -1);
  for (guint i = 0; strings[i]; i++)
    qlist = g_list_append (qlist, g_strdup (strings[i]));

  update_search_terms (self, query);

  ephy_history_service_find_urls (self->history_service,
                                  0, 0,
                                  MAX_COMPLETION_HISTORY_URLS, 0,
                                  qlist,
                                  EPHY_HISTORY_SORT_MOST_VISITED,
                                  cancellable,
                                  (EphyHistoryJobCallback)query_completed_cb,
                                  task);

  g_strfreev (strings);
}

gboolean
ephy_suggestion_model_query_finish (EphySuggestionModel  *self,
                                    GAsyncResult         *result,
                                    GError              **error)
{
  g_assert (EPHY_IS_SUGGESTION_MODEL (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

EphySuggestion *
ephy_suggestion_model_get_suggestion_with_uri (EphySuggestionModel *self,
                                               const char          *uri)
{
  GSequenceIter *iter;

  g_assert (EPHY_IS_SUGGESTION_MODEL (self));
  g_assert (uri != NULL && *uri != '\0');

  for (iter = g_sequence_get_begin_iter (self->items);
       !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
    EphySuggestion *suggestion;

    suggestion = g_sequence_get (iter);
    if (strcmp (ephy_suggestion_get_uri (suggestion), uri) == 0)
      return suggestion;
  }

  return NULL;
}

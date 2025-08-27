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

#include "ephy-about-handler.h"
#include "ephy-embed-shell.h"
#include "ephy-prefs.h"
#include "ephy-search-engine-manager.h"
#include "ephy-settings.h"
#include "ephy-suggestion.h"
#include "ephy-user-agent.h"
#include "ephy-window.h"

#include "dzl-fuzzy-mutable-index.h"
#include <glib/gi18n.h>

#define MAX_SEARCH_ENGINES_SUGGESTIONS 5
#define MAX_URL_ENTRIES             25

struct _EphySuggestionModel {
  GObject parent;
  EphyHistoryService *history_service;
  EphyBookmarksManager *bookmarks_manager;
  GSequence *urls;
  GSequence *items;
  GCancellable *icon_cancellable;
  guint num_custom_entries;
  SoupSession *session;
};

#define QUERY_SCOPE_ALL         ' '
#define QUERY_SCOPE_TABS        '%'
#define QUERY_SCOPE_SUGGESTIONS '?'
#define QUERY_SCOPE_BOOKMARKS   '*'
#define QUERY_SCOPE_HISTORY     '^'
#define MAX_QUERY_SCOPES        4

enum {
  PROP_0,
  PROP_BOOKMARKS_MANAGER,
  PROP_HISTORY_SERVICE,
  N_PROPS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphySuggestionModel, ephy_suggestion_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
ephy_suggestion_model_finalize (GObject *object)
{
  EphySuggestionModel *self = (EphySuggestionModel *)object;

  g_clear_object (&self->bookmarks_manager);
  g_clear_object (&self->history_service);
  g_clear_pointer (&self->urls, g_sequence_free);
  g_clear_pointer (&self->items, g_sequence_free);
  g_clear_object (&self->session);

  g_cancellable_cancel (self->icon_cancellable);
  g_clear_object (&self->icon_cancellable);

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
                         NULL, NULL,
                         EPHY_TYPE_BOOKMARKS_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HISTORY_SERVICE] =
    g_param_spec_object ("history-service",
                         NULL, NULL,
                         EPHY_TYPE_HISTORY_SERVICE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ephy_suggestion_model_init (EphySuggestionModel *self)
{
  self->items = g_sequence_new (g_object_unref);
  self->session = soup_session_new_with_options ("user-agent", ephy_user_agent_get (), NULL);
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
  if (g_sequence_iter_is_end (iter))
    return NULL;

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

static gboolean
should_add_bookmark_to_model (EphySuggestionModel *self,
                              const char          *search_string,
                              const char          *title,
                              const char          *location,
                              GSequence           *tags)
{
  g_autofree gchar *search_casefold = g_utf8_casefold (search_string, -1);
  g_autofree gchar *title_casefold = g_utf8_casefold (title, -1);
  g_autofree gchar *location_casefold = g_utf8_casefold (location, -1);
  g_autofree char *tag_string = NULL;
  g_autofree char *tag_string_casefold = NULL;
  char **tag_array = NULL;
  g_auto (GStrv) search_terms = NULL;
  GSequenceIter *tag_iter;
  guint i;
  gboolean ret = TRUE;

  tag_array = g_malloc0 ((g_sequence_get_length (tags) + 1) * sizeof (char *));

  for (i = 0, tag_iter = g_sequence_get_begin_iter (tags);
       !g_sequence_iter_is_end (tag_iter);
       i++, tag_iter = g_sequence_iter_next (tag_iter)) {
    tag_array[i] = g_sequence_get (tag_iter);
  }

  tag_string = g_strjoinv (" ", tag_array);
  tag_string_casefold = g_utf8_casefold (tag_string, -1);
  search_terms = g_strsplit (search_casefold, " ", -1);

  for (i = 0; i < g_strv_length (search_terms); i++) {
    if (!strstr (title_casefold, search_terms[i]) &&
        !strstr (location_casefold, search_terms[i]) &&
        (tag_string_casefold && !strstr (tag_string_casefold, search_terms[i]))) {
      ret = FALSE;
      break;
    }
  }

  g_free (tag_array);

  return ret;
}

static void
icon_loaded_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  EphySuggestion *suggestion;
  g_autoptr (GdkTexture) texture = NULL;
  cairo_surface_t *favicon;
  gdouble x_scale, y_scale;
  int w, h;

  texture = webkit_favicon_database_get_favicon_finish (database, result, NULL);
  if (!texture)
    return;

  suggestion = EPHY_SUGGESTION (user_data);

  w = gdk_texture_get_width (texture);
  h = gdk_texture_get_height (texture);
  favicon = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  gdk_texture_download (texture,
                        cairo_image_surface_get_data (favicon),
                        cairo_image_surface_get_stride (favicon));
  cairo_surface_mark_dirty (favicon);

  x_scale = (gdouble)w / 16;
  y_scale = (gdouble)h / 16;
  cairo_surface_set_device_scale (favicon, x_scale, y_scale);

  ephy_suggestion_set_favicon (suggestion, favicon);
}

static void
load_favicon (EphySuggestionModel *model,
              EphySuggestion      *suggestion,
              const char          *url)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitFaviconDatabase *database = ephy_embed_shell_get_favicon_database (shell);

  webkit_favicon_database_get_favicon (database,
                                       url,
                                       model->icon_cancellable,
                                       icon_loaded_cb,
                                       suggestion);
}

static gboolean
append_suggestion (EphySuggestionModel *self,
                   EphySuggestion      *suggestion)
{
  if (g_sequence_lookup (self->urls, (char *)ephy_suggestion_get_uri (suggestion), (GCompareDataFunc)g_strcmp0, NULL))
    return FALSE;

  if (self->num_custom_entries < MAX_URL_ENTRIES) {
    const char *uri = ephy_suggestion_get_uri (suggestion);

    g_sequence_append (self->items, g_object_ref (suggestion));
    g_sequence_append (self->urls, g_strdup (uri));

    load_favicon (self, suggestion, uri);
    self->num_custom_entries++;

    return TRUE;
  }

  return FALSE;
}

static guint
add_search_engines (EphySuggestionModel *self,
                    const char          *query)
{
  EphyEmbedShell *shell;
  EphySearchEngineManager *manager;
  guint added;

  shell = ephy_embed_shell_get_default ();
  manager = ephy_embed_shell_get_search_engine_manager (shell);

  for (added = 0; added < g_list_model_get_n_items (G_LIST_MODEL (manager)); added++) {
    g_autoptr (EphySearchEngine) engine = g_list_model_get_item (G_LIST_MODEL (manager), added);
    EphySuggestion *suggestion;
    const char *engine_name;
    g_autofree char *address = NULL;
    g_autofree char *escaped_title = NULL;
    g_autofree char *markup = NULL;
    g_autoptr (GUri) uri = NULL;

    engine_name = ephy_search_engine_get_name (engine);
    address = ephy_search_engine_build_search_address (engine, query);
    escaped_title = g_markup_escape_text (engine_name, -1);
    markup = dzl_fuzzy_highlight (escaped_title, query, FALSE);
    suggestion = ephy_suggestion_new (markup, engine_name, address, FALSE);

    uri = g_uri_parse (address, G_URI_FLAGS_PARSE_RELAXED, NULL);
    if (uri) {
      g_free (address);
      address = g_strdup_printf ("%s://%s/", g_uri_get_scheme (uri), g_uri_get_host (uri));
    }

    load_favicon (self, suggestion, address);

    g_sequence_append (self->items, suggestion);
  }

  return added;
}

typedef struct {
  char *query;
  char scope;
  gboolean include_search_engines;
  GSequence *tabs;
  GSequence *bookmarks;
  GSequence *history;
  GSequence *search_engine_suggestions;
  int active_sources;
} QueryData;

static QueryData *
query_data_new (const char *query,
                gboolean    include_search_engines)
{
  QueryData *data;

  data = g_malloc0 (sizeof (QueryData));
  data->include_search_engines = include_search_engines;
  data->tabs = g_sequence_new (g_object_unref);
  data->bookmarks = g_sequence_new (g_object_unref);
  data->history = g_sequence_new (g_object_unref);
  data->search_engine_suggestions = g_sequence_new (g_object_unref);

  /* Split the search string. */
  if (strlen (query) > 1 && query[1] == ' ' &&
      (query[0] == QUERY_SCOPE_TABS ||
       query[0] == QUERY_SCOPE_BOOKMARKS ||
       query[0] == QUERY_SCOPE_HISTORY ||
       query[0] == QUERY_SCOPE_SUGGESTIONS)) {
    data->query = g_strdup (query + 2);
    data->scope = query[0];
    data->active_sources = 1;
  } else {
    data->query = g_strdup (query);
    data->scope = QUERY_SCOPE_ALL;
    data->active_sources = MAX_QUERY_SCOPES;
  }

  return data;
}

static void
query_data_free (QueryData *data)
{
  g_assert (data);
  g_clear_pointer (&data->tabs, g_sequence_free);
  g_clear_pointer (&data->bookmarks, g_sequence_free);
  g_clear_pointer (&data->history, g_sequence_free);
  g_clear_pointer (&data->search_engine_suggestions, g_sequence_free);
  g_clear_pointer (&data->query, g_free);
  g_free (data);
}

static void
query_collection_done (EphySuggestionModel *self,
                       GTask               *task)
{
  QueryData *data;
  guint removed;
  guint added = 0;

  self = g_task_get_source_object (task);
  data = g_task_get_task_data (task);

  if (--data->active_sources)
    return;

  g_cancellable_cancel (self->icon_cancellable);
  g_clear_object (&self->icon_cancellable);

  self->icon_cancellable = g_cancellable_new ();

  removed = g_sequence_get_length (self->items);

  g_clear_pointer (&self->urls, g_sequence_free);
  self->urls = g_sequence_new (g_free);
  g_clear_pointer (&self->items, g_sequence_free);
  self->items = g_sequence_new (g_object_unref);
  self->num_custom_entries = 0;

  if (strlen (data->query) > 0) {
    /* Search results have the following order:
     * - Open Tabs
     * - Search Suggestions
     * - Bookmarks
     * - History
     * - Search Engines
     */
    for (GSequenceIter *iter = g_sequence_get_begin_iter (data->tabs); !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
      EphySuggestion *tmp = g_sequence_get (iter);

      g_sequence_append (self->items, g_object_ref (tmp));
      added++;
    }

    for (GSequenceIter *iter = g_sequence_get_begin_iter (data->search_engine_suggestions); !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
      EphySuggestion *tmp = g_sequence_get (iter);

      if (append_suggestion (self, tmp))
        added++;
      else
        break;
    }

    for (GSequenceIter *iter = g_sequence_get_begin_iter (data->bookmarks); !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
      EphySuggestion *tmp = g_sequence_get (iter);

      if (append_suggestion (self, tmp))
        added++;
      else
        break;
    }

    for (GSequenceIter *iter = g_sequence_get_begin_iter (data->history); !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
      EphySuggestion *tmp = g_sequence_get (iter);

      if (append_suggestion (self, tmp))
        added++;
      else
        break;
    }

    if (data->scope == QUERY_SCOPE_ALL && data->include_search_engines)
      added += add_search_engines (self, data->query);
  }

  g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static void
tabs_query (EphySuggestionModel *self,
            QueryData           *data,
            GTask               *task)
{
  GApplication *application;
  EphyEmbedShell *shell;
  EphyWindow *window;
  EphyTabView *tab_view;
  GList *windows;
  gint n_pages, selected;

  shell = ephy_embed_shell_get_default ();
  application = G_APPLICATION (shell);
  windows = gtk_application_get_windows (GTK_APPLICATION (application));

  for (guint win_idx = 0; win_idx < g_list_length (windows); win_idx++) {
    window = EPHY_WINDOW (g_list_nth_data (windows, win_idx));

    tab_view = ephy_window_get_tab_view (window);
    n_pages = ephy_tab_view_get_n_pages (tab_view);
    selected = ephy_tab_view_get_selected_index (tab_view);

    for (int i = 0; i < n_pages; i++) {
      EphyEmbed *embed;
      EphyWebView *webview;
      EphySuggestion *suggestion;
      g_autofree gchar *escaped_title = NULL;
      g_autofree gchar *markup = NULL;
      const gchar *display_address;
      g_autofree gchar *address = NULL;
      const gchar *title;
      g_autofree gchar *title_casefold = NULL;
      g_autofree gchar *display_address_casefold = NULL;
      g_autofree gchar *query_casefold = NULL;

      if (win_idx == 0 && i == selected)
        continue;

      embed = EPHY_EMBED (ephy_tab_view_get_nth_page (tab_view, i));
      webview = ephy_embed_get_web_view (embed);
      display_address = ephy_web_view_get_display_address (webview);
      address = g_strdup_printf ("ephy-tab://%d@%d", i, win_idx);
      title = webkit_web_view_get_title (WEBKIT_WEB_VIEW (webview));

      display_address_casefold = g_utf8_casefold (display_address, -1);
      query_casefold = g_utf8_casefold (data->query, -1);
      if (!title)
        title = "";

      title_casefold = g_utf8_casefold (title, -1);

      if ((title_casefold && strstr (title_casefold, query_casefold)) || strstr (display_address_casefold, query_casefold)) {
        char *escaped_address = g_markup_escape_text (display_address, -1);

        if (g_str_has_prefix (escaped_address, EPHY_ABOUT_SCHEME)) {
          g_autofree char *pretty_address = g_strconcat ("about", escaped_address + EPHY_ABOUT_SCHEME_LEN, NULL);
          g_free (escaped_address);
          escaped_address = g_steal_pointer (&pretty_address);
        }

        escaped_title = g_markup_escape_text (title, -1);
        markup = dzl_fuzzy_highlight (escaped_title, data->query, FALSE);
        suggestion = ephy_suggestion_new_with_custom_subtitle (markup, title, escaped_address, address);
        ephy_suggestion_set_secondary_icon (suggestion, "go-jump-symbolic");

        g_sequence_append (data->tabs, g_object_ref (suggestion));
      }
    }
  }

  query_collection_done (self, g_steal_pointer (&task));
}

static void
bookmarks_query (EphySuggestionModel *self,
                 QueryData           *data,
                 GTask               *task)
{
  GSequence *bookmarks;

  bookmarks = ephy_bookmarks_manager_get_bookmarks (self->bookmarks_manager);

  for (GSequenceIter *iter = g_sequence_get_begin_iter (bookmarks);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter)) {
    EphyBookmark *bookmark;
    GSequence *tags;
    const char *url, *title;

    bookmark = g_sequence_get (iter);

    url = ephy_bookmark_get_url (bookmark);
    title = ephy_bookmark_get_title (bookmark);
    if (strlen (title) == 0)
      title = url;

    tags = ephy_bookmark_get_tags (bookmark);
    if (should_add_bookmark_to_model (self, data->query, title, url, tags)) {
      EphySuggestion *suggestion;
      g_autofree gchar *escaped_title = NULL;
      g_autofree gchar *markup = NULL;
      g_autofree gchar *pretty_url = NULL;

      if (g_str_has_prefix (url, EPHY_ABOUT_SCHEME)) {
        pretty_url = g_strconcat ("about", url + EPHY_ABOUT_SCHEME_LEN, NULL);
        url = pretty_url;
      }

      escaped_title = g_markup_escape_text (title, -1);
      markup = dzl_fuzzy_highlight (escaped_title, data->query, FALSE);
      suggestion = ephy_suggestion_new (markup, title, url, FALSE);
      ephy_suggestion_set_secondary_icon (suggestion, "ephy-starred-symbolic");

      g_sequence_append (data->bookmarks, g_object_ref (suggestion));
    }
  }

  query_collection_done (self, g_steal_pointer (&task));
}

static void
history_query_completed_cb (EphyHistoryService *service,
                            gboolean            success,
                            gpointer            result_data,
                            gpointer            user_data)
{
  GTask *task = user_data;
  EphySuggestionModel *self;
  QueryData *data;
  GList *urls;

  self = g_task_get_source_object (task);
  data = g_task_get_task_data (task);
  urls = (GList *)result_data;

  if (strlen (data->query) > 0) {
    for (const GList *p = urls; p; p = p->next) {
      EphyHistoryURL *url = (EphyHistoryURL *)p->data;
      EphySuggestion *suggestion;
      g_autofree gchar *escaped_title = NULL;
      g_autofree gchar *markup = NULL;
      const gchar *title = url->title;

      if (strlen (url->title) == 0)
        title = url->url;

      escaped_title = g_markup_escape_text (title, -1);

      markup = dzl_fuzzy_highlight (escaped_title, data->query, FALSE);
      suggestion = ephy_suggestion_new (markup, title, url->url, FALSE);

      g_sequence_append (data->history, g_steal_pointer (&suggestion));
    }
  }

  query_collection_done (self, g_steal_pointer (&task));
}

static void
search_engine_suggestions_loaded_cb (SoupSession  *session,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  EphySuggestionModel *self = g_task_get_source_object (task);
  g_autoptr (GSequence) suggestions = NULL;
  g_autoptr (GError) error = NULL;
  QueryData *data = g_task_get_task_data (task);

  suggestions = ephy_search_engine_load_suggestions_finish (result, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning (_("Could not load search engines suggestions: %s"), error->message);
    query_collection_done (self, task);
    return;
  }

  g_assert (g_sequence_get_length (data->search_engine_suggestions) == 0);
  g_assert (suggestions);
  /* Nicer than looping manually. */
  g_sequence_move_range (g_sequence_get_end_iter (data->search_engine_suggestions),
                         g_sequence_get_begin_iter (suggestions),
                         g_sequence_get_iter_at_pos (suggestions, MAX_SEARCH_ENGINES_SUGGESTIONS));
  query_collection_done (self, task);
}

static void
search_engine_suggestions_query (EphySuggestionModel *self,
                                 const gchar         *query,
                                 GTask               *task)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  g_autofree char *built_suggestions_url = NULL;
  EphySearchEngine *engine = NULL;

  built_suggestions_url = ephy_search_engine_manager_parse_bang_suggestions (manager, query, &engine);
  /* If it was not a bang search, then use the default search engine. */
  if (!built_suggestions_url) {
    engine = ephy_search_engine_manager_get_default_engine (manager);
    if (!ephy_search_engine_get_suggestions_url (engine)) {
      query_collection_done (self, task);
      return;
    }

    built_suggestions_url = ephy_search_engine_build_suggestions_address (engine, query);
  } else {
    /* Finding a matching engine from the bang doesn't mean the engine has
     * suggestions support, so make sure we stop here if it doesn't.
     */
    if (!ephy_search_engine_get_suggestions_url (engine)) {
      query_collection_done (self, task);
      return;
    }
  }
  ephy_search_engine_load_suggestions_async (built_suggestions_url, engine,
                                             g_task_get_cancellable (task),
                                             (GAsyncReadyCallback)search_engine_suggestions_loaded_cb,
                                             task);
}

void
ephy_suggestion_model_query_async (EphySuggestionModel *self,
                                   const gchar         *query,
                                   gboolean             include_search_engines,
                                   gboolean             include_search_engines_suggestions,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GTask *task = NULL;
  QueryData *data;

  g_assert (EPHY_IS_SUGGESTION_MODEL (self));
  g_assert (query);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ephy_suggestion_model_query_async);

  data = query_data_new (query, include_search_engines);
  g_task_set_task_data (task, data, (GDestroyNotify)query_data_free);

  if (data->scope == QUERY_SCOPE_ALL || data->scope == QUERY_SCOPE_SUGGESTIONS) {
    gboolean is_possible_url = FALSE;

    if (!strstr (query, " ") && strstr (query, "."))
      is_possible_url = TRUE;

    if (include_search_engines_suggestions && !is_possible_url && strlen (query) > 1 && g_settings_get_boolean (EPHY_SETTINGS_MAIN, EPHY_PREFS_USE_SEARCH_SUGGESTIONS))
      search_engine_suggestions_query (self, query, task);
    else
      query_collection_done (self, task);
  }

  if (data->scope == QUERY_SCOPE_ALL || data->scope == QUERY_SCOPE_HISTORY) {
    GList *qlist = NULL;
    g_auto (GStrv) strings = NULL;

    strings = g_strsplit (data->query, " ", -1);

    for (guint i = 0; strings[i]; i++)
      qlist = g_list_append (qlist, g_strdup (strings[i]));

    ephy_history_service_find_urls (self->history_service,
                                    0, 0,
                                    MAX_URL_ENTRIES, 0,
                                    qlist,
                                    EPHY_HISTORY_SORT_MOST_VISITED,
                                    cancellable,
                                    (EphyHistoryJobCallback)history_query_completed_cb,
                                    task);
  }

  if (data->scope == QUERY_SCOPE_ALL || data->scope == QUERY_SCOPE_TABS)
    tabs_query (self, data, task);

  if (data->scope == QUERY_SCOPE_ALL || data->scope == QUERY_SCOPE_BOOKMARKS)
    bookmarks_query (self, data, task);
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
  g_autofree gchar *uri_casefold = g_utf8_casefold (uri, -1);

  g_assert (EPHY_IS_SUGGESTION_MODEL (self));
  g_assert (uri && *uri != '\0');

  for (iter = g_sequence_get_begin_iter (self->items);
       !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
    EphySuggestion *suggestion;
    g_autofree gchar *suggestion_casefold = NULL;

    suggestion = g_sequence_get (iter);
    suggestion_casefold = g_utf8_casefold (ephy_suggestion_get_uri (suggestion), -1);

    if (strcmp (suggestion_casefold, uri_casefold) == 0)
      return suggestion;
  }

  return NULL;
}

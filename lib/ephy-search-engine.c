/* ephy-search-engine.c
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
#include "ephy-search-engine.h"

#include "ephy-suggestion.h"
#include "ephy-user-agent.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

struct _EphySearchEngine {
  GObject parent_instance;

  char *name;
  char *url;
  char *bang;

  /* See the OpenSearch Suggestions specification for information on the expected GET response.
   * https://github.com/dewitt/opensearch/blob/master/mediawiki/Specifications/OpenSearch/Extensions/Suggestions/1.1/Draft%201.wiki
   */
  char *suggestions_url;
  /* Let's keep the URL just in case we want to grab extra information from it
   * in later version (e.g. using the search engine's icon or description).
   */
  char *opensearch_url;
};

G_DEFINE_FINAL_TYPE (EphySearchEngine, ephy_search_engine, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  PROP_URL,
  PROP_BANG,
  PROP_SUGGESTIONS_URL,
  PROP_OPENSEARCH_URL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

const char *
ephy_search_engine_get_name (EphySearchEngine *self)
{
  return self->name;
}

void
ephy_search_engine_set_name (EphySearchEngine *self,
                             const char       *name)
{
  g_assert (name);

  if (g_strcmp0 (name, self->name) == 0)
    return;

  g_free (self->name);
  self->name = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

const char *
ephy_search_engine_get_url (EphySearchEngine *self)
{
  return self->url;
}

void
ephy_search_engine_set_url (EphySearchEngine *self,
                            const char       *url)
{
  g_assert (url);

  if (g_strcmp0 (url, self->url) == 0)
    return;

  g_free (self->url);
  self->url = g_strdup (url);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_URL]);
}

const char *
ephy_search_engine_get_bang (EphySearchEngine *self)
{
  return self->bang;
}

void
ephy_search_engine_set_bang (EphySearchEngine *self,
                             const char       *bang)
{
  g_assert (bang);

  if (g_strcmp0 (bang, self->bang) == 0)
    return;

  g_free (self->bang);
  self->bang = g_strdup (bang);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BANG]);
}

/**
 * ephy_search_engine_get_suggestions_url:
 *
 * Returns: (nullable): Returns @self's OpenSearch suggestions URL if @self supports
 *   suggestions, or %NULL if it doesn't.
 */
const char *
ephy_search_engine_get_suggestions_url (EphySearchEngine *self)
{
  return self->suggestions_url;
}

void
ephy_search_engine_set_suggestions_url (EphySearchEngine *self,
                                        const char       *suggestions_url)
{
  if (g_strcmp0 (suggestions_url, self->suggestions_url) == 0 || *suggestions_url == '\0')
    return;

  g_free (self->suggestions_url);
  self->suggestions_url = g_strdup (suggestions_url);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUGGESTIONS_URL]);
}

/**
 * ephy_search_engine_get_opensearch_url:
 *
 * Returns: (nullable): Returns @self's OpenSearch description file URL if @self
 *   was added as an OpenSearch engine, or %NULL if it wasn't.
 */
const char *
ephy_search_engine_get_opensearch_url (EphySearchEngine *self)
{
  return self->opensearch_url;
}

void
ephy_search_engine_set_opensearch_url (EphySearchEngine *self,
                                       const char       *opensearch_url)
{
  if (g_strcmp0 (opensearch_url, self->opensearch_url) == 0 || *opensearch_url == '\0')
    return;

  g_free (self->opensearch_url);
  self->opensearch_url = g_strdup (opensearch_url);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPENSEARCH_URL]);
}

static void
ephy_search_engine_finalize (GObject *object)
{
  EphySearchEngine *self = (EphySearchEngine *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->url, g_free);
  g_clear_pointer (&self->bang, g_free);

  G_OBJECT_CLASS (ephy_search_engine_parent_class)->finalize (object);
}

static void
ephy_search_engine_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  EphySearchEngine *self = EPHY_SEARCH_ENGINE (object);

  switch (prop_id) {
    case PROP_NAME:
      g_value_set_string (value, ephy_search_engine_get_name (self));
      break;
    case PROP_URL:
      g_value_set_string (value, ephy_search_engine_get_url (self));
      break;
    case PROP_BANG:
      g_value_set_string (value, ephy_search_engine_get_bang (self));
      break;
    case PROP_SUGGESTIONS_URL:
      g_value_set_string (value, ephy_search_engine_get_suggestions_url (self));
      break;
    case PROP_OPENSEARCH_URL:
      g_value_set_string (value, ephy_search_engine_get_opensearch_url (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_search_engine_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  EphySearchEngine *self = EPHY_SEARCH_ENGINE (object);

  switch (prop_id) {
    case PROP_NAME:
      ephy_search_engine_set_name (self, g_value_get_string (value));
      break;
    case PROP_URL:
      ephy_search_engine_set_url (self, g_value_get_string (value));
      break;
    case PROP_BANG:
      ephy_search_engine_set_bang (self, g_value_get_string (value));
      break;
    case PROP_SUGGESTIONS_URL:
      ephy_search_engine_set_suggestions_url (self, g_value_get_string (value));
      break;
    case PROP_OPENSEARCH_URL:
      ephy_search_engine_set_opensearch_url (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_search_engine_class_init (EphySearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_search_engine_finalize;
  object_class->get_property = ephy_search_engine_get_property;
  object_class->set_property = ephy_search_engine_set_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         NULL, NULL,
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));
  properties [PROP_URL] =
    g_param_spec_string ("url",
                         NULL, NULL,
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));
  properties [PROP_BANG] =
    g_param_spec_string ("bang",
                         NULL, NULL,
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));
  properties [PROP_SUGGESTIONS_URL] =
    g_param_spec_string ("suggestions-url",
                         "Suggestions URL",
                         "The OpenSearch suggestions URL for this search engine",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));
  properties [PROP_OPENSEARCH_URL] =
    g_param_spec_string ("opensearch-url",
                         "OpenSearch description file URL",
                         "The OpenSearch description file URL for this search engine",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS |
                          G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ephy_search_engine_init (EphySearchEngine *self)
{
  /* Default values set with the GParamSpec aren't actually set at the end
   * of the GObject construction process, so we must ensure all properties
   * we expect to be non NULL to be kept that way, as we want to allow
   * safely omitting properties when using g_object_new().
   */
  self->name = g_strdup ("");
  self->url = g_strdup ("");
  self->bang = g_strdup ("");

  self->suggestions_url = NULL;
  self->opensearch_url = NULL;
}

static char *
replace_placeholder (const char *url,
                     const char *search_query)
{
  GString *s = g_string_new (url);
  g_autofree char *encoded_query = soup_form_encode ("q", search_query, NULL);

  /* libsoup requires us to pass a field name to get the HTML-form encoded
   * search query. But since we don't require that the search URL has the
   * q= before the placeholder, just skip q= and use the encoded query
   * directly.
   */
  g_string_replace (s, "%s", encoded_query + strlen ("q="), 0);

  return g_string_free (s, FALSE);
}

/**
 * ephy_search_engine_build_search_address:
 * @self: an #EphySearchEngine
 * @search_query: The search query to be used in the search URL.
 *
 * Returns: (transfer full): @self's search URL with all the %s placeholders
 * replaced with @search_query.
 */
char *
ephy_search_engine_build_search_address (EphySearchEngine *self,
                                         const char       *search_query)
{
  return replace_placeholder (self->url, search_query);
}

/**
 * ephy_search_engine_build_suggestions_address:
 * @self: an #EphySearchEngine
 * @search_query: The search query to be used in the search suggestions URL.
 *
 * Returns: (transfer full): @self's search suggestions URL with all the %s
 * placeholders replaced with @search_query.
 */
char *
ephy_search_engine_build_suggestions_address (EphySearchEngine *self,
                                              const char       *search_query)
{
  return replace_placeholder (self->suggestions_url, search_query);
}

typedef gboolean ( *UnicodeStrFilterFunc )(gunichar c);
/**
 * filter_str_with_functor:
 *
 * Filters-out every character that doesn't match @filter.
 *
 * @utf8_str: an UTF-8 string
 * @filter: a function pointer to one of the g_unichar_isX function.
 *
 * Returns: (transfer full): a new UTF-8 string containing only the characters matching @filter.
 */
static char *
filter_str_with_functor (const char           *utf8_str,
                         UnicodeStrFilterFunc  filter_func)
{
  gunichar *filtered_unicode_str = g_new0 (gunichar, strlen (utf8_str) + 1);
  g_autofree gunichar *unicode_str = NULL;
  char *final_utf8_str = NULL;
  int i = 0, j = 0;

  unicode_str = g_utf8_to_ucs4_fast (utf8_str, -1, NULL);

  for (; unicode_str[i] != '\0'; ++i) {
    /* If this characters matches, we add it to the final string. */
    if (filter_func (unicode_str[i]))
      filtered_unicode_str[j++] = unicode_str[i];
  }
  final_utf8_str = g_ucs4_to_utf8 (filtered_unicode_str, -1, NULL, NULL, NULL);
  /* We already assume it's UTF-8 when using g_utf8_to_ucs4_fast() above, and
   * our processing can't create invalid UTF-8 characters as we are only
   * copying existing and already valid UTF-8 characters. So it's safe to assert.
   */
  g_assert (final_utf8_str);
  /* Would be better to use g_autofree but scan-build complains as it doesn't properly handle the cleanup attribute. */
  g_free (filtered_unicode_str);

  return final_utf8_str;
}

/**
 * ephy_search_engine_build_bang_for_name:
 * @name: The search engine name to build a bang for.
 *
 * This utility function automatically builds a bang string from the search engine
 * @name, taking every first character in each word and every uppercase characters.
 * This means name "DuckDuckGo" will build bang "!ddg", "duck duck go" will
 * build bang "!ddg" as well, and "Wikipedia (en)" will build bang "!we".
 *
 * Returns: (transfer full) (nullable): a newly allocated bang string corresponding
 *   to @name
 */
char *
ephy_search_engine_build_bang_for_name (const char *name)
{
  g_autofree char *search_engine_name = g_strstrip (g_strdup (name));
  g_auto (GStrv) words = NULL;
  char *word;
  g_autofree char *acronym = g_strdup ("");
  g_autofree char *lowercase_acronym = NULL;
  g_autofree char *final_bang = NULL;

  g_assert (name);

  /* There's nothing to do if the string is empty. */
  if (g_strcmp0 (search_engine_name, "") == 0)
    return g_strdup ("");

  /* We ignore both the space character and opening parenthesis, as that
   * allows us to get !we as bang with "Wikipedia (en)" as name.
   */
  words = g_strsplit_set (search_engine_name, " (", 0);

  for (guint i = 0; words[i] != NULL; ++i) {
    g_autofree char *uppercase_chars = NULL;
    char *tmp_acronym = NULL;
    /* Fit the largest possible size for an UTF-8 character (4 bytes) and one byte for the NUL string terminator */
    char first_word_char[5] = {0};
    word = words[i];

    /* Ignore empty words. This might happen if there are multiple consecutives spaces between two words. */
    if (strcmp (word, "") == 0)
      continue;

    /* Go to the next character, as we treat the first character of each word separately. */
    uppercase_chars = filter_str_with_functor (g_utf8_find_next_char (word, NULL), g_unichar_isupper);
    /* Keep the first UTF-8 character so that names such as "duck duck go" will produce "ddg". */
    g_utf8_strncpy (first_word_char, word, 1);
    tmp_acronym = g_strconcat (acronym,
                               first_word_char,
                               uppercase_chars, NULL);
    g_free (acronym);
    acronym = tmp_acronym;
  }
  /* Bangs are usually lowercase */
  lowercase_acronym = g_utf8_strdown (acronym, -1);
  if (*lowercase_acronym == '\0')
    return g_strdup ("");

  /* "!" is the prefix for the bang */
  final_bang = g_strconcat ("!", lowercase_acronym, NULL);
  return g_steal_pointer (&final_bang);
}

/**
 * ephy_search_engine_matches_by_autodiscovery_link:
 * @autodiscovery_link: the Opensearch autodiscovery link
 *
 * Checks whether search engine @self matches the search engine pointed by
 * @autodiscovery_link. In practice it only checks that the @autodiscovery_link's
 * own URL matches the search engine's opensearch_url, and that the name matches
 * case insensitively.
 *
 * Returns: whether @autodiscovery_link seems to point to search engine @self
 */
gboolean
ephy_search_engine_matches_by_autodiscovery_link (EphySearchEngine                *self,
                                                  EphyOpensearchAutodiscoveryLink *autodiscovery_link)
{
  if (g_strcmp0 (self->opensearch_url, ephy_opensearch_autodiscovery_link_get_url (autodiscovery_link)) == 0) {
    return TRUE;
  } else {
    g_autofree char *casefolded_link_name = g_utf8_casefold (ephy_opensearch_autodiscovery_link_get_name (autodiscovery_link), -1);
    g_autofree char *casefolded_engine_name = g_utf8_casefold (self->name, -1);

    return g_strcmp0 (casefolded_link_name, casefolded_engine_name) == 0;
  }
}

static void
on_suggestions_downloaded_cb (SoupSession  *session,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  EphySearchEngine *engine = EPHY_SEARCH_ENGINE (g_task_get_task_data (task));
  g_autoptr (GBytes) bytes = NULL;
  GError *error = NULL;
  g_autoptr (JsonParser) json = NULL;
  const char *response_content;
  gsize length;
  g_autoptr (SoupMessage) msg = soup_session_get_async_result_message (session, result);
  g_autofree char *suggestions_url = g_uri_to_string (soup_message_get_uri (msg));
  JsonNode *root_node;
  JsonArray *json_array;
  const char *echoed_query_string;
  JsonArray *suggestions_array;
  const char *error_msg;
  guint suggestions_count;
  g_autoptr (GSequence) suggestions = NULL;

  bytes = soup_session_send_and_read_finish (session, result, &error);
  g_clear_object (&session);
  if (!bytes) {
    g_prefix_error (&error,
                    /* TRANSLATORS: The %s is the URL for the search suggestions. */
                    _("Couldn't load search engine suggestions from %s: "),
                    suggestions_url);
    g_task_return_error (task, error);
    return;
  }

  response_content = g_bytes_get_data (bytes, &length);
  if (!response_content || length == 0) {
    error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                         _("No content provided, length %ld"), length);
    g_task_return_error (task, error);
    return;
  }

  json = json_parser_new ();
  json_parser_load_from_data (json, response_content, length, &error);
  root_node = json_parser_get_root (json);
  /* An empty JSON parses just fine but the root node will be NULL. There's
   * even a test for it, in json-glib/tests/parser.c's test_empty_with_parser()
   * in json-glib's repo.
   */
  if ((!error && !root_node)
      /* Root node must be an array with at least 2 child elements, according
       * to https://raw.githubusercontent.com/dewitt/opensearch/master/mediawiki/Specifications/OpenSearch/Extensions/Suggestions/1.1/Draft%201.wiki
       */
      || !JSON_NODE_HOLDS_ARRAY (root_node)
      || json_array_get_length (json_node_get_array (root_node)) < 2) {
    error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                         _("Got empty JSON response or non-array root node or smaller than 2 elements array root node"));
  }
  if (error) {
    g_prefix_error (&error,
                    /* TRANSLATORS: The first %s is the URL for the search suggestions and the second one is raw JSON content. */
                    _("Could not parse JSON suggestions from %s with JSON %s: "),
                    suggestions_url, response_content);
    g_task_return_error (task, error);
    return;
  }

  json_array = json_node_get_array (root_node);
  echoed_query_string = json_array_get_string_element (json_array, 0);
  /* (Ab)use operator precedence for more concise error handling. */
  if ((!echoed_query_string && (error_msg = "not a string as first element"))
      || (!JSON_NODE_HOLDS_ARRAY (json_array_get_element (json_array, 1)) && (error_msg = "not an array as second element"))) {
    error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED, error_msg);
    /* Directly copied from above. */
    g_prefix_error (&error,
                    /* TRANSLATORS: The first %s is the URL for the search suggestions and the second one is raw JSON content. */
                    _("Could not parse JSON suggestions from %s with JSON %s: "),
                    suggestions_url, response_content);
    g_task_return_error (task, error);
    return;
  }
  suggestions_array = json_array_get_array_element (json_array, 1);
  suggestions_count = json_array_get_length (suggestions_array);
  suggestions = g_sequence_new (g_object_unref);
  for (guint i = 0; i < suggestions_count; i++) {
    const char *suggestion_term = json_array_get_string_element (suggestions_array, i);
    EphySuggestion *suggestion;
    g_autofree char *unescaped_title = NULL;
    g_autofree char *escaped_title = NULL;
    g_autofree char *suggestion_address = NULL;

    /* For now we don't bother if the suggestion term wasn't a string. */
    if (!suggestion_term)
      continue;

    /* TRANSLATORS: This is when you have search engines with suggestions support
     * (e.g. DuckDuckGo when added from the "search" button in the location entry,
     * as an OpenSearch engine): typing any text in the location entry will ask
     * the search engine for suggestions, while showing the suggestions more
     * nicely with this string. The first %s is the suggestion term coming from
     * the search engine, and the second one is the name of the search engine
     * from which the suggestions are coming.
     */
    unescaped_title = g_strdup_printf ("%s — %s", suggestion_term, ephy_search_engine_get_name (engine));
    escaped_title = g_markup_escape_text (unescaped_title, -1);
    suggestion_address = ephy_search_engine_build_search_address (engine, suggestion_term);
    suggestion = ephy_suggestion_new (escaped_title, unescaped_title, suggestion_address, FALSE);

    ephy_suggestion_set_icon (suggestion, "ephy-loupe-plus-symbolic");

    g_sequence_append (suggestions, suggestion);
  }

  g_task_return_pointer (task, g_steal_pointer (&suggestions), (GDestroyNotify)g_sequence_free);
}

/**
 * ephy_search_engine_load_suggestions_async:
 * @built_suggestions_url: The suggestions URL to query using the OpenSearch
 *   method, built using ephy_search_engine_manager_parse_bang_suggestions()
 *   or ephy_search_engine_build_suggestions_address().
 * @engine: The search engine corresponding to the @built_suggestions_url.
 *
 * Fetches the suggestions for a given suggestions URL.
 * Use ephy_search_engine_load_suggestions_finish() to retrieve the suggestions.
 */
void
ephy_search_engine_load_suggestions_async (const char          *built_suggestions_url,
                                           EphySearchEngine    *engine,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_autoptr (SoupMessage) msg = NULL;
  GTask *task = NULL;
  g_autoptr (SoupSession) soup_session = NULL;

  g_assert (EPHY_IS_SEARCH_ENGINE (engine));
  g_assert (built_suggestions_url && *built_suggestions_url != '\0');

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, ephy_search_engine_load_suggestions_async);
  g_task_set_task_data (task, g_object_ref (engine), g_object_unref);

  soup_session = soup_session_new ();
  soup_session_set_user_agent (soup_session, ephy_user_agent_get ());
  msg = soup_message_new (SOUP_METHOD_GET, built_suggestions_url);
  soup_session_send_and_read_async (g_steal_pointer (&soup_session),
                                    g_steal_pointer (&msg),
                                    G_PRIORITY_DEFAULT, cancellable,
                                    (GAsyncReadyCallback)on_suggestions_downloaded_cb,
                                    task);
}

/**
 * ephy_search_engine_load_suggestions_finish:
 *
 * Finishes the asynchronous operation started with ephy_search_engine_load_suggestions_async().
 *
 * Returns: (transfer full) (nullable): a new #GSequence with the loaded #EphySuggestion as items or %NULL in case of error.
 */
GSequence *
ephy_search_engine_load_suggestions_finish (GAsyncResult  *result,
                                            GError       **error)
{
  g_assert (g_task_is_valid (result, NULL));

  return g_task_propagate_pointer (G_TASK (result), error);
}

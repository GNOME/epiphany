/* ephy-opensearch-engine.c
 *
 * Copyright 2022 vanadiae <vanadiae35@gmail.com>
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


#include "ephy-opensearch-engine.h"

#include "ephy-user-agent.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <pango/pango.h>
#include <gtk/gtk.h>

/* This code handles all the parsing of the OpenSearch description file, and
 * creates a new EphySearchEngine from it.
 */

typedef enum  {
  URL_CATEGORY_HTML,
  URL_CATEGORY_SUGGESTIONS,
} UrlCategory;

typedef struct {
  EphyOpensearchAutodiscoveryLink *autodiscovery_link;

  EphySearchEngine *engine;

  /* This is actually something nice I thought about. When we reach a start element
   * we're interested in, we change next_property to be the property name of the
   * EphySearchEngine's member to fill in, and when we reach the next end element,
   * this is set back to NULL and we'll ignore any text callback when this
   * next_property is NULL. Hence, when getting a ShortName start element, we just
   * need to do next_property = "name"; and it'll fill in the name automatically
   * when the text callback is called (without the whitespace all around). So
   * it's just a way of having something nicer with the GMarkupParser (since it
   * doesn't provide us with a tree-like API, and libxml2 API/docs are just awful).
   */
  const char *next_property;

  /* Both are set when encountering a <Url> opening tag and used later in the
   * closing tag handler to process the template URL, to allow child <Param>
   * elements handling.
   */
  UrlCategory url_category;
  GString *url;
} AddOpensearch;

static void
free_add_opensearch (AddOpensearch *self)
{
  g_clear_object (&self->autodiscovery_link);
  g_clear_object (&self->engine);
  if (self->url) {
    g_string_free (self->url, TRUE);
    self->url = NULL;
  }

  self->next_property = NULL;

  g_free (self);
}

static const char *
find_path_start (const char  *url,
                 GError     **error)
{
  /* The OpenSearch spec seems to allow substitution anywhere in the
   * URL, but let's only allow for path+query+fragment for safety.
   *
   * We could do it "properly" and use g_uri_split() to get all URI components
   * and rebuild manually both parts of the URI, but that's really not needed
   * as we know that it's a http(s)://foobar.example/foobar… URL, so we can
   * just look for the first //, then from here go to the next / and we'll
   * have detected the path+query+fragment part.
   */
  const char *after_scheme = strstr (url, "//");
  const char *path_start;

  g_assert (after_scheme);

  after_scheme += strlen ("//");
  /* Look for each URL component's prefix character in the right order, to
   * allow URLs like https://example.com?q=%s, https://example.com#foo-%s.
   * Qwant has such an URL (https://www.qwant.com?q=%s) that would fail if we
   * only looked for /
   */
  path_start = strchr (after_scheme, '/');
  if (!path_start)
    path_start = strchr (after_scheme, '?');
  if (!path_start)
    path_start = strchr (after_scheme, '#');
  if (!path_start) {
    /* If we're here it means there isn't a {searchTerms} anywhere, so useless. */
    *error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                          _("The %s search engine template URL can't be valid."),
                          url);
    return NULL;
  }

  return path_start;
}

static GHashTable *
default_params_init (gpointer user_data)
{
  const struct {
    const char *name;
    const char *value;
  } params[] = {
    {"searchTerms", "%s"},
    /* Note that this is a RFC 5646/4656/3066 lang tag, not the setlocale() POSIX ones. */
    {"language", pango_language_to_string (gtk_get_default_language ())},
    {"inputEncoding", "UTF-8"},
    {"outputEncoding", "UTF-8"},
    /* Handling of Opensearch Referrer extension (https://github.com/dewitt/opensearch/blob/master/mediawiki/Specifications/OpenSearch/Extensions/Referrer/1.0/Draft%201.wiki).
     * Let's not care about checking using the "real" XML namespace URL.
     * This parameter is for e.g. &client=firefox query parameter.
     * Note that we fake ourselves as firefox as that's the best way of
     * making sure we actually get search suggestions. Google requires a
     * client=foo query parameter, but does not accept any value: only e.g.
     * chrome or firefox are actually accepted, and providing no client=
     * returns an error too.
     */
    {"referrer:source", "firefox"},
    /* We (and the vast majority of OpenSearch providers) don't support paged
     * results, and that's mostly useful for Atom/RSS output type.
     */
    {"count", "20"},
    {"startIndex", "1"},
    {"startPage", "1"},
    /* We don't set suggestionIndex, because we don't have the API to
     * provide the information and it is useless anyway since we don't do paged
     * suggestions, nor do we set suggestionPrefix because it's mostly useless
     * and "should" be an optional template parameter, according to
     * https://raw.githubusercontent.com/dewitt/opensearch/master/mediawiki/Specifications/OpenSearch/Extensions/Suggestions/1.1/Draft%201.wiki
     */
  };
  GHashTable *hash_table = g_hash_table_new (g_str_hash, g_str_equal);

  for (guint i = 0; i < G_N_ELEMENTS (params); i++) {
    g_hash_table_insert (hash_table, (gpointer)params[i].name, (gpointer)params[i].value);
  }
  return hash_table;
}

static GHashTable *
get_default_param_values (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return g_once (&once_init, (GThreadFunc)default_params_init, NULL);
}

/**
 * url_template_param_substitution:
 *
 * Performs URL template parameters substitution according to the OpenSearch spec
 * (https://github.com/dewitt/opensearch/blob/master/opensearch-1-1-draft-6.md#opensearch-url-template-syntax),
 * replacing every parameter with a meaningful value if the parameter is known,
 * or with the parameter replaced with an empty string.
 *
 * Returns: (transfer full): the new URL template with param substitution made.
 *   The search term is represented by %s in the returned URL. If there was an error,
 *   %NULL is returned and @error is filled accordingly. All errors are due to broken
 *   URL templates, so most of the time there won't be any error.
 */
static char *
url_template_param_substitution (const char  *url_template,
                                 GError     **error)
{
  g_autoptr (GString) builder = g_string_new (url_template);
  /* Used as a reference to get the relative position with pointer_in_url - url_start */
  const char *url_start = builder->str;
  const char *path_start = find_path_start (builder->str, error);
  const char *param_start = path_start;
  GHashTable *default_param_values = get_default_param_values ();
  gboolean has_search_terms = FALSE;

#define OPTIONAL_PARAM_CHAR '?'

  /* find_path_start() has already set the error if it returned NULL. */
  if (!path_start)
    return NULL;

  while ((param_start = strchr (param_start, '{'))) {
    const char *param_end = strchr (param_start, '}');
    g_autofree char *param_name = NULL;
    gsize param_name_len;
    gboolean is_param_optional;
    const char *param_replacement;

    if (!param_end) {
      *error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Found unclosed URL template parameter curly bracket at pos %d of URL %s."),
                            (guint)(param_start - url_start),
                            url_template);
      return NULL;
    }

    if (param_end - 1 == param_start ||
        /* Account for the {?} case too. */
        (*(param_end - 1) == OPTIONAL_PARAM_CHAR && param_end - 2 == param_start)) {
      *error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Found URL template parameter at %d in URL %s, but without any name."),
                            (guint)(param_start - url_start),
                            url_template);
      return NULL;
    }

    param_name_len = param_end - (param_start + 1);
    param_name = g_strndup (param_start + 1, param_name_len);
    /* Parameters are of the form "{param_name?}" (without the quotes), with the
     * "?" being there only if the parameter is optional.
     */
    is_param_optional = (param_name[param_name_len - 1] == OPTIONAL_PARAM_CHAR);
    /* Replace the "?" character in the parameter name with NUL to simplify things. */
    if (is_param_optional)
      param_name[param_name_len - 1] = '\0';

    g_string_erase (builder,
                    param_start - url_start,
                    /* + 1 because param_end is *at* the }, not just after it, and this arg is a length. */
                    (param_end + 1) - param_start);
    param_replacement = g_hash_table_lookup (default_param_values, param_name);
    if (!param_replacement && !is_param_optional) {
      *error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("Found unknown URL template parameter %s in URL %s."),
                            param_name, url_template);
      return NULL;
    } else {
      /* https://github.com/dewitt/opensearch/blob/master/opensearch-1-1-draft-6.md#optional-template-parameters */
      if (!param_replacement && is_param_optional)
        param_replacement = "";
      /* Now that we've removed the param placeholder, just insert the
       * appropriate value at the same position.
       */
      g_string_insert (builder, param_start - url_start, param_replacement);
    }

    if (g_strcmp0 (param_name, "searchTerms") == 0)
      has_search_terms = TRUE;
  }

  if (!has_search_terms) {
    *error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                          _("Parsing of URL template %s succeeded, but no search terms were found so it's not useful as a search engine."),
                          url_template);
    return NULL;
  }

  return g_string_free (g_steal_pointer (&builder), FALSE);
}

static void
xml_start_element_cb (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      gpointer              user_data,
                      GError              **out_error)
{
  AddOpensearch *data = user_data;
  const char *ns_prefix_colon = strchr (element_name, ':');

  /* Skip the namespace prefix if there is one. This isn't a proper way of
   * handling namespace prefixes, but it should be fine for now. */
  if (ns_prefix_colon) {
    element_name = ns_prefix_colon + 1;
  }

  if (g_strcmp0 (element_name, "OpenSearchDescription") == 0) {
    /* For now we won't enforce it as root element. */
  } else if (g_strcmp0 (element_name, "ShortName") == 0) {
    data->next_property = "name";
  } else if (g_strcmp0 (element_name, "Url") == 0) {
    const char *template_url = NULL, *mime_type = NULL;
    gboolean is_suggestions_url;

    /* There is g_markup_collect_attributes(), but it doesn't have a flag
     * to ignore all unknown attributes. So instead we must loop manually
     * through the attributes, to get the ones we care about.
     */
    for (guint i = 0; attribute_names[i]; i++) {
      const char *attr_name = attribute_names[i];
      const char *attr_value = attribute_values[i];

      if (g_strcmp0 (attr_name, "template") == 0)
        template_url = attr_value;
      else if (g_strcmp0 (attr_name, "type") == 0)
        mime_type = attr_value;
    }

    is_suggestions_url =
      (g_strcmp0 (mime_type, "application/json") == 0
       || g_strcmp0 (mime_type, "application/x-suggestions+json") == 0);
    if (g_strcmp0 (mime_type, "text/html") == 0) {
      data->url = g_string_new (template_url);
      data->url_category = URL_CATEGORY_HTML;
    } else if (is_suggestions_url) {
      data->url = g_string_new (template_url);
      data->url_category = URL_CATEGORY_SUGGESTIONS;
    }
  }
  /* <Param> handling as children elements of <Url>, as seen in the wild due
   * to the way the MDN using it despite not being in any of the OpenSearch
   * specifications.
   */
  else if (g_strcmp0 (element_name, "Param") == 0 && data->url) {
    const char *param_name = NULL, *param_value = NULL;

    for (guint i = 0; attribute_names[i]; i++) {
      const char *attr_name = attribute_names[i];
      const char *attr_value = attribute_values[i];

      if (g_strcmp0 (attr_name, "name") == 0)
        param_name = attr_value;
      else if (g_strcmp0 (attr_name, "value") == 0)
        param_value = attr_value;
    }

    if (!param_name || !param_value) {
      if (out_error)
        *out_error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED, _("Skipping <Param> tag as it has no name or value attribute"));
      return;
    }

    {
      const char *query_param_p = strrchr (data->url->str, '?');
      if (query_param_p) {
        g_autofree char *kv_pair = g_strdup_printf ("%s=%s&", param_name, param_value);
        g_string_insert (data->url, query_param_p - data->url->str + 1, kv_pair);
      } else {
        const char *before_fragment = strrchr (data->url->str, '#');
        g_autofree char *kv_pair = g_strdup_printf ("?%s=%s", param_name, param_value);
        if (!before_fragment)
          before_fragment = strrchr (data->url->str, '\0');
        g_string_insert (data->url, before_fragment - data->url->str, kv_pair);
      }
    }
  }
}

static void
xml_end_element_cb (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    gpointer              user_data,
                    GError              **out_error)
{
  AddOpensearch *data = user_data;

  if (g_strcmp0 (element_name, "Url") == 0 && data->url) {
    if (data->url_category == URL_CATEGORY_HTML) {
      g_autofree char *substituted_url = url_template_param_substitution (data->url->str, out_error);
      if (substituted_url)
        ephy_search_engine_set_url (data->engine, substituted_url);
    } else if (data->url_category == URL_CATEGORY_SUGGESTIONS) {
      g_autofree char *substituted_url = url_template_param_substitution (data->url->str, out_error);
      if (substituted_url)
        ephy_search_engine_set_suggestions_url (data->engine, substituted_url);
    } else {
      g_assert_not_reached ();
    }

    g_string_free (data->url, TRUE);
    data->url = NULL;
  }

  data->next_property = NULL;
}

static void
xml_text_cb (GMarkupParseContext  *context,
             const gchar          *text,
             gsize                 text_len,
             gpointer              user_data,
             GError              **error)
{
  AddOpensearch *data = user_data;
  /* We need to copy the text because it is a length-based string, not a zero
   * terminated one.
   */
  g_autofree char *original_text = g_strndup (text, text_len);
  /* Don't use autofree here because g_strstrip does not allocate, it only
   * returns the correct pointer and set the zero termination properly.
   */
  const char *stripped_text = g_strstrip (original_text);

  /* The markup parser gives _separate_ text events for newlines whitespace.
   * So completely ignore them to only care about the text part, from which
   * we strip its whitespaces in case it's indented.
   */
  if (*stripped_text == '\0')
    return;

  /* If we were asked to fill the buffer with the next text we encounter, then fill it. */
  if (data->next_property)
    g_object_set (data->engine, data->next_property, stripped_text, NULL);
}

static gboolean
parse_opensearch_xml (AddOpensearch  *data,
                      GError        **error,
                      const char     *file_content,
                      gsize           length)
{
  GMarkupParser parser = {
    .start_element = xml_start_element_cb,
    .end_element = xml_end_element_cb,
    .text = xml_text_cb,
    .passthrough = NULL,
    .error = NULL,
  };
  g_autoptr (GMarkupParseContext) ctx =
    g_markup_parse_context_new (&parser,
                                G_MARKUP_TREAT_CDATA_AS_TEXT,
                                data,
                                NULL);
  g_autoptr (GError) markup_error = NULL;

  data->engine = g_object_new (EPHY_TYPE_SEARCH_ENGINE, NULL);
  ephy_search_engine_set_opensearch_url (data->engine,
                                         ephy_opensearch_autodiscovery_link_get_url (data->autodiscovery_link));
  /* We're not really handling things like XML namespaces to keep things simple.
   * We aren't checking either if there's nested elements (for example <ShortName>
   * <Url …></Url></ShortName> would totally be accepted), but at the end of the
   * day it does the job it's supposed to.
   */
  if (!g_markup_parse_context_parse (ctx, file_content, length, &markup_error)) {
    g_propagate_prefixed_error (error, markup_error,
                                /* TRANSLATORS: First %s is the name of the search engine. Second %s is the error message (supposedly partially localized on the GLib level). */
                                _("Couldn't parse search engine description file for %s: %s"),
                                ephy_opensearch_autodiscovery_link_get_name (data->autodiscovery_link),
                                markup_error->message);
    return FALSE;
  }

  /* The spec mandates that every engine has a ShortName, so this shouldn't happen.
   * Anyway if they messed up completely the description file, there's a good chance
   * the rest of the file is incomplete up too.
   */
  if (*ephy_search_engine_get_name (data->engine) == '\0'
      || *ephy_search_engine_get_url (data->engine) == '\0') {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 _("Couldn't create search engine as it did not provide sufficient information!"));
    return FALSE;
  }

  return TRUE;
}

/**
 * ephy_opensearch_engine_load_from_data:
 * @autodiscovery_link: the #EphyOpensearchAutodiscoveryLink of the OpenSearch engine to be loaded
 * @description_file: the content of the OpenSearch XML description file
 * @length: the length of @description_file
 * @error: (nullable) (out): a #GError
 *
 * Returns: (transfer full): the #EphySearchEngine corresponding to this OpenSearch
 *   XML description file, or %NULL if there was an error.
 */
EphySearchEngine *
ephy_opensearch_engine_load_from_data (EphyOpensearchAutodiscoveryLink  *autodiscovery_link,
                                       const char                       *description_file,
                                       gsize                             length,
                                       GError                          **error)
{
  AddOpensearch data = {
    .autodiscovery_link = autodiscovery_link,
    .engine = NULL,
    .next_property = NULL,
  };

  g_assert (autodiscovery_link);
  g_assert (description_file);

  if (!parse_opensearch_xml (&data, error, description_file, length)) {
    g_clear_object (&data.engine);
    return NULL;
  }

  return data.engine;
}

static void
on_opensearch_file_downloaded_cb (SoupSession  *session,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  g_autoptr (GError) error = NULL;
  AddOpensearch *data = g_task_get_task_data (task);
  g_autoptr (GBytes) bytes = NULL;
  gsize length;
  const char *file_content;

  bytes = soup_session_send_and_read_finish (session, result, &error);
  /* One-time use SoupSession so clean it up properly here. */
  g_clear_object (&session);
  if (!bytes) {
    g_prefix_error (&error,
                    /* TRANSLATORS: The %s is the name of the search engine. */
                    _("Couldn't download search engine description file for %s: "),
                    ephy_opensearch_autodiscovery_link_get_name (data->autodiscovery_link));
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  file_content = g_bytes_get_data (bytes, &length);
  if (!parse_opensearch_xml (data, &error, file_content, length)) {
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  g_task_return_pointer (task, g_steal_pointer (&data->engine), g_object_unref);
}

/**
 * ephy_opensearch_engine_load_from_link_async:
 * @autodiscovery_link: (transfer none): The OpenSearch engine's autodiscovered link to be added to @self
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Fetches the OpenSearch XML description file designated by @autodiscovery_link,
 * and creates a new #EphySearchEngine from it.
 *
 * See ephy_opensearch_engine_load_from_link_finish() to obtain the search engine
 * once the file was XML description file was downloaded and the search engine
 * created from it.
 */
void
ephy_opensearch_engine_load_from_link_async (EphyOpensearchAutodiscoveryLink *autodiscovery_link,
                                             GCancellable                    *cancellable,
                                             GAsyncReadyCallback              callback,
                                             gpointer                         user_data)
{
  AddOpensearch *data = NULL;
  g_autoptr (SoupMessage) msg = NULL;
  GTask *task = NULL;
  g_autoptr (SoupSession) soup_session = NULL;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (autodiscovery_link, cancellable, callback, user_data);
  g_task_set_source_tag (task, ephy_opensearch_engine_load_from_link_async);

  data = g_new0 (AddOpensearch, 1);
  data->autodiscovery_link = g_object_ref (autodiscovery_link);
  g_task_set_task_data (task, data, (GDestroyNotify)free_add_opensearch);

  soup_session = soup_session_new ();
  soup_session_set_user_agent (soup_session, ephy_user_agent_get ());
  msg = soup_message_new (SOUP_METHOD_GET,
                          ephy_opensearch_autodiscovery_link_get_url (autodiscovery_link));
  soup_session_send_and_read_async (g_steal_pointer (&soup_session),
                                    msg,
                                    G_PRIORITY_DEFAULT, cancellable,
                                    (GAsyncReadyCallback)on_opensearch_file_downloaded_cb,
                                    task);
}

/**
 * ephy_opensearch_engine_load_from_link_finish:
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Finishes the asynchronous operation started with ephy_opensearch_engine_load_from_link_async().
 *
 * Returns: (transfer full) (nullable): a new #EphySearchEngine with its properties
 *   set to meaningful values taken from the OpenSearch XML description file, or
 *   %NULL on error.
 */
EphySearchEngine *
ephy_opensearch_engine_load_from_link_finish (EphyOpensearchAutodiscoveryLink  *autodiscovery_link,
                                              GAsyncResult                     *result,
                                              GError                          **error)
{
  g_return_val_if_fail (g_task_is_valid (result, autodiscovery_link), FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}

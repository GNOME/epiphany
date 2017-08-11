/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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
 *
 *  Some parts of this file based on the Midori's 'adblock' extension,
 *  licensed with the GNU Lesser General Public License 2.1, Copyright
 *  (C) 2009-2010 Christian Dywan <christian@twotoasts.de> and 2009
 *  Alexander Butenko <a.butenka@gmail.com>. Check Midori's web site
 *  at http://www.twotoasts.de
 */

#include "config.h"
#include "ephy-uri-tester.h"

#include "ephy-debug.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-uri-tester-shared.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <string.h>

#if ENABLE_HTTPS_EVERYWHERE
#include <httpseverywhere.h>
#endif

#define SIGNATURE_SIZE 8

struct _EphyUriTester {
  GObject parent_instance;

  char *adblock_data_dir;

  GHashTable *pattern;
  GHashTable *keys;
  GHashTable *optslist;
  GHashTable *urlcache;

  GHashTable *whitelisted_pattern;
  GHashTable *whitelisted_keys;
  GHashTable *whitelisted_optslist;
  GHashTable *whitelisted_urlcache;

  GString *blockcss;
  GString *blockcssprivate;

  GRegex *regex_third_party;
  GRegex *regex_pattern;
  GRegex *regex_subdocument;
  GRegex *regex_frame_add;

  GMainLoop *load_loop;
  int adblock_filters_to_load;
  gboolean adblock_loaded;
#if ENABLE_HTTPS_EVERYWHERE
  gboolean https_everywhere_loaded;

  HTTPSEverywhereContext *https_everywhere_context;
#endif
};

enum {
  PROP_0,
  PROP_ADBLOCK_DATA_DIR,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE (EphyUriTester, ephy_uri_tester, G_TYPE_OBJECT)

static GString *
ephy_uri_tester_fixup_regexp (const char *prefix, char *src);

static inline int
ephy_uri_tester_check_rule (EphyUriTester *tester,
                            GRegex        *regex,
                            const char    *patt,
                            const char    *req_uri,
                            const char    *page_uri,
                            gboolean       whitelist)
{
  char *opts;
  GHashTable *optslist = tester->optslist;
  if (whitelist)
    optslist = tester->whitelisted_optslist;

  if (!g_regex_match_full (regex, req_uri, -1, 0, 0, NULL, NULL))
    return FALSE;

  opts = g_hash_table_lookup (optslist, patt);
  if (opts && g_regex_match (tester->regex_third_party, opts, 0, NULL)) {
    if (page_uri && g_regex_match_full (regex, page_uri, -1, 0, 0, NULL, NULL))
      return FALSE;
  }
  /* TODO: Domain and document opt check */
  if (whitelist)
    LOG ("whitelisted by pattern regexp=%s -- %s", g_regex_get_pattern (regex), req_uri);
  else
    LOG ("blocked by pattern regexp=%s -- %s", g_regex_get_pattern (regex), req_uri);
  return TRUE;
}

static inline gboolean
ephy_uri_tester_is_matched_by_pattern (EphyUriTester *tester,
                                       const char    *req_uri,
                                       const char    *page_uri,
                                       gboolean       whitelist)
{
  GHashTableIter iter;
  gpointer patt, regex;
  GHashTable *pattern = tester->pattern;
  if (whitelist)
    pattern = tester->whitelisted_pattern;

  g_hash_table_iter_init (&iter, pattern);
  while (g_hash_table_iter_next (&iter, &patt, &regex)) {
    if (ephy_uri_tester_check_rule (tester, regex, patt, req_uri, page_uri, whitelist))
      return TRUE;
  }
  return FALSE;
}

static inline gboolean
ephy_uri_tester_is_matched_by_key (EphyUriTester *tester,
                                   const char    *opts,
                                   const char    *req_uri,
                                   const char    *page_uri,
                                   gboolean       whitelist)
{
  char *uri;
  int len;
  int pos = 0;
  GList *regex_bl = NULL;
  GString *guri;
  gboolean ret = FALSE;
  char sig[SIGNATURE_SIZE + 1];
  GHashTable *keys = tester->keys;
  if (whitelist)
    keys = tester->whitelisted_keys;

  memset (&sig[0], 0, sizeof (sig));
  /* Signatures are made on pattern, so we need to convert url to a pattern as well */
  guri = ephy_uri_tester_fixup_regexp ("", (char *)req_uri);
  uri = guri->str;
  len = guri->len;

  for (pos = len - SIGNATURE_SIZE; pos >= 0; pos--) {
    GRegex *regex;
    strncpy (sig, uri + pos, SIGNATURE_SIZE);
    regex = g_hash_table_lookup (keys, sig);

    /* Dont check if regex is already blacklisted */
    if (!regex || g_list_find (regex_bl, regex))
      continue;
    ret = ephy_uri_tester_check_rule (tester, regex, sig, req_uri, page_uri, whitelist);
    if (ret)
      break;
    regex_bl = g_list_prepend (regex_bl, regex);
  }
  g_string_free (guri, TRUE);
  g_list_free (regex_bl);
  return ret;
}

static gboolean
ephy_uri_tester_is_matched (EphyUriTester *tester,
                            const char    *opts,
                            const char    *req_uri,
                            const char    *page_uri,
                            gboolean       whitelist)
{
  char *value;
  GHashTable *urlcache = tester->urlcache;
  if (whitelist)
    urlcache = tester->whitelisted_urlcache;

  /* Check cached URLs first. */
  if ((value = g_hash_table_lookup (urlcache, req_uri)))
    return GPOINTER_TO_INT (value);

  /* Look for a match either by key or by pattern. */
  if (ephy_uri_tester_is_matched_by_key (tester, opts, req_uri, page_uri, whitelist)) {
    g_hash_table_insert (urlcache, g_strdup (req_uri), g_strdup ("1"));
    return TRUE;
  }

  /* Matching by pattern is pretty expensive, so do it if needed only. */
  if (ephy_uri_tester_is_matched_by_pattern (tester, req_uri, page_uri, whitelist)) {
    g_hash_table_insert (urlcache, g_strdup (req_uri), GINT_TO_POINTER (TRUE));
    return TRUE;
  }

  g_hash_table_insert (urlcache, g_strdup (req_uri), GINT_TO_POINTER (FALSE));
  return FALSE;
}

static GString *
ephy_uri_tester_fixup_regexp (const char *prefix, char *src)
{
  GString *str;

  if (!src)
    return NULL;

  str = g_string_new (prefix);

  /* lets strip first .* */
  if (src[0] == '*') {
    (void)*src++;
  }

  /* NOTE: The '$' is used as separator for the rule options, so rule patterns
     cannot ever contain them. If a rule needs to match it, it uses "%24".
     Splitting the option is done in ephy_uri_tester_add_url_pattern().

     The loop below always escapes square brackets. This way there is no chance
     that they get interpreted as a character class, and it is NOT needed to
     escape '-' because it's only special inside a character class. */
  do {
    switch (*src) {
      case '*':
        g_string_append (str, ".*");
        break;
      case '^':
      /* Matches a separator character, defined as:
       * "anything but a letter, a digit, or one of the following: _ - . %" */
        g_string_append (str, "([^a-zA-Z\\d]|[_\\-\\.%])");
        break;
      case '|':
      /* If at the end of the pattern, the match is anchored at the end. In
       * the middle of a pattern it matches a literal vertical bar and the
       * character must be escaped. */
        if (src[1] == '\0')
          g_string_append (str, "$");
        else
          g_string_append (str, "\\|");
        break;
      /* The following characters are escaped as they have a meaning in
       * regular expressions:
       *   - '.' matches any character.
       *   - '+' matches the preceding pattern one or more times.
       *   - '?' matches the preceding pattern zero or one times.
       *   - '[' ']' are used to define a character class.
       *   - '{' '}' are used to define a min/max quantifier.
       *   - '(' ')' are used to defin a submatch expression.
       *   - '\' has several uses in regexps (shortcut character classes.
       *     matching non-printing characters, using octal/hex, octal
       *     constants, backreferences... they must to be escaped to
       *     match a literal backslash and prevent wrecking havoc!). */
      case '.':
      case '+':
      case '?':
      case '[':
      case ']':
      case '{':
      case '}':
      case '(':
      case ')':
      case '\\':
        g_string_append_printf (str, "\\%c", *src);
        break;
      default:
        g_string_append_printf (str, "%c", *src);
        break;
    }
    src++;
  } while (*src);

  return str;
}

static void
ephy_uri_tester_compile_regexp (EphyUriTester *tester,
                                GString       *gpatt,
                                const char    *opts,
                                gboolean       whitelist)
{
  GHashTable *pattern;
  GHashTable *keys;
  GHashTable *optslist;
  GRegex *regex;
  GError *error = NULL;
  char *patt;
  int len;

  if (!gpatt)
    return;

  patt = gpatt->str;
  len = gpatt->len;

  /* TODO: Play with optimization flags */
  regex = g_regex_new (patt, G_REGEX_OPTIMIZE | G_REGEX_JAVASCRIPT_COMPAT,
                       G_REGEX_MATCH_NOTEMPTY, &error);
  if (error) {
    g_warning ("%s: %s", G_STRFUNC, error->message);
    g_error_free (error);
    return;
  }

  pattern = tester->pattern;
  keys = tester->keys;
  optslist = tester->optslist;
  if (whitelist) {
    pattern = tester->whitelisted_pattern;
    keys = tester->whitelisted_keys;
    optslist = tester->whitelisted_optslist;
  }

  if (!g_regex_match (tester->regex_pattern, patt, 0, NULL)) {
    int signature_count = 0;
    int pos = 0;
    char *sig;

    for (pos = len - SIGNATURE_SIZE; pos >= 0; pos--) {
      sig = g_strndup (patt + pos, SIGNATURE_SIZE);
      if (!strchr (sig, '*') &&
          !g_hash_table_lookup (keys, sig)) {
        LOG ("sig: %s %s", sig, patt);
        g_hash_table_insert (keys, g_strdup (sig), g_regex_ref (regex));
        g_hash_table_insert (optslist, g_strdup (sig), g_strdup (opts));
        signature_count++;
      } else {
        if (sig[0] == '*' &&
            !g_hash_table_lookup (pattern, patt)) {
          LOG ("patt2: %s %s", sig, patt);
          g_hash_table_insert (pattern, g_strdup (patt), g_regex_ref (regex));
          g_hash_table_insert (optslist, g_strdup (patt), g_strdup (opts));
        }
      }
      g_free (sig);
    }
    g_regex_unref (regex);

    if (signature_count > 1 && g_hash_table_lookup (pattern, patt))
      g_hash_table_remove (pattern, patt);
  } else {
    LOG ("patt: %s%s", patt, "");
    /* Pattern is a regexp chars */
    g_hash_table_insert (pattern, g_strdup (patt), regex);
    g_hash_table_insert (optslist, g_strdup (patt), g_strdup (opts));
  }
}

static void
ephy_uri_tester_add_url_pattern (EphyUriTester *tester,
                                 const char    *prefix,
                                 const char    *type,
                                 char          *line,
                                 gboolean       whitelist)
{
  char **data;
  char *patt;
  GString *format_patt;
  const char *opts;

  data = g_strsplit (line, "$", -1);
  if (!data || !data[0]) {
    g_strfreev (data);
    return;
  }

  if (data[1] && data[2]) {
    patt = g_strconcat (data[0], data[1], NULL);
    opts = g_strconcat (type, ",", data[2], NULL);
  } else if (data[1]) {
    patt = data[0];
    opts = g_strconcat (type, ",", data[1], NULL);
  } else {
    patt = data[0];
    opts = type;
  }

  if (g_regex_match (tester->regex_subdocument, opts, 0, NULL)) {
    if (data[1] && data[2])
      g_free (patt);
    if (data[1])
      g_free ((char *)opts);
    g_strfreev (data);
    return;
  }

  format_patt = ephy_uri_tester_fixup_regexp (prefix, patt);

  if (whitelist)
    LOG ("whitelist: %s opts %s", format_patt->str, opts);
  else
    LOG ("blacklist: %s opts %s", format_patt->str, opts);

  ephy_uri_tester_compile_regexp (tester, format_patt, opts, whitelist);

  if (data[1] && data[2])
    g_free (patt);
  if (data[1])
    g_free ((char *)opts);
  g_strfreev (data);

  g_string_free (format_patt, TRUE);
}

static inline void
ephy_uri_tester_frame_add (EphyUriTester *tester, char *line)
{
  const char *separator = " , ";

  (void)*line++;
  (void)*line++;
  if (strchr (line, '\'')
      || (strchr (line, ':')
          && !g_regex_match (tester->regex_frame_add, line, 0, NULL))) {
    return;
  }
  g_string_append (tester->blockcss, separator);
  g_string_append (tester->blockcss, line);
}

static inline void
ephy_uri_tester_frame_add_private (EphyUriTester *tester,
                                   const char    *line,
                                   const char    *sep)
{
  char **data;
  data = g_strsplit (line, sep, 2);

  if (!(data[1] && *data[1])
      || strchr (data[1], '\'')
      || (strchr (data[1], ':')
          && !g_regex_match (tester->regex_frame_add, data[1], 0, NULL))) {
    g_strfreev (data);
    return;
  }

  if (strchr (data[0], ',')) {
    char **domains;
    int i;

    domains = g_strsplit (data[0], ",", -1);
    for (i = 0; domains[i]; i++) {
      g_string_append_printf (tester->blockcssprivate, ";sites['%s']+=',%s'",
                              g_strstrip (domains[i]), data[1]);
    }
    g_strfreev (domains);
  } else {
    g_string_append_printf (tester->blockcssprivate, ";sites['%s']+=',%s'",
                            data[0], data[1]);
  }
  g_strfreev (data);
}

static void
ephy_uri_tester_parse_line (EphyUriTester *tester,
                            char          *line,
                            gboolean       whitelist)
{
  if (!line)
    return;

  g_strchomp (line);
  /* Ignore comments and new lines */
  if (line[0] == '!')
    return;
  /* FIXME: No support for [include] and [exclude] tags */
  if (line[0] == '[')
    return;

  /* Whitelisted exception rules */
  if (g_str_has_prefix (line, "@@")) {
    ephy_uri_tester_parse_line (tester, line + 2, TRUE);
    return;
  }

  /* FIXME: No support for domain= */
  if (strstr (line, "domain="))
    return;

  /* Skip garbage */
  if (line[0] == ' ' || !line[0])
    return;

  /* Got CSS block hider */
  if (line[0] == '#' && line[1] == '#') {
    ephy_uri_tester_frame_add (tester, line);
    return;
  }
  /* Got CSS block hider. Workaround */
  if (line[0] == '#')
    return;

  /* Got per domain CSS hider rule */
  if (strstr (line, "##")) {
    ephy_uri_tester_frame_add_private (tester, line, "##");
    return;
  }

  /* Got per domain CSS hider rule. Workaround */
  if (strchr (line, '#')) {
    ephy_uri_tester_frame_add_private (tester, line, "#");
    return;
  }
  /* Got URL blocker rule */
  if (line[0] == '|' && line[1] == '|') {
    (void)*line++;
    (void)*line++;
    /* set a regex prefix to ensure that '||' patterns are anchored at the
     * start and that any characters (if any) preceding the domain specified
     * by the rule is separated from it by a dot '.'  */
    ephy_uri_tester_add_url_pattern (tester, "^[\\w\\-]+:\\/+(?!\\/)(?:[^\\/]+\\.)?", "fulluri", line, whitelist);
    return;
  }
  if (line[0] == '|') {
    (void)*line++;
    ephy_uri_tester_add_url_pattern (tester, "^", "fulluri", line, whitelist);
    return;
  }
  ephy_uri_tester_add_url_pattern (tester, "", "uri", line, whitelist);
}

static void
ephy_uri_tester_adblock_loaded (EphyUriTester *tester)
{
  if (g_atomic_int_dec_and_test (&tester->adblock_filters_to_load)) {
    tester->adblock_loaded = TRUE;
#if ENABLE_HTTPS_EVERYWHERE
    if (tester->https_everywhere_loaded)
      g_main_loop_quit (tester->load_loop);
#else
    g_main_loop_quit (tester->load_loop);
#endif
  }
}

#if ENABLE_HTTPS_EVERYWHERE
static void
ephy_uri_tester_https_everywhere_loaded (EphyUriTester *tester)
{
  tester->https_everywhere_loaded = TRUE;
  if (tester->adblock_loaded)
    g_main_loop_quit (tester->load_loop);
}
#endif

static void
file_parse_cb (GDataInputStream *stream, GAsyncResult *result, EphyUriTester *tester)
{
  char *line;
  GError *error = NULL;

  line = g_data_input_stream_read_line_finish (stream, result, NULL, &error);
  if (!line) {
    if (error) {
      g_warning ("Error parsing file: %s\n", error->message);
      g_error_free (error);
    }

    ephy_uri_tester_adblock_loaded (tester);
    return;
  }

  ephy_uri_tester_parse_line (tester, line, FALSE);
  g_free (line);

  g_data_input_stream_read_line_async (stream, G_PRIORITY_DEFAULT_IDLE, NULL,
                                       (GAsyncReadyCallback)file_parse_cb, tester);
}

static void
file_read_cb (GFile *file, GAsyncResult *result, EphyUriTester *tester)
{
  GFileInputStream *stream;
  GDataInputStream *data_stream;
  GError *error = NULL;

  stream = g_file_read_finish (file, result, &error);
  if (!stream) {
    char *path;

    path = g_file_get_path (file);
    g_warning ("Error opening file %s for parsing: %s\n", path, error->message);
    g_free (path);
    g_error_free (error);

    ephy_uri_tester_adblock_loaded (tester);
    return;
  }

  data_stream = g_data_input_stream_new (G_INPUT_STREAM (stream));
  g_object_unref (stream);

  g_data_input_stream_read_line_async (data_stream, G_PRIORITY_DEFAULT_IDLE, NULL,
                                       (GAsyncReadyCallback)file_parse_cb, tester);
  g_object_unref (data_stream);
}

static gboolean
ephy_uri_tester_block_uri (EphyUriTester *tester,
                          const char    *req_uri,
                          const char    *page_uri)
{
  /* check whitelisting rules before the normal ones */
  if (ephy_uri_tester_is_matched (tester, NULL, req_uri, page_uri, TRUE))
    return FALSE;
  return ephy_uri_tester_is_matched (tester, NULL, req_uri, page_uri, FALSE);
}

char *
ephy_uri_tester_rewrite_uri (EphyUriTester    *tester,
                             const char       *request_uri,
                             const char       *page_uri,
                             EphyUriTestFlags  flags)
{
  /* Should we block the URL outright? */
  if ((flags & EPHY_URI_TEST_ADBLOCK) &&
      ephy_uri_tester_block_uri (tester, request_uri, page_uri)) {
    g_debug ("Request '%s' blocked (page: '%s')", request_uri, page_uri);

    return NULL;
  }

#if ENABLE_HTTPS_EVERYWHERE
  if ((flags & EPHY_URI_TEST_HTTPS_EVERYWHERE) && tester->https_everywhere_context != NULL)
    return https_everywhere_context_rewrite (tester->https_everywhere_context, request_uri);
#endif

  return g_strdup (request_uri);
}

#if ENABLE_HTTPS_EVERYWHERE
static void
https_everywhere_context_init_cb (HTTPSEverywhereContext *context,
                                  GAsyncResult           *res,
                                  EphyUriTester          *tester)
{
  GError *error = NULL;

  https_everywhere_context_init_finish (context, res, &error);

  if (error) {
    g_warning ("Failed to initialize HTTPS Everywhere context: %s", error->message);
    g_error_free (error);
  }

  ephy_uri_tester_https_everywhere_loaded (tester);
}
#endif

static void
adblock_file_monitor_changed (GFileMonitor     *monitor,
                              GFile            *file,
                              GFile            *other_file,
                              GFileMonitorEvent event_type,
                              EphyUriTester    *tester)
{
  if (event_type != G_FILE_MONITOR_EVENT_RENAMED)
    return;

  g_signal_handlers_disconnect_by_func (monitor, adblock_file_monitor_changed, tester);
  g_file_read_async (other_file, G_PRIORITY_DEFAULT_IDLE, NULL,
                     (GAsyncReadyCallback)file_read_cb,
                     tester);
}

static void
ephy_uri_tester_begin_loading_adblock_filters (EphyUriTester  *tester,
                                               GList         **monitors)
{
  char **filters;

  filters = g_settings_get_strv (EPHY_SETTINGS_MAIN, EPHY_PREFS_ADBLOCK_FILTERS);
  tester->adblock_filters_to_load = g_strv_length (filters);
  for (guint i = 0; filters[i]; i++) {
    GFile *filter_file;

    filter_file = ephy_uri_tester_get_adblock_filter_file (tester->adblock_data_dir, filters[i]);
    if (!g_file_query_exists (filter_file, NULL)) {
      GFileMonitor *monitor;
      GError *error = NULL;

      monitor = g_file_monitor_file (filter_file, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
      if (monitor) {
        *monitors = g_list_prepend (*monitors, monitor);
        g_signal_connect (monitor, "changed", G_CALLBACK (adblock_file_monitor_changed), tester);
      } else {
        g_warning ("Failed to monitor adblock file: %s\n", error->message);
        g_error_free (error);
        ephy_uri_tester_adblock_loaded (tester);
      }
    } else {
      g_file_read_async (filter_file, G_PRIORITY_DEFAULT_IDLE, NULL,
                         (GAsyncReadyCallback)file_read_cb,
                         tester);
    }
    g_object_unref (filter_file);
  }
  g_strfreev (filters);
}

static void
ephy_uri_tester_load_sync (GTask         *task,
                           EphyUriTester *tester)
{
  GMainContext *context;
  GList *monitors = NULL;

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);
  tester->load_loop = g_main_loop_new (context, FALSE);

#if ENABLE_HTTPS_EVERYWHERE
  if (!tester->https_everywhere_loaded) {
    g_assert (tester->https_everywhere_context == NULL);
    tester->https_everywhere_context = https_everywhere_context_new ();
    https_everywhere_context_init (tester->https_everywhere_context, NULL,
                                   (GAsyncReadyCallback)https_everywhere_context_init_cb,
                                   tester);
  }
#endif

  if (!tester->adblock_loaded)
    ephy_uri_tester_begin_loading_adblock_filters (tester, &monitors);

  g_main_loop_run (tester->load_loop);

  g_list_free_full (monitors, g_object_unref);
  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);
  g_main_loop_unref (tester->load_loop);

  g_task_return_boolean (task, TRUE);
}

static void
ephy_uri_tester_init (EphyUriTester *tester)
{
  LOG ("EphyUriTester initializing %p", tester);

  tester->pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           (GDestroyNotify)g_free,
                                           (GDestroyNotify)g_regex_unref);
  tester->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        (GDestroyNotify)g_free,
                                        (GDestroyNotify)g_regex_unref);
  tester->optslist = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            (GDestroyNotify)g_free,
                                            (GDestroyNotify)g_free);
  tester->urlcache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            (GDestroyNotify)g_free,
                                            NULL);

  tester->whitelisted_pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       (GDestroyNotify)g_free,
                                                       (GDestroyNotify)g_regex_unref);
  tester->whitelisted_keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                    (GDestroyNotify)g_free,
                                                    (GDestroyNotify)g_regex_unref);
  tester->whitelisted_optslist = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                        (GDestroyNotify)g_free,
                                                        (GDestroyNotify)g_free);
  tester->whitelisted_urlcache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                        (GDestroyNotify)g_free,
                                                        NULL);

  tester->blockcss = g_string_new ("z-non-exist");
  tester->blockcssprivate = g_string_new ("");

  tester->regex_third_party = g_regex_new (",third-party",
                                           G_REGEX_CASELESS | G_REGEX_OPTIMIZE,
                                           G_REGEX_MATCH_NOTEMPTY,
                                           NULL);
  tester->regex_pattern = g_regex_new ("^/.*[\\^\\$\\*].*/$",
                                       G_REGEX_UNGREEDY | G_REGEX_OPTIMIZE,
                                       G_REGEX_MATCH_NOTEMPTY,
                                       NULL);
  tester->regex_subdocument = g_regex_new ("subdocument",
                                           G_REGEX_CASELESS | G_REGEX_OPTIMIZE,
                                           G_REGEX_MATCH_NOTEMPTY,
                                           NULL);
  tester->regex_frame_add = g_regex_new (".*\\[.*:.*\\].*",
                                         G_REGEX_CASELESS | G_REGEX_OPTIMIZE,
                                         G_REGEX_MATCH_NOTEMPTY,
                                         NULL);
}

static void
ephy_uri_tester_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EphyUriTester *tester = EPHY_URI_TESTER (object);

  switch (prop_id) {
    case PROP_ADBLOCK_DATA_DIR:
      tester->adblock_data_dir = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_uri_tester_dispose (GObject *object)
{
#if ENABLE_HTTPS_EVERYWHERE
  EphyUriTester *tester = EPHY_URI_TESTER (object);
#endif

  LOG ("EphyUriTester disposing %p", object);

#if ENABLE_HTTPS_EVERYWHERE
  g_clear_object (&tester->https_everywhere_context);
#endif

  G_OBJECT_CLASS (ephy_uri_tester_parent_class)->dispose (object);
}

static void
ephy_uri_tester_finalize (GObject *object)
{
  EphyUriTester *tester = EPHY_URI_TESTER (object);

  LOG ("EphyUriTester finalizing %p", object);

  g_free (tester->adblock_data_dir);

  g_hash_table_destroy (tester->pattern);
  g_hash_table_destroy (tester->keys);
  g_hash_table_destroy (tester->optslist);
  g_hash_table_destroy (tester->urlcache);

  g_hash_table_destroy (tester->whitelisted_pattern);
  g_hash_table_destroy (tester->whitelisted_keys);
  g_hash_table_destroy (tester->whitelisted_optslist);
  g_hash_table_destroy (tester->whitelisted_urlcache);

  g_string_free (tester->blockcss, TRUE);
  g_string_free (tester->blockcssprivate, TRUE);

  g_regex_unref (tester->regex_third_party);
  g_regex_unref (tester->regex_pattern);
  g_regex_unref (tester->regex_subdocument);
  g_regex_unref (tester->regex_frame_add);

  G_OBJECT_CLASS (ephy_uri_tester_parent_class)->finalize (object);
}

static void
ephy_uri_tester_class_init (EphyUriTesterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ephy_uri_tester_set_property;
  object_class->dispose = ephy_uri_tester_dispose;
  object_class->finalize = ephy_uri_tester_finalize;

  obj_properties[PROP_ADBLOCK_DATA_DIR] =
    g_param_spec_string ("adblock-data-dir",
                         "Adblock data dir",
                         "The adblock data dir",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyUriTester *
ephy_uri_tester_new (const char *adblock_data_dir)
{
  return EPHY_URI_TESTER (g_object_new (EPHY_TYPE_URI_TESTER, "adblock-data-dir", adblock_data_dir, NULL));
}

static void
ephy_uri_tester_reload_adblock_filters (EphyUriTester *tester)
{
  g_hash_table_remove_all (tester->pattern);
  g_hash_table_remove_all (tester->keys);
  g_hash_table_remove_all (tester->optslist);
  g_hash_table_remove_all (tester->urlcache);

  g_hash_table_remove_all (tester->whitelisted_pattern);
  g_hash_table_remove_all (tester->whitelisted_keys);
  g_hash_table_remove_all (tester->whitelisted_optslist);
  g_hash_table_remove_all (tester->whitelisted_urlcache);

  tester->adblock_loaded = FALSE;
  ephy_uri_tester_load (tester);
}

static void
ephy_uri_tester_adblock_filters_changed_cb (GSettings     *settings,
                                            char          *key,
                                            EphyUriTester *tester)
{
  ephy_uri_tester_reload_adblock_filters (tester);
}

static void
ephy_uri_tester_enable_adblock_changed_cb (GSettings     *settings,
                                           char          *key,
                                           EphyUriTester *tester)
{
  ephy_uri_tester_reload_adblock_filters (tester);
}

void
ephy_uri_tester_load (EphyUriTester *tester)
{
  GTask *task;
  char **trash;

  g_assert (EPHY_IS_URI_TESTER (tester));

  if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_ADBLOCK))
    tester->adblock_loaded = TRUE;

  if (tester->adblock_loaded
#if ENABLE_HTTPS_EVERYWHERE
      && tester->https_everywhere_loaded
#endif
     )
    return;

  g_signal_handlers_disconnect_by_func (EPHY_SETTINGS_WEB, ephy_uri_tester_adblock_filters_changed_cb, tester);
  g_signal_handlers_disconnect_by_func (EPHY_SETTINGS_WEB, ephy_uri_tester_enable_adblock_changed_cb, tester);

  task = g_task_new (tester, NULL, NULL, NULL);
  g_task_run_in_thread_sync (task, (GTaskThreadFunc)ephy_uri_tester_load_sync);
  g_object_unref (task);

  g_signal_connect (EPHY_SETTINGS_MAIN, "changed::" EPHY_PREFS_ADBLOCK_FILTERS,
                    G_CALLBACK (ephy_uri_tester_adblock_filters_changed_cb), tester);
  g_signal_connect (EPHY_SETTINGS_WEB, "changed::" EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                    G_CALLBACK (ephy_uri_tester_enable_adblock_changed_cb), tester);
  /* GSettings never emits the changed signal until after we read the setting
   * the first time after connecting the handler... work around this.*/
  trash = g_settings_get_strv (EPHY_SETTINGS_MAIN, EPHY_PREFS_ADBLOCK_FILTERS);
  g_strfreev (trash);
}

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

#include "ephy-dbus-names.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-settings.h"
#include "ephy-uri-helpers.h"
#include "ephy-uri-tester-interface.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <httpseverywhere.h>
#include <libsoup/soup.h>
#include <string.h>

#define DEFAULT_FILTER_URL "https://easylist-downloads.adblockplus.org/easylist.txt"
#define FILTERS_LIST_FILENAME "filters.list"
#define SIGNATURE_SIZE 8
#define UPDATE_FREQUENCY 24 * 60 * 60 /* In seconds */

struct _EphyUriTester {
  GObject parent_instance;

  GSList *filters;
  char *data_dir;

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

  GCancellable *cancellable;

  HTTPSEverywhereContext *https_everywhere_context;
  GList *deferred_requests;

  GList *dbus_peers;
};

enum {
  PROP_0,
  PROP_FILTERS,
  PROP_BASE_DATA_DIR,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE (EphyUriTester, ephy_uri_tester, G_TYPE_OBJECT)

typedef struct {
  char *request_uri;
  char *page_uri;
  EphyUriTestFlags flags;
  GDBusMethodInvocation *invocation;
} DeferredRequest;

static DeferredRequest *
deferred_request_new (const char            *request_uri,
                      const char            *page_uri,
                      EphyUriTestFlags       flags,
                      GDBusMethodInvocation *invocation)
{
  DeferredRequest *request = g_slice_new (DeferredRequest);
  request->request_uri = g_strdup (request_uri);
  request->page_uri = g_strdup (page_uri);
  request->flags = flags;
  /* Ownership of invocation is passed to g_dbus_method_invocation_return_value(). */
  request->invocation = invocation;
  return request;
}

static void
deferred_request_free (DeferredRequest *request)
{
  g_free (request->request_uri);
  g_free (request->page_uri);
  g_slice_free (DeferredRequest, request);
}

static GString *
ephy_uri_tester_fixup_regexp (const char *prefix, char *src);

static void
ephy_uri_tester_parse_file_at_uri (EphyUriTester *tester, const char *fileuri);

static char *
ephy_uri_tester_ensure_data_dir (const char *base_data_dir)
{
  char *folder;

  /* Ensure adblock's dir is there. */
  folder = g_build_filename (base_data_dir, "adblock", NULL);
  g_mkdir_with_parents (folder, 0700);

  return folder;
}

static char *
ephy_uri_tester_get_fileuri_for_url (EphyUriTester *tester,
                                     const char    *url)
{
  char *filename = NULL;
  char *path = NULL;
  char *uri = NULL;

  if (!strncmp (url, "file", 4))
    return g_strndup (url + 7, strlen (url) - 7);

  filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);

  path = g_build_filename (tester->data_dir, filename, NULL);
  uri = g_filename_to_uri (path, NULL, NULL);

  g_free (filename);
  g_free (path);

  return uri;
}

typedef struct {
  EphyUriTester *tester;
  char *dest_uri;
} RetrieveFilterAsyncData;

static void
ephy_uri_tester_retrieve_filter_finished (GFile                   *src,
                                          GAsyncResult            *result,
                                          RetrieveFilterAsyncData *data)
{
  GError *error = NULL;

  if (!g_file_copy_finish (src, result, &error)) {
    LOG ("Error retrieving filter: %s\n", error->message);
    g_error_free (error);
  } else
    ephy_uri_tester_parse_file_at_uri (data->tester, data->dest_uri);

  g_object_unref (data->tester);
  g_free (data->dest_uri);
  g_slice_free (RetrieveFilterAsyncData, data);
}

static void
ephy_uri_tester_retrieve_filter (EphyUriTester *tester,
                                 const char    *url,
                                 const char    *fileuri)
{
  GFile *src;
  GFile *dest;
  RetrieveFilterAsyncData *data;

  g_return_if_fail (EPHY_IS_URI_TESTER (tester));
  g_return_if_fail (url != NULL);
  g_return_if_fail (fileuri != NULL);

  src = g_file_new_for_uri (url);
  dest = g_file_new_for_uri (fileuri);

  data = g_slice_new (RetrieveFilterAsyncData);
  data->tester = g_object_ref (tester);
  data->dest_uri = g_file_get_uri (dest);

  g_file_copy_async (src, dest,
                     G_FILE_COPY_OVERWRITE,
                     G_PRIORITY_DEFAULT,
                     NULL, NULL, NULL,
                     (GAsyncReadyCallback)ephy_uri_tester_retrieve_filter_finished,
                     data);

  g_object_unref (src);
  g_object_unref (dest);
}

static gboolean
ephy_uri_tester_filter_is_valid (const char *fileuri)
{
  GFile *file = NULL;
  GFileInfo *file_info = NULL;
  gboolean result;

  /* Now check if the local file is too old. */
  file = g_file_new_for_uri (fileuri);
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 NULL);
  result = FALSE;
  if (file_info) {
    GTimeVal current_time;
    GTimeVal mod_time;

    g_get_current_time (&current_time);
    g_file_info_get_modification_time (file_info, &mod_time);

    if (current_time.tv_sec > mod_time.tv_sec) {
      gint64 expire_time = mod_time.tv_sec + UPDATE_FREQUENCY;
      result = current_time.tv_sec < expire_time;
    }
    g_object_unref (file_info);
  }

  g_object_unref (file);

  return result;
}

static void
ephy_uri_tester_load_patterns (EphyUriTester *tester)
{
  GSList *filter = NULL;
  char *url = NULL;
  char *fileuri = NULL;

  /* Load patterns from the list of filters. */
  for (filter = tester->filters; filter; filter = g_slist_next (filter)) {
    url = (char *)filter->data;
    fileuri = ephy_uri_tester_get_fileuri_for_url (tester, url);

    if (!ephy_uri_tester_filter_is_valid (fileuri))
      ephy_uri_tester_retrieve_filter (tester, url, fileuri);
    else
      ephy_uri_tester_parse_file_at_uri (tester, fileuri);

    g_free (fileuri);
  }
}

static void
ephy_uri_tester_set_filters (EphyUriTester *tester, GSList *filters)
{
  if (tester->filters)
    g_slist_free_full (tester->filters, g_free);

  tester->filters = filters;
}

static void
ephy_uri_tester_load_filters (EphyUriTester *tester)
{
  GSList *list = NULL;
  char *filepath = NULL;

  filepath = g_build_filename (tester->data_dir, FILTERS_LIST_FILENAME, NULL);

  if (g_file_test (filepath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
    GFile *file = NULL;
    char *contents = NULL;
    gsize length = 0;
    GError *error = NULL;

    file = g_file_new_for_path (filepath);
    if (g_file_load_contents (file, NULL, &contents, &length, NULL, &error)) {
      char **urls_array = NULL;
      char *url = NULL;
      int i = 0;

      urls_array = g_strsplit (contents, ";", -1);
      for (i = 0; urls_array [i]; i++) {
        url = g_strstrip (g_strdup (urls_array[i]));
        if (!g_str_equal (url, ""))
          list = g_slist_prepend (list, url);
      }
      g_strfreev (urls_array);

      g_free (contents);
    }

    if (error) {
      LOG ("Error loading filters from %s: %s", filepath, error->message);
      g_error_free (error);
    }

    g_object_unref (file);
  } else {
    /* No file exists yet, so use the default filter and save it. */
    list = g_slist_prepend (list, g_strdup (DEFAULT_FILTER_URL));
  }

  g_free (filepath);

  ephy_uri_tester_set_filters (tester, g_slist_reverse (list));
}

#if 0
 TODO: Use this to create a filters dialog, or something.

static void
ephy_uri_tester_save_filters (EphyUriTester *tester)
{
  FILE *file = NULL;
  char *filepath = NULL;

  filepath = g_build_filename (tester->data_dir, FILTERS_LIST_FILENAME, NULL);

  if ((file = g_fopen (filepath, "w"))) {
    GSList *item = NULL;
    char *filter = NULL;

    for (item = tester->filters; item; item = g_slist_next (item)) {
      filter = g_strdup_printf ("%s;", (char *)item->data);
      fputs (filter, file);
      g_free (filter);
    }
    fclose (file);
  }
  g_free (filepath);
}
#endif

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
  int len = 0;

  if (!src)
    return NULL;

  str = g_string_new (prefix);

  /* lets strip first .* */
  if (src[0] == '*') {
    (void)*src++;
  }

  do {
    switch (*src) {
      case '*':
        g_string_append (str, ".*");
        break;
      /*case '.':
         g_string_append (str, "\\.");
         break;*/
      case '?':
      case '[':
      case ']':
        g_string_append_printf (str, "\\%c", *src);
        break;
      case '|':
      /* FIXME: We actually need to match :[0-9]+ or '/'. Sign means
         "here could be port number or nothing". So bla.com^ will match
         bla.com/ or bla.com:8080/ but not bla.com.au/ */
      case '^':
      case '+':
        break;
      default:
        g_string_append_printf (str, "%c", *src);
        break;
    }
    src++;
  } while (*src);

  len = str->len;
  /* We dont need .* in the end of url. Thats stupid */
  if (str->str && str->str[len - 1] == '*' && str->str[len - 2] == '.')
    g_string_erase (str, len - 2, 2);

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
file_parse_cb (GDataInputStream *stream, GAsyncResult *result, EphyUriTester *tester)
{
  char *line;
  GError *error = NULL;

  line = g_data_input_stream_read_line_finish (stream, result, NULL, &error);
  if (!line) {
    if (error) {
      LOG ("Error parsing file: %s\n", error->message);
      g_error_free (error);
    }

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
    LOG ("Error opening file %s for parsing: %s\n", path, error->message);
    g_free (path);
    g_error_free (error);

    return;
  }

  data_stream = g_data_input_stream_new (G_INPUT_STREAM (stream));
  g_object_unref (stream);

  g_data_input_stream_read_line_async (data_stream, G_PRIORITY_DEFAULT_IDLE, NULL,
                                       (GAsyncReadyCallback)file_parse_cb, tester);
  g_object_unref (data_stream);
}

static void
ephy_uri_tester_parse_file_at_uri (EphyUriTester *tester, const char *fileuri)
{
  GFile *file;

  file = g_file_new_for_uri (fileuri);
  g_file_read_async (file, G_PRIORITY_DEFAULT_IDLE, NULL, (GAsyncReadyCallback)file_read_cb, tester);
  g_object_unref (file);
}

static gboolean
ephy_uri_tester_test_uri (EphyUriTester *tester,
                          const char    *req_uri,
                          const char    *page_uri)
{
  /* Always load the main resource. */
  if (g_strcmp0 (req_uri, page_uri) == 0)
    return FALSE;

  /* Always load data requests, as uri_tester won't do any good here. */
  if (g_str_has_prefix (req_uri, SOUP_URI_SCHEME_DATA))
    return FALSE;

  /* check whitelisting rules before the normal ones */
  if (ephy_uri_tester_is_matched (tester, NULL, req_uri, page_uri, TRUE))
    return FALSE;
  return ephy_uri_tester_is_matched (tester, NULL, req_uri, page_uri, FALSE);
}

static char *
ephy_uri_tester_rewrite_uri (EphyUriTester    *tester,
                             const char       *request_uri,
                             const char       *page_uri,
                             EphyUriTestFlags  flags)
{
  char *modified_uri = NULL;
  char *result;

  /* Should we block the URL outright? */
  if ((flags & EPHY_URI_TEST_ADBLOCK) &&
      ephy_uri_tester_test_uri (tester, request_uri, page_uri)) {
    g_debug ("Request '%s' blocked (page: '%s')", request_uri, page_uri);
    return g_strdup ("");
  }

  if ((flags & EPHY_URI_TEST_TRACKING_QUERIES)) {
    /* Remove analytics from URL. Note that this function is a bit annoying to
     * use: it returns NULL if it doesn't remove any query parameters. */
    modified_uri = ephy_remove_tracking_from_uri (request_uri);
  }

  if (!modified_uri)
    modified_uri = g_strdup (request_uri);

  if ((flags & EPHY_URI_TEST_HTTPS_EVERYWHERE) &&
      g_str_has_prefix (request_uri, SOUP_URI_SCHEME_HTTP)) {
    result = https_everywhere_context_rewrite (tester->https_everywhere_context,
                                               modified_uri);
    g_free (modified_uri);
  } else {
    result = modified_uri;
  }

  return result;
}

typedef struct {
  EphyUriTester *tester;
  GDBusConnection *connection;
  guint registration_id;
} DBusPeerInfo;

static DBusPeerInfo *
dbus_peer_info_new (EphyUriTester   *tester,
                    GDBusConnection *connection,
                    guint            registration_id)
{
  DBusPeerInfo *peer = g_slice_new (DBusPeerInfo);
  peer->tester = tester;
  peer->connection = g_object_ref (connection);
  peer->registration_id = registration_id;
  return peer;
}

static void
dbus_peer_info_free (DBusPeerInfo *peer)
{
  g_signal_handlers_disconnect_by_data (peer->connection, peer);

  g_object_unref (peer->connection);
  g_slice_free (DBusPeerInfo, peer);
}

static void
dbus_connection_closed_cb (GDBusConnection *connection,
                           gboolean         remote_peer_vanished,
                           GError          *error,
                           DBusPeerInfo    *peer)
{
  peer->tester->dbus_peers = g_list_remove (peer->tester->dbus_peers, peer);
  dbus_peer_info_free (peer);
}

static void
ephy_uri_tester_return_response (EphyUriTester         *tester,
                                 const char            *request_uri,
                                 const char            *page_uri,
                                 EphyUriTestFlags       flags,
                                 GDBusMethodInvocation *invocation)
{
  char *rewritten_uri;
  rewritten_uri = ephy_uri_tester_rewrite_uri (tester, request_uri, page_uri, flags);
  g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(s)", rewritten_uri));
  g_free (rewritten_uri);
}

static void
handle_method_call (GDBusConnection       *connection,
                    const char            *sender,
                    const char            *object_path,
                    const char            *interface_name,
                    const char            *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  EphyUriTester *tester = EPHY_URI_TESTER (user_data);
  EphyUriTestFlags flags = 0;

  if (g_strcmp0 (interface_name, EPHY_URI_TESTER_INTERFACE) != 0)
    return;

  if (g_strcmp0 (method_name, "MaybeRewriteUri") == 0) {
    const char *request_uri;
    const char *page_uri;

    g_variant_get (parameters, "(&s&si)", &request_uri, &page_uri, &flags);

    if (request_uri == NULL || request_uri == '\0') {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "Request URI cannot be NULL or empty");
      return;
    }

    if (page_uri == NULL || page_uri == '\0') {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "Page URI cannot be NULL or empty");
      return;
    }

    if ((flags & EPHY_URI_TEST_HTTPS_EVERYWHERE) == 0 ||
        !g_str_has_prefix (request_uri, SOUP_URI_SCHEME_HTTP) ||
        https_everywhere_context_get_initialized (tester->https_everywhere_context)) {
      ephy_uri_tester_return_response (tester, request_uri, page_uri, flags, invocation);
    } else {
      DeferredRequest *request = deferred_request_new (request_uri, page_uri, flags, invocation);
      tester->deferred_requests = g_list_append (tester->deferred_requests, request);
    }
  }
}

static const GDBusInterfaceVTable interface_vtable = {
  handle_method_call,
  NULL,
  NULL
};

void
ephy_uri_tester_handle_new_dbus_connection (EphyUriTester   *tester,
                                            GDBusConnection *connection)
{
  static GDBusNodeInfo *introspection_data = NULL;
  DBusPeerInfo *peer;
  guint registration_id;
  GError *error = NULL;

  if (!introspection_data)
    introspection_data = g_dbus_node_info_new_for_xml (ephy_uri_tester_introspection_xml, NULL);

  registration_id =
    g_dbus_connection_register_object (connection,
                                       EPHY_URI_TESTER_OBJECT_PATH,
                                       introspection_data->interfaces[0],
                                       &interface_vtable,
                                       g_object_ref (tester),
                                       g_object_unref,
                                       &error);
  if (!registration_id) {
    g_warning ("Failed to register URI tester object: %s\n", error->message);
    g_error_free (error);
    return;
  }

  peer = dbus_peer_info_new (tester, connection, registration_id);
  tester->dbus_peers = g_list_append (tester->dbus_peers, peer);

  g_signal_connect (connection, "closed",
                    G_CALLBACK (dbus_connection_closed_cb), peer);
}

static void
handle_deferred_request (DeferredRequest *request,
                         EphyUriTester   *tester)
{
  ephy_uri_tester_return_response (tester,
                                   request->request_uri,
                                   request->page_uri,
                                   request->flags,
                                   request->invocation);
}

static void
https_everywhere_update_cb (HTTPSEverywhereUpdater *updater,
                            GAsyncResult *result)
{
  GError *error = NULL;

  https_everywhere_updater_update_finish (updater, result, &error);

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
        !g_error_matches (error, HTTPS_EVERYWHERE_UPDATE_ERROR, HTTPS_EVERYWHERE_UPDATE_ERROR_IN_PROGRESS) &&
        !g_error_matches (error, HTTPS_EVERYWHERE_UPDATE_ERROR, HTTPS_EVERYWHERE_UPDATE_ERROR_NO_UPDATE_AVAILABLE))
      g_warning ("Failed to update HTTPS Everywhere rulesets: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (updater);
}

static void
ephy_uri_tester_update_https_everywhere_rulesets (EphyUriTester *tester)
{
  HTTPSEverywhereUpdater *updater;
  EphyEmbedShell *shell;
  EphyEmbedShellMode mode;

  shell = ephy_embed_shell_get_default ();
  mode = ephy_embed_shell_get_mode (shell);

  if (mode == EPHY_EMBED_SHELL_MODE_TEST || mode == EPHY_EMBED_SHELL_MODE_SEARCH_PROVIDER)
    return;

  /* We might want to be smarter about this in the future. For now,
   * trigger an update of the rulesets once each time an EphyUriTester
   * is created. The new rulesets will get used the next time a new
   * EphyUriTester is created. Since EphyUriTester is only intended to
   * be created once, that means the new rulesets will be used the next
   * time Epiphany is restarted. */
  updater = https_everywhere_updater_new (tester->https_everywhere_context);
  https_everywhere_updater_update (updater,
                                   tester->cancellable,
                                   (GAsyncReadyCallback)https_everywhere_update_cb,
                                   NULL);
}

static void
https_everywhere_context_init_cb (HTTPSEverywhereContext *context,
                                  GAsyncResult           *res,
                                  EphyUriTester          *tester)
{
  GError *error = NULL;

  https_everywhere_context_init_finish (context, res, &error);

  /* Note that if this were not fatal, we would need some way to ensure
   * that future pending requests would not get stuck forever. */
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_error ("Failed to initialize HTTPS Everywhere context: %s", error->message);
  } else {
    g_list_foreach (tester->deferred_requests, (GFunc)handle_deferred_request, tester);
    ephy_uri_tester_update_https_everywhere_rulesets (tester);
  }

  g_list_free_full (tester->deferred_requests, (GDestroyNotify)deferred_request_free);
  g_object_unref (tester);
}

static void
ephy_uri_tester_init (EphyUriTester *tester)
{
  LOG ("EphyUriTester initializing %p", tester);

  tester->filters = NULL;
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
ephy_uri_tester_constructed (GObject *object)
{
  EphyUriTester *tester = EPHY_URI_TESTER (object);

  G_OBJECT_CLASS (ephy_uri_tester_parent_class)->constructed (object);

  tester->cancellable = g_cancellable_new ();

  tester->https_everywhere_context = https_everywhere_context_new ();
  https_everywhere_context_init (tester->https_everywhere_context,
                                 tester->cancellable,
                                 (GAsyncReadyCallback)https_everywhere_context_init_cb,
                                 g_object_ref (tester));

  ephy_uri_tester_load_filters (tester);
  ephy_uri_tester_load_patterns (tester);
}

static void
ephy_uri_tester_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EphyUriTester *tester = EPHY_URI_TESTER (object);

  switch (prop_id) {
    case PROP_FILTERS:
      ephy_uri_tester_set_filters (tester, (GSList *)g_value_get_pointer (value));
      break;
    case PROP_BASE_DATA_DIR:
      tester->data_dir = ephy_uri_tester_ensure_data_dir (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
unregister_dbus_object (DBusPeerInfo  *peer,
                        EphyUriTester *tester)
{
  g_dbus_connection_unregister_object (peer->connection, peer->registration_id);
}

static void
ephy_uri_tester_dispose (GObject *object)
{
  EphyUriTester *tester = EPHY_URI_TESTER (object);

  LOG ("EphyUriTester disposing %p", object);

  if (tester->cancellable) {
    g_cancellable_cancel (tester->cancellable);
    g_clear_object (&tester->cancellable);
  }

  if (tester->dbus_peers) {
    g_list_foreach (tester->dbus_peers, (GFunc)unregister_dbus_object, tester);
    g_list_free_full (tester->dbus_peers, (GDestroyNotify)dbus_peer_info_free);
    tester->dbus_peers = NULL;
  }

  G_OBJECT_CLASS (ephy_uri_tester_parent_class)->dispose (object);
}

static void
ephy_uri_tester_finalize (GObject *object)
{
  EphyUriTester *tester = EPHY_URI_TESTER (object);

  LOG ("EphyUriTester finalizing %p", object);

  g_slist_free_full (tester->filters, g_free);
  g_free (tester->data_dir);

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
  object_class->constructed = ephy_uri_tester_constructed;
  object_class->dispose = ephy_uri_tester_dispose;
  object_class->finalize = ephy_uri_tester_finalize;

  obj_properties[PROP_FILTERS] =
    g_param_spec_pointer ("filters",
                          "filters",
                          "filters",
                          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_BASE_DATA_DIR] =
    g_param_spec_string ("base-data-dir",
                         "Base data dir",
                         "The base dir where to create the adblock data dir",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyUriTester *
ephy_uri_tester_new (void)
{
  EphyEmbedShell *shell;
  EphyEmbedShellMode mode;
  EphyUriTester *tester;
  char *data_dir;

  shell = ephy_embed_shell_get_default ();
  mode = ephy_embed_shell_get_mode (shell);

  /* The filters list is large, so we don't want to store a separate copy per
   * web app, but users should otherwise be able to configure different filters
   * per profile directory. */
  data_dir = mode == EPHY_EMBED_SHELL_MODE_APPLICATION ? ephy_default_dot_dir ()
                                                       : g_strdup (ephy_dot_dir ());

  tester = g_object_new (EPHY_TYPE_URI_TESTER, "base-data-dir", data_dir, NULL);
  g_free (data_dir);
  return tester;
}

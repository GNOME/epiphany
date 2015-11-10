/*
 *  Copyright © 2011 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Some parts of this file based on the Midori's 'adblock' extension,
 *  licensed with the GNU Lesser General Public License 2.1, Copyright
 *  (C) 2009-2010 Christian Dywan <christian@twotoasts.de> and 2009
 *  Alexander Butenko <a.butenka@gmail.com>. Check Midori's web site
 *  at http://www.twotoasts.de
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "ephy-uri-tester.h"

#include "ephy-debug.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

#define DEFAULT_FILTER_URL "https://easylist-downloads.adblockplus.org/easylist.txt"
#define FILTERS_LIST_FILENAME "filters.list"
#define SIGNATURE_SIZE 8
#define UPDATE_FREQUENCY 24 * 60 * 60 /* In seconds */

struct _EphyUriTester
{
  GObject parent_instance;

  GSList *filters;
  char *data_dir;

  GHashTable *pattern;
  GHashTable *optslist;
  GHashTable *urlcache;

  GHashTable *whitelisted_pattern;
  GHashTable *whitelisted_optslist;
  GHashTable *whitelisted_urlcache;

  GString *blockcss;
  GString *blockcssprivate;

  GRegex *regex_third_party;
  GRegex *regex_pattern;
  GRegex *regex_subdocument;
  GRegex *regex_frame_add;
};

enum
{
  PROP_0,
  PROP_FILTERS,
  PROP_BASE_DATA_DIR,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_TYPE (EphyUriTester, ephy_uri_tester, G_TYPE_OBJECT)

/* Private functions. */

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

static char*
ephy_uri_tester_get_fileuri_for_url (EphyUriTester *tester,
                                     const char *url)
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
ephy_uri_tester_retrieve_filter_finished (GFile *src,
                                          GAsyncResult *result,
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
                                 const char *url,
                                 const char *fileuri)
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
  if (file_info)
    {
      GTimeVal current_time;
      GTimeVal mod_time;

      g_get_current_time (&current_time);
      g_file_info_get_modification_time (file_info, &mod_time);

      if (current_time.tv_sec > mod_time.tv_sec)
        {
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
  for (filter = tester->filters; filter; filter = g_slist_next(filter))
    {
      url = (char*)filter->data;
      fileuri = ephy_uri_tester_get_fileuri_for_url (tester, url);

      if (!ephy_uri_tester_filter_is_valid (fileuri))
        ephy_uri_tester_retrieve_filter (tester, url, fileuri);
      else
        ephy_uri_tester_parse_file_at_uri (tester, fileuri);

      g_free (fileuri);
    }
}

static void
ephy_uri_tester_load_filters (EphyUriTester *tester)
{
  GSList *list = NULL;
  char *filepath = NULL;

  filepath = g_build_filename (tester->data_dir, FILTERS_LIST_FILENAME, NULL);

  if (g_file_test (filepath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
    {
      GFile *file = NULL;
      char *contents = NULL;
      gsize length = 0;
      GError *error = NULL;

      file = g_file_new_for_path (filepath);
      if (g_file_load_contents (file, NULL, &contents, &length, NULL, &error))
        {
          char **urls_array = NULL;
          char *url = NULL;
          int i = 0;

          urls_array = g_strsplit (contents, ";", -1);
          for (i = 0; urls_array [i]; i++)
            {
              url = g_strstrip (g_strdup (urls_array[i]));
              if (!g_str_equal (url, ""))
                list = g_slist_prepend (list, url);
            }
          g_strfreev (urls_array);

          g_free (contents);
        }

      if (error)
        {
          LOG ("Error loading filters from %s: %s", filepath, error->message);
          g_error_free (error);
        }

      g_object_unref (file);
    }
  else
    {
      /* No file exists yet, so use the default filter and save it. */
      list = g_slist_prepend (list, g_strdup (DEFAULT_FILTER_URL));
    }

  g_free (filepath);

  ephy_uri_tester_set_filters (tester, g_slist_reverse(list));
}

#if 0
TODO: Use this to create a filters dialog, or something.

static void
ephy_uri_tester_save_filters (EphyUriTester *tester)
{
  FILE *file = NULL;
  char *filepath = NULL;

  filepath = g_build_filename (tester->data_dir, FILTERS_LIST_FILENAME, NULL);

  if ((file = g_fopen (filepath, "w")))
    {
      GSList *item = NULL;
      char *filter = NULL;

      for (item = tester->filters; item; item = g_slist_next (item))
        {
          filter = g_strdup_printf ("%s;", (char*)item->data);
          fputs (filter, file);
          g_free (filter);
        }
      fclose (file);
    }
  g_free (filepath);
}
#endif

static inline int
ephy_uri_tester_check_rule (EphyUriTester  *tester,
                            GRegex     *regex,
                            const char *patt,
                            const char *req_uri,
                            const char *page_uri,
                            gboolean    whitelist)
{
  char *opts;
  GHashTable *optslist = tester->optslist;
  if (whitelist)
    optslist = tester->whitelisted_optslist;

  if (!g_regex_match_full (regex, req_uri, -1, 0, 0, NULL, NULL))
    return FALSE;

  opts = g_hash_table_lookup (optslist, patt);
  if (opts && g_regex_match (tester->regex_third_party, opts, 0, NULL))
    {
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
ephy_uri_tester_is_matched_by_pattern (EphyUriTester  *tester,
                                  const char *req_uri,
                                  const char *page_uri,
                                  gboolean    whitelist)
{
  GHashTableIter iter;
  gpointer patt, regex;
  GHashTable *pattern = tester->pattern;
  if (whitelist)
    pattern = tester->whitelisted_pattern;

  g_hash_table_iter_init (&iter, pattern);
  while (g_hash_table_iter_next (&iter, &patt, &regex))
    {
      if (ephy_uri_tester_check_rule (tester, regex, patt, req_uri, page_uri, whitelist))
        return TRUE;
    }
  return FALSE;
}

static gboolean
ephy_uri_tester_is_matched (EphyUriTester  *tester,
                            const char *opts,
                            const char *req_uri,
                            const char *page_uri,
                            gboolean    whitelist)
{
  char *value;
  GHashTable *urlcache = tester->urlcache;
  if (whitelist)
    urlcache = tester->whitelisted_urlcache;

  /* Check cached URLs first. */
  if ((value = g_hash_table_lookup (urlcache, req_uri)))
    return (value[0] != '0') ? TRUE : FALSE;

  /* Matching by pattern is pretty expensive, so do it if needed only. */
  if (ephy_uri_tester_is_matched_by_pattern (tester, req_uri, page_uri, whitelist))
    {
      g_hash_table_insert (urlcache, g_strdup (req_uri), g_strdup ("1"));
      return TRUE;
    }

  g_hash_table_insert (urlcache, g_strdup (req_uri), g_strdup ("0"));
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
  if (src[0] == '*')
    {
      (void)*src++;
    }

  do
    {
      switch (*src)
        {
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
          g_string_append_printf (str,"%c", *src);
          break;
        }
      src++;
    }
  while (*src);

  len = str->len;
  /* We dont need .* in the end of url. Thats stupid */
  if (str->str && str->str[len-1] == '*' && str->str[len-2] == '.')
    g_string_erase (str, len-2, 2);

  return str;
}

static void
ephy_uri_tester_compile_regexp (EphyUriTester *tester,
                                GString   *gpatt,
                                char      *opts,
                                gboolean   whitelist)
{
  GHashTable *pattern;
  GHashTable *optslist;
  GRegex *regex;
  GError *error = NULL;
  char *patt;

  if (!gpatt)
    return;

  patt = gpatt->str;

  /* TODO: Play with optimization flags */
  regex = g_regex_new (patt, G_REGEX_OPTIMIZE | G_REGEX_JAVASCRIPT_COMPAT,
                       G_REGEX_MATCH_NOTEMPTY, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRFUNC, error->message);
      g_error_free (error);
      return;
    }

  pattern = tester->pattern;
  optslist = tester->optslist;
  if (whitelist)
    {
      pattern = tester->whitelisted_pattern;
      optslist = tester->whitelisted_optslist;
    }

  LOG ("patt: %s", patt);
  g_hash_table_insert (pattern, g_strdup (patt), regex);
  g_hash_table_insert (optslist, g_strdup (patt), g_strdup (opts));
}

static void
ephy_uri_tester_add_url_pattern (EphyUriTester *tester,
                                 char      *prefix,
                                 char      *type,
                                 char      *line,
                                 gboolean   whitelist)
{
    char **data;
    char *patt;
    GString *format_patt;
    char *opts;

    data = g_strsplit (line, "$", -1);
    if (!data || !data[0])
    {
        g_strfreev (data);
        return;
    }

    if (data[1] && data[2])
    {
        patt = g_strconcat (data[0], data[1], NULL);
        opts = g_strconcat (type, ",", data[2], NULL);
    }
    else if (data[1])
    {
        patt = data[0];
        opts = g_strconcat (type, ",", data[1], NULL);
    }
    else
    {
        patt = data[0];
        opts = type;
    }

    if (g_regex_match (tester->regex_subdocument, opts, 0, NULL))
    {
        if (data[1] && data[2])
            g_free (patt);
        if (data[1])
            g_free (opts);
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
        g_free (opts);
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
          && !g_regex_match (tester->regex_frame_add, line, 0, NULL)))
    {
      return;
    }
  g_string_append (tester->blockcss, separator);
  g_string_append (tester->blockcss, line);
}

static inline void
ephy_uri_tester_frame_add_private (EphyUriTester  *tester,
                                   const char *line,
                                   const char *sep)
{
  char **data;
  data = g_strsplit (line, sep, 2);

  if (!(data[1] && *data[1])
      ||  strchr (data[1], '\'')
      || (strchr (data[1], ':')
          && !g_regex_match (tester->regex_frame_add, data[1], 0, NULL)))
    {
      g_strfreev (data);
      return;
    }

  if (strchr (data[0], ','))
    {
      char **domains;
      int i;

      domains = g_strsplit (data[0], ",", -1);
      for (i = 0; domains[i]; i++)
        {
          g_string_append_printf (tester->blockcssprivate, ";sites['%s']+=',%s'",
                                  g_strstrip (domains[i]), data[1]);
        }
      g_strfreev (domains);
    }
  else
    {
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
  if (g_str_has_prefix (line, "@@"))
    {
      ephy_uri_tester_parse_line (tester, line+2, TRUE);
      return;
    }

  /* FIXME: No support for domain= */
  if (strstr (line, "domain="))
    return;

  /* Skip garbage */
  if (line[0] == ' ' || !line[0])
    return;

  /* Got CSS block hider */
  if (line[0] == '#' && line[1] == '#' )
    {
      ephy_uri_tester_frame_add (tester, line);
      return;
    }
  /* Got CSS block hider. Workaround */
  if (line[0] == '#')
    return;

  /* Got per domain CSS hider rule */
  if (strstr (line, "##"))
    {
      ephy_uri_tester_frame_add_private (tester, line, "##");
      return;
    }

  /* Got per domain CSS hider rule. Workaround */
  if (strchr (line, '#'))
    {
      ephy_uri_tester_frame_add_private (tester, line, "#");
      return;
    }
  /* Got URL blocker rule */
  if (line[0] == '|' && line[1] == '|' )
    {
      (void)*line++;
      (void)*line++;
      /* set a regex prefix to ensure that '||' patterns are anchored at the
       * start and that any characters (if any) preceding the domain specified
       * by the rule is separated from it by a dot '.'  */
      ephy_uri_tester_add_url_pattern (tester, "^[\\w\\-]+:\\/+(?!\\/)(?:[^\\/]+\\.)?", "fulluri", line, whitelist);
      return;
    }
  if (line[0] == '|')
    {
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

static void
ephy_uri_tester_init (EphyUriTester *tester)
{
  LOG ("EphyUriTester initializing %p", tester);

  tester->filters = NULL;
  tester->pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           (GDestroyNotify)g_free,
                                           (GDestroyNotify)g_regex_unref);
  tester->optslist = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            (GDestroyNotify)g_free,
                                            (GDestroyNotify)g_free);
  tester->urlcache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            (GDestroyNotify)g_free,
                                            (GDestroyNotify)g_free);

  tester->whitelisted_pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       (GDestroyNotify)g_free,
                                                       (GDestroyNotify)g_regex_unref);
  tester->whitelisted_optslist = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                        (GDestroyNotify)g_free,
                                                        (GDestroyNotify)g_free);
  tester->whitelisted_urlcache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                        (GDestroyNotify)g_free,
                                                        (GDestroyNotify)g_free);

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

  ephy_uri_tester_load_filters (tester);
  ephy_uri_tester_load_patterns (tester);
}

static void
ephy_uri_tester_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  EphyUriTester *tester = EPHY_URI_TESTER (object);

  switch (prop_id)
    {
    case PROP_FILTERS:
      ephy_uri_tester_set_filters (tester, (GSList*) g_value_get_pointer (value));
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
ephy_uri_tester_finalize (GObject *object)
{
  EphyUriTester *tester = EPHY_URI_TESTER (object);

  LOG ("EphyUriTester finalizing %p", object);

  g_slist_free_full (tester->filters, g_free);
  g_free (tester->data_dir);

  g_hash_table_destroy (tester->pattern);
  g_hash_table_destroy (tester->optslist);
  g_hash_table_destroy (tester->urlcache);

  g_hash_table_destroy (tester->whitelisted_pattern);
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
  object_class->finalize = ephy_uri_tester_finalize;

  obj_properties[PROP_FILTERS] =
    g_param_spec_pointer ("filters",
                          "filters",
                          "filters",
                          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS );

  obj_properties[PROP_BASE_DATA_DIR] =
    g_param_spec_string ("base-data-dir",
                         "Base data dir",
                         "The base dir where to create the adblock data dir",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

EphyUriTester *
ephy_uri_tester_new (const char *base_data_dir)
{
  g_return_val_if_fail (base_data_dir != NULL, NULL);

  return g_object_new (EPHY_TYPE_URI_TESTER, "base-data-dir", base_data_dir, NULL);
}

gboolean
ephy_uri_tester_test_uri (EphyUriTester *tester,
                          const char *req_uri,
                          const char *page_uri)
{
  /* check whitelisting rules before the normal ones */
  if (ephy_uri_tester_is_matched (tester, NULL, req_uri, page_uri, TRUE))
    return FALSE;
  return ephy_uri_tester_is_matched (tester, NULL, req_uri, page_uri, FALSE);
}

void
ephy_uri_tester_set_filters (EphyUriTester *tester, GSList *filters)
{
  if (tester->filters)
    g_slist_free_full (tester->filters, g_free);

  tester->filters = filters;
}

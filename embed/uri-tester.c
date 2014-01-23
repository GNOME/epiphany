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
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "uri-tester.h"

#include "ephy-debug.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

#define DEFAULT_FILTER_URL "https://easylist-downloads.adblockplus.org/easylist.txt"
#define FILTERS_LIST_FILENAME "filters.list"
#define SIGNATURE_SIZE 8
#define UPDATE_FREQUENCY 24 * 60 * 60 /* In seconds */

#define URI_TESTER_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), TYPE_URI_TESTER, UriTesterPrivate))

struct _UriTesterPrivate
{
  GSList *filters;
  char *data_dir;

  GHashTable *pattern;
  GHashTable *keys;
  GHashTable *optslist;
  GHashTable *urlcache;

  GString *blockcss;
  GString *blockcssprivate;
};

enum
{
  PROP_0,
  PROP_FILTERS,
  PROP_BASE_DATA_DIR,
};

G_DEFINE_TYPE (UriTester, uri_tester, G_TYPE_OBJECT)

/* Private functions. */

static GString *
uri_tester_fixup_regexp (const char *prefix, char *src);

static gboolean
uri_tester_parse_file_at_uri (UriTester *tester, const char *fileuri);

static char *
uri_tester_ensure_data_dir (const char *base_data_dir)
{
  char *folder;

  /* Ensure adblock's dir is there. */
  folder = g_build_filename (base_data_dir, "adblock", NULL);
  g_mkdir_with_parents (folder, 0700);

  return folder;
}

static char*
uri_tester_get_fileuri_for_url (UriTester *tester,
                                const char *url)
{
  char *filename = NULL;
  char *path = NULL;
  char *uri = NULL;

  if (!strncmp (url, "file", 4))
    return g_strndup (url + 7, strlen (url) - 7);

  filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);

  path = g_build_filename (tester->priv->data_dir, filename, NULL);
  uri = g_filename_to_uri (path, NULL, NULL);

  g_free (filename);
  g_free (path);

  return uri;
}

typedef struct {
  UriTester *tester;
  char *dest_uri;
} RetrieveFilterAsyncData;

static void
uri_tester_retrieve_filter_finished (GFile *src,
                                     GAsyncResult *result,
                                     RetrieveFilterAsyncData *data)
{
  GError *error = NULL;

  if (!g_file_copy_finish (src, result, &error)) {
    LOG ("Error retrieving filter: %s\n", error->message);
    g_error_free (error);
  } else
    uri_tester_parse_file_at_uri (data->tester, data->dest_uri);

  g_object_unref (data->tester);
  g_free (data->dest_uri);
  g_slice_free (RetrieveFilterAsyncData, data);
}

static void
uri_tester_retrieve_filter (UriTester *tester, const char *url, const char *fileuri)
{
  GFile *src;
  GFile *dest;
  RetrieveFilterAsyncData *data;

  g_return_if_fail (IS_URI_TESTER (tester));
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
                     (GAsyncReadyCallback)uri_tester_retrieve_filter_finished,
                     data);

  g_object_unref (src);
  g_object_unref (dest);
}

static gboolean
uri_tester_filter_is_valid (const char *fileuri)
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
uri_tester_load_patterns (UriTester *tester)
{
  GSList *filter = NULL;
  char *url = NULL;
  char *fileuri = NULL;

  /* Load patterns from the list of filters. */
  for (filter = tester->priv->filters; filter; filter = g_slist_next(filter))
    {
      url = (char*)filter->data;
      fileuri = uri_tester_get_fileuri_for_url (tester, url);

      if (!uri_tester_filter_is_valid (fileuri))
        uri_tester_retrieve_filter (tester, url, fileuri);
      else
        uri_tester_parse_file_at_uri (tester, fileuri);

      g_free (fileuri);
    }
}

static void
uri_tester_load_filters (UriTester *tester)
{
  GSList *list = NULL;
  char *filepath = NULL;

  filepath = g_build_filename (tester->priv->data_dir, FILTERS_LIST_FILENAME, NULL);

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

  uri_tester_set_filters (tester, g_slist_reverse(list));
}

static void
uri_tester_save_filters (UriTester *tester)
{
  FILE *file = NULL;
  char *filepath = NULL;

  filepath = g_build_filename (tester->priv->data_dir, FILTERS_LIST_FILENAME, NULL);

  if ((file = g_fopen (filepath, "w")))
    {
      GSList *item = NULL;
      char *filter = NULL;

      for (item = tester->priv->filters; item; item = g_slist_next (item))
        {
          filter = g_strdup_printf ("%s;", (char*)item->data);
          fputs (filter, file);
          g_free (filter);
        }
      fclose (file);
    }
  g_free (filepath);
}

static inline int
uri_tester_check_rule (UriTester  *tester,
                       GRegex     *regex,
                       const char *patt,
                       const char *req_uri,
                       const char *page_uri)
{
  char *opts;

  if (!g_regex_match_full (regex, req_uri, -1, 0, 0, NULL, NULL))
    return FALSE;

  opts = g_hash_table_lookup (tester->priv->optslist, patt);
  if (opts && g_regex_match_simple (",third-party", opts,
                                    G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
    {
      if (page_uri && g_regex_match_full (regex, page_uri, -1, 0, 0, NULL, NULL))
        return FALSE;
    }
  /* TODO: Domain opt check */
  LOG ("blocked by pattern regexp=%s -- %s", g_regex_get_pattern (regex), req_uri);
  return TRUE;
}

static inline gboolean
uri_tester_is_matched_by_pattern (UriTester  *tester,
                                  const char *req_uri,
                                  const char *page_uri)
{
  GHashTableIter iter;
  gpointer patt, regex;

  g_hash_table_iter_init (&iter, tester->priv->pattern);
  while (g_hash_table_iter_next (&iter, &patt, &regex))
    {
      if (uri_tester_check_rule(tester, regex, patt, req_uri, page_uri))
        return TRUE;
    }
  return FALSE;
}

static inline gboolean
uri_tester_is_matched_by_key (UriTester  *tester,
                              const char *opts,
                              const char *req_uri,
                              const char *page_uri)
{
  UriTesterPrivate *priv = NULL;
  char *uri;
  int len;
  int pos = 0;
  GList *regex_bl = NULL;
  GString *guri;
  gboolean ret = FALSE;
  char sig[SIGNATURE_SIZE + 1];

  priv = tester->priv;

  memset (&sig[0], 0, sizeof (sig));
  /* Signatures are made on pattern, so we need to convert url to a pattern as well */
  guri = uri_tester_fixup_regexp ("", (char*)req_uri);
  uri = guri->str;
  len = guri->len;

  for (pos = len - SIGNATURE_SIZE; pos >= 0; pos--)
    {
      GRegex *regex;
      strncpy (sig, uri + pos, SIGNATURE_SIZE);
      regex = g_hash_table_lookup (priv->keys, sig);

      /* Dont check if regex is already blacklisted */
      if (!regex || g_list_find (regex_bl, regex))
        continue;
      ret = uri_tester_check_rule (tester, regex, sig, req_uri, page_uri);
      if (ret)
        break;
      regex_bl = g_list_prepend (regex_bl, regex);
    }
  g_string_free (guri, TRUE);
  g_list_free (regex_bl);
  return ret;
}

static gboolean
uri_tester_is_matched (UriTester  *tester,
                       const char *opts,
                       const char *req_uri,
                       const char *page_uri)
{
  UriTesterPrivate *priv = NULL;
  char *value;

  priv = tester->priv;

  /* Check cached URLs first. */
  if ((value = g_hash_table_lookup (priv->urlcache, req_uri)))
    return (value[0] != '0') ? TRUE : FALSE;

  /* Look for a match either by key or by pattern. */
  if (uri_tester_is_matched_by_key (tester, opts, req_uri, page_uri))
    {
      g_hash_table_insert (priv->urlcache, g_strdup (req_uri), g_strdup("1"));
      return TRUE;
    }

  /* Matching by pattern is pretty expensive, so do it if needed only. */
  if (uri_tester_is_matched_by_pattern (tester, req_uri, page_uri))
    {
      g_hash_table_insert (priv->urlcache, g_strdup (req_uri), g_strdup("1"));
      return TRUE;
    }

  g_hash_table_insert (priv->urlcache, g_strdup (req_uri), g_strdup("0"));
  return FALSE;
}

static GString *
uri_tester_fixup_regexp (const char *prefix, char *src)
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
          g_string_append (str, "\\?");
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

static gboolean
uri_tester_compile_regexp (UriTester *tester,
                           GString   *gpatt,
                           char      *opts)
{
  GRegex *regex;
  GError *error = NULL;
  char *patt;
  int len;

  if (!gpatt)
    return FALSE;

  patt = gpatt->str;
  len = gpatt->len;

  /* TODO: Play with optimization flags */
  regex = g_regex_new (patt, G_REGEX_OPTIMIZE | G_REGEX_JAVASCRIPT_COMPAT,
                       G_REGEX_MATCH_NOTEMPTY, &error);
  if (error)
    {
      g_warning ("%s: %s", G_STRFUNC, error->message);
      g_error_free (error);
      return TRUE;
    }

  if (!g_regex_match_simple ("^/.*[\\^\\$\\*].*/$", patt, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY))
    {
      int signature_count = 0;
      int pos = 0;
      char *sig;

      for (pos = len - SIGNATURE_SIZE; pos >= 0; pos--) {
        sig = g_strndup (patt + pos, SIGNATURE_SIZE);
        if (!g_regex_match_simple ("[\\*]", sig, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY) &&
            !g_hash_table_lookup (tester->priv->keys, sig))
          {
            LOG ("sig: %s %s", sig, patt);
            g_hash_table_insert (tester->priv->keys, g_strdup (sig), g_regex_ref (regex));
            g_hash_table_insert (tester->priv->optslist, g_strdup (sig), g_strdup (opts));
            signature_count++;
          }
        else
          {
            if (g_regex_match_simple ("^\\*", sig, G_REGEX_UNGREEDY, G_REGEX_MATCH_NOTEMPTY) &&
                !g_hash_table_lookup (tester->priv->pattern, patt))
              {
                LOG ("patt2: %s %s", sig, patt);
                g_hash_table_insert (tester->priv->pattern, g_strdup (patt), g_regex_ref (regex));
                g_hash_table_insert (tester->priv->optslist, g_strdup (patt), g_strdup (opts));
              }
          }
        g_free (sig);
      }
      g_regex_unref (regex);

      if (signature_count > 1 && g_hash_table_lookup (tester->priv->pattern, patt))
        {
          g_hash_table_steal (tester->priv->pattern, patt);
          return TRUE;
        }

      return FALSE;
    }
  else
    {
      LOG ("patt: %s%s", patt, "");
      /* Pattern is a regexp chars */
      g_hash_table_insert (tester->priv->pattern, g_strdup (patt), regex);
      g_hash_table_insert (tester->priv->optslist, g_strdup (patt), g_strdup (opts));
      return FALSE;
    }
}

static char*
uri_tester_add_url_pattern (UriTester *tester,
                            char      *prefix,
                            char      *type,
                            char      *line)
{
    char **data;
    char *patt;
    GString *format_patt;
    char *opts;
    gboolean should_free;

    data = g_strsplit (line, "$", -1);
    if (!data || !data[0])
    {
        g_strfreev (data);
        return NULL;
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

    if (g_regex_match_simple ("subdocument", opts,
                              G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY))
    {
        if (data[1] && data[2])
            g_free (patt);
        if (data[1])
            g_free (opts);
        g_strfreev (data);
        return NULL;
    }

    format_patt = uri_tester_fixup_regexp (prefix, patt);

    LOG ("got: %s opts %s", format_patt->str, opts);
    should_free = uri_tester_compile_regexp (tester, format_patt, opts);

    if (data[1] && data[2])
        g_free (patt);
    if (data[1])
        g_free (opts);
    g_strfreev (data);

    return g_string_free (format_patt, should_free);
}

static inline void
uri_tester_frame_add (UriTester *tester, char *line)
{
  const char *separator = " , ";

  (void)*line++;
  (void)*line++;
  if (strchr (line, '\'')
      || (strchr (line, ':')
          && !g_regex_match_simple (".*\\[.*:.*\\].*", line,
                                    G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY)))
    {
      return;
    }
  g_string_append (tester->priv->blockcss, separator);
  g_string_append (tester->priv->blockcss, line);
}

static inline void
uri_tester_frame_add_private (UriTester  *tester,
                              const char *line,
                              const char *sep)
{
  char **data;
  data = g_strsplit (line, sep, 2);

  if (!(data[1] && *data[1])
      ||  strchr (data[1], '\'')
      || (strchr (data[1], ':')
          && !g_regex_match_simple (".*\\[.*:.*\\].*", data[1],
                                    G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY)))
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
          g_string_append_printf (tester->priv->blockcssprivate, ";sites['%s']+=',%s'",
                                  g_strstrip (domains[i]), data[1]);
        }
      g_strfreev (domains);
    }
  else
    {
      g_string_append_printf (tester->priv->blockcssprivate, ";sites['%s']+=',%s'",
                              data[0], data[1]);
    }
  g_strfreev (data);
}

static char*
uri_tester_parse_line (UriTester *tester, char *line)
{
  if (!line)
    return NULL;
  g_strchomp (line);
  /* Ignore comments and new lines */
  if (line[0] == '!')
    return NULL;
  /* FIXME: No support for whitelisting */
  if (line[0] == '@' && line[1] == '@')
    return NULL;
  /* FIXME: No support for [include] and [exclude] tags */
  if (line[0] == '[')
    return NULL;
  /* FIXME: No support for domain= */
  if (strstr (line, "domain="))
    return NULL;

  /* Skip garbage */
  if (line[0] == ' ' || !line[0])
    return NULL;

  /* Got CSS block hider */
  if (line[0] == '#' && line[1] == '#' )
    {
      uri_tester_frame_add (tester, line);
      return NULL;
    }
  /* Got CSS block hider. Workaround */
  if (line[0] == '#')
    return NULL;

  /* Got per domain CSS hider rule */
  if (strstr (line, "##"))
    {
      uri_tester_frame_add_private (tester, line, "##");
      return NULL;
    }

  /* Got per domain CSS hider rule. Workaround */
  if (strchr (line, '#'))
    {
      uri_tester_frame_add_private (tester, line, "#");
      return NULL;
    }
  /* Got URL blocker rule */
  if (line[0] == '|' && line[1] == '|' )
    {
      (void)*line++;
      (void)*line++;
      return uri_tester_add_url_pattern (tester, "", "fulluri", line);
    }
  if (line[0] == '|')
    {
      (void)*line++;
      return uri_tester_add_url_pattern (tester, "^", "fulluri", line);
    }
  return uri_tester_add_url_pattern (tester, "", "uri", line);
}

static gboolean
uri_tester_parse_file_at_uri (UriTester *tester, const char *fileuri)
{
  FILE *file;
  char line[2000];
  char *path = NULL;
  gboolean result = FALSE;

  path = g_filename_from_uri (fileuri, NULL, NULL);
  if ((file = g_fopen (path, "r")))
    {
      while (fgets (line, 2000, file))
        g_free (uri_tester_parse_line (tester, line));
      fclose (file);

      result = TRUE;
    }
  g_free (path);

  return result;
}

static void
uri_tester_init (UriTester *tester)
{
  UriTesterPrivate *priv = NULL;

  LOG ("UriTester initializing %p", tester);

  priv = URI_TESTER_GET_PRIVATE (tester);
  tester->priv = priv;

  priv->filters = NULL;
  priv->pattern = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         (GDestroyNotify)g_free,
                                         (GDestroyNotify)g_regex_unref);
  priv->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
                                      (GDestroyNotify)g_free,
                                      (GDestroyNotify)g_regex_unref);
  priv->optslist = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          NULL,
                                          (GDestroyNotify)g_free);
  priv->urlcache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          (GDestroyNotify)g_free,
                                          (GDestroyNotify)g_free);

  priv->blockcss = g_string_new ("z-non-exist");
  priv->blockcssprivate = g_string_new ("");
}

static void
uri_tester_constructed (GObject *object)
{
  UriTester *tester = URI_TESTER (object);

  G_OBJECT_CLASS (uri_tester_parent_class)->constructed (object);

  uri_tester_load_filters (tester);
  uri_tester_load_patterns (tester);
}

static void
uri_tester_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  UriTester *tester = URI_TESTER (object);

  switch (prop_id)
    {
    case PROP_FILTERS:
      uri_tester_set_filters (tester, (GSList*) g_value_get_pointer (value));
      break;
    case PROP_BASE_DATA_DIR:
      tester->priv->data_dir = uri_tester_ensure_data_dir (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
uri_tester_finalize (GObject *object)
{
  UriTesterPrivate *priv = URI_TESTER_GET_PRIVATE (URI_TESTER (object));

  LOG ("UriTester finalizing %p", object);

  g_slist_foreach (priv->filters, (GFunc) g_free, NULL);
  g_slist_free (priv->filters);
  g_free (priv->data_dir);

  g_hash_table_destroy (priv->pattern);
  g_hash_table_destroy (priv->keys);
  g_hash_table_destroy (priv->optslist);
  g_hash_table_destroy (priv->urlcache);

  g_string_free (priv->blockcss, TRUE);
  g_string_free (priv->blockcssprivate, TRUE);

  G_OBJECT_CLASS (uri_tester_parent_class)->finalize (object);
}

static void
uri_tester_class_init (UriTesterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = uri_tester_set_property;
  object_class->constructed = uri_tester_constructed;
  object_class->finalize = uri_tester_finalize;

  g_object_class_install_property
    (object_class,
     PROP_FILTERS,
     g_param_spec_pointer ("filters",
                           "filters",
                           "filters",
                           G_PARAM_WRITABLE));
  g_object_class_install_property
    (object_class,
     PROP_BASE_DATA_DIR,
     g_param_spec_string ("base-data-dir",
                          "Base data dir",
                          "The base dir where to create the adblock data dir",
                          NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (UriTesterPrivate));
}

UriTester *
uri_tester_new (const char *base_data_dir)
{
  g_return_val_if_fail (base_data_dir != NULL, NULL);

  return g_object_new (TYPE_URI_TESTER, "base-data-dir", base_data_dir, NULL);
}

gboolean
uri_tester_test_uri (UriTester *tester,
                     const char *req_uri,
                     const char *page_uri,
                     AdUriCheckType type)
{
  /* Don't block top level documents. */
  if (type == AD_URI_CHECK_TYPE_DOCUMENT)
    return FALSE;

  return uri_tester_is_matched (tester, NULL, req_uri, page_uri);
}

void
uri_tester_set_filters (UriTester *tester, GSList *filters)
{
  UriTesterPrivate *priv = tester->priv;

  if (priv->filters)
    {
      g_slist_foreach (priv->filters, (GFunc) g_free, NULL);
      g_slist_free (priv->filters);
    }

  /* Update private variable and save to disk. */
  priv->filters = filters;
  uri_tester_save_filters (tester);
}

GSList *
uri_tester_get_filters (UriTester *tester)
{
  return tester->priv->filters;
}

void
uri_tester_reload (UriTester *tester)
{
  GDir *g_data_dir = NULL;

  /* Remove data files in the data dir first. */
  g_data_dir = g_dir_open (tester->priv->data_dir, 0, NULL);
  if (g_data_dir)
    {
      const char *filename = NULL;
      char *filepath = NULL;

      while ((filename = g_dir_read_name (g_data_dir)))
        {
          /* Omit the list of filters. */
          if (!g_strcmp0 (filename, FILTERS_LIST_FILENAME))
            continue;

          filepath = g_build_filename (tester->priv->data_dir, filename, NULL);
          g_unlink (filepath);

          g_free (filepath);
        }

      g_dir_close (g_data_dir);
    }

  /* Load patterns from current filters. */
  uri_tester_load_patterns (tester);
}

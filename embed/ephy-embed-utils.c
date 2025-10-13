/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
 *  Copyright © 2004 Adam Hooper
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
#include "ephy-embed-utils.h"

#include "ephy-about-handler.h"
#include "ephy-embed-shell.h"
#include "ephy-prefs.h"
#include "ephy-reader-handler.h"
#include "ephy-search-engine-manager.h"
#include "ephy-settings.h"
#include "ephy-string.h"
#include "ephy-uri-helpers.h"
#include "ephy-view-source-handler.h"

#include <glib/gi18n.h>
#include <jsc/jsc.h>
#include <libsoup/soup.h>
#include <string.h>

static GRegex *non_search_regex;
static GRegex *domain_regex;

char *
ephy_embed_utils_link_message_parse (const char *message)
{
  char *status_message;
  char **splitted_message;
  int i = 1;
  char *p;
  GString *tmp;

  status_message = ephy_string_blank_chr (g_strdup (message));

  if (!status_message || !g_str_has_prefix (status_message, "mailto:"))
    return status_message;

  /* We first want to eliminate all the things after "?", like cc,
   * subject and alike.
   */
  p = strchr (status_message, '?');
  if (p)
    *p = '\0';

  /* Then we also want to check if there is more than an email address
   * in the mailto: list.
   * Splitting is done as defined in the "to" component at
   * https://datatracker.ietf.org/doc/html/rfc6068#section-2 (so with a comma).
   */
  splitted_message = g_strsplit_set (status_message, ",", -1);
  tmp = g_string_new (g_strdup_printf (_("Send an email message to “%s”"),
                                       (splitted_message[0] + 7)));

  while (splitted_message [i]) {
    /* TRANSLATORS: This string is part of the previous translatable string.
     * It is appended for each extraneous mailto: URI email address. For
     * example if you have mailto:foo@example.com,bar@example.com,baz@example.com
     * it will show
     * Send an email to “foo@example.com”, “bar@example.com”, “baz@example.com”
     * when you hover such link, at the same place as regular URLs when you hover
     * a link in a web page.
     */
    g_string_append_printf (tmp, _(", “%s”"), splitted_message[i]);
    i++;
  }

  g_free (status_message);
  g_strfreev (splitted_message);

  return g_string_free (tmp, FALSE);
}

static gpointer
create_non_search_regex (gpointer user_data)
{
  non_search_regex = g_regex_new (EPHY_WEB_VIEW_NON_SEARCH_REGEX,
                                  G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);
  return non_search_regex;
}

static GRegex *
get_non_search_regex (void)
{
  static GOnce once_init = G_ONCE_INIT;

  return g_once (&once_init, create_non_search_regex, NULL);
}

static gpointer
create_domain_regex (gpointer user_data)
{
  domain_regex = g_regex_new (EPHY_WEB_VIEW_DOMAIN_REGEX,
                              G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);
  return domain_regex;
}

static GRegex *
get_domain_regex (void)
{
  static GOnce once_init = G_ONCE_INIT;

  return g_once (&once_init, create_domain_regex, NULL);
}

gboolean
ephy_embed_utils_address_has_web_scheme (const char *address)
{
  gboolean has_web_scheme;
  int colonpos;

  if (!address)
    return FALSE;

  colonpos = (int)((strstr (address, ":")) - address);

  if (colonpos < 0)
    return FALSE;

  has_web_scheme = !(g_ascii_strncasecmp (address, "http", colonpos) &&
                     g_ascii_strncasecmp (address, "https", colonpos) &&
                     g_ascii_strncasecmp (address, "file", colonpos) &&
                     g_ascii_strncasecmp (address, "javascript", colonpos) &&
                     g_ascii_strncasecmp (address, "data", colonpos) &&
                     g_ascii_strncasecmp (address, "blob", colonpos) &&
                     g_ascii_strncasecmp (address, "about", colonpos) &&
                     g_ascii_strncasecmp (address, "ephy-about", colonpos) &&
                     g_ascii_strncasecmp (address, "ephy-resource", colonpos) &&
                     g_ascii_strncasecmp (address, EPHY_VIEW_SOURCE_SCHEME, colonpos) &&
                     g_ascii_strncasecmp (address, "ephy-reader", colonpos) &&
                     g_ascii_strncasecmp (address, "gopher", colonpos) &&
                     g_ascii_strncasecmp (address, "inspector", colonpos) &&
                     g_ascii_strncasecmp (address, "webkit", colonpos));

  return has_web_scheme;
}

gboolean
ephy_embed_utils_address_is_existing_absolute_filename (const char *address)
{
  g_autofree char *real_address = NULL;

  if (!strchr (address, '#')) {
    real_address = g_strdup (address);
  } else {
    gint pos;

    pos = g_strstr_len (address, -1, "#") - address;
    real_address = g_strndup (address, pos);
  }

  return g_path_is_absolute (real_address) &&
         g_file_test (real_address, G_FILE_TEST_EXISTS);
}

static gboolean
is_public_domain (const char *address)
{
  char *host;
  gboolean retval = FALSE;

  host = ephy_string_get_host_name (address);
  if (!host)
    return FALSE;

  if (g_regex_match (get_domain_regex (), host, 0, NULL)) {
    if (!strcmp (host, "localhost"))
      retval = TRUE;
    else {
      const char *end;

      end = g_strrstr (host, ".");
      if (end && *end != '\0')
        retval = soup_tld_domain_is_public_suffix (end);
    }
  }

  g_free (host);

  return retval;
}

static gboolean
is_host_with_port (const char *address)
{
  g_auto (GStrv) split = NULL;
  gint64 port = 0;

  if (strchr (address, ' '))
    return FALSE;

  if (g_str_has_suffix (address, "/"))
    return TRUE;

  split = g_strsplit (address, ":", -1);
  if (g_strv_length (split) == 2)
    port = g_ascii_strtoll (split[1], NULL, 10);

  return port != 0;
}

/* This function checks whether @address can point to a web page.
 * It accepts as potential sources for web page not only full URI/URLs, but also:
 * - local absolute file path
 * - IP address
 * - host:port
 * - localhost
 */
gboolean
ephy_embed_utils_address_is_valid (const char *address)
{
  char *scheme;
  gboolean retval;
  GAppInfo *info = NULL;

  if (!address)
    return FALSE;

  scheme = g_uri_parse_scheme (address);

  if (scheme) {
    info = g_app_info_get_default_for_uri_scheme (scheme);
    g_free (scheme);
  }

  retval = info ||
           ephy_embed_utils_address_is_existing_absolute_filename (address) ||
           g_regex_match (get_non_search_regex (), address, 0, NULL) ||
           is_public_domain (address) ||
           is_host_with_port (address);

  g_clear_object (&info);

  return retval;
}

static char *
ensure_host_name_is_lowercase (const char *address)
{
  g_autofree gchar *host = ephy_string_get_host_name (address);
  g_autofree gchar *lowercase_host = NULL;

  if (!host)
    return g_strdup (address);

  lowercase_host = g_utf8_strdown (host, -1);

  if (strcmp (host, lowercase_host) != 0)
    return ephy_string_find_and_replace (address, host, lowercase_host);
  else
    return g_strdup (address);
}

/* Does various normalization rules to make sure @input_address ends up
 * with a URI scheme (e.g. absolute filenames or "localhost"), changes
 * the URI scheme to something more appropriate when needed and lowercases
 * the hostname.
 */
char *
ephy_embed_utils_normalize_address (const char *input_address)
{
  char *effective_address = NULL;
  g_autofree gchar *address = NULL;

  g_assert (input_address);

  address = ensure_host_name_is_lowercase (input_address);

  if (ephy_embed_utils_address_is_existing_absolute_filename (address))
    return g_strconcat ("file://", address, NULL);

  if (strcmp (address, "about:gpu") == 0)
    return g_strdup ("webkit://gpu");

  if (g_str_has_prefix (address, "about:") && strcmp (address, "about:blank"))
    return g_strconcat (EPHY_ABOUT_SCHEME, address + strlen ("about"), NULL);

  if (!ephy_embed_utils_address_has_web_scheme (address)) {
    const char *scheme;

    scheme = g_uri_peek_scheme (address);

    /* Auto-prepend https:// to anything that is not
     * one according to GLib, because it probably will be
     * something like "google.com". Special case localhost(:port)
     * and IP(:port), because GUri, correctly, thinks it is a
     * URI with scheme being localhost/IP and, optionally, path
     * being the port. Ideally we should check if we have a
     * handler for the scheme, and since we'll fail for localhost
     * and IP, we'd fallback to loading it as a domain. */
    if (!scheme ||
        !g_strcmp0 (scheme, "localhost") ||
        g_hostname_is_ip_address (scheme) ||
        is_host_with_port (address))
      effective_address = g_strconcat ("https://", address, NULL);
  }

  return effective_address ? effective_address : g_strdup (address);
}

/* Searches @search_key with the default search engine. */
char *
ephy_embed_utils_autosearch_address (const char *search_key)
{
  EphyEmbedShell *shell;
  EphySearchEngineManager *manager;
  EphySearchEngine *engine;

  if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_AUTOSEARCH))
    return g_strdup (search_key);

  shell = ephy_embed_shell_get_default ();
  manager = ephy_embed_shell_get_search_engine_manager (shell);

  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_INCOGNITO)
    engine = ephy_search_engine_manager_get_incognito_engine (manager);
  else
    engine = ephy_search_engine_manager_get_default_engine (manager);
  g_assert (engine);

  return ephy_search_engine_build_search_address (engine, search_key);
}

char *
ephy_embed_utils_normalize_or_autosearch_address (const char *address)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  char *bang_search = ephy_search_engine_manager_parse_bang_search (manager, address);

  if (bang_search)
    return bang_search;
  else if (ephy_embed_utils_address_is_valid (address))
    return ephy_embed_utils_normalize_address (address);
  else
    return ephy_embed_utils_autosearch_address (address);
}

gboolean
ephy_embed_utils_url_is_empty (const char *location)
{
  return (!location ||
          location[0] == '\0' ||
          strcmp (location, "about:blank") == 0 ||
          strcmp (location, "ephy-about:newtab") == 0 ||
          strcmp (location, "ephy-about:overview") == 0 ||
          strcmp (location, "ephy-about:incognito") == 0);
}

/* This is the list of addresses that should never be shown in the
 * window's location entry. */
static const char *do_not_show_address[] = {
  "about:blank",
  "ephy-about:newtab",
  "ephy-about:incognito",
  "ephy-about:overview",
  NULL
};

gboolean
ephy_embed_utils_is_no_show_address (const char *address)
{
  int i;

  if (!address)
    return FALSE;

  for (i = 0; do_not_show_address[i]; i++)
    if (!strcmp (address, do_not_show_address[i]))
      return TRUE;

  return FALSE;
}

char *
ephy_embed_utils_get_title_from_address (const char *address)
{
  g_autofree char *decoded_url = NULL;

  if (g_str_has_prefix (address, "file://"))
    return g_strdup (address + 7);

  if (!strcmp (address, EPHY_ABOUT_SCHEME ":overview") ||
      !strcmp (address, EPHY_ABOUT_SCHEME ":newtab") ||
      !strcmp (address, "about:overview") ||
      !strcmp (address, "about:newtab"))
    return g_strdup (_(NEW_TAB_PAGE_TITLE));

  decoded_url = ephy_uri_decode (address);
  if (!decoded_url)
    return g_strdup (address);

  return ephy_uri_get_decoded_host (decoded_url);
}

void
ephy_embed_utils_shutdown (void)
{
  g_clear_pointer (&non_search_regex, g_regex_unref);
  g_clear_pointer (&domain_regex, g_regex_unref);
}

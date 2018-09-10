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
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-string.h"
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
  if (p != NULL) *p = '\0';

  /* Then we also want to check if there is more than an email address
   * in the mailto: list.
   */
  splitted_message = g_strsplit_set (status_message, ";", -1);
  tmp = g_string_new (g_strdup_printf (_("Send an email message to “%s”"),
                                       (splitted_message[0] + 7)));

  while (splitted_message [i] != NULL) {
    g_string_append_printf (tmp, ", “%s”", splitted_message[i]);
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

  if (address == NULL)
    return FALSE;

  colonpos = (int)((g_strstr_len (address, 12, ":")) - address);

  if (colonpos < 0)
    return FALSE;

  has_web_scheme = !(g_ascii_strncasecmp (address, "http", colonpos) &&
                     g_ascii_strncasecmp (address, "https", colonpos) &&
                     g_ascii_strncasecmp (address, "ftp", colonpos) &&
                     g_ascii_strncasecmp (address, "file", colonpos) &&
                     g_ascii_strncasecmp (address, "javascript", colonpos) &&
                     g_ascii_strncasecmp (address, "data", colonpos) &&
                     g_ascii_strncasecmp (address, "blob", colonpos) &&
                     g_ascii_strncasecmp (address, "about", colonpos) &&
                     g_ascii_strncasecmp (address, "ephy-about", colonpos) &&
                     g_ascii_strncasecmp (address, "ephy-source", colonpos) &&
                     g_ascii_strncasecmp (address, "gopher", colonpos) &&
                     g_ascii_strncasecmp (address, "inspector", colonpos));

  return has_web_scheme;
}

gboolean
ephy_embed_utils_address_is_existing_absolute_filename (const char *address)
{
  return g_path_is_absolute (address) &&
         g_file_test (address, G_FILE_TEST_EXISTS);
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
is_bang_search (const char *address)
{
  EphyEmbedShell *shell;
  EphySearchEngineManager *search_engine_manager;
  char **bangs;
  GString *buffer;

  shell = ephy_embed_shell_get_default ();
  search_engine_manager = ephy_embed_shell_get_search_engine_manager (shell);
  bangs = ephy_search_engine_manager_get_bangs (search_engine_manager);

  for (uint i = 0; bangs[i] != NULL; i++) {
    buffer = g_string_new (bangs[i]);
    g_string_append (buffer, " ");

    if (strstr (address, buffer->str) == address) {
      g_string_free (buffer, TRUE);
      g_free (bangs);
      return TRUE;
    }
    g_string_free (buffer, TRUE);
  }
  g_free (bangs);

  return FALSE;
}

gboolean
ephy_embed_utils_address_is_valid (const char *address)
{
  char *scheme;
  gboolean retval;
  GAppInfo *info = NULL;

  if (!address)
    return FALSE;

  scheme = g_uri_parse_scheme (address);

  if (scheme != NULL) {
    info = g_app_info_get_default_for_uri_scheme (scheme);
    g_free (scheme);
  }

  retval = info ||
           ephy_embed_utils_address_is_existing_absolute_filename (address) ||
           g_regex_match (get_non_search_regex (), address, 0, NULL) ||
           is_public_domain (address) ||
           is_bang_search (address);

  g_clear_object (&info);

  return retval;
}

char *
ephy_embed_utils_normalize_address (const char *address)
{
  char *effective_address = NULL;

  g_assert (address);

  if (is_bang_search (address)) {
    EphyEmbedShell *shell;
    EphySearchEngineManager *search_engine_manager;

    shell = ephy_embed_shell_get_default ();
    search_engine_manager = ephy_embed_shell_get_search_engine_manager (shell);
    return ephy_search_engine_manager_parse_bang_search (search_engine_manager,
                                                         address);
  }

  if (ephy_embed_utils_address_is_existing_absolute_filename (address))
    return g_strconcat ("file://", address, NULL);

  if (g_str_has_prefix (address, "about:") && strcmp (address, "about:blank"))
    return g_strconcat (EPHY_ABOUT_SCHEME, address + strlen ("about"), NULL);

  if (!ephy_embed_utils_address_has_web_scheme (address)) {
    SoupURI *uri;

    uri = soup_uri_new (address);

    /* Auto-prepend http:// to anything that is not
     * one according to soup, because it probably will be
     * something like "google.com". Special case localhost(:port)
     * and IP(:port), because SoupURI, correctly, thinks it is a
     * URI with scheme being localhost/IP and, optionally, path
     * being the port. Ideally we should check if we have a
     * handler for the scheme, and since we'll fail for localhost
     * and IP, we'd fallback to loading it as a domain. */
    if (!uri ||
        (uri && !g_strcmp0 (uri->scheme, "localhost")) ||
        (uri && g_hostname_is_ip_address (uri->scheme)))
      effective_address = g_strconcat ("http://", address, NULL);

    if (uri)
      soup_uri_free (uri);
  }

  return effective_address ? effective_address : g_strdup (address);
}

char *
ephy_embed_utils_autosearch_address (const char *search_key)
{
  char *query_param;
  const char *address_search;
  char *effective_address;
  EphyEmbedShell *shell;
  EphySearchEngineManager *search_engine_manager;

  if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_AUTOSEARCH))
    return g_strdup (search_key);

  shell = ephy_embed_shell_get_default ();
  search_engine_manager = ephy_embed_shell_get_search_engine_manager (shell);
  address_search = ephy_search_engine_manager_get_default_search_address (search_engine_manager);

  query_param = soup_form_encode ("q", search_key, NULL);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  /* Format string under control of user input... but gsettings is trusted input. */
  /* + 2 here is getting rid of 'q=' */
  effective_address = g_strdup_printf (address_search, query_param + 2);
#pragma GCC diagnostic pop
  g_free (query_param);

  return effective_address;
}

char *
ephy_embed_utils_normalize_or_autosearch_address (const char *address)
{
  if (ephy_embed_utils_address_is_valid (address))
    return ephy_embed_utils_normalize_address (address);
  else
    return ephy_embed_utils_autosearch_address (address);
}

gboolean
ephy_embed_utils_url_is_empty (const char *location)
{
  return (location == NULL ||
          location[0] == '\0' ||
          strcmp (location, "about:blank") == 0 ||
          strcmp (location, "ephy-about:overview") == 0 ||
          strcmp (location, "ephy-about:incognito") == 0);
}

/* This is the list of addresses that should never be shown in the
 * window's location entry. */
static const char *do_not_show_address[] = {
  "about:blank",
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

  if (strstr (address, EPHY_VIEW_SOURCE_SCHEME) == address)
    return TRUE;

  return FALSE;
}

char *
ephy_embed_utils_get_title_from_address (const char *address)
{
  if (g_str_has_prefix (address, "file://"))
    return g_strdup (address + 7);

  if (!strcmp (address, EPHY_ABOUT_SCHEME ":overview") ||
      !strcmp (address, "about:overview"))
    return g_strdup (_(OVERVIEW_PAGE_TITLE));

  return ephy_string_get_host_name (address);
}

gboolean
ephy_embed_utils_urls_have_same_origin (const char *a_url,
                                        const char *b_url)
{
  SoupURI *a_uri, *b_uri;
  gboolean retval = FALSE;

  a_uri = soup_uri_new (a_url);
  if (!a_uri)
    return retval;

  b_uri = soup_uri_new (b_url);
  if (b_uri) {
    retval = a_uri->host && b_uri->host && soup_uri_host_equal (a_uri, b_uri);
    soup_uri_free (b_uri);
  }

  soup_uri_free (a_uri);

  return retval;
}

void
ephy_embed_utils_shutdown (void)
{
  g_clear_pointer (&non_search_regex, g_regex_unref);
  g_clear_pointer (&domain_regex, g_regex_unref);
}

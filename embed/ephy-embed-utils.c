/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
 *  Copyright © 2004 Adam Hooper
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-embed-utils.h"

#include "ephy-about-handler.h"
#include "ephy-embed-private.h"
#include "ephy-settings.h"
#include "ephy-string.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <JavaScriptCore/JavaScript.h>

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

  colonpos = (int)((g_strstr_len (address, 11, ":")) - address);

  if (colonpos < 0)
    return FALSE;

  has_web_scheme = !(g_ascii_strncasecmp (address, "http", colonpos) &&
                     g_ascii_strncasecmp (address, "https", colonpos) &&
                     g_ascii_strncasecmp (address, "ftp", colonpos) &&
                     g_ascii_strncasecmp (address, "file", colonpos) &&
                     g_ascii_strncasecmp (address, "javascript", colonpos) &&
                     g_ascii_strncasecmp (address, "data", colonpos) &&
                     g_ascii_strncasecmp (address, "about", colonpos) &&
                     g_ascii_strncasecmp (address, "ephy-about", colonpos) &&
                     g_ascii_strncasecmp (address, "gopher", colonpos));

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
    if (g_str_equal (host, "localhost"))
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

gboolean
ephy_embed_utils_address_is_valid (const char *address)
{
  char *scheme;
  gboolean retval;

  if (!address)
    return FALSE;

  scheme = g_uri_parse_scheme (address);

  retval = scheme ||
    ephy_embed_utils_address_is_existing_absolute_filename (address) ||
    g_regex_match (get_non_search_regex (), address, 0, NULL) ||
    is_public_domain (address);

  g_free (scheme);

  return retval;
}

char*
ephy_embed_utils_normalize_address (const char *address)
{
  char *effective_address = NULL;

  g_return_val_if_fail (address, NULL);

  if (ephy_embed_utils_address_is_existing_absolute_filename (address))
    return g_strconcat ("file://", address, NULL);

  if (g_str_has_prefix (address, "about:") && !g_str_equal (address, "about:blank"))
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
  char *query_param, *url_search;
  char *effective_address;

  url_search = g_settings_get_string (EPHY_SETTINGS_MAIN,
                                      EPHY_PREFS_KEYWORD_SEARCH_URL);
  if (url_search == NULL || url_search[0] == '\0') {
    g_free (url_search);
    url_search = g_strdup (_("https://duckduckgo.com/?q=%s&amp;t=epiphany"));
  }

  query_param = soup_form_encode ("q", search_key, NULL);
  /* + 2 here is getting rid of 'q=' */
  effective_address = g_strdup_printf (url_search, query_param + 2);
  g_free (query_param);
  g_free (url_search);

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
  gboolean is_empty = FALSE;

  if (location == NULL || 
      location[0] == '\0' || 
      strcmp (location, "about:blank") == 0)
    is_empty = TRUE;
  
  return is_empty;
}

/* This is the list of addresses that should never be shown in the
 * window's location entry. */
static const char * do_not_show_address[] = {
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
    if (g_str_equal (address, do_not_show_address[i]))
      return TRUE;

  return FALSE;
}

char *
ephy_embed_utils_get_title_from_address (const char *address)
{
  if (g_str_has_prefix (address, "file://"))
    return g_strdup (address + 7);

  if (!strcmp (address, EPHY_ABOUT_SCHEME":overview") ||
      !strcmp (address, "about:overview"))
    return g_strdup (_("Most Visited"));

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
  if (!b_uri) {
    soup_uri_free (a_uri);
    return retval;
  }

  if (a_uri->host && b_uri->host) {
    retval = soup_uri_host_equal (a_uri, b_uri);
    if (!retval) {
      const char *a_domain;
      const char *b_domain;

      a_domain = soup_tld_get_base_domain (a_uri->host, NULL);
      b_domain = soup_tld_get_base_domain (b_uri->host, NULL);

      retval = a_domain && b_domain && strcmp (a_domain, b_domain) == 0;
    }
  }

  soup_uri_free (a_uri);
  soup_uri_free (b_uri);

  return retval;
}

char *
ephy_embed_utils_get_js_result_as_string (WebKitJavascriptResult *js_result)
{
  JSValueRef js_value;
  JSStringRef js_string;
  size_t max_size;
  char *retval = NULL;

  js_value = webkit_javascript_result_get_value (js_result);
  js_string = JSValueToStringCopy (webkit_javascript_result_get_global_context (js_result),
                                   js_value, NULL);
  max_size = JSStringGetMaximumUTF8CStringSize (js_string);
  if (max_size) {
    retval = g_malloc (max_size);
    JSStringGetUTF8CString (js_string, retval, max_size);
  }
  JSStringRelease (js_string);

  return retval;
}

double
ephy_embed_utils_get_js_result_as_number (WebKitJavascriptResult *js_result)
{
  JSValueRef js_value;

  js_value = webkit_javascript_result_get_value (js_result);

  return JSValueToNumber (webkit_javascript_result_get_global_context (js_result),
                          js_value, NULL);
}

void
ephy_embed_utils_shutdown (void)
{
  g_clear_pointer (&non_search_regex, (GDestroyNotify)g_regex_unref);
  g_clear_pointer (&domain_regex, (GDestroyNotify)g_regex_unref);
}

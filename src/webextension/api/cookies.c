/*
 *  Copyright © 2022 Igalia S.L.
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

#include "ephy-json-utils.h"
#include "ephy-shell.h"

#include "api-utils.h"
#include "cookies.h"

static WebKitCookieManager *
get_cookie_manager (void)
{
  WebKitNetworkSession *network_session = ephy_embed_shell_get_network_session (ephy_embed_shell_get_default ());
  return webkit_network_session_get_cookie_manager (network_session);
}

static const char *
samesite_to_string (SoupSameSitePolicy policy)
{
  switch (policy) {
    case SOUP_SAME_SITE_POLICY_NONE:
      return "no_restriction";
    case SOUP_SAME_SITE_POLICY_LAX:
      return "lax";
    case SOUP_SAME_SITE_POLICY_STRICT:
      return "strict";
  }

  g_assert_not_reached ();
  return "no_restriction";
}

static SoupSameSitePolicy
string_to_samesite (const char *policy)
{
  if (g_strcmp0 (policy, "strict") == 0)
    return SOUP_SAME_SITE_POLICY_STRICT;
  if (g_strcmp0 (policy, "lax") == 0)
    return SOUP_SAME_SITE_POLICY_LAX;
  return SOUP_SAME_SITE_POLICY_NONE;
}

static void
add_cookie_to_json (JsonBuilder *builder,
                    SoupCookie  *cookie)
{
  GDateTime *expires = soup_cookie_get_expires (cookie);

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "name");
  json_builder_add_string_value (builder, soup_cookie_get_name (cookie));
  json_builder_set_member_name (builder, "value");
  json_builder_add_string_value (builder, soup_cookie_get_value (cookie));
  json_builder_set_member_name (builder, "domain");
  json_builder_add_string_value (builder, soup_cookie_get_domain (cookie));
  json_builder_set_member_name (builder, "path");
  json_builder_add_string_value (builder, soup_cookie_get_path (cookie));
  json_builder_set_member_name (builder, "httpOnly");
  json_builder_add_boolean_value (builder, soup_cookie_get_http_only (cookie));
  json_builder_set_member_name (builder, "secure");
  json_builder_add_boolean_value (builder, soup_cookie_get_secure (cookie));
  json_builder_set_member_name (builder, "sameSite");
  json_builder_add_string_value (builder, samesite_to_string (soup_cookie_get_same_site_policy (cookie)));
  if (expires) {
    json_builder_set_member_name (builder, "expirationDate");
    json_builder_add_int_value (builder, g_date_time_to_unix (expires));
  }
  json_builder_end_object (builder);
}

static char *
cookie_to_json (SoupCookie *cookie)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;

  add_cookie_to_json (builder, cookie);

  root = json_builder_get_root (builder);
  return json_to_string (root, FALSE);
}

typedef struct {
  GTask *task;
  char *cookie_name;
  SoupCookie *cookie;
  gboolean remove_after_find;
} CookiesCallbackData;

static void
cookies_callback_data_free (CookiesCallbackData *data)
{
  if (data) {
    g_clear_pointer (&data->cookie_name, g_free);
    g_clear_pointer (&data->cookie, soup_cookie_free);
    g_free (data);
  }
}

static SoupCookie *
compare_best_cookie (SoupCookie *previous,
                     SoupCookie *current)
{
  gint64 path_diff;

  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/cookies/get
   * > If more than one cookie with the same name exists for a given URL, the one with the longest path will be returned.
   * > For cookies with the same path length, the cookie with the earliest creation time will be returned. */

  if (!previous)
    return current;

  path_diff = strlen (soup_cookie_get_path (previous)) - strlen (soup_cookie_get_path (current));

  if (path_diff < 0)
    return current;
  if (path_diff > 0)
    return previous;

  /* We don't have a creation date, for now assume earlier in list is older. */
  return previous;
}

static void
delete_cookie_ready_cb (WebKitCookieManager *cookie_manager,
                        GAsyncResult        *result,
                        CookiesCallbackData *data)
{
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = webkit_cookie_manager_delete_cookie_finish (cookie_manager, result, &error);

  if (!success) {
    g_task_return_error (data->task, g_steal_pointer (&error));
    cookies_callback_data_free (data);
    return;
  }

  g_task_return_pointer (data->task, cookie_to_json (data->cookie), g_free);
  cookies_callback_data_free (data);
}

static void
get_cookies_ready_cb (WebKitCookieManager *cookie_manager,
                      GAsyncResult        *result,
                      CookiesCallbackData *data)
{
  g_autoptr (GError) error = NULL;
  GList *cookies = webkit_cookie_manager_get_cookies_finish (cookie_manager, result, &error);
  SoupCookie *best_match = NULL;

  if (error) {
    g_task_return_error (data->task, g_steal_pointer (&error));
    cookies_callback_data_free (data);
    return;
  }

  for (GList *l = cookies; l; l = g_list_next (l)) {
    SoupCookie *cookie = l->data;
    if (strcmp (soup_cookie_get_name (cookie), data->cookie_name) != 0)
      continue;

    best_match = compare_best_cookie (best_match, cookie);
  }

  if (!best_match)
    g_task_return_pointer (data->task, g_strdup ("null"), g_free);
  else if (!data->remove_after_find)
    g_task_return_pointer (data->task, cookie_to_json (best_match), g_free);
  else {
    data->cookie = soup_cookie_copy (best_match);
    webkit_cookie_manager_delete_cookie (cookie_manager, data->cookie, NULL,
                                         (GAsyncReadyCallback)delete_cookie_ready_cb,
                                         data);
    data = NULL;
  }

  g_list_free_full (cookies, (GDestroyNotify)soup_cookie_free);
  cookies_callback_data_free (data);
}

static void
cookies_handler_get (EphyWebExtensionSender *sender,
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  JsonObject *details = ephy_json_array_get_object (args, 0);
  WebKitCookieManager *cookie_manager = get_cookie_manager ();
  const char *cookie_name;
  const char *url;
  CookiesCallbackData *callback_data;

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.get(): Missing details object");
    return;
  }

  cookie_name = ephy_json_object_get_string (details, "name");
  url = ephy_json_object_get_string (details, "url");

  if (!url || !cookie_name) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.get(): details missing url or name");
    return;
  }

  if (!ephy_web_extension_has_host_permission (sender->extension, url)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "cookies.get(): Permission denied for host '%s'", url);
    return;
  }

  callback_data = g_new0 (CookiesCallbackData, 1);
  callback_data->task = task;
  callback_data->cookie_name = g_strdup (cookie_name);

  /* FIXME: The WebKit API doesn't expose details like first-party URLs to better filter this. The underlying libsoup API does so
   * this just requires additions to WebKitGTK. */
  webkit_cookie_manager_get_cookies (cookie_manager, url, NULL, (GAsyncReadyCallback)get_cookies_ready_cb, callback_data);
}

static void
add_cookie_ready_cb (WebKitCookieManager *cookie_manager,
                     GAsyncResult        *result,
                     CookiesCallbackData *data)
{
  g_autoptr (GError) error = NULL;
  gboolean success = webkit_cookie_manager_add_cookie_finish (cookie_manager, result, &error);

  if (!success) {
    g_task_return_error (data->task, g_steal_pointer (&error));
    cookies_callback_data_free (data);
    return;
  }

  g_task_return_pointer (data->task, cookie_to_json (data->cookie), g_free);
  cookies_callback_data_free (data);
}

static void
cookies_handler_set (EphyWebExtensionSender *sender,
                     const char             *method_name,
                     JsonArray              *args,
                     GTask                  *task)
{
  JsonObject *details = ephy_json_array_get_object (args, 0);
  const char *url;
  const char *domain;
  const char *cookie_name;
  const char *value;
  const char *path;
  const char *same_site_str;
  gboolean secure;
  gboolean http_only;
  gint64 expiration;
  g_autoptr (SoupCookie) new_cookie = NULL;
  g_autoptr (GUri) parsed_uri = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GDateTime) expires_date = NULL;
  WebKitCookieManager *cookie_manager = get_cookie_manager ();
  CookiesCallbackData *callback_data;

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.set(): Missing details object");
    return;
  }

  url = ephy_json_object_get_string (details, "url");
  domain = ephy_json_object_get_string (details, "domain");
  cookie_name = ephy_json_object_get_string (details, "name");
  value = ephy_json_object_get_string (details, "value");
  path = ephy_json_object_get_string (details, "path");
  same_site_str = ephy_json_object_get_string (details, "sameSite");
  expiration = ephy_json_object_get_int (details, "expirationDate");
  secure = ephy_json_object_get_boolean (details, "secure", FALSE);
  http_only = ephy_json_object_get_boolean (details, "httpOnline", FALSE);

  if (!url) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.set(): Missing url property");
    return;
  }

  if (!ephy_web_extension_has_host_permission (sender->extension, url) || (domain && !ephy_web_extension_has_host_permission (sender->extension, domain))) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "cookies.set(): Permission denied for host '%s'", url);
    return;
  }

  parsed_uri = g_uri_parse (url, G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY | G_URI_FLAGS_SCHEME_NORMALIZE, &error);
  if (error) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.set(): Failed to parse URI '%s': %s", url, error->message);
    return;
  }

  new_cookie = soup_cookie_new (cookie_name ? cookie_name : "",
                                value ? value : "",
                                domain ? domain : g_uri_get_host (parsed_uri),
                                path ? path : g_uri_get_path (parsed_uri),
                                -1);
  soup_cookie_set_secure (new_cookie, secure);
  soup_cookie_set_http_only (new_cookie, http_only);
  soup_cookie_set_same_site_policy (new_cookie, string_to_samesite (same_site_str));
  if (expiration != -1) {
    expires_date = g_date_time_new_from_unix_local (expiration);
    soup_cookie_set_expires (new_cookie, expires_date);
  }

  callback_data = g_new0 (CookiesCallbackData, 1);
  callback_data->task = task;
  callback_data->cookie = g_steal_pointer (&new_cookie);

  webkit_cookie_manager_add_cookie (cookie_manager, callback_data->cookie, NULL, (GAsyncReadyCallback)add_cookie_ready_cb, callback_data);
}

static void
cookies_handler_remove (EphyWebExtensionSender *sender,
                        const char             *method_name,
                        JsonArray              *args,
                        GTask                  *task)
{
  JsonObject *details = ephy_json_array_get_object (args, 0);
  const char *url;
  const char *cookie_name;
  WebKitCookieManager *cookie_manager = get_cookie_manager ();
  CookiesCallbackData *callback_data;

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.remove(): Missing details object");
    return;
  }

  url = ephy_json_object_get_string (details, "url");
  cookie_name = ephy_json_object_get_string (details, "name");

  if (!url || !cookie_name) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.remove(): Missing url or name property");
    return;
  }

  if (!ephy_web_extension_has_host_permission (sender->extension, url)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "cookies.remove(): Permission denied for host '%s'", url);
    return;
  }

  callback_data = g_new0 (CookiesCallbackData, 1);
  callback_data->task = task;
  callback_data->cookie_name = g_strdup (cookie_name);
  callback_data->remove_after_find = TRUE;

  webkit_cookie_manager_get_cookies (cookie_manager, url, NULL, (GAsyncReadyCallback)get_cookies_ready_cb, callback_data);
}

typedef struct {
  GTask *task;
  char *domain;
  char *name;
  char *path;
  ApiTriStateValue secure;
  ApiTriStateValue session;
} GetAllCookiesCallbackData;

static void
get_all_cookies_callback_data_free (GetAllCookiesCallbackData *data)
{
  g_free (data->domain);
  g_free (data->name);
  g_free (data->path);
  g_free (data);
}

static gboolean
cookie_matches_filter (SoupCookie                *cookie,
                       GetAllCookiesCallbackData *data)
{
  if (data->name && strcmp (soup_cookie_get_name (cookie), data->name) != 0)
    return FALSE;
  if (data->domain && !soup_cookie_domain_matches (cookie, data->domain))
    return FALSE;
  if (data->path && strcmp (soup_cookie_get_path (cookie), data->path) != 0)
    return FALSE;
  if (data->secure != API_VALUE_UNSET && soup_cookie_get_secure (cookie) != data->secure)
    return FALSE;
  if (data->session != API_VALUE_UNSET) {
    gpointer expires = soup_cookie_get_expires (cookie);
    if (data->session && expires)
      return FALSE;
    if (!data->session && !expires)
      return FALSE;
  }

  return TRUE;
}

static int
cookie_compare_func (SoupCookie *a,
                     SoupCookie *b)
{
  const char *path_a = soup_cookie_get_path (a);
  const char *path_b = soup_cookie_get_path (b);

  return strlen (path_b) - strlen (path_a);
}

static void
get_all_cookies_ready_cb (WebKitCookieManager       *cookie_manager,
                          GAsyncResult              *result,
                          GetAllCookiesCallbackData *data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  GList *cookies = webkit_cookie_manager_get_cookies_finish (cookie_manager, result, &error);

  if (error) {
    g_task_return_error (data->task, g_steal_pointer (&error));
    get_all_cookies_callback_data_free (data);
    return;
  }

  /* Sort by path length. */
  cookies = g_list_sort (cookies, (GCompareFunc)cookie_compare_func);

  json_builder_begin_array (builder);
  for (GList *l = cookies; l; l = g_list_next (l)) {
    SoupCookie *cookie = l->data;

    if (cookie_matches_filter (cookie, data))
      add_cookie_to_json (builder, cookie);
  }
  json_builder_end_array (builder);

  root = json_builder_get_root (builder);
  g_task_return_pointer (data->task, json_to_string (root, FALSE), g_free);

  g_list_free_full (cookies, (GDestroyNotify)soup_cookie_free);
  get_all_cookies_callback_data_free (data);
}

static void
cookies_handler_get_all (EphyWebExtensionSender *sender,
                         const char             *method_name,
                         JsonArray              *args,
                         GTask                  *task)
{
  JsonObject *details = ephy_json_array_get_object (args, 0);
  WebKitCookieManager *cookie_manager = get_cookie_manager ();
  const char *url;
  GetAllCookiesCallbackData *callback_data;

  if (!details) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.getAll(): Missing details object");
    return;
  }

  url = ephy_json_object_get_string (details, "url");

  /* TODO: We can handle the case of no url by using webkit_website_data_manager_fetch() to list all domains and then get all cookies
   * for all domains, but this is rather an ugly amount of work compared to libsoup directly. */
  if (!url) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "cookies.getAll(): details missing url");
    return;
  }

  if (!ephy_web_extension_has_host_permission (sender->extension, url)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "cookies.getAll(): Permission denied for host '%s'", url);
    return;
  }

  callback_data = g_new0 (GetAllCookiesCallbackData, 1);
  callback_data->task = task;
  callback_data->name = ephy_json_object_dup_string (details, "name");
  callback_data->domain = ephy_json_object_dup_string (details, "domain");
  callback_data->path = ephy_json_object_dup_string (details, "path");
  callback_data->secure = ephy_json_object_get_boolean (details, "secure", API_VALUE_UNSET);
  callback_data->session = ephy_json_object_get_boolean (details, "session", API_VALUE_UNSET);

  /* FIXME: The WebKit API doesn't expose details like first-party URLs to better filter this. The underlying libsoup API does so
   * this just requires additions to WebKitGTK. */
  webkit_cookie_manager_get_cookies (cookie_manager, url, NULL, (GAsyncReadyCallback)get_all_cookies_ready_cb, callback_data);
}

static JsonNode *
create_array_of_all_tab_ids (void)
{
  JsonNode *node = json_node_init_array (json_node_alloc (), json_array_new ());
  JsonArray *array = json_node_get_array (node);
  GList *windows;

  windows = gtk_application_get_windows (GTK_APPLICATION (ephy_shell_get_default ()));
  for (GList *win_list = windows; win_list; win_list = g_list_next (win_list)) {
    EphyWindow *window = EPHY_WINDOW (win_list->data);
    EphyTabView *tab_view = ephy_window_get_tab_view (window);

    for (int i = 0; i < ephy_tab_view_get_n_pages (tab_view); i++) {
      EphyWebView *web_view = ephy_embed_get_web_view (EPHY_EMBED (ephy_tab_view_get_nth_page (tab_view, i)));

      json_array_add_int_element (array, ephy_web_view_get_uid (web_view));
    }
  }

  return node;
}

static void
cookies_handler_get_all_cookie_stores (EphyWebExtensionSender *sender,
                                       const char             *method_name,
                                       JsonArray              *args,
                                       GTask                  *task)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;

  /* We only have a single store so we create a single object with a list
   * of every tab. */
  json_builder_begin_array (builder);
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "id");
  json_builder_add_string_value (builder, "default");
  json_builder_set_member_name (builder, "incognito");
  json_builder_add_boolean_value (builder, ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_INCOGNITO);
  json_builder_set_member_name (builder, "tabIds");
  json_builder_add_value (builder, create_array_of_all_tab_ids ());
  json_builder_end_object (builder);
  json_builder_end_array (builder);

  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static EphyWebExtensionApiHandler cookies_async_handlers[] = {
  {"get", cookies_handler_get},
  {"getAll", cookies_handler_get_all},
  {"getAllCookieStores", cookies_handler_get_all_cookie_stores},
  {"set", cookies_handler_set},
  {"remove", cookies_handler_remove},
};

void
ephy_web_extension_api_cookies_handler (EphyWebExtensionSender *sender,
                                        const char             *method_name,
                                        JsonArray              *args,
                                        GTask                  *task)
{
  if (!ephy_web_extension_has_permission (sender->extension, "cookies")) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "Permission Denied");
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (cookies_async_handlers); idx++) {
    EphyWebExtensionApiHandler handler = cookies_async_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "Not Implemented");
}

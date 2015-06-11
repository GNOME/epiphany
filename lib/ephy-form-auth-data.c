/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2013 Igalia S.L.
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
#include "ephy-form-auth-data.h"

#include "ephy-string.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>

const SecretSchema *
ephy_form_auth_data_get_password_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.FormPassword", SECRET_SCHEMA_NONE,
    {
      { URI_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { FORM_USERNAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { FORM_PASSWORD_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { USERNAME_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    }
  };
  return &schema;
}

static void
normalize_and_prepare_uri (SoupURI  *uri,
                           gboolean  remove_path)
{
  g_assert (uri != NULL);

  /* We normalize https? schemes here so that we use passwords
   * we stored in https sites in their http counterparts, and
   * vice-versa. */
  if (uri->scheme == SOUP_URI_SCHEME_HTTPS)
    soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTP);

  soup_uri_set_query (uri, NULL);
  if (remove_path)
    soup_uri_set_path (uri, "/");
}

static GHashTable *
ephy_form_auth_data_get_secret_attributes_table (const char *uri,
                                                 const char *field_username,
                                                 const char *field_password,
                                                 const char *username)
{
  if (field_username)
    return secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA,
                                    URI_KEY, uri,
                                    FORM_USERNAME_KEY, field_username,
                                    FORM_PASSWORD_KEY, field_password,
                                    username ? USERNAME_KEY : NULL, username,
                                    NULL);
  else
    return secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA,
                                    URI_KEY, uri,
                                    FORM_PASSWORD_KEY, field_password,
                                    username ? USERNAME_KEY : NULL, username,
                                    NULL);
}

static void
store_form_password_cb (SecretService *service,
                        GAsyncResult *res,
                        GTask *task)
{
  GError *error = NULL;

  secret_service_store_finish (service, res, &error);
  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}

void
ephy_form_auth_data_store (const char *uri,
                           const char *form_username,
                           const char *form_password,
                           const char *username,
                           const char *password,
                           GAsyncReadyCallback callback,
                           gpointer userdata)
{
  SoupURI *fake_uri;
  char *fake_uri_str;
  SecretValue *value;
  GHashTable *attributes;
  char *label;
  GTask *task;

  g_return_if_fail (uri);
  g_return_if_fail (form_password);
  g_return_if_fail (password);
  g_return_if_fail ((form_username && username) || (!form_username && !username));

  fake_uri = soup_uri_new (uri);
  g_return_if_fail (fake_uri);

  task = g_task_new (NULL, NULL, callback, userdata);

  /* Mailman passwords need the full URI */
  if (!form_username && g_strcmp0 (form_password, "adminpw") == 0)
    normalize_and_prepare_uri (fake_uri, FALSE);
  else
    normalize_and_prepare_uri (fake_uri, TRUE);
  fake_uri_str = soup_uri_to_string (fake_uri, FALSE);
  value = secret_value_new (password, -1, "text/plain");
  attributes = ephy_form_auth_data_get_secret_attributes_table (fake_uri_str, form_username,
                                                                form_password, username);
  if (username != NULL) {
    /* Translators: The first %s is the username and the second one is the
     * hostname where this is happening. Example: gnome@gmail.com and
     * mail.google.com.
     */
    label = g_strdup_printf (_("Password for %s in a form in %s"),
                             username, fake_uri_str);
  } else {
    /* Translators: The first %s is the hostname where this is happening.
     * Example: mail.google.com.
     */
    label = g_strdup_printf (_("Password in a form in %s"), fake_uri_str);
  }
  secret_service_store (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                        attributes, NULL, label, value,
                        NULL,
                        (GAsyncReadyCallback)store_form_password_cb,
                        g_object_ref (task));

  g_free (label);
  secret_value_unref (value);
  g_hash_table_unref (attributes);
  soup_uri_free (fake_uri);
  g_free (fake_uri_str);
  g_object_unref (task);
}


gboolean
ephy_form_auth_data_store_finish (GAsyncResult *result,
                                  GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct
{
  EphyFormAuthDataQueryCallback callback;
  gpointer data;
  GDestroyNotify destroy_data;
} EphyFormAuthDataQueryClosure;

static void
ephy_form_auth_data_query_closure_free (EphyFormAuthDataQueryClosure *closure)
{
  if (closure->destroy_data)
    closure->destroy_data (closure->data);

  g_slice_free (EphyFormAuthDataQueryClosure, closure);
}

static void
search_form_data_cb (SecretService *service,
                     GAsyncResult *res,
                     EphyFormAuthDataQueryClosure *closure)
{
  GList *results;
  SecretItem *item;
  const char* username = NULL, *password = NULL;
  SecretValue *value = NULL;
  GHashTable *attributes = NULL;
  GError *error = NULL;

  results = secret_service_search_finish (service, res, &error);
  if (error) {
    g_warning ("Couldn't retrieve form data: %s", error->message);
    g_error_free (error);
    goto out;
  }

  if (!results)
    goto out;

  item = (SecretItem*)results->data;
  attributes = secret_item_get_attributes (item);
  username = g_hash_table_lookup (attributes, USERNAME_KEY);
  value = secret_item_get_secret (item);
  password = secret_value_get (value, NULL);

  g_list_free_full (results, (GDestroyNotify)g_object_unref);

out:
  if (closure->callback)
    closure->callback (username, password, closure->data);

  if (value)
    secret_value_unref (value);
  if (attributes)
    g_hash_table_unref (attributes);

  ephy_form_auth_data_query_closure_free (closure);
}

void
ephy_form_auth_data_query (const char *uri,
                           const char *form_username,
                           const char *form_password,
                           const char *username,
                           EphyFormAuthDataQueryCallback callback,
                           gpointer user_data,
                           GDestroyNotify destroy_data)
{
  SoupURI *key;
  char *key_str;
  EphyFormAuthDataQueryClosure *closure;
  GHashTable *attributes;

  g_return_if_fail (uri);
  g_return_if_fail (form_password);

  key = soup_uri_new (uri);
  g_return_if_fail (key);

  if (!form_username && g_strcmp0 (form_password, "adminpw") == 0)
    normalize_and_prepare_uri (key, FALSE);
  else
    normalize_and_prepare_uri (key, TRUE);

  key_str = soup_uri_to_string (key, FALSE);

  attributes = ephy_form_auth_data_get_secret_attributes_table (key_str,
                                                                form_username,
                                                                form_password,
                                                                username);

  closure = g_slice_new0 (EphyFormAuthDataQueryClosure);
  closure->callback = callback;
  closure->data = user_data;
  closure->destroy_data = destroy_data;

  secret_service_search (NULL,
                         EPHY_FORM_PASSWORD_SCHEMA,
                         attributes,
                         SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                         NULL, (GAsyncReadyCallback)search_form_data_cb,
                         closure);

  g_hash_table_unref (attributes);
  soup_uri_free (key);
  g_free (key_str);
}

static EphyFormAuthData *
ephy_form_auth_data_new (const char *form_username,
                         const char *form_password,
                         const char *username)
{
  EphyFormAuthData *data;

  data = g_slice_new (EphyFormAuthData);
  data->form_username = g_strdup (form_username);
  data->form_password = g_strdup (form_password);
  data->username = g_strdup (username);

  return data;
}

static void
ephy_form_auth_data_free (EphyFormAuthData *data)
{
  g_free (data->form_username);
  g_free (data->form_password);
  g_free (data->username);

  g_slice_free (EphyFormAuthData, data);
}

static void
screcet_service_search_finished (SecretService *service,
                                 GAsyncResult *result,
                                 EphyFormAuthDataCache *cache)
{
  GList *results, *p;
  GError *error = NULL;

  results = secret_service_search_finish (service, result, &error);
  if (error != NULL) {
    g_warning ("Error caching form data: %s", error->message);
    g_error_free (error);
    return;
  }

  for (p = results; p; p = p->next) {
    SecretItem *item = (SecretItem *)p->data;
    GHashTable *attributes;
    char *host;

    attributes = secret_item_get_attributes (item);
    host = ephy_string_get_host_name (g_hash_table_lookup (attributes, URI_KEY));
    ephy_form_auth_data_cache_add (cache, host,
                                   g_hash_table_lookup (attributes, FORM_USERNAME_KEY),
                                   g_hash_table_lookup (attributes, FORM_PASSWORD_KEY),
                                   g_hash_table_lookup (attributes, USERNAME_KEY));

    g_free (host);
    g_hash_table_unref (attributes);
  }

  g_list_free_full (results, g_object_unref);
}

static void
ephy_form_auth_data_cache_init (EphyFormAuthDataCache *cache)
{
  GHashTable *attributes;

  attributes = secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA, NULL);
  secret_service_search (NULL,
                         EPHY_FORM_PASSWORD_SCHEMA,
                         attributes,
                         SECRET_SEARCH_UNLOCK | SECRET_SEARCH_ALL,
                         NULL,
                         (GAsyncReadyCallback)screcet_service_search_finished,
                         cache);
  g_hash_table_unref (attributes);
}

struct _EphyFormAuthDataCache {
  GHashTable  *form_auth_data_map;
};

EphyFormAuthDataCache *
ephy_form_auth_data_cache_new (void)
{
  EphyFormAuthDataCache *cache = g_slice_new (EphyFormAuthDataCache);

  cache->form_auth_data_map = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     g_free,
                                                     NULL);
  ephy_form_auth_data_cache_init (cache);

  return cache;
}

static void
form_auth_data_map_free_value (gpointer key,
                               gpointer value,
                               gpointer user_data)
{
  g_slist_free_full ((GSList *)value, (GDestroyNotify)ephy_form_auth_data_free);
}

void
ephy_form_auth_data_cache_free (EphyFormAuthDataCache *cache)
{
  g_return_if_fail (cache);

  g_hash_table_foreach (cache->form_auth_data_map,
                        (GHFunc)form_auth_data_map_free_value,
                        NULL);
  g_hash_table_destroy (cache->form_auth_data_map);

  g_slice_free (EphyFormAuthDataCache, cache);
}

void
ephy_form_auth_data_cache_add (EphyFormAuthDataCache *cache,
                               const char *uri,
                               const char *form_username,
                               const char *form_password,
                               const char *username)
{
  EphyFormAuthData *data;
  GSList *l;

  g_return_if_fail (cache);
  g_return_if_fail (uri);
  g_return_if_fail (form_password);

  data = ephy_form_auth_data_new (form_username, form_password, username);
  l = g_hash_table_lookup (cache->form_auth_data_map, uri);
  l = g_slist_append (l, data);
  g_hash_table_replace (cache->form_auth_data_map,
                        g_strdup (uri), l);
}

GSList *
ephy_form_auth_data_cache_get_list (EphyFormAuthDataCache *cache,
                                    const char *uri)
{
  g_return_val_if_fail (cache, NULL);
  g_return_val_if_fail (uri, NULL);

  return g_hash_table_lookup (cache->form_auth_data_map, uri);
}

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
normalize_and_prepare_uri (SoupURI *uri)
{
  g_assert (uri != NULL);

  /* We normalize https? schemes here so that we use passwords
   * we stored in https sites in their http counterparts, and
   * vice-versa. */
  if (uri->scheme == SOUP_URI_SCHEME_HTTPS)
    soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTP);

  soup_uri_set_path (uri, "/");
}

static GHashTable *
ephy_form_auth_data_get_secret_attributes_table (const char *uri,
                                                 const char *field_username,
                                                 const char *field_password,
                                                 const char *username)
{
  return secret_attributes_build (EPHY_FORM_PASSWORD_SCHEMA,
                                  URI_KEY, uri,
                                  FORM_USERNAME_KEY, field_username,
                                  FORM_PASSWORD_KEY, field_password,
                                  username ? USERNAME_KEY : NULL, username,
                                  NULL);
}

static void
store_form_password_cb (SecretService *service,
                        GAsyncResult *res,
                        GSimpleAsyncResult *async)
{
  GError *error = NULL;

  secret_service_store_finish (service, res, &error);
  if (error != NULL)
    g_simple_async_result_take_error (async, error);

  g_simple_async_result_complete (async);
  g_object_unref (async);
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
  GSimpleAsyncResult *res;

  g_return_if_fail (uri);
  g_return_if_fail (form_username);
  g_return_if_fail (form_password);
  g_return_if_fail (username);
  g_return_if_fail (password);

  fake_uri = soup_uri_new (uri);
  g_return_if_fail (fake_uri);

  res = g_simple_async_result_new (NULL, callback, userdata, ephy_form_auth_data_store);

  normalize_and_prepare_uri (fake_uri);
  fake_uri_str = soup_uri_to_string (fake_uri, FALSE);
  value = secret_value_new (password, -1, "text/plain");
  attributes = ephy_form_auth_data_get_secret_attributes_table (fake_uri_str, form_username,
                                                                form_password, username);
  /* Translators: The first %s is the username and the second one is the
   * hostname where this is happening. Example: gnome@gmail.com and
   * mail.google.com.
   */
  label = g_strdup_printf (_("Password for %s in a form in %s"),
                           username, fake_uri_str);
  secret_service_store (NULL, EPHY_FORM_PASSWORD_SCHEMA,
                        attributes, NULL, label, value,
                        NULL,
                        (GAsyncReadyCallback)store_form_password_cb,
                        g_object_ref (res));

  g_free (label);
  secret_value_unref (value);
  g_hash_table_unref (attributes);
  soup_uri_free (fake_uri);
  g_free (fake_uri_str);
  g_object_unref (res);
}


gboolean
ephy_form_auth_data_store_finish (GAsyncResult *result,
                                  GError **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL, ephy_form_auth_data_store), FALSE);

  return !g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error);
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
                           EphyFormAuthDataQueryCallback callback,
                           gpointer user_data,
                           GDestroyNotify destroy_data)
{
  SoupURI *key;
  char *key_str;
  EphyFormAuthDataQueryClosure *closure;
  GHashTable *attributes;

  g_return_if_fail (uri);
  g_return_if_fail (form_username);
  g_return_if_fail (form_password);

  key = soup_uri_new (uri);
  g_return_if_fail (key);

  normalize_and_prepare_uri (key);

  key_str = soup_uri_to_string (key, FALSE);

  attributes = ephy_form_auth_data_get_secret_attributes_table (key_str, form_username,
                                                                form_password, NULL);

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


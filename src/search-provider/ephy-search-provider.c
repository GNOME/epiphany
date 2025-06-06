/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright (c) 2013 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#include "ephy-search-provider.h"

#include "ephy-bookmarks-manager.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "ephy-profile-utils.h"
#include "ephy-shell.h"
#include "ephy-suggestion-model.h"
#include "ephy-uri-helpers.h"

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <string.h>

struct _EphySearchProvider {
  GApplication parent_instance;

  EphyShellSearchProvider2 *skeleton;
  GCancellable *cancellable;

  GSettings *settings;
  EphyBookmarksManager *bookmarks_manager;
  EphySuggestionModel *model;
};

struct _EphySearchProviderClass {
  EphyEmbedShellClass parent_class;
};

G_DEFINE_FINAL_TYPE (EphySearchProvider, ephy_search_provider, EPHY_TYPE_EMBED_SHELL)

#define INACTIVITY_TIMEOUT 60 * 1000 /* One minute, in milliseconds */

static void
on_model_updated (GObject      *source_object,
                  GAsyncResult *result,
                  GTask        *task)
{
  EphySearchProvider *self = g_task_get_source_object (task);
  EphySuggestion *suggestion;
  GPtrArray *results;
  const char *search_string;
  guint n_items;
  GError *error = NULL;
  results = g_ptr_array_new ();

  if (ephy_suggestion_model_query_finish (self->model,
                                          result,
                                          &error)) {
    n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
    for (guint i = 0; i < n_items; i++) {
      suggestion = g_list_model_get_item (G_LIST_MODEL (self->model), i);
      g_ptr_array_add (results, g_strdup (ephy_suggestion_get_uri (suggestion)));
    }
  } else {
    g_warning ("Failed to query suggestion model: %s", error->message);
    g_error_free (error);
  }

  search_string = g_task_get_task_data (task);
  g_ptr_array_add (results, g_strdup_printf ("special:search:%s", search_string));
  g_ptr_array_add (results, NULL);

  g_task_return_pointer (task,
                         g_ptr_array_free (results, FALSE),
                         (GDestroyNotify)g_strfreev);
}

static char **
gather_results_finish (EphySearchProvider  *self,
                       GAsyncResult        *result,
                       GError             **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gather_results_async (EphySearchProvider   *self,
                      char                **terms,
                      GCancellable         *cancellable,
                      GAsyncReadyCallback   callback,
                      gpointer              user_data)
{
  GTask *task;
  char *search_string;

  task = g_task_new (self, cancellable, callback, user_data);

  search_string = g_strjoinv (" ", terms);
  g_task_set_task_data (task, search_string, g_free);

  ephy_suggestion_model_query_async (self->model,
                                     search_string,
                                     FALSE,
                                     FALSE,
                                     cancellable,
                                     (GAsyncReadyCallback)on_model_updated,
                                     task);
}

static void
complete_request (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  EphySearchProvider *self = EPHY_SEARCH_PROVIDER (object);
  char **results;
  GError *error;
  error = NULL;

  results = gather_results_finish (self, result, &error);

  if (results) {
    g_dbus_method_invocation_return_value (user_data,
                                           g_variant_new ("(^as)", results));
  } else {
    g_dbus_method_invocation_take_error (user_data, error);
  }

  g_application_release (G_APPLICATION (self));
}

static gboolean
handle_get_initial_result_set (EphyShellSearchProvider2  *skeleton,
                               GDBusMethodInvocation     *invocation,
                               char                     **terms,
                               EphySearchProvider        *self)
{
  g_application_hold (G_APPLICATION (self));
  g_cancellable_reset (self->cancellable);

  gather_results_async (self, terms, self->cancellable,
                        complete_request, invocation);

  return TRUE;
}

static gboolean
handle_get_subsearch_result_set (EphyShellSearchProvider2  *skeleton,
                                 GDBusMethodInvocation     *invocation,
                                 char                     **previous_results,
                                 char                     **terms,
                                 EphySearchProvider        *self)
{
  g_application_hold (G_APPLICATION (self));
  g_cancellable_reset (self->cancellable);

  if (g_strv_length (terms) == 1) {
    gboolean is_uri = ephy_embed_utils_address_is_valid (terms[0]);

    if (is_uri) {
      GPtrArray *results = g_ptr_array_new ();

      g_ptr_array_add (results, g_strdup_printf ("special:load:%s", terms[0]));
      g_ptr_array_add (results, NULL);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(^as)", g_ptr_array_free (results, FALSE)));

      g_application_release (G_APPLICATION (self));
      return TRUE;
    }
  }

  gather_results_async (self, terms, self->cancellable,
                        complete_request, invocation);

  return TRUE;
}

static gboolean
handle_get_result_metas (EphyShellSearchProvider2  *skeleton,
                         GDBusMethodInvocation     *invocation,
                         char                     **results,
                         EphySearchProvider        *self)
{
  int i;
  GVariantBuilder builder;

  g_application_hold (G_APPLICATION (self));
  g_cancellable_cancel (self->cancellable);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (i = 0; results[i]; i++) {
    if (g_str_has_prefix (results[i], "special:search:")) {
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}",
                             "id", g_variant_new_string ("special:search"));
      g_variant_builder_add (&builder, "{sv}",
                             "name", g_variant_new_take_string (g_strdup_printf (_("Search the web for “%s”"),
                                                                                 results[i] + strlen ("special:search:"))));
      g_variant_builder_add (&builder, "{sv}",
                             "gicon", g_variant_new_string (APPLICATION_ID));
      g_variant_builder_close (&builder);
    } else if (g_str_has_prefix (results[i], "special:load:")) {
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}",
                             "id", g_variant_new_string ("special:load"));
      g_variant_builder_add (&builder, "{sv}",
                             "name", g_variant_new_take_string (g_strdup_printf (_("Load “%s”"),
                                                                                 results[i] + strlen ("special:load:"))));
      g_variant_builder_add (&builder, "{sv}",
                             "gicon", g_variant_new_string (APPLICATION_ID));
      g_variant_builder_close (&builder);
    } else {
      EphySuggestion *suggestion;
      const char *title;
      const char *uri;
      g_autofree char *decoded_uri = NULL;

      suggestion = ephy_suggestion_model_get_suggestion_with_uri (self->model, results[i]);
      if (!suggestion)
        continue;

      title = ephy_suggestion_get_unescaped_title (suggestion);
      uri = ephy_suggestion_get_uri (suggestion);
      decoded_uri = ephy_uri_decode (uri);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}",
                             "id", g_variant_new_string (decoded_uri));
      g_variant_builder_add (&builder, "{sv}",
                             "name", g_variant_new_string (title));
      g_variant_builder_add (&builder, "{sv}",
                             "gicon", g_variant_new_string ("text-html"));
      g_variant_builder_close (&builder);
    }
  }

  ephy_shell_search_provider2_complete_get_result_metas (skeleton,
                                                         invocation,
                                                         g_variant_builder_end (&builder));

  g_application_release (G_APPLICATION (self));

  return TRUE;
}

static void
launch_uri (const char *uri,
            guint       timestamp)
{
  g_autofree char *str = NULL;

  /* TODO: Handle the timestamp */
  str = g_strdup_printf ("epiphany %s", uri);
  g_spawn_command_line_async (str, NULL);
}

static void
launch_search (EphySearchProvider  *self,
               char               **terms,
               guint                timestamp)
{
  g_autofree char *search_string = NULL;
  g_autofree char *effective_url = NULL;

  search_string = g_strjoinv (" ", terms);
  effective_url = ephy_embed_utils_autosearch_address (search_string);

  launch_uri (effective_url, timestamp);
}

static gboolean
handle_activate_result (EphyShellSearchProvider2  *skeleton,
                        GDBusMethodInvocation     *invocation,
                        char                      *identifier,
                        char                     **terms,
                        guint                      timestamp,
                        EphySearchProvider        *self)
{
  g_application_hold (G_APPLICATION (self));
  g_cancellable_cancel (self->cancellable);

  if (strcmp (identifier, "special:search") == 0)
    launch_search (self, terms, timestamp);
  else if (strcmp (identifier, "special:load") == 0)
    launch_uri (terms[0], timestamp);
  else
    launch_uri (identifier, timestamp);

  ephy_shell_search_provider2_complete_activate_result (skeleton, invocation);
  g_application_release (G_APPLICATION (self));

  return TRUE;
}

static gboolean
handle_launch_search (EphyShellSearchProvider2  *skeleton,
                      GDBusMethodInvocation     *invocation,
                      char                     **terms,
                      guint                      timestamp,
                      EphySearchProvider        *self)
{
  g_application_hold (G_APPLICATION (self));
  g_cancellable_cancel (self->cancellable);

  launch_search (self, terms, timestamp);

  ephy_shell_search_provider2_complete_launch_search (skeleton, invocation);
  g_application_release (G_APPLICATION (self));

  return TRUE;
}

static void
ephy_search_provider_init (EphySearchProvider *self)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  g_application_set_flags (G_APPLICATION (self), G_APPLICATION_IS_SERVICE);

  self->settings = g_settings_new (EPHY_PREFS_SCHEMA);

  self->bookmarks_manager = ephy_bookmarks_manager_new ();
  self->model = ephy_suggestion_model_new (ephy_embed_shell_get_global_history_service (shell),
                                           self->bookmarks_manager);

  self->cancellable = g_cancellable_new ();

  g_application_set_inactivity_timeout (G_APPLICATION (self), INACTIVITY_TIMEOUT);
}

static gboolean
ephy_search_provider_dbus_register (GApplication     *application,
                                    GDBusConnection  *connection,
                                    const gchar      *object_path,
                                    GError          **error)
{
  EphySearchProvider *self;

  if (!G_APPLICATION_CLASS (ephy_search_provider_parent_class)->dbus_register (application,
                                                                               connection,
                                                                               object_path,
                                                                               error))
    return FALSE;

  self = EPHY_SEARCH_PROVIDER (application);
  self->skeleton = ephy_shell_search_provider2_skeleton_new ();

  g_signal_connect (self->skeleton, "handle-get-initial-result-set",
                    G_CALLBACK (handle_get_initial_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-subsearch-result-set",
                    G_CALLBACK (handle_get_subsearch_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-result-metas",
                    G_CALLBACK (handle_get_result_metas), self);
  g_signal_connect (self->skeleton, "handle-activate-result",
                    G_CALLBACK (handle_activate_result), self);
  g_signal_connect (self->skeleton, "handle-launch-search",
                    G_CALLBACK (handle_launch_search), self);

  return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                           connection, object_path, error);
}

static void
ephy_search_provider_dbus_unregister (GApplication    *application,
                                      GDBusConnection *connection,
                                      const gchar     *object_path)
{
  EphySearchProvider *self;
  GDBusInterfaceSkeleton *skeleton;

  self = EPHY_SEARCH_PROVIDER (application);
  skeleton = G_DBUS_INTERFACE_SKELETON (self->skeleton);
  if (g_dbus_interface_skeleton_has_connection (skeleton, connection))
    g_dbus_interface_skeleton_unexport_from_connection (skeleton, connection);

  g_clear_object (&self->skeleton);

  G_APPLICATION_CLASS (ephy_search_provider_parent_class)->dbus_unregister (application,
                                                                            connection,
                                                                            object_path);
}

static void
ephy_search_provider_dispose (GObject *object)
{
  EphySearchProvider *self;

  self = EPHY_SEARCH_PROVIDER (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->model);
  g_clear_object (&self->bookmarks_manager);

  G_OBJECT_CLASS (ephy_search_provider_parent_class)->dispose (object);
}

static void
ephy_search_provider_class_init (EphySearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = ephy_search_provider_dispose;

  application_class->dbus_register = ephy_search_provider_dbus_register;
  application_class->dbus_unregister = ephy_search_provider_dbus_unregister;
}

EphySearchProvider *
ephy_search_provider_new (void)
{
  g_autofree gchar *app_id = g_strconcat (APPLICATION_ID, ".SearchProvider", NULL);

  return g_object_new (EPHY_TYPE_SEARCH_PROVIDER,
                       "application-id", app_id,
                       "mode", EPHY_EMBED_SHELL_MODE_SEARCH_PROVIDER,
                       NULL);
}

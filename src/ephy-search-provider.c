/*
 * Copyright (c) 2013 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "ephy-search-provider.h"

#include "ephy-completion-model.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "ephy-profile-utils.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <libsoup/soup.h>

struct _EphySearchProvider
{
  GApplication parent;

  EphyShellSearchProvider2 *skeleton;
  GCancellable             *cancellable;

  GSettings                *settings;
  EphyHistoryService       *history_service;
  EphyBookmarks            *bookmarks;
  EphyCompletionModel      *model;
};

struct _EphySearchProviderClass
{
  GApplicationClass parent_class;
};

G_DEFINE_TYPE (EphySearchProvider, ephy_search_provider, G_TYPE_APPLICATION)

#define INACTIVITY_TIMEOUT 60 * 1000 /* One minute, in milliseconds */

static void
on_model_updated (EphyHistoryService *service,
		  gboolean            success,
		  gpointer            result_data,
		  gpointer            user_data)
{
  GTask *task = user_data;
  EphySearchProvider *self = g_task_get_source_object (task);
  GtkTreeModel *model = GTK_TREE_MODEL (self->model);
  GtkTreeIter iter;
  GPtrArray *results;
  const char *search_string;
  gboolean ok;

  results = g_ptr_array_new ();

  ok = gtk_tree_model_get_iter_first (model, &iter);
  while (ok) {
    char *result;

    result = gtk_tree_model_get_string_from_iter (model, &iter);
    g_ptr_array_add (results, result);

    ok = gtk_tree_model_iter_next (model, &iter);
  }

  search_string = g_task_get_task_data (task);
  g_ptr_array_add (results, g_strdup_printf ("special:search:%s", search_string));
  g_ptr_array_add (results, NULL);

  g_task_return_pointer (task,
                         g_ptr_array_free (results, FALSE),
                         (GDestroyNotify) g_strfreev);
}

static char **
gather_results_finish (EphySearchProvider *self,
                       GAsyncResult       *result,
                       GError            **error)
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
  g_task_set_check_cancellable (task, TRUE);

  search_string = g_strjoinv (" ", terms);
  g_task_set_task_data (task, search_string, g_free);

  ephy_completion_model_update_for_string (self->model,
                                           search_string,
                                           on_model_updated,
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
  GtkTreeModel *model = GTK_TREE_MODEL (self->model);
  GtkTreeIter iter;
  int i;
  GVariantBuilder builder;
  GIcon *favicon;
  char *name, *url;
  gboolean is_bookmark;

  g_application_hold (G_APPLICATION (self));
  g_cancellable_cancel (self->cancellable);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (i = 0; results[i]; i++)
    {
      if (g_str_has_prefix (results[i], "special:search:")) {
        g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
        g_variant_builder_add (&builder, "{sv}",
                               "id", g_variant_new_string ("special:search"));
        g_variant_builder_add (&builder, "{sv}",
                               "name", g_variant_new_take_string (g_strdup_printf(_("Search the Web for %s"),
										  results[i] + strlen("special:search:"))));
        g_variant_builder_add (&builder, "{sv}",
                               "gicon", g_variant_new_string ("web-browser"));
        g_variant_builder_close (&builder);
        continue;
      }

      if (!gtk_tree_model_get_iter_from_string (model, &iter, results[i]))
        continue;

      gtk_tree_model_get (model, &iter,
                          EPHY_COMPLETION_TEXT_COL, &name,
                          EPHY_COMPLETION_URL_COL, &url,
                          EPHY_COMPLETION_FAVICON_COL, &favicon,
                          EPHY_COMPLETION_EXTRA_COL, &is_bookmark,
                          -1);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add (&builder, "{sv}",
                             "id", g_variant_new_string (url));
      g_variant_builder_add (&builder, "{sv}",
                             "name", g_variant_new_string (name));

      if (favicon == NULL) {
	char *type;

        type = g_content_type_from_mime_type ("text/html");
        favicon = g_content_type_get_icon (type);

        if (is_bookmark) {
          GEmblem *emblem;
          GIcon *emblem_icon, *emblemed;

          emblem_icon = g_themed_icon_new ("emblem-favorite");
          emblem = g_emblem_new (emblem_icon);

          emblemed = g_emblemed_icon_new (favicon, emblem);

	  g_object_unref (emblem);
	  g_object_unref (emblem_icon);
	  g_object_unref (favicon);
	  favicon = emblemed;
	}
      }

      g_variant_builder_add (&builder, "{sv}",
			     "icon", g_icon_serialize (favicon));
      g_variant_builder_close (&builder);

      g_object_unref (favicon);
      g_free (name);
      g_free (url);
    }

  ephy_shell_search_provider2_complete_get_result_metas (skeleton,
                                                         invocation,
                                                         g_variant_builder_end (&builder));

  g_application_release (G_APPLICATION (self));

  return TRUE;
}

static void
launch_uri (const char  *uri,
            guint        timestamp)
{
  char *str;

  /* TODO: Handle the timestamp */
  str = g_strdup_printf ("epiphany %s", uri);
  g_spawn_command_line_async (str, NULL);
  g_free (str);
}

static void
launch_search (EphySearchProvider  *self,
               char               **terms,
               guint                timestamp)
{
  char *search_string, *url_search, *query_param, *effective_url;

  url_search = g_settings_get_string (self->settings, EPHY_PREFS_KEYWORD_SEARCH_URL);

  if (url_search == NULL || url_search[0] == '\0') {
    g_free (url_search);

    url_search = g_strdup (_("https://duckduckgo.com/?q=%s&amp;t=epiphany"));
  }

  search_string = g_strjoinv (" ", terms);
  query_param = soup_form_encode ("q", search_string, NULL);
  /* + 2 here is getting rid of 'q=' */
  effective_url = g_strdup_printf (url_search, query_param + 2);

  launch_uri (effective_url, timestamp);

  g_free (query_param);
  g_free (url_search);
  g_free (effective_url);
  g_free (search_string);
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
  char *filename;

  g_application_set_flags (G_APPLICATION (self), G_APPLICATION_IS_SERVICE);

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

  self->settings = g_settings_new (EPHY_PREFS_SCHEMA);

  filename = g_build_filename (ephy_dot_dir (), EPHY_HISTORY_FILE, NULL);
  self->history_service = ephy_history_service_new (filename, TRUE);
  self->bookmarks = ephy_bookmarks_new ();
  self->model = ephy_completion_model_new (self->history_service, self->bookmarks, FALSE);
  g_free (filename);

  self->cancellable = g_cancellable_new ();

  g_application_set_inactivity_timeout (G_APPLICATION (self), INACTIVITY_TIMEOUT);
}

static gboolean
ephy_search_provider_dbus_register (GApplication    *application,
                                    GDBusConnection *connection,
                                    const gchar     *object_path,
                                    GError         **error)
{
  EphySearchProvider *self;
  GDBusInterfaceSkeleton *skeleton;

  if (!G_APPLICATION_CLASS (ephy_search_provider_parent_class)->dbus_register (application,
                                                                               connection,
                                                                               object_path,
                                                                               error))
    return FALSE;

  self = EPHY_SEARCH_PROVIDER (application);
  skeleton = G_DBUS_INTERFACE_SKELETON (self->skeleton);

  return g_dbus_interface_skeleton_export (skeleton, connection, object_path, error);
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
  g_clear_object (&self->history_service);
  g_clear_object (&self->bookmarks);

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
  return g_object_new (EPHY_TYPE_SEARCH_PROVIDER,
                       "application-id", "org.gnome.EpiphanySearchProvider",
                       NULL);
}


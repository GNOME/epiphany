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

#include "ephy-bookmarks.h"
#include "ephy-completion-model.h"
#include "ephy-file-helpers.h"
#include "ephy-history-service.h"
#include "ephy-prefs.h"
#include "ephy-profile-utils.h"
#include "ephy-shell.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libsoup/soup.h>

struct _EphySearchProvider
{
  GObject parent;

  EphyShellSearchProvider2 *skeleton;
  GCancellable             *cancellable;

  GSettings                *settings;
  EphyCompletionModel      *model;
};

struct _EphySearchProviderClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (EphySearchProvider, ephy_search_provider, G_TYPE_OBJECT);

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

  g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static gboolean
handle_get_initial_result_set (EphyShellSearchProvider2  *skeleton,
                               GDBusMethodInvocation     *invocation,
                               char                     **terms,
                               EphySearchProvider        *self)
{
  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));
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
  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));
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

  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));
  g_cancellable_cancel (self->cancellable);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (i = 0; results[i]; i++)
    {
      if (g_str_has_prefix (results[i], "special:search:") == 0) {
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

  g_application_release (G_APPLICATION (ephy_shell_get_default ()));

  return TRUE;
}

static void
launch_uri (const char  *uri,
            guint        timestamp)
{
  const char *uris[2];

  uris[0] = uri;
  uris[1] = NULL;

  ephy_shell_open_uris (ephy_shell_get_default (), uris, 0, timestamp);
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

    url_search = g_strdup (_("http://duckduckgo.com/?q=%s&amp;t=epiphany"));
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
  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));
  g_cancellable_cancel (self->cancellable);

  if (strcmp (identifier, "special:search") == 0)
    launch_search (self, terms, timestamp);
  else
    launch_uri (identifier, timestamp);

  ephy_shell_search_provider2_complete_activate_result (skeleton, invocation);
  g_application_release (G_APPLICATION (ephy_shell_get_default ()));

  return TRUE;
}

static gboolean
handle_launch_search (EphyShellSearchProvider2  *skeleton,
                      GDBusMethodInvocation     *invocation,
                      char                     **terms,
                      guint                      timestamp,
                      EphySearchProvider        *self)
{
  g_application_hold (G_APPLICATION (ephy_shell_get_default ()));
  g_cancellable_cancel (self->cancellable);

  launch_search (self, terms, timestamp);

  ephy_shell_search_provider2_complete_launch_search (skeleton, invocation);
  g_application_release (G_APPLICATION (ephy_shell_get_default ()));

  return TRUE;
}

static void
ephy_search_provider_init (EphySearchProvider *self)
{
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

  self->model = ephy_completion_model_new ();
  self->cancellable = g_cancellable_new ();
}

gboolean
ephy_search_provider_dbus_register (EphySearchProvider  *self,
                                  GDBusConnection   *connection,
                                  const gchar       *object_path,
                                  GError           **error)
{
  GDBusInterfaceSkeleton *skeleton;

  skeleton = G_DBUS_INTERFACE_SKELETON (self->skeleton);

  return g_dbus_interface_skeleton_export (skeleton, connection, object_path, error);
}

void
ephy_search_provider_dbus_unregister (EphySearchProvider *self,
                                    GDBusConnection  *connection,
                                    const gchar      *object_path)
{
  GDBusInterfaceSkeleton *skeleton;

  skeleton = G_DBUS_INTERFACE_SKELETON (self->skeleton);

  if (g_dbus_interface_skeleton_has_connection (skeleton, connection))
      g_dbus_interface_skeleton_unexport_from_connection (skeleton, connection);
}

static void
ephy_search_provider_dispose (GObject *object)
{
  EphySearchProvider *self;

  self = EPHY_SEARCH_PROVIDER (object);

  g_clear_object (&self->skeleton);
  g_clear_object (&self->settings);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (ephy_search_provider_parent_class)->dispose (object);
}

static void
ephy_search_provider_class_init (EphySearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_search_provider_dispose;
}

EphySearchProvider *
ephy_search_provider_new (void)
{
  return g_object_new (EPHY_TYPE_SEARCH_PROVIDER, NULL);
}


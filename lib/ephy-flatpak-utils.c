/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Igalia S.L.
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

/* For O_PATH */
#define _GNU_SOURCE

#include <config.h>
#include "ephy-flatpak-utils.h"

#ifdef __linux__

#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

gboolean
ephy_is_running_inside_flatpak (void)
{
  static _Thread_local gboolean decided = FALSE;
  static _Thread_local gboolean under_flatpak = FALSE;

  if (decided)
    return under_flatpak;

  under_flatpak = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
  decided = TRUE;
  return under_flatpak;
}

static void
response_cb (GDBusConnection *connection,
             const char      *sender_name,
             const char      *object_path,
             const char      *interface_name,
             const char      *signal_name,
             GVariant        *parameters,
             gpointer         user_data)
{
  GTask *task = G_TASK (user_data);
  guint32 response;
  guint signal_id;

  signal_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (task), "signal-id"));
  g_dbus_connection_signal_unsubscribe (connection, signal_id);

  g_task_return_error_if_cancelled (task);

  g_variant_get (parameters, "(u@a{sv})", &response, NULL);
  if (response == 0)
    g_task_return_boolean (task, TRUE);
  else if (response == 1)
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Operation cancelled");
  else /* yes, this is abuse of G_IO_ERROR, but I don't want to make a new error quark */
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Portal failed to open file");
}

static void
open_file_complete_cb (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (source);
  GTask *task = G_TASK (user_data);
  GVariant *return_value = NULL;
  const char *handle;
  char *object_path = NULL;
  GError *error = NULL;

  return_value = g_dbus_proxy_call_with_unix_fd_list_finish (proxy, NULL, result, &error);
  if (!return_value) {
    g_warning ("Failed to open file via portal: %s", error->message);
    g_task_return_error (task, error);
    goto out;
  }

  /* Copied from Gio source code. To understand the signal resubscription
   * dance, refer to the org.freedesktop.portal.Request documentation. */
  g_variant_get (return_value, "(o)", &object_path);
  handle = (const char *)g_object_get_data (G_OBJECT (task), "handle");
  if (strcmp (handle, object_path) != 0) {
    GDBusConnection *connection;
    guint signal_id;

    connection = g_dbus_proxy_get_connection (proxy);
    signal_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (task), "signal-id"));
    g_dbus_connection_signal_unsubscribe (connection, signal_id);

    signal_id = g_dbus_connection_signal_subscribe (connection,
                                                    "org.freedesktop.portal.Desktop",
                                                    "org.freedesktop.portal.Request",
                                                    "Response",
                                                    handle,
                                                    NULL,
                                                    G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                    response_cb,
                                                    task,
                                                    NULL);
    g_object_set_data (G_OBJECT (task), "signal-id", GUINT_TO_POINTER (signal_id));
  }

out:
  if (return_value)
    g_variant_unref (return_value);
  if (object_path)
    g_free (object_path);
}

static void
portal_proxy_created_cb (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GTask *task;
  GDBusProxy *proxy;
  GVariantBuilder builder;
  GDBusConnection *connection;
  GUnixFDList *fd_list;
  int fd;
  guint signal_id;
  char *sender;
  char *token;
  char *handle;
  GError *error = NULL;

  task = G_TASK (user_data);
  fd = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "fd"));

  proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
  if (!proxy) {
    g_warning ("Failed to create D-Bus proxy for OpenURI portal: %s", error->message);
    g_task_return_error (task, error);
    close (fd);
    return;
  }

  g_object_set_data_full (G_OBJECT (task), "proxy", proxy, g_object_unref);

  /* Refer to org.freedesktop.portal.Request documentation. */
  connection = g_dbus_proxy_get_connection (proxy);
  sender = g_strdup (g_dbus_connection_get_unique_name (connection) + 1);
  for (guint i = 0; sender[i] != '\0'; i++)
    if (sender[i] == '.')
      sender[i] = '_';
  token = g_strdup_printf ("epiphany%u", g_random_int ());
  handle = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
  g_object_set_data_full (G_OBJECT (task), "handle", handle, g_free);
  g_free (sender);

  signal_id = g_dbus_connection_signal_subscribe (connection,
                                                  "org.freedesktop.portal.Desktop",
                                                  "org.freedesktop.portal.Request",
                                                  "Response",
                                                  handle,
                                                  NULL,
                                                  G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                  response_cb,
                                                  task,
                                                  NULL);
  g_object_set_data (G_OBJECT (task), "signal-id", GUINT_TO_POINTER (signal_id));

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", "handle_token", g_variant_new_string (token));
  g_free (token);

  fd_list = g_unix_fd_list_new_from_array (&fd, 1);
  g_dbus_proxy_call_with_unix_fd_list (proxy,
                                       "OpenFile",
                                       g_variant_new ("(s@h@a{sv})",
                                                      "",
                                                      g_variant_new("h", 0),
                                                      g_variant_builder_end (&builder)),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       fd_list,
                                       NULL,
                                       open_file_complete_cb,
                                       task);
  g_object_unref (fd_list);
}

void
ephy_open_file_via_flatpak_portal (const char          *path,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GTask *task;
  int fd;

  fd = open (path, O_PATH | O_CLOEXEC);
  if (fd == -1) {
    g_warning ("Failed to open %s: %s", path, g_strerror (errno));
    return;
  }

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_object_set_data (G_OBJECT (task), "fd", GINT_TO_POINTER (fd));

  /* We have to do this manually. The recommended solution is to use
   * g_app_info_launch_default_for_uri(), but that will fail if trying
   * to open anything that Epiphany itself can open... like text files.
   * The file will be opened in Epiphany, but we want to get an app
   * chooser. Otherwise, trying to view page source is just going to open
   * another browser tab displaying the page in question. Ugh. */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                            NULL,
                            "org.freedesktop.portal.Desktop",
                            "/org/freedesktop/portal/desktop",
                            "org.freedesktop.portal.OpenURI",
                            NULL,
                            portal_proxy_created_cb,
                            task);
}

gboolean
ephy_open_file_via_flatpak_portal_finish (GAsyncResult  *result,
                                          GError       **error)
{
  gboolean ret;

  ret = g_task_propagate_boolean (G_TASK (result), error);
  g_object_unref (result);
  return ret;
}

#else /* __linux__ */

gboolean
ephy_is_running_inside_flatpak (void)
{
  return FALSE;
}

void
ephy_open_file_via_flatpak_portal (const char          *path,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_assert_not_reached ();
}

gboolean
ephy_open_file_via_flatpak_portal_finish (GAsyncResult  *result,
                                          GError       **error)
{
  g_assert_not_reached ();
}

#endif /* __linux__ */

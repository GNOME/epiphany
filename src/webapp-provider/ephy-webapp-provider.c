/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright (c) 2021 Matthew Leeds <mwleeds@protonmail.com>
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

#include "ephy-webapp-provider.h"

#include "ephy-web-app-utils.h"
#include "ephy-flatpak-utils.h"

#include <gio/gio.h>
#include <glib/gi18n.h>

struct _EphyWebAppProviderService {
  GApplication parent_instance;

  EphyWebAppProvider *skeleton;
};

struct _EphyWebAppProviderServiceClass {
  GApplicationClass parent_class;
};

G_DEFINE_FINAL_TYPE (EphyWebAppProviderService, ephy_web_app_provider_service, G_TYPE_APPLICATION)

#define INACTIVITY_TIMEOUT 60 * 1000 /* One minute, in milliseconds */

typedef enum {
  EPHY_WEBAPP_PROVIDER_ERROR_FAILED,
  EPHY_WEBAPP_PROVIDER_ERROR_INVALID_ARGS,
  EPHY_WEBAPP_PROVIDER_ERROR_NOT_INSTALLED,
  EPHY_WEBAPP_PROVIDER_ERROR_LAST = EPHY_WEBAPP_PROVIDER_ERROR_NOT_INSTALLED, /*< skip >*/
} EphyWebAppProviderError;

static const GDBusErrorEntry ephy_webapp_provider_error_entries[] = {
  { EPHY_WEBAPP_PROVIDER_ERROR_FAILED, "org.gnome.Epiphany.WebAppProvider.Error.Failed" },
  { EPHY_WEBAPP_PROVIDER_ERROR_INVALID_ARGS, "org.gnome.Epiphany.WebAppProvider.Error.InvalidArgs" },
  { EPHY_WEBAPP_PROVIDER_ERROR_NOT_INSTALLED, "org.gnome.Epiphany.WebAppProvider.Error.NotInstalled" },
};

/* Ensure that every error code has an associated D-Bus error name */
G_STATIC_ASSERT (G_N_ELEMENTS (ephy_webapp_provider_error_entries) == EPHY_WEBAPP_PROVIDER_ERROR_LAST + 1);

#define EPHY_WEBAPP_PROVIDER_ERROR (ephy_webapp_provider_error_quark ())
GQuark
ephy_webapp_provider_error_quark (void)
{
  static gsize quark = 0;
  g_dbus_error_register_error_domain ("ephy-webapp-provider-error-quark",
                                      &quark,
                                      ephy_webapp_provider_error_entries,
                                      G_N_ELEMENTS (ephy_webapp_provider_error_entries));
  return (GQuark)quark;
}

static gboolean
handle_get_installed_apps (EphyWebAppProvider        *skeleton,
                           GDBusMethodInvocation     *invocation,
                           EphyWebAppProviderService *self)
{
  g_auto (GStrv) desktop_ids = NULL;

  g_debug ("%s", G_STRFUNC);

  g_application_hold (G_APPLICATION (self));

  desktop_ids = ephy_web_application_get_desktop_id_list ();

  ephy_web_app_provider_complete_get_installed_apps (skeleton, invocation,
                                                     (const gchar * const *)desktop_ids);

  g_application_release (G_APPLICATION (self));

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_install (EphyWebAppProvider        *skeleton,
                GDBusMethodInvocation     *invocation,
                char                      *url,
                char                      *name,
                char                      *install_token,
                EphyWebAppProviderService *self)
{
  g_autofree char *id = NULL;
  g_autofree char *desktop_path = NULL;
  g_autofree char *desktop_file_id = NULL;
  g_autoptr (GError) local_error = NULL;

  g_debug ("%s", G_STRFUNC);

  g_application_hold (G_APPLICATION (self));

  /* We need an install token acquired by a trusted system component such as
   * gnome-software because otherwise the Flatpak/Snap sandbox prevents us from
   * installing the app without using a portal (which would not be appropriate
   * since Epiphany is not the focused application). We use the same code path
   * when not running under a sandbox too.
   */
  if (!install_token || *install_token == '\0') {
    g_dbus_method_invocation_return_error (invocation, EPHY_WEBAPP_PROVIDER_ERROR,
                                           EPHY_WEBAPP_PROVIDER_ERROR_INVALID_ARGS,
                                           _("The install_token is required for the Install() method"));
    goto out;
  }
  if (!g_uri_is_valid (url, G_URI_FLAGS_PARSE_RELAXED, NULL)) {
    g_dbus_method_invocation_return_error (invocation, EPHY_WEBAPP_PROVIDER_ERROR,
                                           EPHY_WEBAPP_PROVIDER_ERROR_INVALID_ARGS,
                                           _("The url passed was not valid: ‘%s’"), url);
    goto out;
  }
  if (!name || *name == '\0') {
    g_dbus_method_invocation_return_error (invocation, EPHY_WEBAPP_PROVIDER_ERROR,
                                           EPHY_WEBAPP_PROVIDER_ERROR_INVALID_ARGS,
                                           _("The name passed was not valid"));
    goto out;
  }

  id = ephy_web_application_get_app_id_from_name (name);

  if (!ephy_web_application_create (id, url,
                                    install_token,
                                    EPHY_WEB_APPLICATION_NONE,
                                    &local_error)) {
    g_dbus_method_invocation_return_error (invocation, EPHY_WEBAPP_PROVIDER_ERROR,
                                           EPHY_WEBAPP_PROVIDER_ERROR_FAILED,
                                           _("Installing the web application ‘%s’ (%s) failed: %s"),
                                           name, url, local_error->message);
    g_clear_error (&local_error);
    goto out;
  }

  desktop_path = ephy_web_application_get_desktop_path (id);
  desktop_file_id = g_path_get_basename (desktop_path);
  ephy_web_app_provider_complete_install (skeleton, invocation, desktop_file_id);

out:
  g_application_release (G_APPLICATION (self));

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static gboolean
handle_uninstall (EphyWebAppProvider        *skeleton,
                  GDBusMethodInvocation     *invocation,
                  char                      *desktop_file_id,
                  EphyWebAppProviderService *self)
{
  EphyWebAppFound app_found;

  g_debug ("%s", G_STRFUNC);

  g_application_hold (G_APPLICATION (self));

  if (!desktop_file_id || !g_str_has_suffix (desktop_file_id, ".desktop") ||
      !g_str_has_prefix (desktop_file_id, EPHY_WEB_APP_GAPPLICATION_ID_PREFIX)) {
    g_dbus_method_invocation_return_error (invocation, EPHY_WEBAPP_PROVIDER_ERROR,
                                           EPHY_WEBAPP_PROVIDER_ERROR_INVALID_ARGS,
                                           _("The desktop file ID passed ‘%s’ was not valid"),
                                           desktop_file_id ? desktop_file_id : "(null)");
    goto out;
  }

  if (!ephy_web_application_delete_by_desktop_file_id (desktop_file_id, &app_found)) {
    if (app_found == EPHY_WEB_APP_NOT_FOUND) {
      g_dbus_method_invocation_return_error (invocation, EPHY_WEBAPP_PROVIDER_ERROR,
                                             EPHY_WEBAPP_PROVIDER_ERROR_NOT_INSTALLED,
                                             _("The web application ‘%s’ does not exist"),
                                             desktop_file_id);
    } else {
      g_dbus_method_invocation_return_error (invocation, EPHY_WEBAPP_PROVIDER_ERROR,
                                             EPHY_WEBAPP_PROVIDER_ERROR_FAILED,
                                             _("The web application ‘%s’ could not be deleted"),
                                             desktop_file_id);
    }
    goto out;
  }

  ephy_web_app_provider_complete_uninstall (skeleton, invocation);

out:
  g_application_release (G_APPLICATION (self));

  return G_DBUS_METHOD_INVOCATION_HANDLED;
}

static void
ephy_web_app_provider_service_init (EphyWebAppProviderService *self)
{
  g_application_set_flags (G_APPLICATION (self), G_APPLICATION_IS_SERVICE);

  g_application_set_inactivity_timeout (G_APPLICATION (self), INACTIVITY_TIMEOUT);
}

static gboolean
ephy_web_app_provider_service_dbus_register (GApplication     *application,
                                             GDBusConnection  *connection,
                                             const gchar      *object_path,
                                             GError          **error)
{
  EphyWebAppProviderService *self;

  g_debug ("registering at object path %s", object_path);

  if (!G_APPLICATION_CLASS (ephy_web_app_provider_service_parent_class)->dbus_register (application,
                                                                                        connection,
                                                                                        object_path,
                                                                                        error))
    return FALSE;

  self = EPHY_WEB_APP_PROVIDER_SERVICE (application);
  self->skeleton = ephy_web_app_provider_skeleton_new ();
  ephy_web_app_provider_set_version (EPHY_WEB_APP_PROVIDER (self->skeleton), 1);

  g_signal_connect (self->skeleton, "handle-get-installed-apps",
                    G_CALLBACK (handle_get_installed_apps), self);
  g_signal_connect (self->skeleton, "handle-install",
                    G_CALLBACK (handle_install), self);
  g_signal_connect (self->skeleton, "handle-uninstall",
                    G_CALLBACK (handle_uninstall), self);

  return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                           connection, object_path, error);
}

static void
ephy_web_app_provider_service_dbus_unregister (GApplication    *application,
                                               GDBusConnection *connection,
                                               const gchar     *object_path)
{
  EphyWebAppProviderService *self;
  GDBusInterfaceSkeleton *skeleton;

  g_debug ("unregistering at object path %s", object_path);

  self = EPHY_WEB_APP_PROVIDER_SERVICE (application);
  skeleton = G_DBUS_INTERFACE_SKELETON (self->skeleton);
  if (g_dbus_interface_skeleton_has_connection (skeleton, connection))
    g_dbus_interface_skeleton_unexport_from_connection (skeleton, connection);

  g_clear_object (&self->skeleton);

  G_APPLICATION_CLASS (ephy_web_app_provider_service_parent_class)->dbus_unregister (application,
                                                                                     connection,
                                                                                     object_path);
}

static void
ephy_web_app_provider_service_class_init (EphyWebAppProviderServiceClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  application_class->dbus_register = ephy_web_app_provider_service_dbus_register;
  application_class->dbus_unregister = ephy_web_app_provider_service_dbus_unregister;
}

EphyWebAppProviderService *
ephy_web_app_provider_service_new (void)
{
  /* Note the application ID is constant for release/devel/canary builds
   * because we want to always use the same well-known D-Bus name.
   */
  g_autofree gchar *app_id = g_strconcat ("org.gnome.Epiphany.WebAppProvider", NULL);

  return g_object_new (EPHY_TYPE_WEB_APP_PROVIDER_SERVICE,
                       "application-id", app_id,
                       NULL);
}

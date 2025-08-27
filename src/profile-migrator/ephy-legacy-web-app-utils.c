/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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
#include "ephy-web-app-utils.h"

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/* Web apps are installed in the default data dir of the user. Every
 * app has its own profile directory. To create a web app, an ID needs
 * to be generated using the given name as <normalized-name>-checksum.
 * The ID is used to uniquely identify the app.
 *
 *  - Name: the user-visible pretty app name
 *  - Normalized name: lowercase name
 *  - Checksum: SHA-1 of name
 *  - ID: <normalized-name>-<checksum>
 *  - GApplication ID: see below
 *  - Profile directory: <gapplication-id>
 *  - Desktop file: <profile-dir>/<gapplication-id>.desktop
 *
 * Note that our ID and GApplication ID are different. Yes, this is confusing.
 *
 * For GApplication ID, there are two cases:
 *
 *  - If <id> is safe to use in a D-Bus identifier,
 *    then: GApplication ID is org.gnome.Epiphany.WebApp-<id>
 *  - otherwise: GAppliation ID is org.gnome.Epiphany.WebApp-<checksum>
 *
 * System web applications have a profile dir without a desktop file.
 */

#define EPHY_WEB_APP_LEGACY_GAPPLICATION_ID_PREFIX "org.gnome.Epiphany.WebApp-"

static char *
get_encoded_path (const char *path)
{
  g_autofree char *encoded = NULL;
  g_autoptr (GError) error = NULL;

  encoded = g_filename_from_utf8 (path, -1, NULL, NULL, &error);
  if (error) {
    g_warning ("%s", error->message);
    return NULL;
  }

  return g_steal_pointer (&encoded);
}

static const char *
get_app_id_from_gapplication_id (const char *name)
{
  if (!g_str_has_prefix (name, EPHY_WEB_APP_LEGACY_GAPPLICATION_ID_PREFIX)) {
    g_warning ("GApplication ID %s does not begin with required prefix %s", name, EPHY_WEB_APP_LEGACY_GAPPLICATION_ID_PREFIX);
    return NULL;
  }

  return name + strlen (EPHY_WEB_APP_LEGACY_GAPPLICATION_ID_PREFIX);
}

static char *
get_gapplication_id_from_id (const char *id)
{
  g_autofree char *gapplication_id = NULL;
  const char *final_hyphen;
  const char *checksum;

  /* FIXME: Ideally we would convert hyphens to underscores here, because
   * hyphens are not very friendly to D-Bus. However, changing this
   * would require a new profile migration, because the GApplication ID
   * must exactly match the desktop file basename.
   *
   * If we're willing to do another migration in the future, then we
   * should drop this path and always return just the prefix plus hash,
   * using underscores rather than hyphens.
   */
  gapplication_id = g_strconcat (EPHY_WEB_APP_LEGACY_GAPPLICATION_ID_PREFIX, id, NULL);
  if (g_application_id_is_valid (gapplication_id))
    return g_steal_pointer (&gapplication_id);

  /* Split ID into: <normalized-name>-<checksum> */
  final_hyphen = strrchr (id, '-');
  if (!final_hyphen) {
    g_warning ("Web app ID %s is broken: must contain a hyphen", id);
    return NULL;
  }
  checksum = final_hyphen + 1;

  if (*checksum == '\0') {
    g_warning ("Web app ID %s is broken: should end with checksum, not hyphen", id);
    return NULL;
  }

  /* We'll simply omit the <normalized-name> from the app ID, in order
   * to avoid problematic characters. Ideally we would use an underscore
   * here too, rather than a hyphen, but let's be consistent with
   * existing web apps.
   */
  g_clear_pointer (&gapplication_id, g_free);
  gapplication_id = g_strconcat (EPHY_WEB_APP_LEGACY_GAPPLICATION_ID_PREFIX, checksum, NULL);

  if (!g_application_id_is_valid (gapplication_id)) {
    g_warning ("Web app ID %s is broken: derived GApplication ID %s is not a valid app ID (is the final component alphanumeric?)", id, gapplication_id);
    return NULL;
  }

  return g_steal_pointer (&gapplication_id);
}

static char *
get_app_profile_directory_name (const char *id)
{
  g_autofree char *gapplication_id = NULL;

  gapplication_id = get_gapplication_id_from_id (id);
  if (!gapplication_id)
    g_error ("Failed to get GApplication ID from app ID %s", id);

  return get_encoded_path (gapplication_id);
}

static char *
get_app_desktop_filename (const char *id)
{
  g_autofree char *gapplication_id = NULL;
  g_autofree char *filename = NULL;

  /* Warning: the GApplication ID must exactly match the desktop file's
   * basename. Don't overthink this or stuff will break, e.g. GNotification.
   */
  gapplication_id = get_gapplication_id_from_id (id);
  if (!gapplication_id)
    g_error ("Failed to get GApplication ID from app ID %s", id);

  filename = g_strconcat (gapplication_id, ".desktop", NULL);
  return get_encoded_path (filename);
}

static const char *
ephy_legacy_web_application_get_gapplication_id_from_profile_directory (const char *profile_dir)
{
  const char *name;

  /* Just get the basename */
  name = strrchr (profile_dir, G_DIR_SEPARATOR);
  if (!name) {
    g_warning ("Profile directory %s is not a valid path", profile_dir);
    return NULL;
  }

  name++; /* Strip '/' */

  /* Legacy web app support */
  if (g_str_has_prefix (name, "app-"))
    name += strlen ("app-");

  if (!g_str_has_prefix (name, EPHY_WEB_APP_LEGACY_GAPPLICATION_ID_PREFIX)) {
    g_warning ("Profile directory %s does not begin with required web app prefix %s", profile_dir, EPHY_WEB_APP_LEGACY_GAPPLICATION_ID_PREFIX);
    return NULL;
  }

  return name;
}

static const char *
get_app_id_from_profile_directory (const char *profile_dir)
{
  const char *gapplication_id;

  gapplication_id = ephy_legacy_web_application_get_gapplication_id_from_profile_directory (profile_dir);
  return gapplication_id ? get_app_id_from_gapplication_id (gapplication_id) : NULL;
}

static char *
ephy_web_application_get_directory_under (const char *id,
                                          const char *path)
{
  g_autofree char *app_dir = NULL;

  app_dir = get_app_profile_directory_name (id);
  if (!app_dir)
    return NULL;

  return g_build_filename (path, app_dir, NULL);
}

/**
 * ephy_legacy_web_application_get_profile_directory:
 * @id: the application identifier
 *
 * Gets the directory where the profile for @id is meant to be stored.
 *
 * Returns: (transfer full): A newly allocated string.
 **/
char *
ephy_legacy_web_application_get_profile_directory (const char *id)
{
  return ephy_web_application_get_directory_under (id, g_get_user_data_dir ());
}

static EphyWebApplication *
ephy_legacy_web_application_for_profile_directory (const char *profile_dir)
{
  g_autoptr (EphyWebApplication) app = NULL;
  g_autofree char *desktop_file_path = NULL;
  const char *id;
  g_autoptr (GDesktopAppInfo) desktop_info = NULL;
  const char *exec;
  int argc;
  g_auto (GStrv) argv = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) file_info = NULL;

  id = get_app_id_from_profile_directory (profile_dir);
  if (!id)
    return NULL;

  app = g_new0 (EphyWebApplication, 1);
  app->id = g_strdup (id);

  app->desktop_file = get_app_desktop_filename (id);
  desktop_file_path = g_build_filename (profile_dir, app->desktop_file, NULL);
  desktop_info = g_desktop_app_info_new_from_filename (desktop_file_path);
  if (!desktop_info) {
    g_clear_pointer (&app, ephy_web_application_free); /* avoid a scan-build warning */
    return NULL;
  }

  app->name = g_strdup (g_app_info_get_name (G_APP_INFO (desktop_info)));
  app->icon_path = g_desktop_app_info_get_string (desktop_info, "Icon");
  exec = g_app_info_get_commandline (G_APP_INFO (desktop_info));
  if (g_shell_parse_argv (exec, &argc, &argv, NULL))
    app->url = g_strdup (argv[argc - 1]);

  file = g_file_new_for_path (desktop_file_path);

  /* FIXME: this should use TIME_CREATED but it does not seem to be working. */
  file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
  app->install_date_uint64 = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  return g_steal_pointer (&app);
}

/**
 * ephy_web_application_get_legacy_application_list:
 *
 * Gets a list of the currently installed legacy web applications.
 * This is only used for the profile migrator as it gets
 * applications in the legacy directory.
 *
 * Free the returned GList with
 * ephy_web_application_free_application_list.
 *
 * Returns: (transfer-full): a #GList of #EphyWebApplication objects
 **/
GList *
ephy_web_application_get_legacy_application_list (void)
{
  g_autoptr (GFileEnumerator) children = NULL;
  GList *applications = NULL;
  g_autofree char *parent_directory_path = NULL;
  g_autoptr (GFile) parent_directory = NULL;

  parent_directory_path = g_build_filename (g_get_user_config_dir (), "epiphany", NULL);

  parent_directory = g_file_new_for_path (parent_directory_path);
  children = g_file_enumerate_children (parent_directory,
                                        "standard::name",
                                        0, NULL, NULL);
  if (!children)
    return NULL;

  for (;;) {
    g_autoptr (GFileInfo) info = g_file_enumerator_next_file (children, NULL, NULL);
    const char *name;

    if (!info)
      break;

    name = g_file_info_get_name (info);
    if (g_str_has_prefix (name, "app-")) {
      g_autoptr (EphyWebApplication) app = NULL;
      g_autofree char *profile_dir = NULL;

      profile_dir = g_build_filename (parent_directory_path, name, NULL);
      app = ephy_legacy_web_application_for_profile_directory (profile_dir);
      if (app)
        applications = g_list_prepend (applications, g_steal_pointer (&app));
    }
  }

  return g_list_reverse (applications);
}

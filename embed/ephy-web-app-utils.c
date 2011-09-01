/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2011 Igalia S.L.
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

#include "ephy-web-app-utils.h"

#include "ephy-file-helpers.h"
#include "ephy-web-view.h"

static gboolean
_g_directory_delete_recursively (GFile *directory, GError **error)
{
  GFileEnumerator *children = NULL;
  GFileInfo *info;
  gboolean ret = TRUE;

  children = g_file_enumerate_children (directory,
                                        "standard::name,standard::type",
                                        0, NULL, error);
  if (error)
    goto out;

  info = g_file_enumerator_next_file (children, NULL, error);
  while (info || error) {
    GFile *child;
    const char *name;
    GFileType type;

    if (error)
      goto out;

    name = g_file_info_get_name (info);
    child = g_file_get_child (directory, name);
    type = g_file_info_get_file_type (info);

    if (type == G_FILE_TYPE_DIRECTORY)
      ret = _g_directory_delete_recursively (child, error);
    else if (type == G_FILE_TYPE_REGULAR)
      ret =  g_file_delete (child, NULL, error);

    g_object_unref (info);

    if (!ret)
      goto out;

    info = g_file_enumerator_next_file (children, NULL, error);
  }

  ret = TRUE;

  g_file_delete (directory, NULL, error);

out:

  if (children)
    g_object_unref (children);

  return ret;
}

/**
 * ephy_web_application_get_directory:
 * @app_name: the application name
 *
 * Gets the directory whre the profile for @app_name is meant
 * to be stored.
 *
 * Returns: (transfer full): A newly allocated string.
 **/
char *
ephy_web_application_get_profile_directory (const char *app_name)
{
  char *app_dir, *profile_dir;

  app_dir = g_strconcat (EPHY_WEB_APP_PREFIX, app_name, NULL);
  profile_dir = g_build_filename (ephy_dot_dir (), app_dir, NULL);
  g_free (app_dir);

  return profile_dir;
}

/**
 * ephy_delete_web_application:
 * @name: the name of the web application do delete
 * 
 * Deletes all the data associated with a Web Application created by
 * Epiphany.
 * 
 * Returns: %TRUE if the web app was succesfully deleted, %FALSE otherwise
 **/
gboolean
ephy_delete_web_application (const char *name)
{
  char *profile_dir = NULL;
  char *desktop_file = NULL, *desktop_path = NULL;
  GFile *profile = NULL, *launcher = NULL;
  gboolean return_value = FALSE;

  g_return_val_if_fail (name, FALSE);

  profile_dir = ephy_web_application_get_profile_directory (name);
  /* If there's no profile dir for this app, it means it does not
   * exist. */
  if (!g_file_test (profile_dir, G_FILE_TEST_IS_DIR)) {
    g_print ("No application with name '%s' is installed.\n", name);
    goto out;
  }

  profile = g_file_new_for_path (profile_dir);
  if (!_g_directory_delete_recursively (profile, NULL))
    goto out;
  g_print ("Deleted application profile.\n");

  desktop_file = g_strconcat (name, ".desktop", NULL);
  desktop_path = g_build_filename (g_get_user_data_dir (), "applications", desktop_file, NULL);
  launcher = g_file_new_for_path (desktop_path);
  if (!g_file_delete (launcher, NULL, NULL))
    goto out;
  g_print ("Deleted application launcher.\n");

  return_value = TRUE;

out:

  if (profile)
    g_object_unref (profile);
  g_free (profile_dir);

  if (launcher)
    g_object_unref (launcher);
  g_free (desktop_file);
  g_free (desktop_path);

  return return_value;
}

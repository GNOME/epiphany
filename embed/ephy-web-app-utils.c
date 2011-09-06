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

#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-web-view.h"

#include <glib/gstdio.h>
#include <libsoup/soup-gnome.h>

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
 * ephy_web_application_delete:
 * @name: the name of the web application do delete
 * 
 * Deletes all the data associated with a Web Application created by
 * Epiphany.
 * 
 * Returns: %TRUE if the web app was succesfully deleted, %FALSE otherwise
 **/
gboolean
ephy_web_application_delete (const char *name)
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
  if (!ephy_file_delete_dir_recursively (profile, NULL))
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

#define EPHY_WEB_APP_TOOLBAR "<?xml version=\"1.0\"?>" \
                             "<toolbars version=\"1.1\">" \
                             "  <toolbar name=\"DefaultToolbar\" hidden=\"true\" editable=\"false\">" \
                             "    <toolitem name=\"NavigationBack\"/>" \
                             "    <toolitem name=\"NavigationForward\"/>" \
                             "    <toolitem name=\"ViewReload\"/>" \
                             "    <toolitem name=\"ViewCancel\"/>" \
                             "  </toolbar>" \
                             "</toolbars>"

#define EPHY_TOOLBARS_XML_FILE "epiphany-toolbars-3.xml"

static char *
create_desktop_file (EphyWebView *view,
                     const char *profile_dir,
                     const char *title,
                     GdkPixbuf *icon)
{
  GKeyFile *file;
  char *exec_string;
  char *data;
  char *filename, *desktop_file_path;
  char *link_path;
  GFile *link;

  g_return_val_if_fail (profile_dir, NULL);

  file = g_key_file_new ();
  g_key_file_set_value (file, "Desktop Entry", "Name", title);
  exec_string = g_strdup_printf ("epiphany --application-mode --profile=\"%s\" %s",
                                 profile_dir,
                                 webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view)));
  g_key_file_set_value (file, "Desktop Entry", "Exec", exec_string);
  g_free (exec_string);
  g_key_file_set_value (file, "Desktop Entry", "StartupNotification", "true");
  g_key_file_set_value (file, "Desktop Entry", "Terminal", "false");
  g_key_file_set_value (file, "Desktop Entry", "Type", "Application");

  if (icon) {
    GOutputStream *stream;
    char *path;
    GFile *image;

    path = g_build_filename (profile_dir, "app-icon.png", NULL);
    image = g_file_new_for_path (path);

    stream = (GOutputStream*)g_file_create (image, 0, NULL, NULL);
    gdk_pixbuf_save_to_stream (icon, stream, "png", NULL, NULL, NULL);
    g_key_file_set_value (file, "Desktop Entry", "Icon", path);

    g_object_unref (stream);
    g_object_unref (image);
    g_free (path);
  }

  g_key_file_set_value (file, "Desktop Entry", "StartupWMClass", title);

  data = g_key_file_to_data (file, NULL, NULL);
  filename = g_strconcat (title, ".desktop", NULL);
  desktop_file_path = g_build_filename (profile_dir, filename, NULL);
  g_key_file_free (file);

  if (!g_file_set_contents (desktop_file_path, data, -1, NULL)) {
    g_free (desktop_file_path);
    desktop_file_path = NULL;
  }

  g_free (data);

  /* Create a symlink in XDG_DATA_DIR/applications for the Shell to
   * pick up this application. */
  link_path = g_build_filename (g_get_user_data_dir (), "applications", filename, NULL);
  link = g_file_new_for_path (link_path);
  g_free (link_path);
  g_file_make_symbolic_link (link, desktop_file_path, NULL, NULL);
  g_object_unref (link);
  g_free (filename);

  return desktop_file_path;
}

static void
create_cookie_jar_for_domain (EphyWebView *view, const char *directory)
{
  SoupSession *session;
  GSList *cookies, *p;
  SoupCookieJar *current_jar, *new_jar;
  char *domain, *filename;
  SoupURI *uri;

  /* Create the new cookie jar */
  filename = g_build_filename (directory, "cookies.sqlite", NULL);
  new_jar = (SoupCookieJar*)soup_cookie_jar_sqlite_new (filename, FALSE);
  g_free (filename);

  /* The app domain for the current view */
  uri = soup_uri_new (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view)));
  domain = uri->host;

  /* The current cookies */
  session = webkit_get_default_session ();
  current_jar = (SoupCookieJar*)soup_session_get_feature (session, SOUP_TYPE_COOKIE_JAR);
  cookies = soup_cookie_jar_all_cookies (current_jar);

  for (p = cookies; p; p = p->next) {
    SoupCookie *cookie = (SoupCookie*)p->data;

    if (g_str_has_suffix (cookie->domain, domain))
      soup_cookie_jar_add_cookie (new_jar, cookie);
    else
      soup_cookie_free (cookie);
  }

  soup_uri_free (uri);
  g_slist_free (cookies);
}

/**
 * ephy_web_application_create:
 * @view: an #EphyWebView
 * @title: the title for the new web application
 * @icon: the icon for the new web application
 * 
 * Creates a new Web Application from the currently loaded URI in the @view.
 * 
 * Returns: (transfer-full): the path to the desktop file representing the new application
 **/
char *
ephy_web_application_create (EphyWebView *view, const char *title, GdkPixbuf *icon)
{
  char *profile_dir = NULL;
  char *toolbar_path = NULL;
  char *desktop_file_path = NULL;

  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), NULL);

  /* If there's already a WebApp profile for the contents of this
   * view, do nothing. TODO: create a method to check this and use it
   * to ask the user if she wants to overwrite the existing WebApp. */
  profile_dir = ephy_web_application_get_profile_directory (title);
  if (g_file_test (profile_dir, G_FILE_TEST_IS_DIR))
    goto out;

  /* Create the profile directory, populate it. */
  if (g_mkdir (profile_dir, 488) == -1) {
    LOG ("Failed to create directory %s", profile_dir);
    goto out;
  }

  /* Things we need in a WebApp's profile:
     - Toolbar layout
     - Our own cookies file, copying the relevant cookies for the
       app's domain.
  */
  toolbar_path = g_build_filename (profile_dir, EPHY_TOOLBARS_XML_FILE, NULL);
  if (!g_file_set_contents (toolbar_path, EPHY_WEB_APP_TOOLBAR, -1, NULL))
    goto out;

  create_cookie_jar_for_domain (view, profile_dir);

  /* Create the deskop file. */
  desktop_file_path = create_desktop_file (view, profile_dir, title, icon);

out:
  if (toolbar_path)
    g_free (toolbar_path);

  if (profile_dir)
    g_free (profile_dir);

  return desktop_file_path;
}

GList *
ephy_web_application_get_application_list ()
{
  GFileEnumerator *children = NULL;
  GFileInfo *info;
  GList *applications = NULL;
  GFile *dot_dir;

  dot_dir = g_file_new_for_path (ephy_dot_dir ());
  children = g_file_enumerate_children (dot_dir,
                                        "standard::name",
                                        0, NULL, NULL);
  g_object_unref (dot_dir);

  info = g_file_enumerator_next_file (children, NULL, NULL);
  while (info) {
    EphyWebApplication *app;
    const char *name;
    glong prefix_length = g_utf8_strlen (EPHY_WEB_APP_PREFIX, -1);

    name = g_file_info_get_name (info);
    if (g_str_has_prefix (name, EPHY_WEB_APP_PREFIX)) {
      char *profile_dir;
      guint64 created;
      GDate *date;
      char *desktop_file, *desktop_file_path;
      char *contents;
      GFile *file;
      GFileInfo *desktop_info;

      app = g_slice_new0 (EphyWebApplication);
      app->name = g_strdup (name + prefix_length);

      profile_dir = ephy_web_application_get_profile_directory (app->name);
      app->icon_url = g_build_filename (profile_dir, "app-icon.png", NULL);

      desktop_file = g_strconcat (app->name, ".desktop", NULL);
      desktop_file_path = g_build_filename (profile_dir, desktop_file, NULL);
      if (g_file_get_contents (desktop_file_path, &contents, NULL, NULL)) {
        char *exec;
        char **strings;
        GKeyFile *key;
        int i;

        key = g_key_file_new ();
        g_key_file_load_from_data (key, contents, -1, 0, NULL);
        exec = g_key_file_get_string (key, "Desktop Entry", "Exec", NULL);
        strings = g_strsplit (exec, " ", -1);

        for (i = 0; strings[i]; i++);
        app->url = g_strdup (strings[i - 1]);

        g_strfreev (strings);
        g_free (exec);
        g_key_file_free (key);
      }

      g_free (contents);
      g_free (desktop_file);
      g_free (profile_dir);

      file = g_file_new_for_path (desktop_file_path);
      g_free (desktop_file_path);

      /* FIXME: this should use TIME_CREATED but it does not seem to be working. */
      desktop_info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
      created = g_file_info_get_attribute_uint64 (desktop_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
      date = g_date_new ();
      g_date_set_time_t (date, (time_t)created);
      g_date_strftime (app->install_date, 127, "%x", date);
      g_date_free (date);
      g_object_unref (file);
      g_object_unref (desktop_info);

      applications = g_list_append (applications, app);

    }

    g_object_unref (info);

    info = g_file_enumerator_next_file (children, NULL, NULL);
  }

  g_object_unref (children);

  return applications;
}

static void
ephy_web_application_free (EphyWebApplication *app)
{
  g_free (app->name);
  g_free (app->icon_url);
  g_free (app->url);
  g_slice_free (EphyWebApplication, app);
}

void
ephy_web_application_free_application_list (GList *list)
{
  GList *p;

  for (p = list; p; p = p->next)
    ephy_web_application_free ((EphyWebApplication*)p->data);

  g_list_free (list);
}

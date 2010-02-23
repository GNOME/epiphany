/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2004, 2005, 2006 Christian Persch
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

#include "ephy-file-helpers.h"

#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-string.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdk.h>
#include <libxml/xmlreader.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/**
 * SECTION:ephy-file-helpers
 * @short_description: miscellaneous file related utility functions
 *
 * File related functions, including functions to launch, browse or move files
 * atomically.
 */

#define EPHY_UUID		"0d82d98f-7079-401c-abff-203fcde1ece3"
#define EPHY_UUID_ENVVAR	"EPHY_UNIQUE"
#define EPHY_UUID_ENVSTRING	EPHY_UUID_ENVVAR "=" EPHY_UUID

#define DELAY_MAX_TICKS	64
#define INITIAL_TICKS	2

static GHashTable *files = NULL;
static GHashTable *mime_table = NULL;

static gboolean have_private_profile = FALSE;
static gboolean keep_temp_directory = FALSE; /* for debug purposes */
static char *dot_dir = NULL;
static char *tmp_dir = NULL;
static GList *del_on_exit = NULL;

GQuark ephy_file_helpers_error_quark;

/**
 * ephy_file_tmp_dir:
 *
 * Returns the name of the temp dir for the running Epiphany instance.
 *
 * Returns: the name of the temp dir, this string belongs to Epiphany.
 **/
const char *
ephy_file_tmp_dir (void)
{
	if (tmp_dir == NULL)
	{
		char *partial_name;
		char *full_name;

		partial_name = g_strconcat ("epiphany-", g_get_user_name (),
					    "-XXXXXX", NULL);
		full_name = g_build_filename (g_get_tmp_dir (), partial_name,
					      NULL);
#ifdef HAVE_MKDTEMP
		tmp_dir = mkdtemp (full_name);
#else
#error no mkdtemp implementation
#endif
		g_free (partial_name);

		if (tmp_dir == NULL)
		{
			g_free (full_name);
		}
	}

	return tmp_dir;
}

/**
 * ephy_file_downloads_dir:
 *
 * Gets the basename for Epiphany's Downloads dir. This depends on the current
 * locale. For the full path to the downloads directory see
 * ephy_file_get_downloads_dir().
 *
 * Returns: a newly-allocated string containing the downloads dir basename.
 **/
char *
ephy_file_downloads_dir (void)
{
	const char *translated_folder;
	const char *xdg_download_dir;
	char *desktop_dir, *converted, *downloads_dir;

	xdg_download_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
	if (xdg_download_dir != NULL)
		return g_strdup (xdg_download_dir);

	/* The name of the default downloads folder */
	translated_folder = _("Downloads");

	converted = g_filename_from_utf8 (translated_folder, -1, NULL,
					  NULL, NULL);

	desktop_dir = ephy_file_desktop_dir ();
	downloads_dir = g_build_filename (desktop_dir, converted, NULL);

	g_free (desktop_dir);
	g_free (converted);

	return downloads_dir;
}

/**
 * ephy_file_get_downloads_dir:
 *
 * Gets the full path to the downloads dir. This uses ephy_file_downloads_dir()
 * internally and hence is locale dependant. Note that this can return %NULL if
 * not even the user homedir path can be found.
 *
 * Returns: a newly-allocated string containing the path to the downloads dir.
 **/
char *
ephy_file_get_downloads_dir (void)
{
	char *download_dir, *expanded;

	download_dir = eel_gconf_get_string (CONF_STATE_DOWNLOAD_DIR);

	if (download_dir && strcmp (download_dir, _("Downloads")) == 0)
	{
		g_free (download_dir);
		download_dir = ephy_file_downloads_dir ();
	}
  	else if (download_dir && strcmp (download_dir, _("Desktop")) == 0)
	{
		g_free (download_dir);
		download_dir = ephy_file_desktop_dir ();
	}
	else if (download_dir)
	{
		char *converted_dp;

		converted_dp = g_filename_from_utf8 (download_dir, -1, NULL, NULL, NULL);
		g_free (download_dir);
		download_dir = converted_dp;
	}

	/* Emergency download destination */
	if (download_dir == NULL)
	{
		const char *home_dir;
		home_dir = g_get_home_dir ();
		download_dir = g_strdup (home_dir != NULL ? home_dir : "/");
	}

	g_return_val_if_fail (download_dir != NULL, NULL);

	expanded = ephy_string_expand_initial_tilde (download_dir);
	g_free (download_dir);

	return expanded;
}

/**
 * ephy_file_desktop_dir:
 *
 * Gets the XDG desktop dir path or a default homedir/Desktop alternative.
 *
 * Returns: a newly-allocated string containing the desktop dir path.
 **/
char *
ephy_file_desktop_dir (void)
{
	const char *xdg_desktop_dir;

	xdg_desktop_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
	if (xdg_desktop_dir != NULL)
		return g_strdup (xdg_desktop_dir);

	/* If we don't have XDG user dirs info, return an educated guess. */
	return g_build_filename	(g_get_home_dir (), _("Desktop"), NULL);
}

/**
 * ephy_file_tmp_filename:
 * @base: the base name of the temp file to create
 * @extension: an optional extension for @base or %NULL
 *
 * Creates a temp file with mkstemp() using @base as the name with an optional
 * @extension.
 *
 * Returns: a newly-allocated string containing the name of the created temp
 * file or %NULL.
 **/
char *
ephy_file_tmp_filename (const char *base,
			const char *extension)
{
	int fd;
	char *name = g_strdup (base);

	fd = mkstemp (name);

	if (fd != -1)
	{
		unlink (name);
		close (fd);
	}
	else
	{
		g_free (name);

		return NULL;
	}

	if (extension)
	{
		char *tmp;
		tmp = g_strconcat (name, ".",
				   extension, NULL);
		g_free (name);
		name = tmp;
	}

	return name;
}

/**
 * ephy_file:
 * @filename: the name of the Epiphany file requested
 *
 * Looks for @filename in Epiphany's directories and relevant paths.
 *
 * Returns: the full path to the requested file
 **/
const char *
ephy_file (const char *filename)
{
	char *ret;
	guint i;

	static const char * const paths[] =
	{
		SHARE_DIR "/",
		SHARE_DIR "/glade/",
		SHARE_DIR "/art/",
		SHARE_UNINSTALLED_DIR "/",
		SHARE_UNINSTALLED_DIR "/glade/",
		SHARE_UNINSTALLED_DIR "/art/"
	};

	g_assert (files != NULL);

	ret = g_hash_table_lookup (files, filename);
	if (ret != NULL)
		return ret;

	for (i = 0; i < G_N_ELEMENTS (paths); i++)
	{
		ret = g_strconcat (paths[i], filename, NULL);
		if (g_file_test (ret, G_FILE_TEST_EXISTS) == TRUE)
		{
			g_hash_table_insert (files, g_strdup (filename), ret);
			return (const char *) ret;
		}
		g_free (ret);
	}

	g_warning ("Failed to find %s\n", filename);

	return NULL;
}

/**
 * ephy_dot_dir:
 *
 * Gets Epiphany's configuration directory, usually .gnome2/epiphany under
 * user's homedir.
 *
 * Returns: the full path to Epiphany's configuration directory
 **/
const char *
ephy_dot_dir (void)
{
	return dot_dir;
}

/**
 * ephy_has_private_profile:
 *
 * Whether Epiphany is running with a private profile (-p command line option).
 *
 * Returns: %TRUE if a private profile is in use
 **/
gboolean
ephy_has_private_profile (void)
{
	return have_private_profile;
}

/**
 * ephy_file_helpers_init:
 * @profile_dir: directory to use as Epiphany's profile
 * @private_profile: %TRUE if we should use a private profile
 * @keep_temp_dir: %TRUE to omit deleting the temp dir on exit
 * @error: an optional #GError
 *
 * Initializes Epiphany file helper functions, sets @profile_dir as Epiphany's
 * profile dir and whether the running session will be private.
 *
 * Returns: %FALSE if the profile dir couldn't be created or accesed
 **/
gboolean
ephy_file_helpers_init (const char *profile_dir,
			gboolean private_profile,
			gboolean keep_temp_dir,
			GError **error)
{
	const char *uuid;

	/* See if we've been calling ourself, and abort if we have */
	uuid = g_getenv (EPHY_UUID_ENVVAR);
	if (uuid && strcmp (uuid, EPHY_UUID) == 0)
	{
		g_warning ("Self call detected, exiting!\n");
		exit (1);
	}

	/* Put marker in env */
	g_setenv (EPHY_UUID_ENVVAR, EPHY_UUID, TRUE);

	ephy_file_helpers_error_quark = g_quark_from_static_string ("ephy-file-helpers-error");

	files = g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) g_free);

	have_private_profile = private_profile;
	keep_temp_directory = keep_temp_dir;

	if (private_profile && profile_dir != NULL)
	{
		dot_dir = g_strdup (profile_dir);
	}
	else if (private_profile)
	{
		if (ephy_file_tmp_dir () == NULL)
		{
			g_set_error (error,
				     EPHY_FILE_HELPERS_ERROR_QUARK,
				     0,
				     _("Could not create a temporary directory in “%s”."),
				     g_get_tmp_dir ());
			return FALSE;
		}

		dot_dir = g_build_filename (ephy_file_tmp_dir (),
					    "epiphany",
					    NULL);
	}
	else
	{
		dot_dir = g_build_filename (g_get_home_dir (),
					    GNOME_DOT_GNOME,
					    "epiphany",
					    NULL);
	}
	
	return ephy_ensure_dir_exists (ephy_dot_dir (), error);
}

static void
delete_files (GList *l)
{
	for (; l != NULL; l = l->next)
	{
		unlink (l->data);
	}
}

/**
 * ephy_file_helpers_shutdown:
 *
 * Cleans file helpers information, corresponds to ephy_file_helpers_init().
 **/
void
ephy_file_helpers_shutdown (void)
{
	g_hash_table_destroy (files);

	del_on_exit = g_list_reverse (del_on_exit);
	delete_files (del_on_exit);
	g_list_foreach (del_on_exit, (GFunc)g_free, NULL);
	g_list_free (del_on_exit);
	del_on_exit = NULL;

	if (mime_table != NULL)
	{
		LOG ("Destroying mime type hashtable");
		g_hash_table_destroy (mime_table);
		mime_table = NULL;
	}

	g_free (dot_dir);
	dot_dir = NULL;

	if (tmp_dir != NULL)
	{
		if (!keep_temp_directory)
		{
			/* recursively delete the contents and the directory */
			ephy_file_delete_directory (tmp_dir);
		}

		g_free (tmp_dir);
		tmp_dir = NULL;
	}
}

/**
 * ephy_ensure_dir_exists:
 * @dir: path to a directory
 * @error: an optional GError to fill or %NULL
 *
 * Checks if @dir exists and is a directory, if it it exists and it's not a
 * directory %FALSE is returned. If @dir doesn't exist and can't be created
 * then %FALSE is returned.
 *
 * Returns: %TRUE if @dir exists and is a directory
 **/
gboolean
ephy_ensure_dir_exists (const char *dir,
		        GError **error)
{
	if (g_file_test (dir, G_FILE_TEST_EXISTS) &&
	    !g_file_test (dir, G_FILE_TEST_IS_DIR))
	{
		g_set_error (error,
			     EPHY_FILE_HELPERS_ERROR_QUARK,
			     0,
			     _("The file “%s” exists. Please move it out of the way."),
			     dir);
		return FALSE;
	}

	if (!g_file_test (dir, G_FILE_TEST_EXISTS) &&
            g_mkdir_with_parents (dir, 488) != 0)
	{
		g_set_error (error,
			     EPHY_FILE_HELPERS_ERROR_QUARK,
			     0,
			     _("Failed to create directory “%s”."),
			     dir);
		return FALSE;
	}

	return TRUE;
}

static void
ephy_find_file_recursive (const char *path,
			  const char *fname,
			  GSList **list,
			  gint depth,
			  gint maxdepth)
{
	GDir *dir;
	const gchar *file;

	dir = g_dir_open (path, 0, NULL);
	if (dir != NULL)
	{
		while ((file = g_dir_read_name (dir)))
		{
			if (depth < maxdepth)
			{
				char *new_path = g_build_filename (path, file, NULL);
				ephy_find_file_recursive (new_path, fname, list,
							  depth + 1, maxdepth);
				g_free (new_path);
			}
			if (strcmp (file, fname) == 0)
			{
				char *new_path = g_build_filename (path, file, NULL);
				*list = g_slist_prepend (*list, new_path);
			}
		}

		g_dir_close (dir);
	}
}

/**
 * ephy_file_find:
 * @path: path to search for @fname
 * @fname: filename to search for
 * @maxdepth: maximum directory depth when searching @path
 *
 * Searchs for @fname in @path with a maximum depth of @maxdepth.
 *
 * Returns: a GSList of matches
 **/
GSList *
ephy_file_find (const char *path,
	        const char *fname,
	        gint maxdepth)
{
	GSList *ret = NULL;
	ephy_find_file_recursive (path, fname, &ret, 0, maxdepth);
	return ret;
}

/**
 * ephy_file_switch_temp_file:
 * @file_dest: destination file
 * @file_temp: file to move to @file
 *
 * Moves @file_temp to @file_dest atomically, doing a backup and restoring it if
 * something fails.
 *
 * Returns: %TRUE if the switch was successful
 **/
gboolean
ephy_file_switch_temp_file (GFile *file_dest,
			    GFile *file_temp)
{
	char *file_dest_path, *file_temp_path;
	char *backup_path;
	gboolean dest_exists;
	gboolean retval = TRUE;
	GFile *backup_file;

	file_dest_path = g_file_get_path (file_dest);
	file_temp_path = g_file_get_path (file_temp);

	dest_exists = g_file_test (file_dest_path, G_FILE_TEST_EXISTS);

	backup_path = g_strconcat (file_dest_path, ".old", NULL);
	backup_file = g_file_new_for_path (backup_path);

	if (dest_exists)
	{
		if (g_file_move (file_dest, backup_file,
				 G_FILE_COPY_OVERWRITE,
				 NULL, NULL, NULL, NULL) == FALSE)
		{
			g_warning ("Failed to backup %s to %s",
				   file_dest_path, backup_path);

			retval = FALSE;
			goto failed;
		}
	}

	if (g_file_move (file_temp, file_dest,
			 G_FILE_COPY_OVERWRITE,
			 NULL, NULL, NULL, NULL) == FALSE)
	{
		g_warning ("Failed to replace %s with %s",
			   file_temp_path, file_dest_path);

		if (g_file_move (backup_file, file_dest,
				 G_FILE_COPY_OVERWRITE,
				 NULL, NULL, NULL, NULL) == FALSE)
		{
			g_warning ("Failed to restore %s from %s",
				   file_dest_path, file_temp_path);
		}

		retval = FALSE;
		goto failed;
	}

	if (dest_exists)
	{
		if (g_file_delete (backup_file,
				   NULL, NULL) == FALSE)
		{
			g_warning ("Failed to delete old file %s", backup_path);
		}
	}

failed:
	g_free (file_dest_path);
	g_free (file_temp_path);

	g_free (backup_path);
	g_object_unref (backup_file);

	return retval;
}

/**
 * ephy_file_delete_on_exit:
 * @file: a #GFile
 *
 * Schedules @file to be deleted when Epiphany exits. This function currently
 * does nothing.
 **/
void
ephy_file_delete_on_exit (GFile *file)
{
	/* does nothing now */
}

/**
 * ephy_file_add_recent_item:
 * @uri: an URI
 * @mime_type: the mime type corresponding to @uri
 *
 * Adds @uri to the default #GtkRecentManager, with @mime_type as its type.
 **/
void
ephy_file_add_recent_item (const char *uri,
			   const char *mime_type)
{	
	GtkRecentManager *manager = gtk_recent_manager_get_default ();

	g_return_if_fail (mime_type != NULL && uri != NULL);

	gtk_recent_manager_add_item (manager, uri);
}

static void
load_mime_from_xml (void)
{
	xmlTextReaderPtr reader;
	const char *xml_file;
	int ret;
	EphyMimePermission permission = EPHY_MIME_PERMISSION_UNKNOWN;

	g_return_if_fail (mime_table == NULL);

	mime_table = g_hash_table_new_full (g_str_hash, g_str_equal,
					    xmlFree, NULL);

	xml_file = ephy_file ("mime-types-permissions.xml");
	if (xml_file == NULL)
	{
		g_warning ("MIME types permissions file not found!\n");
		return;
	}

	reader = xmlNewTextReaderFilename (xml_file);
	if (reader == NULL)
	{
		g_warning ("Could not load MIME types permissions file!\n");
		return;
	}

	ret = xmlTextReaderRead (reader);
	while (ret == 1)
	{
		const xmlChar *tag;
		xmlReaderTypes type;

		tag = xmlTextReaderConstName (reader);
		type = xmlTextReaderNodeType (reader);

		if (xmlStrEqual (tag, (const xmlChar *)"safe") && type == XML_READER_TYPE_ELEMENT)
		{
			permission = EPHY_MIME_PERMISSION_SAFE;
		}
		else if (xmlStrEqual (tag, (const xmlChar *)"unsafe") && type == XML_READER_TYPE_ELEMENT)
		{
			permission = EPHY_MIME_PERMISSION_UNSAFE;
		}
		else if (xmlStrEqual (tag, (const xmlChar *)"mime-type"))
		{
			xmlChar *type;

			type = xmlTextReaderGetAttribute (reader, (const xmlChar *)"type");
			g_hash_table_insert (mime_table, type,
					     GINT_TO_POINTER (permission));
		}

		ret = xmlTextReaderRead (reader);
	}

	xmlFreeTextReader (reader);
}

/**
 * ephy_file_check_mime:
 * @mime_type: a mime type
 *
 * Checks @mime_type against our safe/unsafe database of types, returns an
 * #EphyMimePermission.
 *
 * Returns: an #EphyMimePermission
 **/
EphyMimePermission
ephy_file_check_mime (const char *mime_type)
{
	EphyMimePermission permission;
	gpointer tmp;

	g_return_val_if_fail (mime_type != NULL, EPHY_MIME_PERMISSION_UNKNOWN);

	if (mime_table == NULL)
	{
		load_mime_from_xml ();
	}

	tmp = g_hash_table_lookup (mime_table, mime_type);
	if (tmp == NULL)
	{
		permission = EPHY_MIME_PERMISSION_UNKNOWN;
	}
	else
	{
		permission = GPOINTER_TO_INT (tmp);
	}

	return permission;
}

/**
 * ephy_file_launch_application:
 * @app: the application to launch
 * @files: files to pass to @app
 * @user_time: user time to prevent focus stealing
 * @widget: a relevant widget from where to get the #GdkScreen and #GdkDisplay
 *
 * Launches @app to open @files. If @widget is set the screen and display from
 * it will be used to launch the application, otherwise the defaults will be
 * used.
 *
 * Returns: %TRUE if g_app_info_launch() succeeded
 **/
gboolean
ephy_file_launch_application (GAppInfo *app,
			      GList *files,
			      guint32 user_time,
			      GtkWidget *widget)
{
	GdkAppLaunchContext *context;
	GdkDisplay *display;
	GdkScreen *screen;
	gboolean res;

	context = gdk_app_launch_context_new ();
	if (widget)
	{
		display = gtk_widget_get_display (widget);
		screen = gtk_widget_get_screen (widget);
	}
	else
	{
		display = gdk_display_get_default ();
		screen = gdk_screen_get_default ();
	}
	
	gdk_app_launch_context_set_display (context, display);
	gdk_app_launch_context_set_screen (context, screen);
	gdk_app_launch_context_set_timestamp (context, user_time);

	res = g_app_info_launch (app, files,
				 G_APP_LAUNCH_CONTEXT (context), NULL);
	g_object_unref (context);

	return res;
}

/**
 * ephy_file_launch_desktop_file:
 * @filename: the path to the .desktop file
 * @parameter: path to a parameter file to pass to the application
 * @user_time: user time to prevent focus stealing
 * @widget: an optional widget for ephy_file_launch_application()
 *
 * Calls ephy_file_launch_application() for the application described by the
 * .desktop file @filename. Can pass @parameter as optional file arguments.
 *
 * Returns: %TRUE if the application launch was successful
 **/
gboolean
ephy_file_launch_desktop_file (const char *filename,
			       const char *parameter,
			       guint32 user_time,
			       GtkWidget *widget)
{
	GDesktopAppInfo *app;
	GFile *file;
	GList *list = NULL;
	gboolean ret;

	app = g_desktop_app_info_new (filename);
	file = g_file_new_for_path (parameter);
	list = g_list_append (list, file);
	
	ret = ephy_file_launch_application (G_APP_INFO (app), list, user_time, widget);
	g_list_free (list);
	g_object_unref (file);
	return ret;
}

/**
 * ephy_file_launch_handler:
 * @mime_type: the mime type of @file or %NULL
 * @file: a #GFile to pass as argument
 * @user_time: user time to prevent focus stealing
 *
 * Launches @file with its default handler application, if @mime_type is %NULL
 * then @file will be queried for its type.
 *
 * Returns: %TRUE on success
 **/
gboolean
ephy_file_launch_handler (const char *mime_type,
			  GFile *file,
			  guint32 user_time)
{
	GAppInfo *app = NULL;
	gboolean ret = FALSE;

	g_return_val_if_fail (file != NULL, FALSE);

	if (mime_type != NULL)
	{
		app = g_app_info_get_default_for_type (mime_type,
						       FALSE);
	}
	else
	{
		GFileInfo *file_info;
		char *type;

		/* Sniff mime type and check if it's safe to open */
		file_info = g_file_query_info (file,
					       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
					       0, NULL, NULL);
		if (file_info == NULL) {
			return FALSE;
		}
		type = g_strdup (g_file_info_get_content_type (file_info));
		
		g_object_unref (file_info);

		if (type != NULL && type[0] != '\0' &&
		    ephy_file_check_mime (type) == EPHY_MIME_PERMISSION_SAFE)
		{
			/* FIXME rename tmp file to right extension ? */
			app = g_app_info_get_default_for_type (type, FALSE);
		}
		g_free (type);
	}

	if (app != NULL)
	{
		GList *list = NULL;
		
		list = g_list_append (list, file);
		ret = ephy_file_launch_application (app, list, user_time, NULL);
		g_list_free (list);
	}
	else
		ret = FALSE;

	return ret;
}

/**
 * ephy_file_browse_to:
 * @file: a #GFile
 * @user_time: user_time to prevent focus stealing
 *
 * Launches the default application for browsing directories, with @file's
 * parent directory as its target. Passes @user_time to
 * ephy_file_launch_handler() to prevent focus stealing.
 *
 * Returns: %TRUE if the launch succeeded
 **/
gboolean
ephy_file_browse_to (GFile *file,
		     guint32 user_time)
{
	GFile *parent, *desktop;
	char *desktop_dir;
	gboolean ret;

	desktop_dir = ephy_file_desktop_dir ();
	desktop = g_file_new_for_path (desktop_dir);
	
	/* Don't do anything if destination is the desktop */
	if (g_file_has_prefix (file, desktop))
	{
		ret = FALSE;
	}
	else
	{
		parent = g_file_get_parent (file);
		/* TODO find a way to make nautilus scroll to the actual file */
		ret = ephy_file_launch_handler ("x-directory/normal",
						parent,
						user_time);
	}
	
	g_free (desktop_dir);

	return ret;
}

/**
 * ephy_file_delete_directory:
 * @path: the path to remove
 *
 * Remove @path and its contents. Like calling rm -rf @path.
 **/
void
ephy_file_delete_directory (const char *path)
{
	GFile *file;
	gboolean ret;
	
	file = g_file_new_for_path (path);
	
	ret = g_file_delete (file, NULL, NULL);
	
	if (ret == TRUE)
	{
		LOG ("Deleted dir '%s'", path);
	}
	else
	{
		LOG ("Couldn't delete dir '%s'", path);
	}
	g_object_unref (file);
}

/**
 * ephy_file_delete_uri
 * @uri: URI of the file to be deleted
 *
 * Remove the given URI.
 */
void
ephy_file_delete_uri (const char *uri)
{
	GFile *file;
	gboolean ret;

	g_return_if_fail (uri);

	file = g_file_new_for_uri (uri);

	ret = g_file_delete (file, NULL, NULL);

	if (ret == TRUE)
	{
		LOG ("Deleted file at URI '%s'", uri);
	}
	else
	{
		LOG ("Couldn't file at URI '%s'", uri);
	}
	g_object_unref (file);
}

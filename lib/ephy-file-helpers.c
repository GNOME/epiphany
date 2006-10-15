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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-file-helpers.h"

#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-init.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libxml/xmlreader.h>

/* bug http://bugzilla.gnome.org/show_bug.cgi?id=156687 */
#undef GNOME_DISABLE_DEPRECATED
#include <libgnome/gnome-desktop-item.h>

#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

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

char *
ephy_file_downloads_dir (void)
{
	const char *translated_folder;
	char *desktop_dir, *converted, *downloads_dir;

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

char *
ephy_file_get_downloads_dir (void)
{
	char *download_dir, *expanded;

	download_dir = eel_gconf_get_string (CONF_STATE_DOWNLOAD_DIR);

	if (download_dir && strcmp (download_dir, "Downloads") == 0)
	{
		g_free (download_dir);
		download_dir = ephy_file_downloads_dir ();
	}
  	else if (download_dir && strcmp (download_dir, "Desktop") == 0)
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

	expanded = gnome_vfs_expand_initial_tilde (download_dir);
	g_free (download_dir);

	return expanded;
}

char *
ephy_file_desktop_dir (void)
{
	char *downloads_dir;
	gboolean desktop_is_home;

	desktop_is_home = eel_gconf_get_boolean (CONF_DESKTOP_IS_HOME_DIR);

	if (desktop_is_home)
	{
		downloads_dir = g_strdup (g_get_home_dir ()); 
	}
	else
	{
		downloads_dir = g_build_filename
			(g_get_home_dir (), "Desktop", NULL);
	}

	return downloads_dir;
}

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

const char *
ephy_dot_dir (void)
{
	return dot_dir;
}

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
			/* recursively delete the contents */
			ephy_file_delete_directory (tmp_dir);

			/* delete the directory itself too */
			rmdir (tmp_dir);
		}

		g_free (tmp_dir);
		tmp_dir = NULL;
	}
}

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
            mkdir (dir, 488) != 0)
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

GSList *
ephy_file_find (const char *path,
	        const char *fname,
	        gint maxdepth)
{
	GSList *ret = NULL;
	ephy_find_file_recursive (path, fname, &ret, 0, maxdepth);
	return ret;
}

gboolean
ephy_file_switch_temp_file (const char *filename,
			    const char *filename_temp)
{
	char *old_file;
	gboolean old_exist;
	gboolean retval = TRUE;

	old_file = g_strconcat (filename, ".old", NULL);

	old_exist = g_file_test (filename, G_FILE_TEST_EXISTS);

	if (old_exist)
	{
		if (rename (filename, old_file) < 0)
		{
			g_warning ("Failed to rename %s to %s", filename, old_file);
			retval = FALSE;
			goto failed;
		}
	}

	if (rename (filename_temp, filename) < 0)
	{
		g_warning ("Failed to rename %s to %s", filename_temp, filename);

		if (rename (old_file, filename) < 0)
		{
			g_warning ("Failed to restore %s from %s",
				   filename, filename_temp);
		}
		retval = FALSE;
		goto failed;
	}

	if (old_exist)
	{
		if (unlink (old_file) < 0)
		{
			g_warning ("Failed to delete old file %s", old_file);
		}
	}

failed:
	g_free (old_file);

	return retval;
}

void
ephy_file_delete_on_exit (const char *path)
{
	/* does nothing now */
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

/* Copied from nautilus-program-choosing.c */

extern char **environ;

/* Cut and paste from gdkspawn-x11.c */
static gchar **
my_gdk_spawn_make_environment_for_screen (GdkScreen  *screen,
					  gchar     **envp)
{
  gchar **retval = NULL;
  gchar  *display_name;
  gint    i, j = 0, env_len;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (envp == NULL)
    envp = environ;

  env_len = g_strv_length (envp);
  retval = g_new (char *, env_len + 3);

  display_name = gdk_screen_make_display_name (screen);

  for (i = 0; envp[i] != NULL; i++)
    if (!g_str_has_prefix (envp[i], "DISPLAY=") &&
        !g_str_has_prefix (envp[i], EPHY_UUID_ENVVAR "="))
      retval[j++] = g_strdup (envp[i]);

  retval[j++] = g_strconcat ("DISPLAY=", display_name, NULL);
  retval[j++] = g_strdup (EPHY_UUID_ENVSTRING);
  retval[j] = NULL;

  g_free (display_name);

  return retval;
}

static void
sn_error_trap_push (SnDisplay *display,
		    Display   *xdisplay)
{
	gdk_error_trap_push ();
}

static void
sn_error_trap_pop (SnDisplay *display,
		   Display   *xdisplay)
{
	gdk_error_trap_pop ();
}

static char **
make_spawn_environment_for_sn_context (SnLauncherContext *sn_context,
				       char             **envp)
{
	char **retval;
	int    i, j, len;

	retval = NULL;
	
	if (envp == NULL) {
		envp = environ;
	}
	
	len = g_strv_length (envp);
	retval = g_new (char *, len + 3);

	for (i = 0, j = 0; envp[i] != NULL; i++) {
		if (!g_str_has_prefix (envp[i], "DESKTOP_STARTUP_ID=") &&
		    !g_str_has_prefix (envp[i], EPHY_UUID_ENVVAR "=")) {
			retval[j++] = g_strdup (envp[i]);
	        }
	}

	retval[j++] = g_strdup_printf ("DESKTOP_STARTUP_ID=%s",
				       sn_launcher_context_get_startup_id (sn_context));
	retval[j++] = g_strdup (EPHY_UUID_ENVSTRING);
	retval[j] = NULL;

	return retval;
}

/* This should be fairly long, as it's confusing to users if a startup
 * ends when it shouldn't (it appears that the startup failed, and
 * they have to relaunch the app). Also the timeout only matters when
 * there are bugs and apps don't end their own startup sequence.
 *
 * This timeout is a "last resort" timeout that ignores whether the
 * startup sequence has shown activity or not.  Metacity and the
 * tasklist have smarter, and correspondingly able-to-be-shorter
 * timeouts. The reason our timeout is dumb is that we don't monitor
 * the sequence (don't use an SnMonitorContext)
 */
#define STARTUP_TIMEOUT_LENGTH (30 /* seconds */ * 1000)

typedef struct
{
	GdkScreen *screen;
	GSList *contexts;
	guint timeout_id;
} StartupTimeoutData;

static void
free_startup_timeout (void *data)
{
	StartupTimeoutData *std;

	std = data;

	g_slist_foreach (std->contexts,
			 (GFunc) sn_launcher_context_unref,
			 NULL);
	g_slist_free (std->contexts);

	if (std->timeout_id != 0) {
		g_source_remove (std->timeout_id);
		std->timeout_id = 0;
	}

	g_free (std);
}

static gboolean
startup_timeout (void *data)
{
	StartupTimeoutData *std;
	GSList *tmp;
	GTimeVal now;
	int min_timeout;

	std = data;

	min_timeout = STARTUP_TIMEOUT_LENGTH;
	
	g_get_current_time (&now);
	
	tmp = std->contexts;
	while (tmp != NULL) {
		SnLauncherContext *sn_context;
		GSList *next;
		long tv_sec, tv_usec;
		double elapsed;
		
		sn_context = tmp->data;
		next = tmp->next;
		
		sn_launcher_context_get_last_active_time (sn_context,
							  &tv_sec, &tv_usec);

		elapsed =
			((((double)now.tv_sec - tv_sec) * G_USEC_PER_SEC +
			  (now.tv_usec - tv_usec))) / 1000.0;

		if (elapsed >= STARTUP_TIMEOUT_LENGTH) {
			std->contexts = g_slist_remove (std->contexts,
							sn_context);
			sn_launcher_context_complete (sn_context);
			sn_launcher_context_unref (sn_context);
		} else {
			min_timeout = MIN (min_timeout, (STARTUP_TIMEOUT_LENGTH - elapsed));
		}
		
		tmp = next;
	}

	if (std->contexts == NULL) {
		std->timeout_id = 0;
	} else {
		std->timeout_id = g_timeout_add (min_timeout,
						 startup_timeout,
						 std);
	}

	/* always remove this one, but we may have reinstalled another one. */
	return FALSE;
}

static void
add_startup_timeout (GdkScreen         *screen,
		     SnLauncherContext *sn_context)
{
	StartupTimeoutData *data;

	data = g_object_get_data (G_OBJECT (screen), "nautilus-startup-data");
	if (data == NULL) {
		data = g_new (StartupTimeoutData, 1);
		data->screen = screen;
		data->contexts = NULL;
		data->timeout_id = 0;
		
		g_object_set_data_full (G_OBJECT (screen), "nautilus-startup-data",
					data, free_startup_timeout);		
	}

	sn_launcher_context_ref (sn_context);
	data->contexts = g_slist_prepend (data->contexts, sn_context);
	
	if (data->timeout_id == 0) {
		data->timeout_id = g_timeout_add (STARTUP_TIMEOUT_LENGTH,
						  startup_timeout,
						  data);		
	}
}

gboolean
ephy_file_launch_application (GnomeVFSMimeApplication *application,
			      const char *parameter,
			      guint32 user_time)
{
	GdkScreen       *screen;
	GList           *uris = NULL;
	char            *uri;
	char           **envp;
	GnomeVFSResult   result;
	SnLauncherContext *sn_context;
	SnDisplay *sn_display;

	g_return_val_if_fail (application != NULL, FALSE);
	g_return_val_if_fail (parameter != NULL, FALSE);

	uri = gnome_vfs_make_uri_canonical (parameter);
	if (uri == NULL) return FALSE;

	uris = g_list_prepend (NULL, uri);

	/* FIXME multihead! */
	screen = gdk_screen_get_default ();
	envp = my_gdk_spawn_make_environment_for_screen (screen, NULL);
	
	sn_display = sn_display_new (gdk_display,
				     sn_error_trap_push,
				     sn_error_trap_pop);

	
	/* Only initiate notification if application supports it. */
	if (gnome_vfs_mime_application_supports_startup_notification (application))
	{
		char *name;

		sn_context = sn_launcher_context_new (sn_display,
						      screen ? gdk_screen_get_number (screen) :
						      DefaultScreen (gdk_display));
		
		name = g_filename_display_basename (uri);
		if (name != NULL) {
			char *description;
			
			sn_launcher_context_set_name (sn_context, name);

			/* FIXME: i18n after string freeze! */
			description = g_strdup_printf ("Opening %s", name);
			
			sn_launcher_context_set_description (sn_context, description);

			g_free (name);
			g_free (description);
		}
		
		if (!sn_launcher_context_get_initiated (sn_context)) {
			const char *binary_name;
			char **old_envp;

			binary_name = gnome_vfs_mime_application_get_binary_name (application);
		
			sn_launcher_context_set_binary_name (sn_context,
							     binary_name);
			
			sn_launcher_context_initiate (sn_context,
						      g_get_prgname () ? g_get_prgname () : "unknown",
						      binary_name,
						      (Time) user_time);

			old_envp = envp;
			envp = make_spawn_environment_for_sn_context (sn_context, envp);
			g_strfreev (old_envp);
		}
	} else {
		sn_context = NULL;
	}
	
	result = gnome_vfs_mime_application_launch_with_env (application, uris, envp);

	if (sn_context != NULL) {
		if (result != GNOME_VFS_OK) {
			sn_launcher_context_complete (sn_context); /* end sequence */
		} else {
			add_startup_timeout (screen ? screen :
					     gdk_display_get_default_screen (gdk_display_get_default ()),
					     sn_context);
		}
		sn_launcher_context_unref (sn_context);
	}
	
	sn_display_unref (sn_display);

	g_strfreev (envp);
	g_list_foreach (uris, (GFunc) g_free,NULL);
	g_list_free (uris);

	if (result != GNOME_VFS_OK)
	{
		g_warning ("Cannot launch application '%s'\n",
			   gnome_vfs_mime_application_get_name (application));
	}

	return result == GNOME_VFS_OK;
}

/* End cut-paste-adapt from nautilus */

static int
launch_desktop_item (const char *desktop_file,
		     const char *parameter,
		     guint32 user_time,
		     GError **error)
{
	GnomeDesktopItem *item = NULL;
	GdkScreen *screen;
	GList *uris = NULL;
	char *canonical;
	int ret = -1;
	char **envp;
	
	/* FIXME multihead! */
	screen = gdk_screen_get_default ();
	envp = my_gdk_spawn_make_environment_for_screen (screen, NULL);

	item = gnome_desktop_item_new_from_file (desktop_file, 0, NULL);
	if (item == NULL) return FALSE;
		
	if (parameter != NULL)
	{
		canonical = gnome_vfs_make_uri_canonical (parameter);
		uris = g_list_append (uris, canonical);
	}

	gnome_desktop_item_set_launch_time (item, user_time);
	ret = gnome_desktop_item_launch_with_env (item, uris, 0, envp, error);

	g_list_foreach (uris, (GFunc) g_free, NULL);
	g_list_free (uris);
	g_strfreev (envp);
	gnome_desktop_item_unref (item);

	return ret;
}

gboolean
ephy_file_launch_desktop_file (const char *filename,
			       const char *parameter,
			       guint32 user_time)
{
	GError *error = NULL;
	const char * const *dirs;
	char *path = NULL;
	int i, ret = -1;

	dirs = g_get_system_data_dirs ();
	if (dirs == NULL) return FALSE;

	for (i = 0; dirs[i] != NULL; i++)
	{
		path = g_build_filename (dirs[i], "applications", filename, NULL);

		if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) break;

		g_free (path);
		path = NULL;
	}

	if (path != NULL)
	{
		ret = launch_desktop_item (path, parameter, user_time, &error);

		if (ret == -1 || error != NULL)
		{
			g_warning ("Cannot launch desktop item '%s': %s\n",
				path, error ? error->message : "(unknown error)");
			g_clear_error (&error);
		}

		g_free (path);
	}

	return ret >= 0;
}

gboolean
ephy_file_launch_handler (const char *mime_type,
			  const char *address,
			  guint32 user_time)
{
	GnomeVFSMimeApplication *app = NULL;
	GnomeVFSFileInfo *info = NULL;
	char *canonical;
	gboolean ret = FALSE;

	g_return_val_if_fail (address != NULL, FALSE);

	canonical = gnome_vfs_make_uri_canonical (address);
	if (canonical == NULL) return FALSE;

	if (mime_type != NULL)
	{
		app = gnome_vfs_mime_get_default_application (mime_type);
	}
	else
	{
		/* Sniff mime type and check if it's safe to open */
		info = gnome_vfs_file_info_new ();
		if (gnome_vfs_get_file_info (canonical, info,
					     GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					     GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE) == GNOME_VFS_OK &&
		    (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) &&
		    info->mime_type != NULL &&
		    info->mime_type[0] != '\0' &&
		    ephy_file_check_mime (info->mime_type) == EPHY_MIME_PERMISSION_SAFE)
		{
			/* FIXME rename tmp file to right extension ? */
			app = gnome_vfs_mime_get_default_application (info->mime_type);
		}
		gnome_vfs_file_info_unref (info);
	}

	if (app != NULL)
	{
		ret = ephy_file_launch_application (app, address, user_time);

		gnome_vfs_mime_application_free (app);
	}
	else
	{
		/* FIXME: warn user? */
		g_warning ("No handler for found or file type is unsafe!\n");
	}

	g_free (canonical);

	return ret;
}

gboolean
ephy_file_browse_to (const char *parameter,
		     guint32 user_time)
{
	GnomeVFSURI *uri, *parent_uri;
	gboolean ret;

	uri = gnome_vfs_uri_new (parameter);
	parent_uri = gnome_vfs_uri_get_parent (uri);

	/* TODO find a way to make nautilus scroll to the actual file */
	ret = ephy_file_launch_handler ("x-directory/normal", 
					gnome_vfs_uri_get_path (parent_uri), 
					user_time);
	
	gnome_vfs_uri_unref (uri);
	gnome_vfs_uri_unref (parent_uri);

	return ret;
}

struct _EphyFileMonitor
{
	GnomeVFSMonitorHandle *handle;
	EphyFileMonitorFunc callback;
	EphyFileMonitorDelayFunc delay_func;
	gpointer user_data;
	char *uri;
	guint delay;
	guint timeout_id;
	guint ticks;
	GnomeVFSMonitorEventType type;
};

static gboolean
ephy_file_monitor_timeout_cb (EphyFileMonitor *monitor)
{
	if (monitor->ticks > 0)
	{
		monitor->ticks--;

		/* Run again */
		return TRUE;
	}

	if (monitor->delay_func &&
	    monitor->delay_func (monitor, monitor->user_data))
	{
		monitor->ticks = DELAY_MAX_TICKS / 2;

		/* Run again */
		return TRUE;
	}

	monitor->timeout_id = 0;

	monitor->callback (monitor, monitor->uri, monitor->type, monitor->user_data);

	/* don't run again */
	return FALSE;
}

static void
ephy_file_monitor_cb (GnomeVFSMonitorHandle *handle,
		      const char *monitor_uri,
		      const char *info_uri,
		      GnomeVFSMonitorEventType event_type,
		      EphyFileMonitor *monitor)
{
	LOG ("File '%s' has changed, scheduling reload", monitor_uri);

	switch (event_type)
	{
		case GNOME_VFS_MONITOR_EVENT_CHANGED:
			monitor->ticks = INITIAL_TICKS;
			/* fall-through */
		case GNOME_VFS_MONITOR_EVENT_CREATED:
			/* We make a lot of assumptions here, but basically we know
			 * that we just have to reload, by construction.
			 * Delay the reload a little bit so we don't endlessly
			 * reload while a file is written.
			 */
			monitor->type = event_type;

			if (monitor->ticks == 0)
			{
				monitor->ticks = 1;
			}
			else
			{
				/* Exponential backoff */
				monitor->ticks = MIN (monitor->ticks * 2,
						      DELAY_MAX_TICKS);
			}

			if (monitor->timeout_id == 0)
			{
				monitor->timeout_id = 
					g_timeout_add (monitor->delay,
						       (GSourceFunc) ephy_file_monitor_timeout_cb,
						       monitor);
			}

			break;

		case GNOME_VFS_MONITOR_EVENT_DELETED:
			if (monitor->timeout_id != 0)
			{
				g_source_remove (monitor->timeout_id);
				monitor->timeout_id = 0;
			}
			monitor->ticks = 0;

			monitor->callback (monitor, monitor->uri, event_type, monitor->user_data);
			break;
		case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING:
		case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING:
		case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED:
		default:
			break;
	}
}

EphyFileMonitor *
ephy_file_monitor_add (const char *uri,
		       GnomeVFSMonitorType monitor_type,
		       guint delay,
		       EphyFileMonitorFunc callback,
		       EphyFileMonitorDelayFunc delay_func,
		       gpointer user_data)
{
	EphyFileMonitor *monitor;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (callback, NULL);

	monitor = g_new (EphyFileMonitor, 1);
	monitor->callback = callback;
	monitor->delay_func = delay_func;
	monitor->user_data = user_data;
	monitor->uri = g_strdup (uri);
	monitor->delay = delay;
	monitor->ticks = 0;
	monitor->timeout_id = 0;

	if (gnome_vfs_monitor_add (&monitor->handle, uri, monitor_type,
				   (GnomeVFSMonitorCallback) ephy_file_monitor_cb,
				   monitor) != GNOME_VFS_OK)
	{
		LOG ("Failed to add file monitor for '%s'", uri);

		g_free (monitor->uri);
		g_free (monitor);
		return NULL;
	}

	LOG ("File monitor for '%s' added", uri);

	return monitor;
}

void
ephy_file_monitor_cancel (EphyFileMonitor *monitor)
{
	g_return_if_fail (monitor != NULL);
	g_return_if_fail (monitor->handle != NULL);
	g_return_if_fail (monitor->uri != NULL);

	LOG ("Cancelling file monitor for '%s'", monitor->uri);

	gnome_vfs_monitor_cancel (monitor->handle);

	if (monitor->timeout_id != 0)
	{
		g_source_remove (monitor->timeout_id);
	}

	g_free (monitor->uri);
	g_free (monitor);
}

void
ephy_file_delete_directory (const char *path)
{
	/* FIXME not implemented yet */
}

/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2004 Christian Persch
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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-init.h>
#include <libxml/xmlreader.h>

#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

static GHashTable *files = NULL;
static GHashTable *mime_table = NULL;

static char *dot_dir = NULL;
static char *tmp_dir = NULL;
static GList *del_on_exit = NULL;

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
	int i;

	static char *paths[] =
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

	for (i = 0; i < (int) G_N_ELEMENTS (paths); i++)
	{
		ret = g_strconcat (paths[i], filename, NULL);
		if (g_file_test (ret, G_FILE_TEST_EXISTS) == TRUE)
		{
			g_hash_table_insert (files, g_strdup (filename), ret);
			return (const char *) ret;
		}
		g_free (ret);
	}

	g_warning (_("Failed to find %s"), filename);

	return NULL;
}

const char *
ephy_dot_dir (void)
{
	if (dot_dir == NULL)
	{
		dot_dir = g_build_filename (g_get_home_dir (),
					    GNOME_DOT_GNOME,
					    "epiphany",
					    NULL);
	}

	return dot_dir;
}

void
ephy_file_helpers_init (void)
{
	files = g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) g_free);
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

	if (tmp_dir != NULL)
	{
		rmdir (tmp_dir);
		g_free (tmp_dir);
		tmp_dir = NULL;
	}

	g_free (dot_dir);
	dot_dir = NULL;
}

gboolean
ephy_ensure_dir_exists (const char *dir)
{
	if (g_file_test (dir, G_FILE_TEST_IS_DIR) == FALSE)
	{
		if (g_file_test (dir, G_FILE_TEST_EXISTS) == TRUE)
		{
			g_warning (_("%s exists, please move it out of the way."), dir);
			return FALSE;
		}

		if (mkdir (dir, 488) != 0)
		{
			g_warning (_("Failed to create directory %s."), dir);
			return FALSE;
		}
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
	del_on_exit = g_list_prepend (del_on_exit,
				      g_strdup (path));
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

		if (xmlStrEqual (tag, "safe") && type == XML_READER_TYPE_ELEMENT)
		{
			permission = EPHY_MIME_PERMISSION_SAFE;
		}
		else if (xmlStrEqual (tag, "unsafe") && type == XML_READER_TYPE_ELEMENT)
		{
			permission = EPHY_MIME_PERMISSION_UNSAFE;
		}
		else if (xmlStrEqual (tag, "mime-type"))
		{
			xmlChar *type;

			type = xmlTextReaderGetAttribute (reader, "type");
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

/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-autocompletion-source.h"
#include "ephy-filesystem-autocompletion.h"
#include "ephy-gobject-misc.h"
#include "ephy-debug.h"

#include <string.h>
#include  <libgnomevfs/gnome-vfs-async-ops.h>

/**
 * Private data
 */
struct _EphyFilesystemAutocompletionPrivate {
	gchar *current_dir;
	gchar *base_dir;
	GnomeVFSURI *base_dir_uri;
	gchar *basic_key;
	gchar *basic_key_dir;
	GSList *files;

	guint score;
	GnomeVFSAsyncHandle *load_handle;
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_filesystem_autocompletion_class_init	(EphyFilesystemAutocompletionClass *klass);
static void		ephy_filesystem_autocompletion_init		(EphyFilesystemAutocompletion *as);
static void		ephy_filesystem_autocompletion_finalize_impl	(GObject *o);
static void		ephy_filesystem_autocompletion_autocompletion_source_init (EphyAutocompletionSourceIface *iface);
static void		ephy_filesystem_autocompletion_autocompletion_source_foreach (EphyAutocompletionSource *source,
										     const gchar *current_text,
										     EphyAutocompletionSourceForeachFunc func,
										     gpointer data);
void			ephy_filesystem_autocompletion_autocompletion_source_set_basic_key (EphyAutocompletionSource *source,
											   const gchar *basic_key);
static void		ephy_filesystem_autocompletion_emit_autocompletion_source_data_changed (EphyFilesystemAutocompletion *gh);
static void		ephy_filesystem_autocompletion_set_current_dir	(EphyFilesystemAutocompletion *fa, const gchar *d);


static gpointer g_object_class;

/**
 * FilesystemAutocompletion object
 */
MAKE_GET_TYPE_IFACE (ephy_filesystem_autocompletion, "EphyFilesystemAutocompletion", EphyFilesystemAutocompletion,
		     ephy_filesystem_autocompletion_class_init, ephy_filesystem_autocompletion_init, G_TYPE_OBJECT,
		     ephy_filesystem_autocompletion_autocompletion_source_init, EPHY_TYPE_AUTOCOMPLETION_SOURCE);

static void
ephy_filesystem_autocompletion_autocompletion_source_init (EphyAutocompletionSourceIface *iface)
{
	iface->foreach = ephy_filesystem_autocompletion_autocompletion_source_foreach;
	iface->set_basic_key = ephy_filesystem_autocompletion_autocompletion_source_set_basic_key;
}

static void
ephy_filesystem_autocompletion_class_init (EphyFilesystemAutocompletionClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_filesystem_autocompletion_finalize_impl;

	g_object_class = g_type_class_peek_parent (klass);
}

static void 
ephy_filesystem_autocompletion_init (EphyFilesystemAutocompletion *e)
{
	EphyFilesystemAutocompletionPrivate *p = g_new0 (EphyFilesystemAutocompletionPrivate, 1);
	e->priv = p;

	p->score = G_MAXINT / 2;
	p->base_dir = g_strdup ("");
}

static void
ephy_filesystem_autocompletion_finalize_impl (GObject *o)
{
	EphyFilesystemAutocompletion *as = GUL_FILESYSTEM_AUTOCOMPLETION (o);
	EphyFilesystemAutocompletionPrivate *p = as->priv;

	LOG ("Finalize")

	g_free (p->basic_key);
	g_free (p->basic_key_dir);
	g_free (p->current_dir);
	g_free (p->base_dir);
	if (p->base_dir_uri) 
	{
		gnome_vfs_uri_unref (p->base_dir_uri);
	}

	g_free (p);

	G_OBJECT_CLASS (g_object_class)->finalize (o);
}

EphyFilesystemAutocompletion *
ephy_filesystem_autocompletion_new (void)
{
	EphyFilesystemAutocompletion *ret = g_object_new (GUL_TYPE_FILESYSTEM_AUTOCOMPLETION, NULL);
	return ret;
}


static gchar *
gfa_get_nearest_dir (const gchar *path)
{
	gchar *ret;
	const gchar *lastslash = rindex (path, '/');
	
	if (lastslash)
	{
		if (!strcmp (path, "file://"))
		{
			/* without this, gnome-vfs does not recognize it as a dir */
			ret = g_strdup ("file:///");
		}
		else
		{
			ret = g_strndup (path, lastslash - path + 1);
		}
	}
	else
	{
		ret = g_strdup ("");
	}

	return ret;
}

static void
ephy_filesystem_autocompletion_autocompletion_source_foreach (EphyAutocompletionSource *source,
							     const gchar *basic_key,
							     EphyAutocompletionSourceForeachFunc func,
							     gpointer data)
{
	EphyFilesystemAutocompletion *fa = GUL_FILESYSTEM_AUTOCOMPLETION (source);
	EphyFilesystemAutocompletionPrivate *p = fa->priv;
	GSList *li;

	ephy_filesystem_autocompletion_autocompletion_source_set_basic_key (source, basic_key);

	for (li = p->files; li; li = li->next)
	{
		func (source, li->data, li->data, li->data, FALSE, FALSE, p->score, data);
	}
	
}

static void
ephy_filesystem_autocompletion_emit_autocompletion_source_data_changed (EphyFilesystemAutocompletion *fa)
{
	g_signal_emit_by_name (fa, "data-changed");
}

static void
gfa_load_directory_cb (GnomeVFSAsyncHandle *handle,
		       GnomeVFSResult result,
		       GList *list,
		       guint entries_read,
		       gpointer callback_data)
{
	EphyFilesystemAutocompletion *fa = callback_data;
	EphyFilesystemAutocompletionPrivate *p = fa->priv;
	GList *li;
	gchar *cd;

	g_return_if_fail (p->load_handle == handle);

	LOG ("entries_read == %d", entries_read)

	if (entries_read <= 0)
	{
		return;
	}
	
	if (p->basic_key_dir[strlen (p->basic_key_dir) - 1] == G_DIR_SEPARATOR
	    || p->basic_key_dir[0] == '\0')
	{
		cd = g_strdup (p->basic_key_dir);
	}
	else
	{
		cd = g_strconcat (p->basic_key_dir, G_DIR_SEPARATOR_S, NULL);
	}

	for (li = list; li; li = li->next)
	{
		GnomeVFSFileInfo *i = li->data;
		if (!(i->name[0] == '.' 
		      && (i->name[1] == '\0'
			  || (i->name[1] == '.'
			      && i->name[2] == '\0'))))
		{
			gchar *f = g_strconcat (cd, i->name, NULL);
			p->files = g_slist_prepend (p->files, f);
			
			LOG ("+ %s", f)
		}
	}

	g_free (cd);

	ephy_filesystem_autocompletion_emit_autocompletion_source_data_changed (fa);
}

static void
ephy_filesystem_autocompletion_set_current_dir (EphyFilesystemAutocompletion *fa, const gchar *d)
{
	EphyFilesystemAutocompletionPrivate *p = fa->priv;
	GnomeVFSURI *cd_uri;

	if (p->base_dir_uri)
	{
		cd_uri = gnome_vfs_uri_append_path (p->base_dir_uri, d);
	}
	else
	{
		cd_uri = gnome_vfs_uri_new (d);
	}

	if (p->load_handle)
	{
		gnome_vfs_async_cancel (p->load_handle);
		p->load_handle = NULL;
	}

	if (p->files)
	{
		g_slist_foreach (p->files, (GFunc) g_free, NULL);
		g_slist_free (p->files);
		p->files = NULL;

		ephy_filesystem_autocompletion_emit_autocompletion_source_data_changed (fa);
	}
	
	if (!cd_uri)
	{
		LOG ("Can't load dir %s", d)
		return;
	}

	g_free (p->current_dir);
	p->current_dir = gnome_vfs_uri_to_string (cd_uri, GNOME_VFS_URI_HIDE_NONE);

	LOG ("Loading dir: %s", p->current_dir)

	gnome_vfs_async_load_directory_uri (&p->load_handle,
					    cd_uri,
					    GNOME_VFS_FILE_INFO_DEFAULT,
					    100,
					    0,
					    gfa_load_directory_cb,
					    fa);

	gnome_vfs_uri_unref (cd_uri);
}

void
ephy_filesystem_autocompletion_autocompletion_source_set_basic_key (EphyAutocompletionSource *source,
								   const gchar *basic_key)
{
	EphyFilesystemAutocompletion *fa = GUL_FILESYSTEM_AUTOCOMPLETION (source);
	EphyFilesystemAutocompletionPrivate *p = fa->priv;
	gchar *new_basic_key_dir;
	
	if (p->basic_key && !strcmp (p->basic_key, basic_key))
	{
		return;
	}

	g_free (p->basic_key);
	p->basic_key = g_strdup (basic_key);

	new_basic_key_dir = gfa_get_nearest_dir (basic_key);
	if (p->basic_key_dir && !strcmp (p->basic_key_dir, new_basic_key_dir))
	{
		g_free (new_basic_key_dir);
	}
	else
	{
		g_free (p->basic_key_dir);
		p->basic_key_dir = new_basic_key_dir;
		ephy_filesystem_autocompletion_set_current_dir (fa, p->basic_key_dir);
	}
}

void
ephy_filesystem_autocompletion_set_base_dir (EphyFilesystemAutocompletion *fa, const gchar *d)
{
	EphyFilesystemAutocompletionPrivate *p = fa->priv;

	g_free (p->base_dir);
	p->base_dir = g_strdup (d);

	if (p->base_dir_uri) 
	{
		gnome_vfs_uri_unref (p->base_dir_uri);
	}

	if (p->base_dir[0])
	{
		p->base_dir_uri = gnome_vfs_uri_new (p->base_dir);
	}
	else
	{
		p->base_dir_uri = NULL;
	}

	if (p->base_dir_uri)
	{
		gchar *t = gnome_vfs_uri_to_string (p->base_dir_uri, GNOME_VFS_URI_HIDE_NONE);
		LOG ("base_dir: %s", t)
		g_free (t);
	}
}


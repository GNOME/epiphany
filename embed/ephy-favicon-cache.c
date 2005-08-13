/*
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright (C) 2003-2004 Marco Pesenti Gritti
 *  Copyright (C) 2004 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include "ephy-favicon-cache.h"

#include <libgnomevfs/gnome-vfs-ops.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "ephy-embed-shell.h"
#include "ephy-embed-persist.h"
#include "ephy-embed-factory.h"
#include "ephy-file-helpers.h"
#include "ephy-node-common.h"
#include "ephy-node.h"
#include <glib/gstdio.h>
#include <libgnomeui/libgnomeui.h>
#include "ephy-debug.h"

#define EPHY_FAVICON_CACHE_XML_ROOT    "ephy_favicons_cache"
#define EPHY_FAVICON_CACHE_XML_VERSION "1.1"

#define EPHY_FAVICON_CACHE_OBSOLETE_DAYS 30

/* this is very generous, most files are 4k */
#define EPHY_FAVICON_MAX_SIZE	64 * 1024 /* byte */

static void ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass);
static void ephy_favicon_cache_init	  (EphyFaviconCache *cache);
static void ephy_favicon_cache_finalize	  (GObject *object);
static gboolean kill_download		  (const char*, EphyEmbedPersist*, EphyFaviconCache*);

#define EPHY_FAVICON_CACHE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FAVICON_CACHE, EphyFaviconCachePrivate))

struct _EphyFaviconCachePrivate
{
	char *directory;
	char *xml_file;
	EphyNodeDb *db;
	EphyNode *icons;
	GHashTable *icons_hash;
	GHashTable *downloads_hash;
};

enum
{
	CHANGED,
	LAST_SIGNAL
};

enum
{
	EPHY_NODE_FAVICON_PROP_URL	 = 2,
	EPHY_NODE_FAVICON_PROP_FILENAME	 = 3,
	EPHY_NODE_FAVICON_PROP_LAST_USED = 4,
	EPHY_NODE_FAVICON_PROP_STATE	 = 5,
	EPHY_NODE_FAVICON_PROP_CHECKOLD  = 6,
	EPHY_NODE_FAVICON_PROP_CHECKED	 = 7
};

enum
{
	NEEDS_TYPE_CHECK = 1 << 0,
	NEEDS_RENAME	 = 1 << 1,
	NEEDS_ICO_CHECK	 = 1 << 2,
	NEEDS_MASK	 = 0x3f
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
ephy_favicon_cache_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyFaviconCacheClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_favicon_cache_class_init,
			NULL,
			NULL,
			sizeof (EphyFaviconCache),
			0,
			(GInstanceInitFunc) ephy_favicon_cache_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyFaviconCache",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_favicon_cache_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyFaviconCacheClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (EphyFaviconCachePrivate));
}

EphyFaviconCache *
ephy_favicon_cache_new (void)
{
	return EPHY_FAVICON_CACHE (g_object_new (EPHY_TYPE_FAVICON_CACHE, NULL));
}

static gboolean
icon_is_obsolete (EphyNode *node, GDate *now)
{
	int last_visit;
	GDate date;

	last_visit = ephy_node_get_property_int
		(node, EPHY_NODE_FAVICON_PROP_LAST_USED);

	g_date_clear (&date, 1);
	g_date_set_time (&date, last_visit);

	return (g_date_days_between (&date, now) >=
		EPHY_FAVICON_CACHE_OBSOLETE_DAYS);
}

static void
icons_added_cb (EphyNode *node,
		EphyNode *child,
		EphyFaviconCache *eb)
{
	g_hash_table_insert (eb->priv->icons_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_FAVICON_PROP_URL),
			     child);
}

static void
icons_removed_cb (EphyNode *node,
		  EphyNode *child,
		  guint old_index,
		  EphyFaviconCache *eb)
{
	g_hash_table_remove (eb->priv->icons_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_FAVICON_PROP_URL));
}

static void
remove_obsolete_icons (EphyFaviconCache *eb)
{
	GPtrArray *children;
	int i;
	GTime now;
	GDate current_date;

	now = time (NULL);
	g_date_clear (&current_date, 1);
	g_date_set_time (&current_date, time (NULL));

	children = ephy_node_get_children (eb->priv->icons);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (icon_is_obsolete (kid, &current_date))
		{
			const char *filename;
			char *path;

			filename = ephy_node_get_property_string
				(kid, EPHY_NODE_FAVICON_PROP_FILENAME);
			path = g_build_filename (eb->priv->directory,
						 filename, NULL);
			gnome_vfs_unlink (path);

			g_free (path);
			ephy_node_unref (kid);
		}
	}
}

static void
prepare_close_cb (EphyEmbedShell *shell,
		  EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv = cache->priv;

	g_hash_table_foreach_remove (priv->downloads_hash,
				     (GHRFunc) kill_download, cache);
}

static void
ephy_favicon_cache_init (EphyFaviconCache *cache)
{
	EphyNodeDb *db;

	cache->priv = EPHY_FAVICON_CACHE_GET_PRIVATE (cache);

	db = ephy_node_db_new (EPHY_NODE_DB_SITEICONS);
	cache->priv->db = db;

	cache->priv->xml_file = g_build_filename (ephy_dot_dir (),
						  "ephy-favicon-cache.xml",
						  NULL);

	cache->priv->directory = g_build_filename (ephy_dot_dir (),
						   "favicon_cache",
						   NULL);

	if (g_file_test (cache->priv->directory, G_FILE_TEST_IS_DIR) == FALSE)
	{
		if (mkdir (cache->priv->directory, 488) != 0)
		{
			g_error ("Couldn't mkdir %s.", cache->priv->directory);
		}
	}

	cache->priv->icons_hash = g_hash_table_new (g_str_hash,
						    g_str_equal);
	cache->priv->downloads_hash = g_hash_table_new_full (g_str_hash,
							     g_str_equal,
							     g_free,
							     NULL);

	/* Icons */
	cache->priv->icons = ephy_node_new_with_id (db, ICONS_NODE_ID);
	ephy_node_signal_connect_object (cache->priv->icons,
					 EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback) icons_added_cb,
					 G_OBJECT (cache));
	ephy_node_signal_connect_object (cache->priv->icons,
					 EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) icons_removed_cb,
					 G_OBJECT (cache));

	ephy_node_db_load_from_file (cache->priv->db, cache->priv->xml_file,
				     EPHY_FAVICON_CACHE_XML_ROOT,
				     EPHY_FAVICON_CACHE_XML_VERSION);

	/* listen to prepare-close on the shell */
	g_signal_connect_object (embed_shell, "prepare-close",
				 G_CALLBACK (prepare_close_cb), cache, 0);
}

static gboolean
kill_download (const char *key,
	       EphyEmbedPersist *persist,
	       EphyFaviconCache *cache)
{
	EphyNode *icon;

	/* disconnect "completed" and "cancelled" callbacks */
	g_signal_handlers_disconnect_matched
		(persist,G_SIGNAL_MATCH_DATA , 0, 0, NULL, NULL, cache);

	/* now cancel the download */
	ephy_embed_persist_cancel (persist);

	icon = g_hash_table_lookup (cache->priv->icons_hash, key);
	g_return_val_if_fail (EPHY_IS_NODE (icon), TRUE);

	ephy_node_unref (icon);

	return TRUE;
}

static void
ephy_favicon_cache_finalize (GObject *object)
{
	EphyFaviconCache *cache = EPHY_FAVICON_CACHE (object);

	LOG ("EphyFaviconCache finalising");

	g_hash_table_foreach_remove (cache->priv->downloads_hash,
				     (GHRFunc) kill_download, cache);
	remove_obsolete_icons (cache);

	ephy_node_db_write_to_xml_safe
		(cache->priv->db, cache->priv->xml_file,
		 EPHY_FAVICON_CACHE_XML_ROOT,
		 EPHY_FAVICON_CACHE_XML_VERSION,
		 NULL,
		 cache->priv->icons, 0, NULL);

	g_free (cache->priv->xml_file);
	g_free (cache->priv->directory);
	g_hash_table_destroy (cache->priv->icons_hash);
	g_hash_table_destroy (cache->priv->downloads_hash);

	g_object_unref (cache->priv->db);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
favicon_download_completed_cb (EphyEmbedPersist *persist,
			       EphyFaviconCache *cache)
{
	const char *url;

	url = ephy_embed_persist_get_source (persist);
	g_return_if_fail (url != NULL);

	LOG ("Favicon cache download completed for %s", url);

	g_hash_table_remove (cache->priv->downloads_hash, url);

	g_signal_emit (G_OBJECT (cache), signals[CHANGED], 0, url);

	g_object_unref (persist);
}

static void
favicon_download_cancelled_cb (EphyEmbedPersist *persist,
			       EphyFaviconCache *cache)
{
	const char *url, *dest;

	url = ephy_embed_persist_get_source (persist);
	g_return_if_fail (url != NULL);

	LOG ("Favicon cache download cancelled %s", url);

	g_hash_table_remove (cache->priv->downloads_hash, url);

	/* remove a partially downloaded file */
	dest = ephy_embed_persist_get_dest (persist);
	gnome_vfs_unlink (dest);

	/* FIXME: re-schedule to try again after n days? */

	g_object_unref (persist);
}

static void
ephy_favicon_cache_download (EphyFaviconCache *cache,
			     const char *favicon_url,
			     const char *filename)
{
	EphyEmbedPersist *persist;
	char *dest;

	LOG ("Download favicon: %s", favicon_url);

	g_return_if_fail (EPHY_IS_FAVICON_CACHE (cache));
	g_return_if_fail (favicon_url != NULL);
	g_return_if_fail (filename != NULL);

	dest = g_build_filename (cache->priv->directory, filename, NULL);

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	ephy_embed_persist_set_dest (persist, dest);
	ephy_embed_persist_set_flags (persist, EPHY_EMBED_PERSIST_NO_VIEW |
					       EPHY_EMBED_PERSIST_DO_CONVERSION);
	ephy_embed_persist_set_max_size (persist, EPHY_FAVICON_MAX_SIZE);
	ephy_embed_persist_set_source (persist, favicon_url);

	g_free (dest);

	g_signal_connect (G_OBJECT (persist), "completed",
			  G_CALLBACK (favicon_download_completed_cb), cache);
	g_signal_connect (G_OBJECT (persist), "cancelled",
			  G_CALLBACK (favicon_download_cancelled_cb), cache);

	g_hash_table_insert (cache->priv->downloads_hash,
			     g_strdup (favicon_url), persist);

	ephy_embed_persist_save (persist);
}

GdkPixbuf *
ephy_favicon_cache_get (EphyFaviconCache *cache,
			const char *url)
{
	GTime now;
	EphyNode *icon;
	GValue value = { 0, };
	char *pix_file;
	GdkPixbuf *pixbuf = NULL;
	guint checklevel = NEEDS_MASK;

	if (url == NULL) return NULL;

	now = time (NULL);

	icon = g_hash_table_lookup (cache->priv->icons_hash, url);

	if (!icon)
	{
		char *filename;

		filename = gnome_thumbnail_md5 (url);

		icon = ephy_node_new (cache->priv->db);
		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, url);
		ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_URL,
					&value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, filename);
		ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_FILENAME,
					&value);
		g_value_unset (&value);

		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, (int) NEEDS_MASK & ~NEEDS_RENAME);
		ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_CHECKED,
					&value);
		g_value_unset (&value);

		ephy_node_add_child (cache->priv->icons, icon);

		ephy_favicon_cache_download (cache, url, filename);

		g_free (filename);
	}

	/* update timestamp */
	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, now);
	ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_LAST_USED,
				&value);
	g_value_unset (&value);

	if (g_hash_table_lookup (cache->priv->downloads_hash, url) != NULL)
	{
		/* still downloading, return NULL */
		return NULL;
	}

	pix_file = g_build_filename
		(cache->priv->directory,
		 ephy_node_get_property_string (icon, EPHY_NODE_FAVICON_PROP_FILENAME),
		 NULL);

	/* FIXME queue re-download? */
	if (g_file_test (pix_file, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (pix_file);
		return NULL;
	}

	/* Check for supported icon types */
	if (ephy_node_get_property (icon, EPHY_NODE_FAVICON_PROP_CHECKED, &value) &&
	    G_VALUE_HOLDS (&value, G_TYPE_INT))
	{
		checklevel = (guint) g_value_get_int (&value);
		g_value_unset (&value);
	}

	/* First, update the filename */
	if (checklevel & NEEDS_RENAME)
	{
		char *new_pix_file, *urlhash;

		urlhash = gnome_thumbnail_md5 (url);
		new_pix_file = g_build_filename (cache->priv->directory, urlhash, NULL);

		g_value_init (&value, G_TYPE_STRING);
		g_value_take_string (&value, urlhash);
		ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_FILENAME, &value);
		g_value_unset (&value);

		/* rename the file */
		g_rename (pix_file, new_pix_file);

		g_free (pix_file);
		pix_file = new_pix_file;

		checklevel &= ~NEEDS_RENAME;
		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, (int) checklevel);
		ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_CHECKED, &value);
		g_value_unset (&value);
	}

	/* Now check the type. We renamed the file above, so gnome-vfs does NOT
	 * fall back to extension checking if the slow mime check fails for 
	 * whatever reason
	 */
	if (checklevel & NEEDS_TYPE_CHECK)
	{
		GnomeVFSFileInfo *info;
		gboolean valid = FALSE, is_ao = FALSE;;

		/* Sniff mime type and check if it's safe to open */
		info = gnome_vfs_file_info_new ();
		if (gnome_vfs_get_file_info (pix_file, info,
					     GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					     GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE) == GNOME_VFS_OK &&
		    (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) &&
		    info->mime_type != NULL)
		{
			valid = strcmp (info->mime_type, "image/x-ico") == 0 ||
				strcmp (info->mime_type, "image/png") == 0 ||
				strcmp (info->mime_type, "image/gif") == 0;
			is_ao = strcmp (info->mime_type, "application/octet-stream") == 0;
		}
		gnome_vfs_file_info_unref (info);

		/* As a special measure, we try to load an application/octet-stream file
		 * as an ICO file, since we cannot detect a ICO file without .ico extension
		 * (the mime system has no magic for it).
		 */
		if (is_ao)
		{
			GdkPixbufLoader *loader;
			char *buf = NULL;
			gsize count = 0;

			loader = gdk_pixbuf_loader_new_with_type ("ico", NULL);
			if (loader != NULL)
			{
				if (g_file_get_contents (pix_file, &buf, &count, NULL))
				{
					gdk_pixbuf_loader_write (loader, (const guchar *) buf, count, NULL);
					g_free (buf);
				}
				
				gdk_pixbuf_loader_close (loader, NULL);

				pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
				if (pixbuf != NULL)
				{
					g_object_ref (pixbuf);
					valid = TRUE;
				}
	
				g_object_unref (loader);
			}
		}

		/* persist the check value */
		if (valid)
		{
			checklevel &= ~NEEDS_TYPE_CHECK;
		}
		else
		{
			/* remove invalid file from cache */
			/* gnome_vfs_unlink (pix_file); */
		}

		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, (int) checklevel);
		ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_CHECKED, &value);
		g_value_unset (&value);

		/* epiphany 1.6 compat */
		g_value_init (&value, G_TYPE_BOOLEAN);
		g_value_set_boolean (&value, valid);
		ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_CHECKOLD, &value);
		g_value_unset (&value);
	}

	/* if it still needs the check, mime type couldn't be checked. Deny! */
	if (checklevel & NEEDS_TYPE_CHECK)
	{
		g_free (pix_file);
		return NULL;
	}

	/* we could already have a pixbuf from the application/octet-stream check */
	if (pixbuf == NULL)
	{
		pixbuf = gdk_pixbuf_new_from_file (pix_file, NULL);
	}

	g_free (pix_file);

	if (pixbuf == NULL)
	{
		return NULL;
	}

	if (gdk_pixbuf_get_width (pixbuf) > 16 ||
	    gdk_pixbuf_get_height (pixbuf) > 16)
	{
		GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf, 16, 16,
							     GDK_INTERP_NEAREST);
		g_object_unref (G_OBJECT (pixbuf));
		pixbuf = scaled;
	}

	return pixbuf;
}

/*
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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
 */

#include <libxml/tree.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "ephy-embed-persist.h"
#include "ephy-file-helpers.h"
#include "ephy-favicon-cache.h"
#include "ephy-node.h"
#include "ephy-debug.h"

#define EPHY_FAVICON_CACHE_XML_VERSION "0.1"

#define EPHY_FAVICON_CACHE_OBSOLETE_DAYS 30

static void ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass);
static void ephy_favicon_cache_init (EphyFaviconCache *ma);
static void ephy_favicon_cache_finalize (GObject *object);

struct EphyFaviconCachePrivate
{
	char *directory;
	char *xml_file;
	EphyNode *icons;
	GHashTable *icons_hash;
	GStaticRWLock *icons_hash_lock;
	GHashTable *downloads_hash;
};

enum
{
	CHANGED,
	LAST_SIGNAL
};

static guint ephy_favicon_cache_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
ephy_favicon_cache_get_type (void)
{
	static GType ephy_favicon_cache_type = 0;

	if (ephy_favicon_cache_type == 0)
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

		ephy_favicon_cache_type = g_type_register_static (G_TYPE_OBJECT,
								    "EphyFaviconCache",
								     &our_info, 0);
	}

	return ephy_favicon_cache_type;
}

static void
ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_favicon_cache_finalize;

	ephy_favicon_cache_signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyFaviconCacheClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);
}

EphyFaviconCache *
ephy_favicon_cache_new (void)
{
	EphyFaviconCache *cache;

	cache = EPHY_FAVICON_CACHE (g_object_new (EPHY_TYPE_FAVICON_CACHE, NULL));

	g_return_val_if_fail (cache->priv != NULL, NULL);

	return cache;
}

static void
ephy_favicon_cache_load (EphyFaviconCache *eb)
{
	xmlDocPtr doc;
	xmlNodePtr root, child;
	char *tmp;

	if (g_file_test (eb->priv->xml_file, G_FILE_TEST_EXISTS) == FALSE)
		return;

	doc = xmlParseFile (eb->priv->xml_file);
	g_assert (doc != NULL);

	root = xmlDocGetRootElement (doc);

	tmp = xmlGetProp (root, "version");
	g_assert (tmp != NULL && strcmp (tmp, EPHY_FAVICON_CACHE_XML_VERSION) == 0);
	g_free (tmp);

	for (child = root->children; child != NULL; child = child->next)
	{
		EphyNode *node;

		node = ephy_node_new_from_xml (child);
	}

	xmlFreeDoc (doc);
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
	g_static_rw_lock_writer_lock (eb->priv->icons_hash_lock);

	g_hash_table_insert (eb->priv->icons_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_FAVICON_PROP_URL),
			     child);

	g_static_rw_lock_writer_unlock (eb->priv->icons_hash_lock);
}

static void
icons_removed_cb (EphyNode *node,
		  EphyNode *child,
		  EphyFaviconCache *eb)
{
	g_static_rw_lock_writer_lock (eb->priv->icons_hash_lock);

	g_hash_table_remove (eb->priv->icons_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_FAVICON_PROP_URL));

	g_static_rw_lock_writer_unlock (eb->priv->icons_hash_lock);
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
	ephy_node_thaw (eb->priv->icons);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (icon_is_obsolete (kid, &current_date))
		{
			const char *filename;
			const char *path;

			filename = ephy_node_get_property_string
				(kid, EPHY_NODE_FAVICON_PROP_FILENAME);
			path = g_build_filename (eb->priv->directory,
						 filename, NULL);
			gnome_vfs_unlink (path);
			g_object_unref (kid);
		}
	}
}

static void
ephy_favicon_cache_save (EphyFaviconCache *eb)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	GPtrArray *children;
	int i;

	/* save nodes to xml */
	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "ephy_favicons_cache", NULL);
	xmlSetProp (root, "version", EPHY_FAVICON_CACHE_XML_VERSION);
	xmlDocSetRootElement (doc, root);

	children = ephy_node_get_children (eb->priv->icons);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		ephy_node_save_to_xml (kid, root);
	}
	ephy_node_thaw (eb->priv->icons);

	xmlSaveFormatFile (eb->priv->xml_file, doc, 1);
}

static void
ephy_favicon_cache_init (EphyFaviconCache *cache)
{
	cache->priv = g_new0 (EphyFaviconCachePrivate, 1);

	cache->priv->xml_file = g_build_filename (ephy_dot_dir (),
					          "ephy-favicon-cache.xml",
					          NULL);

	cache->priv->directory = g_build_filename (ephy_dot_dir (),
						   "favicon_cache/",
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
	cache->priv->icons_hash_lock = g_new0 (GStaticRWLock, 1);
	g_static_rw_lock_init (cache->priv->icons_hash_lock);
	cache->priv->downloads_hash = g_hash_table_new (g_str_hash,
			                                g_str_equal);

	/* Icons */
	cache->priv->icons = ephy_node_new_with_id (ICONS_NODE_ID);
	ephy_node_ref (cache->priv->icons);
	g_signal_connect_object (G_OBJECT (cache->priv->icons),
				 "child_added",
				 G_CALLBACK (icons_added_cb),
				 G_OBJECT (cache),
				 0);
	g_signal_connect_object (G_OBJECT (cache->priv->icons),
				 "child_removed",
				 G_CALLBACK (icons_removed_cb),
				 G_OBJECT (cache),
				 0);

	ephy_favicon_cache_load (cache);
}

static gboolean
kill_download (gpointer key,
	       gpointer value,
	       gpointer data)
{
	EphyEmbedPersist *persist = EPHY_EMBED_PERSIST (value);
	EphyFaviconCache *cache = EPHY_FAVICON_CACHE (data);
	EphyNode *icon;

	ephy_embed_persist_cancel (persist);
	g_object_unref (persist);

	g_static_rw_lock_reader_lock (cache->priv->icons_hash_lock);
	icon = g_hash_table_lookup (cache->priv->icons_hash, (char *)key);
	g_static_rw_lock_reader_unlock (cache->priv->icons_hash_lock);

	g_object_unref (icon);

	return TRUE;
}


static void
cleanup_downloads_hash (EphyFaviconCache *cache)
{
	g_hash_table_foreach_remove (cache->priv->downloads_hash,
				     kill_download, cache);
}

static void
ephy_favicon_cache_finalize (GObject *object)
{
	EphyFaviconCache *cache;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_FAVICON_CACHE (object));

	cache = EPHY_FAVICON_CACHE (object);

	g_return_if_fail (cache->priv != NULL);

	cleanup_downloads_hash (cache);
	remove_obsolete_icons (cache);
	ephy_favicon_cache_save (cache);

	g_free (cache->priv->xml_file);
	g_free (cache->priv->directory);
	g_hash_table_destroy (cache->priv->icons_hash);
	g_static_rw_lock_free (cache->priv->icons_hash_lock);
	g_hash_table_destroy (cache->priv->downloads_hash);

	g_free (cache->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static char *
favicon_name_build (const char *url)
{
	char *res;
	char *slashpos;

	res = g_strdup (url);

	while ((slashpos = strstr (res, "/")) != NULL)
		*slashpos = '_';

	return res;
}

static void
favicon_download_completed_cb (EphyEmbedPersist *persist,
			       EphyFaviconCache *cache)
{
	char *url;

	url = g_object_get_data (G_OBJECT (persist), "url"),

	g_hash_table_remove (cache->priv->downloads_hash, url);
	g_object_unref (persist);

	g_signal_emit (G_OBJECT (cache), ephy_favicon_cache_signals[CHANGED], 0, url);
}

static void
ephy_favicon_cache_download (EphyFaviconCache *cache,
			     const char *favicon_url,
			     const char *filename)
{
	EphyEmbedPersist *persist;
	const char *dest;

	LOG ("Download favicon: %s", favicon_url)

	g_return_if_fail (EPHY_IS_FAVICON_CACHE (cache));
	g_return_if_fail (favicon_url != NULL);
	g_return_if_fail (filename != NULL);

	dest = g_build_filename (cache->priv->directory, filename, NULL);

	persist = ephy_embed_persist_new (NULL);

	ephy_embed_persist_set_max_size (persist, 100);
	ephy_embed_persist_set_flags    (persist, EMBED_PERSIST_BYPASSCACHE);
	ephy_embed_persist_set_source   (persist, favicon_url);
	ephy_embed_persist_set_dest     (persist, dest);

	g_object_set_data_full (G_OBJECT (persist), "url",
				g_strdup (favicon_url), g_free);

	g_signal_connect (G_OBJECT (persist),
			  "completed",
			  G_CALLBACK (favicon_download_completed_cb),
			  cache);

	ephy_embed_persist_save (persist);

	g_hash_table_insert (cache->priv->downloads_hash,
			     g_strdup (favicon_url), persist);
}

GdkPixbuf *
ephy_favicon_cache_get (EphyFaviconCache *cache,
			const char *url)
{
	GTime now;
	EphyNode *icon;
	GValue value = { 0, };
	const char *pix_file;
	GdkPixbuf *pixbuf;

	now = time (NULL);

	g_static_rw_lock_reader_lock (cache->priv->icons_hash_lock);
	icon = g_hash_table_lookup (cache->priv->icons_hash, url);
	g_static_rw_lock_reader_unlock (cache->priv->icons_hash_lock);

	if (!icon)
	{
		char *filename;

		filename = favicon_name_build (url);

		icon = ephy_node_new ();
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

		ephy_node_add_child (cache->priv->icons, icon);

		ephy_favicon_cache_download (cache, url, filename);

		g_free (filename);
	}

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, now);
	ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_LAST_USED,
			        &value);

	if (g_hash_table_lookup (cache->priv->downloads_hash, url) != NULL)
	{
		/* still downloading, return NULL */
		return NULL;
	}

	pix_file = g_build_filename
		(cache->priv->directory,
		 ephy_node_get_property_string (icon, EPHY_NODE_FAVICON_PROP_FILENAME),
		 NULL);

	g_hash_table_lookup (cache->priv->icons_hash, url);

	pixbuf = gdk_pixbuf_new_from_file (pix_file, NULL);

	if (pixbuf &&
	    (gdk_pixbuf_get_width (pixbuf) > 16 ||
	     gdk_pixbuf_get_height (pixbuf) > 16))
	{
		GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf, 16, 16,
							     GDK_INTERP_NEAREST);
		g_object_unref (G_OBJECT (pixbuf));
		pixbuf = scaled;
	}

	return pixbuf;
}



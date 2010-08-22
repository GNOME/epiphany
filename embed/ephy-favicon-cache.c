/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright © 2003-2004 Marco Pesenti Gritti
 *  Copyright © 2004, 2005 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ephy-favicon-cache.h"

#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-node-common.h"
#include "ephy-node.h"
#include "ephy-debug.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <webkit/webkit.h>

#define EPHY_FAVICON_CACHE_XML_ROOT    (const xmlChar *)"ephy_favicons_cache"
#define EPHY_FAVICON_CACHE_XML_VERSION (const xmlChar *)"1.1"

#define EPHY_FAVICON_CACHE_OBSOLETE_DAYS 30
#define SECS_PER_DAY (60*60*24)

/* how often to save the cache, in seconds */
#define CACHE_SAVE_INTERVAL	(10 * 60) /* seconds */

/* how often to delete old pixbufs from the cache, in seconds */
#define CACHE_CLEANUP_INTERVAL	(5 * 60) /* seconds */

/* how long to keep pixbufs in cache, in seconds. This should be longer than CACHE_CLEANUP_INTERVAL */
#define PIXBUF_CACHE_KEEP_TIME	10 * 60 /* s */

/* this is very generous, most files are 4k */
#define EPHY_FAVICON_MAX_SIZE	64 * 1024 /* byte */

static void ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass);
static void ephy_favicon_cache_init	  (EphyFaviconCache *cache);
static void ephy_favicon_cache_finalize	  (GObject *object);
static gboolean kill_download		  (const char*, WebKitDownload*, EphyFaviconCache*);

#define EPHY_FAVICON_CACHE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FAVICON_CACHE, EphyFaviconCachePrivate))

struct _EphyFaviconCachePrivate
{
	char *directory;
	char *xml_file;
	EphyNodeDb *db;
	EphyNode *icons;
	GHashTable *icons_hash;
	GHashTable *downloads_hash;
	guint autosave_timeout;
	guint cleanup_timeout;
	guint dirty : 1;
	guint64 requests;
	guint64 cached;
};

typedef struct
{
	EphyNode *node;
	time_t timestamp;
	GdkPixbuf *pixbuf;
	guint load_failed : 1;
} PixbufCacheEntry;

typedef gboolean (* FilterFunc) (EphyNode*, time_t);

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
	EPHY_NODE_FAVICON_PROP_CHECKED	 = 7,
};

enum
{
	NEEDS_TYPE_CHECK = 1 << 0,
	NEEDS_RENAME	 = 1 << 1,
	NEEDS_ICO_CHECK	 = 1 << 2,
	NEEDS_MASK	 = 0x3f
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyFaviconCache, ephy_favicon_cache, G_TYPE_OBJECT)

static void
ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

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

static void
pixbuf_cache_entry_free (PixbufCacheEntry *entry)
{
	if (entry->pixbuf != NULL)
	{
		g_object_unref (entry->pixbuf);
	}

	g_slice_free (PixbufCacheEntry, entry);
}

static gboolean
icon_is_obsolete (EphyNode *node, time_t now)
{
	int last_visit;

	last_visit = ephy_node_get_property_int
		(node, EPHY_NODE_FAVICON_PROP_LAST_USED);
	return now - last_visit >= EPHY_FAVICON_CACHE_OBSOLETE_DAYS*SECS_PER_DAY;
}

static void
icons_added_cb (EphyNode *node,
		EphyNode *child,
		EphyFaviconCache *eb)
{
	PixbufCacheEntry *entry = g_slice_new0 (PixbufCacheEntry);

	entry->node = child;

	g_hash_table_insert (eb->priv->icons_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_FAVICON_PROP_URL),
			     entry);
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
remove_obsolete_icons (EphyFaviconCache *cache,
		       FilterFunc filter)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	GPtrArray *children;
	int i;
	time_t now;

	now = time (NULL);

	children = ephy_node_get_children (priv->icons);
	for (i = (int) children->len - 1; i >= 0; i--)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (!filter || filter (kid, now))
		{
			const char *filename;
			char *path;

			filename = ephy_node_get_property_string
				(kid, EPHY_NODE_FAVICON_PROP_FILENAME);
			path = g_build_filename (priv->directory,
						 filename, NULL);
			if (g_unlink (path) < 0)
			{
				LOG ("Unable to delete %s", path);
			}

			g_free (path);
			ephy_node_unref (kid);
			priv->dirty = TRUE;
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
ephy_favicon_cache_save (EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	int ret;

	if (priv->dirty)
	{
		LOG ("Saving favicon cache\n");

		ret = ephy_node_db_write_to_xml_safe
			(cache->priv->db, (const xmlChar *) priv->xml_file,
			 EPHY_FAVICON_CACHE_XML_ROOT,
			 EPHY_FAVICON_CACHE_XML_VERSION,
			 NULL,
			 priv->icons, NULL, NULL,
			 NULL);

		/* still dirty if save failed */
		priv->dirty = ret < 0;
	}
}

static gboolean
periodic_save_cb (EphyFaviconCache *cache)
{
	ephy_favicon_cache_save (cache);
	return TRUE;
}

static void
cleanup_entry (char *key,
	       PixbufCacheEntry *entry,
	       time_t* now)
{
	if (entry->pixbuf != NULL &&
	    *now - entry->timestamp > PIXBUF_CACHE_KEEP_TIME)
	{
		LOG ("Evicting pixbuf for \"%s\" from pixbuf cache", key);

		g_object_unref (entry->pixbuf);
		entry->pixbuf = NULL;
		entry->load_failed = FALSE;
		entry->timestamp = 0;
	}
}

static gboolean
periodic_cleanup_cb (EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	time_t now;

	LOG ("Cleanup");

	now = time (NULL);
	g_hash_table_foreach (priv->icons_hash, (GHFunc) cleanup_entry, &now);

	return TRUE;
}

static void
ephy_favicon_cache_init (EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv;
	EphyNodeDb *db;

	priv = cache->priv = EPHY_FAVICON_CACHE_GET_PRIVATE (cache);

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

	/* The key is owned by the node */
	priv->icons_hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
						  (GDestroyNotify) pixbuf_cache_entry_free);
	priv->downloads_hash = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      g_free, NULL);
	
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

	priv->autosave_timeout = g_timeout_add_seconds (CACHE_SAVE_INTERVAL,
						(GSourceFunc) periodic_save_cb,
						cache);
	priv->cleanup_timeout = g_timeout_add_seconds (CACHE_CLEANUP_INTERVAL,
					       (GSourceFunc) periodic_cleanup_cb,
					       cache);
}

static gboolean
kill_download (const char *key,
	       WebKitDownload *download,
	       EphyFaviconCache *cache)
{
	EphyNode *icon;

	/* disconnect "completed" and "cancelled" callbacks */
	g_signal_handlers_disconnect_matched
		(download,G_SIGNAL_MATCH_DATA , 0, 0, NULL, NULL, cache);

	/* now cancel the download */
	webkit_download_cancel (download);

	icon = g_hash_table_lookup (cache->priv->icons_hash, key);
	g_return_val_if_fail (EPHY_IS_NODE (icon), TRUE);

	ephy_node_unref (icon);

	return TRUE;
}

static void
delete_file (GFile *dir,
	     GFileInfo *file_info)
{
	GFileType type;
	
	type = g_file_info_get_file_type (file_info);
	
	if (type == G_FILE_TYPE_REGULAR)
	{
		char *path;
		
		path = g_build_filename (g_file_get_path (dir), 
					 g_file_info_get_name (file_info),
					 NULL);
		if (g_unlink (path) < 0)
		{
			LOG ("Unable to delete %s", path);
		}
		
		g_free (path);
	}							      
}

static void
ephy_favicon_cache_finalize (GObject *object)
{
	EphyFaviconCache *cache = EPHY_FAVICON_CACHE (object);
	EphyFaviconCachePrivate *priv = cache->priv;

	LOG ("EphyFaviconCache finalising");

	g_hash_table_foreach_remove (priv->downloads_hash,
				     (GHRFunc) kill_download, cache);

	if (priv->autosave_timeout != 0)
	{
		g_source_remove (priv->autosave_timeout);
	}

	if (priv->cleanup_timeout != 0)
	{
		g_source_remove (priv->cleanup_timeout);
	}

	remove_obsolete_icons (cache, icon_is_obsolete);

	ephy_favicon_cache_save (cache);

	g_free (priv->xml_file);
	g_free (priv->directory);
	g_hash_table_destroy (priv->icons_hash);
	g_hash_table_destroy (priv->downloads_hash);

	g_object_unref (priv->db);

	LOG ("Requests: cached %" G_GUINT64_FORMAT " / total %" G_GUINT64_FORMAT " = %.3f",
	     priv->cached, priv->requests, ((double) priv->cached) / ((double) priv->requests));

	G_OBJECT_CLASS (ephy_favicon_cache_parent_class)->finalize (object);
}

static void
favicon_download_status_changed_cb (WebKitDownload *download,
				    GParamSpec *spec,
				    EphyFaviconCache *cache)
{
	WebKitDownloadStatus status = webkit_download_get_status (download);
	const char* url = webkit_download_get_uri (download);

	switch (status) {
	case WEBKIT_DOWNLOAD_STATUS_FINISHED:
		LOG ("Favicon cache download completed for %s", url);

		g_hash_table_remove (cache->priv->downloads_hash, url);

		g_signal_emit (G_OBJECT (cache), signals[CHANGED], 0, url);

		g_object_unref (download);

		cache->priv->dirty = TRUE;

		break;
	case WEBKIT_DOWNLOAD_STATUS_ERROR:
	case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
		LOG ("Favicon cache download cancelled %s", url);

		g_hash_table_remove (cache->priv->downloads_hash, url);

		/* TODO: remove a partially downloaded file */
		/* FIXME: re-schedule to try again after n days? */

		g_object_unref (download);

		cache->priv->dirty = TRUE;
		break;
	default:
		break;
	}
}

static void
ephy_favicon_cache_download (EphyFaviconCache *cache,
			     const char *favicon_url,
			     const char *filename)
{
	WebKitNetworkRequest *request;
	WebKitDownload *download;
	char *dest;
	char *dest_uri;

	LOG ("Download favicon: %s", favicon_url);

	g_return_if_fail (EPHY_IS_FAVICON_CACHE (cache));
	g_return_if_fail (favicon_url != NULL);
	g_return_if_fail (filename != NULL);

	request = webkit_network_request_new (favicon_url);
	download = webkit_download_new (request);
	g_object_unref (request);

	dest = g_build_filename (cache->priv->directory, filename, NULL);
	dest_uri = g_filename_to_uri (dest, NULL, NULL);

	webkit_download_set_destination_uri (download, dest_uri);

	g_free (dest);
	g_free (dest_uri);

	g_signal_connect (G_OBJECT (download), "notify::status",
			  G_CALLBACK (favicon_download_status_changed_cb), cache);

	g_hash_table_insert (cache->priv->downloads_hash,
			     g_strdup (favicon_url), download);

	webkit_download_start (download);
}

/**
 * ephy_favicons_cache_get:
 * @cache: an #EphyFaviconCache
 * @url: the URL of the icon to retrieve
 * 
 * Note: This will always return %NULL for non-http URLs.
 * 
 * Return value: the site icon at @url as a #GdkPixbuf, or %NULL if
 * if could not be retrieved. Unref when you don't need it anymore.
 */
GdkPixbuf *
ephy_favicon_cache_get (EphyFaviconCache *cache,
			const char *url)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	time_t now;
	PixbufCacheEntry *entry;
	EphyNode *icon;
	char *pix_file;
	GdkPixbuf *pixbuf = NULL;
	guint checklevel = NEEDS_MASK;
	int width, height;
	int favicon_checked;

	if (url == NULL) return NULL;

	if (!g_str_has_prefix (url, "http://") &&
	    !g_str_has_prefix (url, "https://")) return NULL;

	priv->requests += 1;

	now = time (NULL);

	entry = g_hash_table_lookup (priv->icons_hash, url);

	if (entry != NULL && entry->pixbuf != NULL)
	{
		LOG ("Pixbuf in cache for \"%s\"", url);

		priv->cached += 1;
		entry->timestamp = now;
		return g_object_ref (entry->pixbuf);
	}
	else if (entry != NULL && entry->load_failed)
	{
		/* No need to try again */
		priv->cached += 1;
		return NULL;
	}

	if (entry == NULL)
	{
		char *filename;

		filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);

		icon = ephy_node_new (cache->priv->db);
		ephy_node_set_property_string (icon,
					       EPHY_NODE_FAVICON_PROP_URL,
					       url);
		ephy_node_set_property_string (icon,
					       EPHY_NODE_FAVICON_PROP_FILENAME,
					       filename);
		ephy_node_set_property_int (icon,
					    EPHY_NODE_FAVICON_PROP_CHECKED,
					    (int) NEEDS_MASK & ~NEEDS_RENAME);

		/* This will also add an entry to the icons hash */
		ephy_node_add_child (priv->icons, icon);

		ephy_favicon_cache_download (cache, url, filename);

		g_free (filename);

		entry = g_hash_table_lookup (priv->icons_hash, url);
	}

	g_return_val_if_fail (entry != NULL, NULL);
	g_return_val_if_fail (entry->pixbuf == NULL, NULL);

	icon = entry->node;

	/* update timestamp */
	ephy_node_set_property_int (icon, EPHY_NODE_FAVICON_PROP_LAST_USED,
				    now);

	priv->dirty = TRUE;

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
	favicon_checked = ephy_node_get_property_int
		(icon, EPHY_NODE_FAVICON_PROP_CHECKED);
	if (favicon_checked != -1)
	{
		checklevel = (guint) favicon_checked;
	}

	/* First, update the filename */
	if (checklevel & NEEDS_RENAME)
	{
		char *new_pix_file, *urlhash;
		GValue value = { 0, };

		urlhash = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
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
		ephy_node_set_property_int (icon,
					    EPHY_NODE_FAVICON_PROP_CHECKED,
					    (int) checklevel);
	}

	/* Now check the type. We renamed the file above, so glib does NOT
	 * fall back to extension checking if the slow mime check fails for 
	 * whatever reason
	 */
	if (checklevel & NEEDS_TYPE_CHECK)
	{
		GFile *file;
		GFileInfo *file_info;
		const char *mime_type;
		gboolean valid = FALSE, is_ao = FALSE;
		
		file = g_file_new_for_path (pix_file);
	
		/* Sniff mime type and check if it's safe to open */
		file_info = g_file_query_info (file,
					       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
					       0, NULL, NULL);
		mime_type = g_file_info_get_content_type (file_info);
		if (file_info == NULL)
		{
			return NULL;
		}
		valid = strcmp (mime_type, "image/x-ico") == 0 ||
			strcmp (mime_type, "image/vnd.microsoft.icon") == 0 ||
			strcmp (mime_type, "image/png") == 0 ||
			strcmp (mime_type, "image/gif") == 0;
		is_ao = strcmp (mime_type, "application/octet-stream") == 0;

		g_object_unref (file_info);
		g_object_unref (file);

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

		ephy_node_set_property_int (icon,
					    EPHY_NODE_FAVICON_PROP_CHECKED,
					    (int) checklevel);
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
		entry->load_failed = TRUE;
		return NULL;
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* Reject icons that are too small */
	if (width < 12 || height < 12)
	{
		entry->load_failed = TRUE;
		return NULL;
	}

	/* Scale icons that are too big */
	if (width > 16 || height > 16)
	{
		GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf, 16, 16,
							     GDK_INTERP_NEAREST);
		g_object_unref (G_OBJECT (pixbuf));
		pixbuf = scaled;
	}

	/* Put the icon in the cache */
	LOG ("Putting pixbuf for \"%s\" in cache", url);

	entry->pixbuf = g_object_ref (pixbuf);
	entry->timestamp = now;
	entry->load_failed = FALSE;

	return pixbuf;
}

/**
 * ephy_favicons_cache_clear:
 * @cache: the #EphyFaviconCache to clear
 * 
 * Clears the favicon cache and removes any stored icon files from disk.
 */
void
ephy_favicon_cache_clear (EphyFaviconCache *cache)
{
	GFileEnumerator *file_enum;
	GFile *dir;
	GFileInfo *file_info = NULL;
	EphyFaviconCachePrivate *priv = cache->priv;

	g_return_if_fail (EPHY_IS_FAVICON_CACHE (cache));

	remove_obsolete_icons (cache, NULL);
	ephy_favicon_cache_save (cache);

	/* Now remove any remaining files from the cache directory */
	dir = g_file_new_for_path (priv->directory);
	file_enum = g_file_enumerate_children (dir,
					       G_FILE_ATTRIBUTE_STANDARD_TYPE ","
					       G_FILE_ATTRIBUTE_STANDARD_NAME ","
					       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
					       0, NULL, NULL);
	file_info = g_file_enumerator_next_file (file_enum, NULL, NULL);
	while (file_info != NULL)
	{
		delete_file (dir, file_info);
		file_info = g_file_enumerator_next_file (file_enum, NULL, NULL);
		g_object_unref (file_info);
	}
	g_object_unref (dir);
	g_file_enumerator_close (file_enum, NULL, NULL);
}

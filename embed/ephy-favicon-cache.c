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

#include <libgnomevfs/gnome-vfs-uri.h>
#include <libxml/tree.h>
#include <string.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtkstock.h>
#include <sys/stat.h>

#include "ephy-embed-persist.h"
#include "ephy-file-helpers.h"
#include "ephy-favicon-cache.h"

static void ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass);
static void ephy_favicon_cache_init (EphyFaviconCache *ma);
static void ephy_favicon_cache_finalize (GObject *object);
static void ephy_favicon_cache_insert (EphyFaviconCache *cache,
			               const char *url,
			               const char *pixbuf_location);
static char *ephy_favicon_cache_dest (EphyFaviconCache *cache,
				      const char *url);
static void favicon_download_completed_cb (EphyEmbedPersist *persist,
			                   EphyFaviconCache *cache);

struct EphyFaviconCachePrivate
{
	char *directory;

	GdkPixbuf *default_pixbuf;
	EphyHistory *history;
};

enum
{
	CHANGED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_HISTORY
};

enum
{
	EPHY_NODE_PAGE_PROP_FAVICON = 100
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
ephy_favicon_cache_set_property (GObject *object,
		                 guint prop_id,
		                 const GValue *value,
		                 GParamSpec *pspec)
{
	EphyFaviconCache *cache = EPHY_FAVICON_CACHE (object);

	switch (prop_id)
	{
	case PROP_HISTORY:
		cache->priv->history = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_favicon_cache_get_property (GObject *object,
		                 guint prop_id,
		                 GValue *value,
		                 GParamSpec *pspec)
{
	EphyFaviconCache *cache = EPHY_FAVICON_CACHE (object);

	switch (prop_id)
	{
	case PROP_HISTORY:
		g_value_set_object (value, cache->priv->history);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
static void
ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_favicon_cache_finalize;
	object_class->set_property = ephy_favicon_cache_set_property;
	object_class->get_property = ephy_favicon_cache_get_property;

	g_object_class_install_property (object_class,
					 PROP_HISTORY,
					 g_param_spec_object ("History",
							      "Source history",
							      "Source history",
							      EPHY_HISTORY_TYPE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));


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

static void
ephy_favicon_cache_init (EphyFaviconCache *cache)
{
	GtkWidget *dummy;

	cache->priv = g_new0 (EphyFaviconCachePrivate, 1);

	cache->priv->directory = g_build_filename (ephy_dot_dir (),
						   "favicon_cache/",
						   NULL);

	if (g_file_test (cache->priv->directory, G_FILE_TEST_IS_DIR) == FALSE)
	{
		if (g_file_test (cache->priv->directory, G_FILE_TEST_EXISTS))
		{
			g_error ("Please remove %s to continue.", cache->priv->directory);
		}

		if (mkdir (cache->priv->directory, 488) != 0)
		{
			g_error ("Couldn't mkdir %s.", cache->priv->directory);
		}
	}

	dummy = gtk_toolbar_new ();
	cache->priv->default_pixbuf = gtk_widget_render_icon (dummy,
							      GTK_STOCK_JUMP_TO,
							      GTK_ICON_SIZE_MENU, NULL);
	gtk_widget_destroy (dummy);
}

static void
ephy_favicon_cache_finalize (GObject *object)
{
	EphyFaviconCache *cache;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_FAVICON_CACHE (object));

	cache = EPHY_FAVICON_CACHE (object);

	g_return_if_fail (cache->priv != NULL);

	g_object_unref (G_OBJECT (cache->priv->default_pixbuf));

	g_object_unref (cache->priv->history);

	g_free (cache->priv->directory);

	g_free (cache->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyFaviconCache *
ephy_favicon_cache_new (EphyHistory *history)
{
	EphyFaviconCache *cache;

	cache = EPHY_FAVICON_CACHE (g_object_new (EPHY_TYPE_FAVICON_CACHE,
						  "History", history,
						  NULL));

	g_return_val_if_fail (cache->priv != NULL, NULL);

	return cache;
}

GdkPixbuf *
ephy_favicon_cache_lookup (EphyFaviconCache *cache,
			   const char *url)
{
	GdkPixbuf *ret;

	g_return_val_if_fail (EPHY_IS_FAVICON_CACHE (cache), NULL);

	if (url == NULL)
	{
		return cache->priv->default_pixbuf;
	}

	ret = ephy_favicon_cache_lookup_direct (cache, url);

	if (ret == NULL)
	{
		return cache->priv->default_pixbuf;
	}

	return ret;
}

GdkPixbuf *
ephy_favicon_cache_lookup_direct (EphyFaviconCache *cache,
				  const char *cache_url)
{
	GdkPixbuf *pixbuf;
	EphyNode *node;
	const char *pix_file;

	node = ephy_history_get_page (cache->priv->history, cache_url);
	if (node == NULL) return NULL;

	pix_file = ephy_node_get_property_string
		(node, EPHY_NODE_PAGE_PROP_FAVICON);
	if (pix_file == NULL) return NULL;

	pixbuf = gdk_pixbuf_new_from_file (pix_file, NULL);
	g_return_val_if_fail (pixbuf != NULL, NULL);

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

static void
ephy_favicon_cache_insert (EphyFaviconCache *cache,
			   const char *url,
			   const char *pixbuf_location)
{
	EphyNode *node;
	GValue value = { 0, };

	node = ephy_history_get_page (cache->priv->history, url);
	g_return_if_fail (node != NULL);

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, pixbuf_location);
	ephy_node_set_property (node, EPHY_NODE_PAGE_PROP_FAVICON,
			        &value);
	g_value_unset (&value);

	g_signal_emit (G_OBJECT (cache), ephy_favicon_cache_signals[CHANGED], 0, url);
}

static char *
ephy_favicon_cache_dest (EphyFaviconCache *cache, const char *url)
{
	char *slashpos, *dest, *my_url;

	my_url = g_strdup (url);

	while ((slashpos = strstr (my_url, "/")) != NULL)
		*slashpos = '_';

	dest = g_build_filename (cache->priv->directory, my_url, NULL);

	g_free (my_url);

	return dest;
}

void
ephy_favicon_cache_insert_from_url (EphyFaviconCache *cache,
				    const char *url,
				    const char *favicon_url)
{
	EphyEmbedPersist *persist;
	char *dest;

	g_return_if_fail (EPHY_IS_FAVICON_CACHE (cache));
	g_return_if_fail (url != NULL);
	g_return_if_fail (favicon_url != NULL);

	dest = ephy_favicon_cache_dest (cache, favicon_url);
	g_return_if_fail (dest != NULL);

	if (g_file_test (dest, G_FILE_TEST_EXISTS)) return;

	persist = ephy_embed_persist_new (NULL);

	ephy_embed_persist_set_max_size (persist, 100);
	ephy_embed_persist_set_flags    (persist, EMBED_PERSIST_BYPASSCACHE);
	ephy_embed_persist_set_source   (persist, favicon_url);
	ephy_embed_persist_set_dest     (persist, dest);

	g_object_set_data_full (G_OBJECT (persist), "url", g_strdup (url), g_free);
	g_object_set_data_full (G_OBJECT (persist), "favicon", dest, g_free);

	g_signal_connect (G_OBJECT (persist),
			  "completed",
			  G_CALLBACK (favicon_download_completed_cb),
			  cache);

	ephy_embed_persist_save (persist);
}

static void
favicon_download_completed_cb (EphyEmbedPersist *persist,
			       EphyFaviconCache *cache)
{
	ephy_favicon_cache_insert (cache,
				   g_object_get_data (G_OBJECT (persist), "url"),
				   g_object_get_data (G_OBJECT (persist), "favicon"));

	g_object_unref (G_OBJECT (persist));
}

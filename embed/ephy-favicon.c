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

#include <gtk/gtkwidget.h>
#include <string.h>

#include "ephy-favicon.h"
#include "ephy-embed-shell.h"

static void ephy_favicon_class_init (EphyFaviconClass *klass);
static void ephy_favicon_init (EphyFavicon *ma);
static void ephy_favicon_finalize (GObject *object);
static void ephy_favicon_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec);
static void ephy_favicon_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec);
static void ephy_favicon_update_image (EphyFavicon *favicon);
static void cache_changed_cb (EphyFaviconCache *cache,
		              const char *url,
		              EphyFavicon *favicon);

struct EphyFaviconPrivate
{
	EphyFaviconCache *cache;

	char *url;
};

enum
{
	PROP_0,
	PROP_CACHE,
	PROP_URL
};

enum
{
	CHANGED,
	LAST_SIGNAL
};

static guint ephy_favicon_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
ephy_favicon_get_type (void)
{
	static GType ephy_favicon_type = 0;

	if (ephy_favicon_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyFaviconClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_favicon_class_init,
			NULL,
			NULL,
			sizeof (EphyFavicon),
			0,
			(GInstanceInitFunc) ephy_favicon_init
		};

		ephy_favicon_type = g_type_register_static (GTK_TYPE_IMAGE,
							      "EphyFavicon",
							      &our_info, 0);
	}

	return ephy_favicon_type;
}

static void
ephy_favicon_class_init (EphyFaviconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_favicon_finalize;

	object_class->set_property = ephy_favicon_set_property;
	object_class->get_property = ephy_favicon_get_property;

	g_object_class_install_property (object_class,
					 PROP_CACHE,
					 g_param_spec_object ("cache",
							      "Favicon cache",
							      "Favicon cache",
							      EPHY_TYPE_FAVICON_CACHE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_URL,
					 g_param_spec_string ("url",
							      "Associated URL",
							      "Associated URL",
							      NULL,
							      G_PARAM_READWRITE));

	ephy_favicon_signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyFaviconClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}

static void
ephy_favicon_init (EphyFavicon *ma)
{
	ma->priv = g_new0 (EphyFaviconPrivate, 1);

	gtk_widget_set_size_request (GTK_WIDGET (ma), 16, 16);
}

static void
ephy_favicon_finalize (GObject *object)
{
	EphyFavicon *ma;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_FAVICON (object));

	ma = EPHY_FAVICON (object);

	g_return_if_fail (ma->priv != NULL);

	g_free (ma->priv->url);

	g_free (ma->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_favicon_set_property (GObject *object,
			   guint prop_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	EphyFavicon *favicon = EPHY_FAVICON (object);

	switch (prop_id)
	{
	case PROP_CACHE:
		favicon->priv->cache = g_value_get_object (value);

		g_signal_connect_object (G_OBJECT (favicon->priv->cache),
				         "changed",
				         G_CALLBACK (cache_changed_cb),
				         favicon,
				         0);

		ephy_favicon_update_image (favicon);
		break;
	case PROP_URL:
		g_free (favicon->priv->url);
		favicon->priv->url = g_strdup (g_value_get_string (value));

		ephy_favicon_update_image (favicon);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_favicon_get_property (GObject *object,
			   guint prop_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	EphyFavicon *favicon = EPHY_FAVICON (object);

	switch (prop_id)
	{
	case PROP_CACHE:
		g_value_set_object (value, favicon->priv->cache);
		break;
	case PROP_URL:
		g_value_set_string (value, favicon->priv->url);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

GtkWidget *
ephy_favicon_new (const char *url)
{
	EphyFavicon *favicon;
	EphyFaviconCache *cache = ephy_embed_shell_get_favicon_cache (embed_shell);

	g_return_val_if_fail (EPHY_IS_FAVICON_CACHE (cache), NULL);

	favicon = EPHY_FAVICON (g_object_new (EPHY_TYPE_FAVICON,
						"cache", cache,
						"url", url,
						NULL));

	g_return_val_if_fail (favicon->priv != NULL, NULL);

	return GTK_WIDGET (favicon);
}

void
ephy_favicon_set_url (EphyFavicon *favicon,
		      const char *url)
{
	g_return_if_fail (EPHY_IS_FAVICON (favicon));

	g_object_set (G_OBJECT (favicon),
		      "url", url,
		      NULL);
}

const char *
ephy_favicon_get_url (EphyFavicon *favicon)
{
	char *url;

	g_return_val_if_fail (EPHY_IS_FAVICON (favicon), NULL);

	g_object_get (G_OBJECT (favicon),
		      "url", &url,
		      NULL);

	return (const char *) url;
}

static void
cache_changed_cb (EphyFaviconCache *cache,
		  const char *url,
		  EphyFavicon *favicon)
{
	if (strcmp (url, favicon->priv->url) == 0)
	{
		ephy_favicon_update_image (favicon);
	}
}

static void
ephy_favicon_update_image (EphyFavicon *favicon)
{
	GdkPixbuf *pixbuf;

	g_return_if_fail (EPHY_IS_FAVICON_CACHE (favicon->priv->cache));

	pixbuf = ephy_favicon_cache_lookup (favicon->priv->cache,
					      favicon->priv->url);

	gtk_image_set_from_pixbuf (GTK_IMAGE (favicon), pixbuf);

	g_signal_emit (G_OBJECT (favicon), ephy_favicon_signals[CHANGED], 0);
}

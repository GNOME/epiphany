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

#include <string.h>

#include "ephy-embed-favicon.h"
#include "ephy-embed-shell.h"

static void ephy_embed_favicon_class_init (EphyEmbedFaviconClass *klass);
static void ephy_embed_favicon_init (EphyEmbedFavicon *ma);
static void ephy_embed_favicon_finalize (GObject *object);
static void ephy_embed_favicon_set_property (GObject *object,
					     guint prop_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void ephy_embed_favicon_get_property (GObject *object,
					     guint prop_id,
					     GValue *value,
					     GParamSpec *pspec);

struct EphyEmbedFaviconPrivate
{
	EphyEmbed *embed;
};

enum
{
	PROP_0,
	PROP_EMBED
};

static GObjectClass *parent_class = NULL;

GType
ephy_embed_favicon_get_type (void)
{
	static GType ephy_embed_favicon_type = 0;

	if (ephy_embed_favicon_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEmbedFaviconClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_embed_favicon_class_init,
			NULL,
			NULL,
			sizeof (EphyEmbedFavicon),
			0,
			(GInstanceInitFunc) ephy_embed_favicon_init
		};

		ephy_embed_favicon_type = g_type_register_static (EPHY_TYPE_FAVICON,
							            "EphyEmbedFavicon",
							            &our_info, 0);
	}

	return ephy_embed_favicon_type;
}

static void
ephy_embed_favicon_class_init (EphyEmbedFaviconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_embed_favicon_finalize;

	object_class->set_property = ephy_embed_favicon_set_property;
	object_class->get_property = ephy_embed_favicon_get_property;

	g_object_class_install_property (object_class,
					 PROP_EMBED,
					 g_param_spec_object ("embed",
							      "Associated embed",
							      "Associated embed",
							      G_TYPE_OBJECT,
							      G_PARAM_READWRITE));
}

static void
ephy_embed_favicon_init (EphyEmbedFavicon *ma)
{
	ma->priv = g_new0 (EphyEmbedFaviconPrivate, 1);
}

static void
ephy_embed_favicon_finalize (GObject *object)
{
	EphyEmbedFavicon *ma;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_EMBED_FAVICON (object));

	ma = EPHY_EMBED_FAVICON (object);

	g_return_if_fail (ma->priv != NULL);

	g_free (ma->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
update_url (EphyEmbedFavicon *favicon)
{
	char *location;

	ephy_embed_get_location (favicon->priv->embed,
				 TRUE, &location);

	if (location)
	{
		ephy_favicon_set_url (EPHY_FAVICON (favicon), location);
		g_free (location);
	}
}

static void
location_changed_cb (EphyEmbed *embed,
	             EphyEmbedFavicon *favicon)
{
	update_url (favicon);
}

static void
favicon_cb (EphyEmbed *embed,
	    const char *favicon_url,
	    EphyEmbedFavicon *favicon)
{
	char *url = NULL;
	EphyFaviconCache *cache;

	if (favicon->priv->embed == NULL)
		return;

	ephy_embed_get_location (favicon->priv->embed, TRUE, &url);

	g_object_get (G_OBJECT (favicon),
		      "cache", &cache,
		      NULL);

	ephy_favicon_cache_insert_from_url (cache,
					    url,
					    favicon_url);

	g_object_unref (cache);

	g_free (url);
}

static void
ephy_embed_favicon_set_property (GObject *object,
			         guint prop_id,
			         const GValue *value,
			         GParamSpec *pspec)
{
	EphyEmbedFavicon *favicon = EPHY_EMBED_FAVICON (object);

	switch (prop_id)
	{
	case PROP_EMBED:
		favicon->priv->embed = g_value_get_object (value);

		if (favicon->priv->embed != NULL)
		{
			g_signal_connect_object (G_OBJECT (favicon->priv->embed),
					         "ge_favicon",
					         G_CALLBACK (favicon_cb),
					         favicon,
						 0);
			g_signal_connect_object (G_OBJECT (favicon->priv->embed),
					         "ge_location",
					         G_CALLBACK (location_changed_cb),
					         favicon,
						 0);
			update_url (favicon);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_embed_favicon_get_property (GObject *object,
			         guint prop_id,
			         GValue *value,
			         GParamSpec *pspec)
{
	EphyEmbedFavicon *favicon = EPHY_EMBED_FAVICON (object);

	switch (prop_id)
	{
	case PROP_EMBED:
		g_value_set_object (value, favicon->priv->embed);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

GtkWidget *
ephy_embed_favicon_new (EphyEmbed *embed)
{
	EphyEmbedFavicon *favicon;
	EphyFaviconCache *cache = ephy_embed_shell_get_favicon_cache (embed_shell);

	g_return_val_if_fail (EPHY_IS_FAVICON_CACHE (cache), NULL);

	favicon = EPHY_EMBED_FAVICON (g_object_new (EPHY_TYPE_EMBED_FAVICON,
						      "cache", cache,
						      "embed", embed,
						      NULL));

	g_return_val_if_fail (favicon->priv != NULL, NULL);

	return GTK_WIDGET (favicon);
}

void
ephy_embed_favicon_set_embed (EphyEmbedFavicon *favicon,
			        EphyEmbed *embed)
{
	g_return_if_fail (EPHY_IS_EMBED_FAVICON (favicon));

	g_object_set (G_OBJECT (favicon),
		      "embed", embed,
		      NULL);
}

EphyEmbed *
ephy_embed_favicon_get_embed (EphyEmbedFavicon *favicon)
{
	EphyEmbed *embed;

	g_return_val_if_fail (EPHY_IS_EMBED_FAVICON (favicon), NULL);

	g_object_get (G_OBJECT (favicon),
		      "embed", &embed,
		      NULL);

	return embed;
}

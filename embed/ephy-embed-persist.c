/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-embed-persist.h"
#include "mozilla-embed-persist.h"
#include "ephy-debug.h"

enum
{
	PROP_0,
	PROP_DEST,
	PROP_EMBED,
	PROP_FILECHOOSER_TITLE,
	PROP_FILECHOOSER_PARENT,
	PROP_FLAGS,
	PROP_HANDLER,
	PROP_MAX_SIZE,
	PROP_PERSISTKEY,
	PROP_SOURCE,	
};

#define EPHY_EMBED_PERSIST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_PERSIST, EphyEmbedPersistPrivate))

struct EphyEmbedPersistPrivate
{
	char *dest;
	char *source;
	char *fc_title;
	char *persist_key;
	EphyEmbed *embed;
	long max_size;
	EmbedPersistFlags flags;
	GtkWindow *fc_parent;
};

static void	ephy_embed_persist_class_init	(EphyEmbedPersistClass *klass);
static void	ephy_embed_persist_init		(EphyEmbedPersist *ges);

static GObjectClass *parent_class = NULL;

GType
ephy_embed_persist_get_type (void)
{
       static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEmbedPersistClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_embed_persist_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EphyEmbedPersist),
			0,    /* n_preallocs */
			(GInstanceInitFunc) ephy_embed_persist_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyEmbedPersist",
					       &our_info, 0);
	}

	return type;
}

void
ephy_embed_persist_set_dest (EphyEmbedPersist *persist,
			     const char *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->dest = g_strdup (value);
}

void
ephy_embed_persist_set_embed (EphyEmbedPersist *persist,
			      EphyEmbed *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->embed = value;
}

void
ephy_embed_persist_set_fc_title (EphyEmbedPersist *persist,
				 const char *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->fc_title = g_strdup (value);
}

void
ephy_embed_persist_set_fc_parent (EphyEmbedPersist *persist,
				  GtkWindow *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->fc_parent = value;
}

void
ephy_embed_persist_set_flags (EphyEmbedPersist *persist,
			      EmbedPersistFlags value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->flags = value;
}

void
ephy_embed_persist_set_max_size (EphyEmbedPersist *persist,
				 long value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->max_size = value;
}

void
ephy_embed_persist_set_persist_key (EphyEmbedPersist *persist,
				    const char *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->persist_key = g_strdup (value);
}

void
ephy_embed_persist_set_source (EphyEmbedPersist *persist,
			       const char *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->source = g_strdup (value);
}

const char *
ephy_embed_persist_get_dest (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->dest;
}

EphyEmbed *
ephy_embed_persist_get_embed (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->embed;
}

const char *
ephy_embed_persist_get_fc_title (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->fc_title;
}

GtkWindow *
ephy_embed_persist_get_fc_parent (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->fc_parent;
}

EmbedPersistFlags
ephy_embed_persist_get_flags (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), 0);

	return persist->priv->flags;
}

int
ephy_embed_persist_get_max_size (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), 0);

	return persist->priv->max_size;
}

const char *
ephy_embed_persist_get_persist_key (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->persist_key;
}

const char *
ephy_embed_persist_get_source (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->source;
}

static void
ephy_embed_persist_set_property (GObject *object,
				 guint prop_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	EphyEmbedPersist *persist = EPHY_EMBED_PERSIST (object);

	switch (prop_id)
	{
		case PROP_DEST:
			ephy_embed_persist_set_dest (persist, g_value_get_string (value));
			break;
		case PROP_EMBED:
			ephy_embed_persist_set_embed (persist, g_value_get_object (value));
			break;
		case PROP_FILECHOOSER_TITLE:
			ephy_embed_persist_set_fc_title (persist, g_value_get_string (value));
			break;
		case PROP_FILECHOOSER_PARENT:
			ephy_embed_persist_set_fc_parent (persist, g_value_get_object (value));
			break;
		case PROP_FLAGS:
			ephy_embed_persist_set_flags (persist, g_value_get_int (value));
			break;
		case PROP_MAX_SIZE:
			ephy_embed_persist_set_max_size (persist, g_value_get_long (value));
			break;
		case PROP_PERSISTKEY:
			ephy_embed_persist_set_persist_key (persist, g_value_get_string (value));
			break;
		case PROP_SOURCE:
			ephy_embed_persist_set_source (persist, g_value_get_string (value));
			break;
	}
}

static void
ephy_embed_persist_get_property (GObject *object,
				 guint prop_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	EphyEmbedPersist *persist = EPHY_EMBED_PERSIST (object);

	switch (prop_id)
	{
		case PROP_DEST:
			g_value_set_string (value, ephy_embed_persist_get_dest (persist));
			break;
		case PROP_EMBED:
			g_value_set_object (value, ephy_embed_persist_get_embed (persist));
			break;
		case PROP_FILECHOOSER_TITLE:
			g_value_set_string (value, ephy_embed_persist_get_fc_title (persist));
			break;
		case PROP_FILECHOOSER_PARENT:
			g_value_set_object (value, ephy_embed_persist_get_fc_parent (persist));
			break;
		case PROP_FLAGS:
			g_value_set_int (value, ephy_embed_persist_get_flags (persist));
			break;
		case PROP_MAX_SIZE:
			g_value_set_long (value, ephy_embed_persist_get_max_size (persist));
			break;
		case PROP_PERSISTKEY:
			g_value_set_string (value, ephy_embed_persist_get_persist_key (persist));
			break;
		case PROP_SOURCE:
			g_value_set_string (value, ephy_embed_persist_get_source (persist));
			break;
	}
}

static void
ephy_embed_persist_init (EphyEmbedPersist *persist)
{
	persist->priv = EPHY_EMBED_PERSIST_GET_PRIVATE (persist);

	LOG ("EphyEmbedPersist initialising %p", persist)

	persist->priv->dest = NULL;
	persist->priv->source = NULL;
	persist->priv->fc_title = NULL;
	persist->priv->fc_parent = NULL;
	persist->priv->flags = 0;
	persist->priv->max_size = -1;
	persist->priv->persist_key = NULL;
}

static void
ephy_embed_persist_finalize (GObject *object)
{
	EphyEmbedPersist *persist = EPHY_EMBED_PERSIST (object);

	g_free (persist->priv->dest);
	g_free (persist->priv->source);
	g_free (persist->priv->fc_title);
	g_free (persist->priv->persist_key);

	LOG ("EphyEmbedPersist finalised %p", object)

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_embed_persist_class_init (EphyEmbedPersistClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_embed_persist_finalize;
	object_class->set_property = ephy_embed_persist_set_property;
	object_class->get_property = ephy_embed_persist_get_property;

	/* init signals */
	g_signal_new ("completed",
		      G_OBJECT_CLASS_TYPE (object_class),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyEmbedPersistClass, completed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);
	g_signal_new ("cancelled",
		      G_OBJECT_CLASS_TYPE (object_class),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyEmbedPersistClass, cancelled),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	g_object_class_install_property (object_class,
					 PROP_DEST,
					 g_param_spec_string ("dest",
							      "Destination",
							      "Destination file path",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_EMBED,
					 g_param_spec_object ("embed",
							      "Embed",
							      "The embed containing the document",
							      G_TYPE_OBJECT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_FILECHOOSER_TITLE,
					 g_param_spec_string  ("filechooser-title",
							       "Filechooser title",
							       "Title to use if showing filechooser",
							       NULL,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_FILECHOOSER_PARENT,
					 g_param_spec_object ("filechooser-parent",
							      "Filechooser parent",
							      "The parent window for the filechooser",
							      GTK_TYPE_WINDOW,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_FLAGS,
					 g_param_spec_int    ("flags",
							      "Flags",
							      "Flags",
							      0,
							      G_MAXINT,
							      0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_MAX_SIZE,
					 g_param_spec_long   ("max_size",
							      "Maxsize",
							      "Maximum size of the file",
							      0,
							      G_MAXLONG,
							      0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_PERSISTKEY,
					 g_param_spec_string  ("persist-key",
							       "persist key",
							       "Path persistence gconf key",
							       NULL,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SOURCE,
					 g_param_spec_string  ("source",
							       "Source",
							       "Url of the document to save",
							       NULL,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(EphyEmbedPersistPrivate));
}

void
ephy_embed_persist_cancel (EphyEmbedPersist *persist)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	klass->cancel (persist);
}

gboolean
ephy_embed_persist_save (EphyEmbedPersist *persist)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->save (persist);
}
char *
ephy_embed_persist_to_string (EphyEmbedPersist *persist)
{
       EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
       return klass->to_string (persist);
}


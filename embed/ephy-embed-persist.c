/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#include <config.h>

#include "ephy-embed-persist.h"
#include "ephy-debug.h"
#include "mozilla-embed.h"
#include "mozilla-embed-persist.h"

enum
{
        COMPLETED,
        LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_EMBED,
  PROP_SOURCE,
  PROP_DEST,
  PROP_MAX_SIZE,
  PROP_FLAGS,
  PROP_HANDLER
};

#define EPHY_EMBED_PERSIST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_PERSIST, EphyEmbedPersistPrivate))

struct EphyEmbedPersistPrivate
{
	char *dir;
	char *src;
	PersistHandlerInfo *handler;
	EphyEmbed *embed;
	int max_size;
	EmbedPersistFlags flags;
};

static void
ephy_embed_persist_class_init (EphyEmbedPersistClass *klass);
static void
ephy_embed_persist_init (EphyEmbedPersist *ges);
static void
ephy_embed_persist_finalize (GObject *object);
static void
ephy_embed_persist_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec);
static void
ephy_embed_persist_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec);

static gresult
impl_set_source (EphyEmbedPersist *persist,
		 const char *url);
static gresult
impl_set_dest (EphyEmbedPersist *persist,
	       const char *dir);
static gresult
impl_set_max_size (EphyEmbedPersist *persist,
		   int kb_size);
static gresult
impl_set_embed (EphyEmbedPersist *persist,
		EphyEmbed *embed);
static gresult
impl_set_flags (EphyEmbedPersist *persist,
		EmbedPersistFlags flags);
static gresult
impl_set_handler (EphyEmbedPersist *persist,
		  const char *command,
		  gboolean need_terminal);

static GObjectClass *parent_class = NULL;
static guint ephy_embed_persist_signals[LAST_SIGNAL] = { 0 };

GType
ephy_embed_persist_get_type (void)
{
       static GType ephy_embed_persist_type = 0;

        if (ephy_embed_persist_type == 0)
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

                ephy_embed_persist_type = g_type_register_static (G_TYPE_OBJECT,
                                                                    "EphyEmbedPersist",
                                                                    &our_info, 0);
        }

        return ephy_embed_persist_type;
}

static void
ephy_embed_persist_class_init (EphyEmbedPersistClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_embed_persist_finalize;
        object_class->set_property = ephy_embed_persist_set_property;
	object_class->get_property = ephy_embed_persist_get_property;

	klass->set_source = impl_set_source;
	klass->set_dest   = impl_set_dest;
	klass->set_embed  = impl_set_embed;
	klass->set_max_size = impl_set_max_size;
	klass->set_flags = impl_set_flags;
	klass->set_handler = impl_set_handler;

	/* init signals */
        ephy_embed_persist_signals[COMPLETED] =
                g_signal_new ("completed",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EphyEmbedPersistClass, completed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);

	g_object_class_install_property (object_class,
                                         PROP_EMBED,
                                         g_param_spec_object ("embed",
                                                              "Embed",
                                                              "The embed containing the document",
                                                              G_TYPE_OBJECT,
                                                              G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_SOURCE,
                                         g_param_spec_string  ("source",
                                                               "Source",
                                                               "Url of the document to save",
                                                               NULL,
                                                               G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
                                         PROP_DEST,
                                         g_param_spec_string ("dest",
                                                              "Destination",
                                                              "Destination directory",
                                                              NULL,
                                                              G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
                                         PROP_MAX_SIZE,
                                         g_param_spec_int    ("max_size",
                                                              "Maxsize",
                                                              "Maximum size of the file",
                                                              0,
							      G_MAXINT,
							      0,
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
                                         PROP_HANDLER,
                                         g_param_spec_pointer ("handler",
                                                              "Handler",
                                                              "Handler",
                                                              G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(EphyEmbedPersistPrivate));
}

static void
ephy_embed_persist_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	EphyEmbedPersist *persist;
	PersistHandlerInfo *h;

	persist = EPHY_EMBED_PERSIST (object);

	switch (prop_id)
	{
		case PROP_EMBED:
			ephy_embed_persist_set_embed (persist,
							g_value_get_object (value));
			break;
		case PROP_SOURCE:
			ephy_embed_persist_set_source (persist,
							 g_value_get_string (value));
			break;
		case PROP_DEST:
			ephy_embed_persist_set_dest  (persist,
							 g_value_get_string (value));
			break;
		case PROP_MAX_SIZE:
			ephy_embed_persist_set_max_size  (persist,
							    g_value_get_int (value));
			break;
		case PROP_FLAGS:
			ephy_embed_persist_set_flags
				(persist,
				(EmbedPersistFlags)g_value_get_int (value));
			break;
		case PROP_HANDLER:
			h = (PersistHandlerInfo *)g_value_get_pointer (value);
			ephy_embed_persist_set_handler
				(persist, h->command, h->need_terminal);
			break;
	}
}

static void
ephy_embed_persist_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	EphyEmbedPersist *persist;

	persist = EPHY_EMBED_PERSIST (object);

	switch (prop_id)
	{
		case PROP_EMBED:
			g_value_set_object (value, persist->priv->embed);
			break;
		case PROP_SOURCE:
			g_value_set_string (value, persist->priv->src);
			break;
		case PROP_DEST:
			g_value_set_string (value, persist->priv->dir);
			break;
		case PROP_MAX_SIZE:
			g_value_set_int (value, persist->priv->max_size);
			break;
		case PROP_FLAGS:
			g_value_set_int (value, (int)persist->priv->flags);
			break;
		case PROP_HANDLER:
			g_value_set_pointer (value, persist->priv->handler);
	}
}

static void
ephy_embed_persist_init (EphyEmbedPersist *persist)
{
	persist->priv = EPHY_EMBED_PERSIST_GET_PRIVATE (persist);

	persist->priv->src = NULL;
	persist->priv->dir = NULL;
	persist->priv->handler = NULL;

	LOG ("Embed persist init")
}

static void
ephy_embed_persist_finalize (GObject *object)
{
        EphyEmbedPersist *persist = EPHY_EMBED_PERSIST (object);

	g_free (persist->priv->dir);
	g_free (persist->priv->src);

	if (persist->priv->handler)
	{
		g_free (persist->priv->handler->command);
		g_free (persist->priv->handler);
	}

	LOG ("Embed persist finalize")

        G_OBJECT_CLASS (parent_class)->finalize (object);
}


EphyEmbedPersist *
ephy_embed_persist_new (EphyEmbed *embed)
{
	EphyEmbedPersist *persist;

        persist = EPHY_EMBED_PERSIST (g_object_new (MOZILLA_TYPE_EMBED_PERSIST,
						    "embed", embed,
						    NULL));

	return persist;
}

gresult
ephy_embed_persist_set_source (EphyEmbedPersist *persist,
				 const char *url)
{
	 EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	 return klass->set_source (persist, url);
}

gresult
ephy_embed_persist_get_source (EphyEmbedPersist *persist,
				 const char **url)
{
	*url = persist->priv->src;

	return G_OK;
}

gresult
ephy_embed_persist_get_dest (EphyEmbedPersist *persist,
			       const char **dir)
{
	*dir = persist->priv->dir;

	return G_OK;
}

gresult
ephy_embed_persist_set_dest (EphyEmbedPersist *persist,
			       const char *dir)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->set_dest (persist, dir);
}

gresult
ephy_embed_persist_cancel (EphyEmbedPersist *persist)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->cancel (persist);
}

gresult
ephy_embed_persist_save (EphyEmbedPersist *persist)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->save (persist);
}

gresult
ephy_embed_persist_set_max_size (EphyEmbedPersist *persist,
			           int kb_size)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->set_max_size (persist, kb_size);
}

gresult
ephy_embed_persist_set_embed (EphyEmbedPersist *persist,
		                EphyEmbed *embed)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->set_embed (persist, embed);
}

gresult
ephy_embed_persist_get_embed (EphyEmbedPersist *persist,
		                EphyEmbed **embed)
{
	*embed = persist->priv->embed;

	return G_OK;
}

gresult
ephy_embed_persist_set_flags (EphyEmbedPersist *persist,
		                EmbedPersistFlags flags)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->set_flags (persist, flags);
}

gresult
ephy_embed_persist_get_flags (EphyEmbedPersist *persist,
		                EmbedPersistFlags *flags)
{
	*flags = persist->priv->flags= persist->priv->flags;

	return G_OK;
}

gresult
ephy_embed_persist_set_handler (EphyEmbedPersist *persist,
				  const char *command,
				  gboolean need_terminal)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->set_handler (persist, command, need_terminal);
}

static gresult
impl_set_handler (EphyEmbedPersist *persist,
		  const char *command,
		  gboolean need_terminal)
{
	persist->priv->handler = g_new0 (PersistHandlerInfo, 1);
	persist->priv->handler->command = g_strdup (command);
	persist->priv->handler->need_terminal = need_terminal;

	g_object_notify (G_OBJECT(persist), "handler");

	return G_OK;
}

static gresult
impl_set_flags (EphyEmbedPersist *persist,
		EmbedPersistFlags flags)
{
	persist->priv->flags = flags;

	g_object_notify (G_OBJECT(persist), "flags");

	return G_OK;
}

static gresult
impl_set_source (EphyEmbedPersist *persist,
		 const char *url)
{
	persist->priv->src = g_strdup(url);

	g_object_notify (G_OBJECT(persist), "source");

	return G_OK;
}

static gresult
impl_set_dest (EphyEmbedPersist *persist,
	       const char *dir)
{
	persist->priv->dir = g_strdup (dir);

	g_object_notify (G_OBJECT(persist), "dest");

	return G_OK;
}

static gresult
impl_set_max_size (EphyEmbedPersist *persist,
		   int kb_size)
{
	persist->priv->max_size = kb_size;

	g_object_notify (G_OBJECT(persist), "max_size");

	return G_OK;
}

static gresult
impl_set_embed (EphyEmbedPersist *persist,
		EphyEmbed *embed)
{
	persist->priv->embed = embed;

	g_object_notify (G_OBJECT(persist), "embed");

	return G_OK;
}

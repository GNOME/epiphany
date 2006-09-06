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

#include "config.h"

#include "ephy-embed-persist.h"
#include "mozilla-embed-persist.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-debug.h"

#include <gtk/gtkmain.h>

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
	PROP_USER_TIME	
};

#define EPHY_EMBED_PERSIST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_PERSIST, EphyEmbedPersistPrivate))

struct _EphyEmbedPersistPrivate
{
	char *dest;
	char *source;
	char *fc_title;
	char *persist_key;
	EphyEmbed *embed;
	gint64 max_size;
	EphyEmbedPersistFlags flags;
	GtkWindow *fc_parent;
	guint32 user_time;
};

static void	ephy_embed_persist_class_init	(EphyEmbedPersistClass *klass);
static void	ephy_embed_persist_init		(EphyEmbedPersist *ges);

static GObjectClass *parent_class = NULL;

GType
ephy_embed_persist_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
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
					       &our_info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

/**
 * ephy_embed_persist_set_dest:
 * @persist: an #EphyEmbedPersist
 * @value: the path to which @persist should save data
 *
 * Sets the path to which @persist should save data.
 **/
void
ephy_embed_persist_set_dest (EphyEmbedPersist *persist,
			     const char *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->dest = g_strdup (value);
}

/**
 * ephy_embed_persist_set_embed:
 * @persist: an #EphyEmbedPersist
 * @value: a parent #EphyEmbed
 *
 * Sets the #EphyEmbed from which @persist will download data.
 *
 * An #EphyEmbed is absolutely required to download if @persist's
 * #EphyEmbedPersistFlags include %EPHY_EMBED_PERSIST_COPY_PAGE. Regardless, an
 * #EphyEmbed should be set for <emphasis>every</emphasis> #EphyEmbedPersist,
 * since it determines request information such as the referring page.
 **/
void
ephy_embed_persist_set_embed (EphyEmbedPersist *persist,
			      EphyEmbed *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->embed = value;
}

/**
 * ephy_embed_persist_set_fc_title:
 * @persist: an #EphyEmbedPersist
 * @value: the title to be displayed by the filechooser
 *
 * Sets the title of the filechooser window. The filechooser will only be
 * displayed if %EPHY_EMBED_PERSIST_ASK_DESTINATION has been set with
 * ephy_embed_persist_set_flags().
 **/
void
ephy_embed_persist_set_fc_title (EphyEmbedPersist *persist,
				 const char *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->fc_title = g_strdup (value);
}

/**
 * ephy_embed_persist_set_fc_parent:
 * @persist: an #EphyEmbedPersist
 * @value: the #EphyWindow which should be the filechooser's parent
 *
 * Sets the #EphyWindow which should be @persist's filechooser's parent. The
 * filechooser will only be displayed if %EPHY_EMBED_PERSIST_ASK_DESTINATION has been
 * set with ephy_embed_persist_set_flags().
 **/
void
ephy_embed_persist_set_fc_parent (EphyEmbedPersist *persist,
				  GtkWindow *value)
{
	EphyEmbedPersistPrivate *priv;
	GtkWindow **wptr;

	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	priv = persist->priv;

	priv->fc_parent = value;
	wptr = &priv->fc_parent;
	g_object_add_weak_pointer (G_OBJECT (priv->fc_parent),
				   (gpointer *) wptr);
}

/**
 * ephy_embed_persist_set_flags:
 * @persist: an #EphyEmbedPersist
 * @value: the desired #EphyEmbedPersistFlags
 *
 * Sets the flags to be used for @persist's download.
 **/
void
ephy_embed_persist_set_flags (EphyEmbedPersist *persist,
			      EphyEmbedPersistFlags value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->flags = value;
}

/**
 * ephy_embed_persist_set_max_size:
 * @persist: an #EphyEmbedPersist
 * @value: maximum size of requested download, in bytes
 *
 * Sets the maximum size of @persist's download.
 *
 * If the requested file is discovered to be larger than @value, the download
 * will be aborted. Note that @persist will have to actually begin downloading
 * before it can abort, since it doesn't know the filesize before the download
 * starts.
 **/
void
ephy_embed_persist_set_max_size (EphyEmbedPersist *persist,
				 gint64 value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->max_size = value;
}

/**
 * ephy_embed_persist_set_persist_key:
 * @persist: an #EphyEmbedPersist
 * @value: a GConf key
 *
 * Sets the GConf key from which @persist will determine the default download
 * directory.
 **/
void
ephy_embed_persist_set_persist_key (EphyEmbedPersist *persist,
				    const char *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->persist_key = g_strdup (value);
}

/**
 * ephy_embed_persist_set_source:
 * @persist: an #EphyEmbedPersist
 * @value: the URL from which @persist should download
 *
 * Sets the URL from which @persist should download. This should be used in
 * conjunction with ephy_embed_persist_set_embed().
 **/
void
ephy_embed_persist_set_source (EphyEmbedPersist *persist,
			       const char *value)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	persist->priv->source = g_strdup (value);
}

/**
 * ephy_embed_persist_set_user_time:
 * @persist: an #EphyEmbedPersist
 * @user_time: a timestamp, or 0
 *
 * Sets the time stamp of the user action which created @persist.
 * Defaults to gtk_get_current_event_time() when @persist is created.
 **/
void
ephy_embed_persist_set_user_time (EphyEmbedPersist *persist,
				  guint32 user_time)
{
	g_return_if_fail (EPHY_IS_EMBED_PERSIST (persist));

	LOG ("ephy_embed_persist_set_user_time persist %p user-time %d",
	     persist, user_time);

	persist->priv->user_time = user_time;
}

/**
 * ephy_embed_persist_get_dest:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the destination filename to which @persist will save its download.
 *
 * Return value: @persist's destination filename
 **/
const char *
ephy_embed_persist_get_dest (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->dest;
}

/**
 * ephy_embed_persist_get_embed:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the #EphyEmbed from which @persist will download.
 *
 * Return value: the #EphyEmbed from which @persist will download
 **/
EphyEmbed *
ephy_embed_persist_get_embed (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->embed;
}

/**
 * ephy_embed_persist_get_fc_title:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the title to be displayed in @persist's filechooser.
 *
 * Return value: the title to be displayed in @persist's filechooser
 **/
const char *
ephy_embed_persist_get_fc_title (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->fc_title;
}

/**
 * ephy_embed_persist_get_fc_parent:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the #EphyWindow which should serve as a parent for @persist's
 * filechooser.
 *
 * Return value: the #EphyWindow parent for @persist's filechooser
 **/
GtkWindow *
ephy_embed_persist_get_fc_parent (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->fc_parent;
}

/**
 * ephy_embed_persist_get_flags:
 * @persist: an #EphyEmbedPersist
 *
 * Returns @persist's #EphyEmbedPersistFlags.
 *
 * Return value: @persist's #EphyEmbedPersistFlags
 **/
EphyEmbedPersistFlags
ephy_embed_persist_get_flags (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), 0);

	return persist->priv->flags;
}

/**
 * ephy_embed_persist_get_max_size:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the maximum size of @persist's requested download, in bytes.
 *
 * Return value: the maximum size of @persist's requested download, in bytes
 **/
gint64
ephy_embed_persist_get_max_size (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), 0);

	return persist->priv->max_size;
}

/**
 * ephy_embed_persist_get_persist_key:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the GConf key which determines Epiphany's default download directory.
 *
 * Return value: the GConf key to the default download directory
 **/
const char *
ephy_embed_persist_get_persist_key (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->persist_key;
}

/**
 * ephy_embed_persist_get_source:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the source URL of the file to download.
 *
 * Return value: a source URL
 **/
const char *
ephy_embed_persist_get_source (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), NULL);

	return persist->priv->source;
}

/**
 * ephy_embed_persist_get_user_time:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the timestamp of the user action which created @persist.
 * If not set explicitly, defaults to gtk_get_current_event_time ()
 * at the time of creation of @persist.
 *
 * Return value: a timestamp, or 0
 **/
guint32
ephy_embed_persist_get_user_time (EphyEmbedPersist *persist)
{
	g_return_val_if_fail (EPHY_IS_EMBED_PERSIST (persist), 0);

	return persist->priv->user_time;
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
			ephy_embed_persist_set_flags (persist, g_value_get_flags (value));
			break;
		case PROP_MAX_SIZE:
			ephy_embed_persist_set_max_size (persist, g_value_get_int64 (value));
			break;
		case PROP_PERSISTKEY:
			ephy_embed_persist_set_persist_key (persist, g_value_get_string (value));
			break;
		case PROP_SOURCE:
			ephy_embed_persist_set_source (persist, g_value_get_string (value));
			break;
		case PROP_USER_TIME:
			ephy_embed_persist_set_user_time (persist, g_value_get_uint (value));
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
			g_value_set_flags (value, ephy_embed_persist_get_flags (persist));
			break;
		case PROP_MAX_SIZE:
			g_value_set_int64 (value, ephy_embed_persist_get_max_size (persist));
			break;
		case PROP_PERSISTKEY:
			g_value_set_string (value, ephy_embed_persist_get_persist_key (persist));
			break;
		case PROP_SOURCE:
			g_value_set_string (value, ephy_embed_persist_get_source (persist));
			break;
		case PROP_USER_TIME:
			g_value_set_uint (value, ephy_embed_persist_get_user_time (persist));
			break;
	}
}

static void
ephy_embed_persist_init (EphyEmbedPersist *persist)
{
	persist->priv = EPHY_EMBED_PERSIST_GET_PRIVATE (persist);

	LOG ("EphyEmbedPersist initialising %p", persist);

	persist->priv->max_size = -1;

	ephy_embed_persist_set_user_time (persist, gtk_get_current_event_time ());
}

static void
ephy_embed_persist_finalize (GObject *object)
{
	EphyEmbedPersist *persist = EPHY_EMBED_PERSIST (object);
	EphyEmbedPersistPrivate *priv = persist->priv;
	GtkWindow **wptr;

	g_free (priv->dest);
	g_free (priv->source);
	g_free (priv->fc_title);
	g_free (priv->persist_key);

	if (priv->fc_parent != NULL)
	{
		wptr = &priv->fc_parent;
		g_object_remove_weak_pointer (G_OBJECT (priv->fc_parent),
					      (gpointer *) wptr);
	}

	LOG ("EphyEmbedPersist finalised %p", object);

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
/**
 * EphyEmbedPersist::completed:
 *
 * The ::completed signal is emitted when @persist has finished downloading. The
 * download must have started with ephy_embed_persist_save().
 **/
	g_signal_new ("completed",
		      G_OBJECT_CLASS_TYPE (object_class),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyEmbedPersistClass, completed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);
/**
 * EphyEmbedPersist::cancelled:
 *
 * The ::cancelled signal is emitted when @persist's download has been
 * cancelled with ephy_embed_persist_cancel().
 **/
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
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_EMBED,
					 g_param_spec_object ("embed",
							      "Embed",
							      "The embed containing the document",
							      G_TYPE_OBJECT,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_FILECHOOSER_TITLE,
					 g_param_spec_string  ("filechooser-title",
							       "Filechooser title",
							       "Title to use if showing filechooser",
							       NULL,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_FILECHOOSER_PARENT,
					 g_param_spec_object ("filechooser-parent",
							      "Filechooser parent",
							      "The parent window for the filechooser",
							      GTK_TYPE_WINDOW,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_FLAGS,
					 g_param_spec_flags  ("flags",
							      "Flags",
							      "Flags",
							      EPHY_TYPE_EMBED_PERSIST_FLAGS,
							      0,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_MAX_SIZE,
					 g_param_spec_int64  ("max-size",
							      "Maxsize",
							      "Maximum size of the file",
							      -1,
							      G_MAXINT64,
							      -1,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_PERSISTKEY,
					 g_param_spec_string  ("persist-key",
							       "persist key",
							       "Path persistence gconf key",
							       NULL,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_SOURCE,
					 g_param_spec_string  ("source",
							       "Source",
							       "Url of the document to save",
							       NULL,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_USER_TIME,
					 g_param_spec_uint   ("user-time",
							      "User Time",
							      "User Time",
							      0,
							      G_MAXUINT,
							      0,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof(EphyEmbedPersistPrivate));
}

/**
 * ephy_embed_persist_cancel:
 * @persist: an #EphyEmbedPersist
 *
 * Cancels @persist's download. This will not delete the partially downloaded
 * file.
 **/
void
ephy_embed_persist_cancel (EphyEmbedPersist *persist)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	klass->cancel (persist);
}

/**
 * ephy_embed_persist_save:
 * @persist: an #EphyEmbedPersist
 *
 * Begins saving the file specified in @persist.
 *
 * If @persist's #EphyEmbedPersistFlags include %EPHY_EMBED_PERSIST_ASK_DESTINATION, a
 * filechooser dialog will be shown first.
 *
 * The file will continue to download in the background until either the
 * ::completed or the ::cancelled signals are emitted by @persist.
 *
 * Return value: %TRUE if the download began successfully
 **/
gboolean
ephy_embed_persist_save (EphyEmbedPersist *persist)
{
	EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
	return klass->save (persist);
}

/**
 * ephy_embed_persist_to_string:
 * @persist: an #EphyEmbedPersist
 *
 * Returns the download specified by @persist as a string instead of downloading
 * it to a file.
 *
 * The download is synchronous. An #EphyEmbed must be specified with
 * ephy_embed_persist_set_embed(). The function implicitly assumes that
 * @persist's #EphyEmbedPersistFlags include %EPHY_EMBED_PERSIST_COPY_PAGE. If @persist's
 * #EphyEmbed has not finished downloading, this function will only return the
 * portion of data which has already been downloaded.
 *
 * The document will be modified: it will only include absolute links and it
 * will be encoded as UTF-8.
 *
 * Return value: The contents of @persist's #EphyEmbed's web page
 **/
char *
ephy_embed_persist_to_string (EphyEmbedPersist *persist)
{
       EphyEmbedPersistClass *klass = EPHY_EMBED_PERSIST_GET_CLASS (persist);
       return klass->to_string (persist);
}

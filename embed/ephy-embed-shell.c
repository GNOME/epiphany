/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-factory.h"
#include "ephy-marshal.h"
#include "ephy-file-helpers.h"
#include "ephy-history.h"
#include "ephy-favicon-cache.h"
#include "mozilla-embed-single.h"
#include "downloader-view.h"
#include "ephy-encodings.h"
#include "ephy-debug.h"

#define EPHY_EMBED_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellPrivate))

struct _EphyEmbedShellPrivate
{
	EphyHistory *global_history;
	DownloaderView *downloader_view;
	EphyFaviconCache *favicon_cache;
	EphyEmbedSingle *embed_single;
	EphyEncodings *encodings;
};

enum
{
	PREPARE_CLOSE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void ephy_embed_shell_class_init	(EphyEmbedShellClass *klass);
static void ephy_embed_shell_init	(EphyEmbedShell *shell);

EphyEmbedShell *embed_shell = NULL;

static GObjectClass *parent_class = NULL;

GType
ephy_embed_shell_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEmbedShellClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_embed_shell_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EphyEmbedShell),
			0,    /* n_preallocs */
			(GInstanceInitFunc) ephy_embed_shell_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyEmbedShell",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_embed_shell_dispose (GObject *object)
{
	EphyEmbedShell *shell = EPHY_EMBED_SHELL (object);

	if (shell->priv->downloader_view)
	{
		LOG ("Unref downloader");
		g_object_remove_weak_pointer
			(G_OBJECT(shell->priv->downloader_view),
			 (gpointer *) &shell->priv->downloader_view);
		g_object_unref (shell->priv->downloader_view);
	}

	if (shell->priv->favicon_cache)
	{
		LOG ("Unref favicon cache");
		g_object_unref (G_OBJECT (shell->priv->favicon_cache));
	}

	if (shell->priv->encodings)
	LOG ("Unref encodings");
	{
		LOG ("Unref encodings");
		g_object_unref (G_OBJECT (shell->priv->encodings));
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ephy_embed_shell_finalize (GObject *object)
{
	EphyEmbedShell *shell = EPHY_EMBED_SHELL (object);

	if (shell->priv->global_history)
	{
		LOG ("Unref history");
		g_object_unref (shell->priv->global_history);
	}

	if (shell->priv->embed_single)
	{
		LOG ("Unref embed single");
		g_object_unref (G_OBJECT (shell->priv->embed_single));
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * ephy_embed_shell_get_favicon_cache:
 * @shell: the #EphyEmbedShell
 *
 * Returns the favicons cache.
 *
 * Return value: the favicons cache
 **/
GObject *
ephy_embed_shell_get_favicon_cache (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->favicon_cache == NULL)
	{
		shell->priv->favicon_cache = ephy_favicon_cache_new ();
	}

	return G_OBJECT (shell->priv->favicon_cache);
}

GObject *
ephy_embed_shell_get_global_history (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->global_history == NULL)
	{
		shell->priv->global_history = ephy_history_new ();
	}

	return G_OBJECT (shell->priv->global_history);
}

GObject *
ephy_embed_shell_get_downloader_view (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->downloader_view == NULL)
	{
		shell->priv->downloader_view = downloader_view_new ();
		g_object_add_weak_pointer
			(G_OBJECT(shell->priv->downloader_view),
			 (gpointer *)&shell->priv->downloader_view);
	}

	return G_OBJECT (shell->priv->downloader_view);
}

static GObject *
impl_get_embed_single (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->embed_single == NULL)
	{
		shell->priv->embed_single = EPHY_EMBED_SINGLE
			(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_SINGLE));
	}

	return G_OBJECT (shell->priv->embed_single);
}

GObject *
ephy_embed_shell_get_embed_single (EphyEmbedShell *shell)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);

	return klass->get_embed_single (shell);
}

GObject *
ephy_embed_shell_get_encodings (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->encodings == NULL)
	{
		shell->priv->encodings = ephy_encodings_new ();
	}

	return G_OBJECT (shell->priv->encodings);
}

void
ephy_embed_shell_prepare_close (EphyEmbedShell *shell)
{
	g_signal_emit (shell, signals[PREPARE_CLOSE], 0);
}

static void
ephy_embed_shell_init (EphyEmbedShell *shell)
{
	shell->priv = EPHY_EMBED_SHELL_GET_PRIVATE (shell);

	/* globally accessible singleton */
	g_assert (embed_shell == NULL);
	embed_shell = shell;
}

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	object_class->dispose = ephy_embed_shell_dispose;
	object_class->finalize = ephy_embed_shell_finalize;

	klass->get_embed_single = impl_get_embed_single;

/**
 * EphyEmbed::prepare-close:
 * @shell:
 * 
 * The ::prepare-close signal is emitted when epiphany is preparing to
 * quit on command from the session manager. You can use it when you need
 * to do something special (shut down a service, for example).
 **/
	signals[PREPARE_CLOSE] =
		g_signal_new ("prepare-close",
			      EPHY_TYPE_EMBED_SHELL,
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedShellClass, prepare_close),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	g_type_class_add_private (object_class, sizeof (EphyEmbedShellPrivate));
}

/**
 * ephy_embed_shell_get_default:
 *
 * Retrieves the default #EphyEmbedShell object
 *
 * ReturnValue: the default #EphyEmbedShell
 **/
EphyEmbedShell *
ephy_embed_shell_get_default (void)
{
	return embed_shell;
}

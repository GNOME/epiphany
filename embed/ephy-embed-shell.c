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
 */

#include <config.h>

#include "ephy-embed-shell.h"
#include "ephy-marshal.h"
#include "ephy-favicon-cache.h"
#include "mozilla-embed-single.h"
#include "ephy-debug.h"

#include <string.h>

struct EphyEmbedShellPrivate
{
	EphyHistory *global_history;
	DownloaderView *downloader_view;
	EphyFaviconCache *favicon_cache;
	EphyEmbedSingle *embed_single;
};

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass);
static void
ephy_embed_shell_init (EphyEmbedShell *ges);
static void
ephy_embed_shell_finalize (GObject *object);

static EphyHistory *
impl_get_global_history (EphyEmbedShell *shell);
static DownloaderView *
impl_get_downloader_view (EphyEmbedShell *shell);

static GObjectClass *parent_class = NULL;

EphyEmbedShell *embed_shell;

GType
ephy_embed_shell_get_type (void)
{
       static GType ephy_embed_shell_type = 0;

        if (ephy_embed_shell_type == 0)
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

                ephy_embed_shell_type = g_type_register_static (G_TYPE_OBJECT,
								"EphyEmbedShell",
								&our_info, 0);
        }

        return ephy_embed_shell_type;
}

static void
ephy_embed_shell_class_init (EphyEmbedShellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

        object_class->finalize = ephy_embed_shell_finalize;
	klass->get_downloader_view = impl_get_downloader_view;
	klass->get_global_history = impl_get_global_history;
}

static void
ephy_embed_shell_init (EphyEmbedShell *ges)
{
	/* Singleton, globally accessible */
	embed_shell = ges;

	ges->priv = g_new0 (EphyEmbedShellPrivate, 1);

	ges->priv->global_history = NULL;
	ges->priv->downloader_view = NULL;
	ges->priv->favicon_cache = NULL;
}

static void
ephy_embed_shell_finalize (GObject *object)
{
	EphyEmbedShell *ges;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EMBED_SHELL (object));

        ges = EPHY_EMBED_SHELL (object);

        g_return_if_fail (ges->priv != NULL);

	LOG ("Unref history")
	if (ges->priv->global_history)
	{
		g_object_unref (ges->priv->global_history);
	}

	LOG ("Unref downloader")
	if (ges->priv->downloader_view)
	{
		g_object_remove_weak_pointer
			(G_OBJECT(ges->priv->downloader_view),
			 (gpointer *)&ges->priv->downloader_view);
		g_object_unref (ges->priv->downloader_view);
	}

	LOG ("Unref favicon cache")
	if (ges->priv->favicon_cache)
	{
		g_object_unref (G_OBJECT (ges->priv->favicon_cache));
	}

	LOG ("Unref embed single")
	if (ges->priv->embed_single)
	{
		g_object_unref (G_OBJECT (ges->priv->embed_single));
	}

        g_free (ges->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyEmbedShell *
ephy_embed_shell_new (const char *type)
{
	if (strcmp (type, "mozilla") == 0)
	{
		return EPHY_EMBED_SHELL (g_object_new
			(EPHY_EMBED_SHELL_TYPE, NULL));
	}

	g_assert_not_reached ();
	return NULL;
}

/**
 * ephy_embed_shell_get_favicon_cache:
 * @gs: a #EphyShell
 *
 * Returns the favicons cache.
 *
 * Return value: the favicons cache
 **/
EphyFaviconCache *
ephy_embed_shell_get_favicon_cache (EphyEmbedShell *ees)
{
	if (ees->priv->favicon_cache == NULL)
	{
		ees->priv->favicon_cache = ephy_favicon_cache_new ();
	}

	return ees->priv->favicon_cache;
}

EphyHistory *
ephy_embed_shell_get_global_history (EphyEmbedShell *shell)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->get_global_history (shell);
}

DownloaderView *
ephy_embed_shell_get_downloader_view (EphyEmbedShell *shell)
{
	EphyEmbedShellClass *klass = EPHY_EMBED_SHELL_GET_CLASS (shell);
        return klass->get_downloader_view (shell);
}

EphyEmbedSingle *
ephy_embed_shell_get_embed_single (EphyEmbedShell *shell)
{
	if (!shell->priv->embed_single)
	{
		shell->priv->embed_single = ephy_embed_single_new ("mozilla");
	}

	return shell->priv->embed_single;
}

static EphyHistory *
impl_get_global_history (EphyEmbedShell *shell)
{
	if (!shell->priv->global_history)
	{
		shell->priv->global_history = ephy_history_new ();
	}

	return shell->priv->global_history;
}

static DownloaderView *
impl_get_downloader_view (EphyEmbedShell *shell)
{
	if (!shell->priv->downloader_view)
	{
		shell->priv->downloader_view = downloader_view_new ();
		g_object_add_weak_pointer
			(G_OBJECT(shell->priv->downloader_view),
			 (gpointer *)&shell->priv->downloader_view);
	}

	return shell->priv->downloader_view;
}


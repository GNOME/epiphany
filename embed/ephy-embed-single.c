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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-embed-shell.h"
#include "ephy-marshal.h"
#include "ephy-favicon-cache.h"
#include "mozilla-embed-single.h"
#include "ephy-debug.h"
#include "downloader-view.h"

#include <string.h>

#define EPHY_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSinglePrivate))

struct EphyEmbedSinglePrivate
{
	EphyHistory *global_history;
	DownloaderView *downloader_view;
	EphyFaviconCache *favicon_cache;
};

static void
ephy_embed_single_class_init (EphyEmbedSingleClass *klass);
static void
ephy_embed_single_init (EphyEmbedSingle *ges);

static GObjectClass *parent_class = NULL;

GType
ephy_embed_single_get_type (void)
{
       static GType ephy_embed_single_type = 0;

	if (ephy_embed_single_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEmbedSingleClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_embed_single_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EphyEmbedSingle),
			0,    /* n_preallocs */
			(GInstanceInitFunc) ephy_embed_single_init
		};

		ephy_embed_single_type = g_type_register_static (G_TYPE_OBJECT,
								"EphyEmbedSingle",
								&our_info, 0);
	}

	return ephy_embed_single_type;
}

static void
ephy_embed_single_class_init (EphyEmbedSingleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	g_type_class_add_private (object_class, sizeof(EphyEmbedSinglePrivate));
}

static void
ephy_embed_single_init (EphyEmbedSingle *ges)
{
	ges->priv = EPHY_EMBED_SINGLE_GET_PRIVATE (ges);

	ges->priv->global_history = NULL;
	ges->priv->downloader_view = NULL;

	ges->priv->favicon_cache = NULL;
}

void
ephy_embed_single_clear_cache (EphyEmbedSingle *shell)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	klass->clear_cache (shell);
}

void
ephy_embed_single_set_offline_mode (EphyEmbedSingle *shell,
				   gboolean offline)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	klass->set_offline_mode (shell, offline);
}

void
ephy_embed_single_load_proxy_autoconf (EphyEmbedSingle *shell,
				      const char* url)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	klass->load_proxy_autoconf (shell, url);
}

GList *
ephy_embed_single_get_font_list (EphyEmbedSingle *shell,
				 const char *langGroup)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	return klass->get_font_list (shell, langGroup);
}

GList *
ephy_embed_single_list_cookies (EphyEmbedSingle *shell)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	return klass->list_cookies (shell);
}

void
ephy_embed_single_remove_cookies (EphyEmbedSingle *shell,
				 GList *cookies)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	klass->remove_cookies (shell, cookies);
}

GList *
ephy_embed_single_list_passwords (EphyEmbedSingle *shell,
				  PasswordType type)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	return klass->list_passwords (shell, type);
}

void
ephy_embed_single_remove_passwords (EphyEmbedSingle *shell,
				    GList *passwords,
				    PasswordType type)
{
	EphyEmbedSingleClass *klass = EPHY_EMBED_SINGLE_GET_CLASS (shell);
	klass->remove_passwords (shell, passwords, type);
}

void
ephy_embed_single_free_cookies (EphyEmbedSingle *shell,
				GList *cookies)
{
	GList *l;

	for (l = cookies; l != NULL; l = l->next)
	{
		CookieInfo *info = (CookieInfo *)l->data;

		g_free (info->domain);
		g_free (info->name);
		g_free (info->value);
		g_free (info->path);
		g_free (info->secure);
		g_free (info->expire);
		g_free (info);
	}

	g_list_free (cookies);
}

void
ephy_embed_single_free_passwords (EphyEmbedSingle *shell,
				  GList *passwords)
{
	GList *l;

	for (l = passwords; l != NULL; l = l->next)
	{
		PasswordInfo *info = (PasswordInfo *)l->data;
		g_free (info->host);
		g_free (info->username);
		g_free (info);
	}

	g_list_free (passwords);
}

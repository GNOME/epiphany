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

#include <libxml/xmlreader.h>
#include <string.h>

#define EPHY_EMBED_SHELL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_SHELL, EphyEmbedShellPrivate))

struct EphyEmbedShellPrivate
{
	EphyHistory *global_history;
	DownloaderView *downloader_view;
	EphyFaviconCache *favicon_cache;
	EphyEmbedSingle *embed_single;
	EphyEncodings *encodings;
	GHashTable *mime_table;
};

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
ephy_embed_shell_finalize (GObject *object)
{
	EphyEmbedShell *shell = EPHY_EMBED_SHELL (object);

	LOG ("Unref history")
	if (shell->priv->global_history)
	{
		g_object_unref (shell->priv->global_history);
	}

	LOG ("Unref downloader")
	if (shell->priv->downloader_view)
	{
		g_object_remove_weak_pointer
			(G_OBJECT(shell->priv->downloader_view),
			 (gpointer *) &shell->priv->downloader_view);
		g_object_unref (shell->priv->downloader_view);
	}

	LOG ("Unref favicon cache")
	if (shell->priv->favicon_cache)
	{
		g_object_unref (G_OBJECT (shell->priv->favicon_cache));
	}

	LOG ("Unref encodings")
	if (shell->priv->encodings)
	{
		g_object_unref (G_OBJECT (shell->priv->encodings));
	}

	LOG ("Unref embed single")
	if (shell->priv->embed_single)
	{
		g_object_unref (G_OBJECT (shell->priv->embed_single));
	}

	LOG ("Destroying mime type hashtable")
	if (shell->priv->mime_table)
	{
		g_hash_table_destroy (shell->priv->mime_table);
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

GObject *
ephy_embed_shell_get_embed_single (EphyEmbedShell *shell)
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
ephy_embed_shell_get_encodings (EphyEmbedShell *shell)
{
	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), NULL);

	if (shell->priv->encodings == NULL)
	{
		shell->priv->encodings = ephy_encodings_new ();
	}

	return G_OBJECT (shell->priv->encodings);
}

static void
load_mime_from_xml (EphyEmbedShell *shell)
{
	xmlTextReaderPtr reader;
	const char *xml_file;
	int ret;
	EphyMimePermission permission = EPHY_MIME_PERMISSION_UNKNOWN;

	xml_file = ephy_file ("mime-types-permissions.xml");
	if (xml_file == NULL)
	{
		g_warning ("MIME types permissions file not found!\n");
		return;
	}

	reader = xmlNewTextReaderFilename (xml_file);
	if (reader == NULL)
	{
		g_warning ("Could not load MIME types permissions file!\n");
		return;
	}

	ret = xmlTextReaderRead (reader);
	while (ret == 1)
	{
		const xmlChar *tag;
		xmlReaderTypes type;

		tag = xmlTextReaderConstName (reader);
		type = xmlTextReaderNodeType (reader);

		if (xmlStrEqual (tag, "safe") && type == XML_READER_TYPE_ELEMENT)
		{
			permission = EPHY_MIME_PERMISSION_SAFE;
		}
		else if (xmlStrEqual (tag, "unsafe") && type == XML_READER_TYPE_ELEMENT)
		{
			permission = EPHY_MIME_PERMISSION_UNSAFE;
		}
		else if (xmlStrEqual (tag, "mime-type"))
		{
			xmlChar *type;

			type = xmlTextReaderGetAttribute (reader, "type");
			g_hash_table_insert (shell->priv->mime_table,
					     type, GINT_TO_POINTER (permission));
		}

		ret = xmlTextReaderRead (reader);
	}

	xmlFreeTextReader (reader);
}

EphyMimePermission
ephy_embed_shell_check_mime (EphyEmbedShell *shell, const char *mime_type)
{
	EphyMimePermission permission;
	gpointer tmp;

	g_return_val_if_fail (EPHY_IS_EMBED_SHELL (shell), EPHY_MIME_PERMISSION_UNKNOWN);

	if (shell->priv->mime_table == NULL)
	{
		shell->priv->mime_table = g_hash_table_new_full
			(g_str_hash, g_str_equal, xmlFree, NULL);
		load_mime_from_xml (shell);
	}

	tmp = g_hash_table_lookup (shell->priv->mime_table, mime_type);
	if (tmp == NULL)
	{
		permission = EPHY_MIME_PERMISSION_UNKNOWN;
	}
	else
	{
		permission = GPOINTER_TO_INT (tmp);
	}

	return permission;
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

	object_class->finalize = ephy_embed_shell_finalize;

	g_type_class_add_private (object_class, sizeof (EphyEmbedShellPrivate));
}

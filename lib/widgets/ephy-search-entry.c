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
 *
 *  $Id$
 */

#include "config.h"

#include <gtk/gtklabel.h>
#include <glib/gi18n.h>
#include <string.h>

#include "ephy-search-entry.h"

static void ephy_search_entry_class_init (EphySearchEntryClass *klass);
static void ephy_search_entry_init (EphySearchEntry *entry);

#define EPHY_SEARCH_ENTRY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SEARCH_ENTRY, EphySearchEntryPrivate))

struct _EphySearchEntryPrivate
{
	gboolean clearing;

	guint timeout;
};

enum
{
	SEARCH,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static guint ephy_search_entry_signals[LAST_SIGNAL] = { 0 };

GType
ephy_search_entry_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphySearchEntryClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_search_entry_class_init,
			NULL,
			NULL,
			sizeof (EphySearchEntry),
			0,
			(GInstanceInitFunc) ephy_search_entry_init
		};

		type = g_type_register_static (GTK_TYPE_ENTRY,
					       "EphySearchEntry",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_search_entry_class_init (EphySearchEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	ephy_search_entry_signals[SEARCH] =
		g_signal_new ("search",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphySearchEntryClass, search),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (EphySearchEntryPrivate));
}

static gboolean
ephy_search_entry_timeout_cb (EphySearchEntry *entry)
{
	g_signal_emit (G_OBJECT (entry), ephy_search_entry_signals[SEARCH], 0,
		       gtk_entry_get_text (GTK_ENTRY (entry)));
	entry->priv->timeout = 0;

	return FALSE;
}

static void
ephy_search_entry_changed_cb (GtkEditable *editable,
			      EphySearchEntry *entry)
{
	if (entry->priv->clearing == TRUE)
		return;

	if (entry->priv->timeout != 0)
	{
		g_source_remove (entry->priv->timeout);
		entry->priv->timeout = 0;
	}

	entry->priv->timeout = g_timeout_add (300, (GSourceFunc) ephy_search_entry_timeout_cb, entry);
}

static void
ephy_search_entry_destroy_cb (GtkEditable *editable,
			      EphySearchEntry *entry)
{
	if (entry->priv->timeout)
	{
		g_source_remove (entry->priv->timeout);
		entry->priv->timeout = 0;
	}
}

static void
ephy_search_entry_init (EphySearchEntry *entry)
{
	entry->priv = EPHY_SEARCH_ENTRY_GET_PRIVATE (entry);

	g_signal_connect (G_OBJECT (entry),
			  "destroy",
			  G_CALLBACK (ephy_search_entry_destroy_cb),
			  entry);
	g_signal_connect (G_OBJECT (entry),
			  "changed",
			  G_CALLBACK (ephy_search_entry_changed_cb),
			  entry);
}

GtkWidget *
ephy_search_entry_new (void)
{
	GtkWidget *entry;

	entry = GTK_WIDGET (g_object_new (EPHY_TYPE_SEARCH_ENTRY,
					  NULL));

	return entry;
}

void
ephy_search_entry_clear (EphySearchEntry *entry)
{
	if (entry->priv->timeout != 0)
	{
		g_source_remove (entry->priv->timeout);
		entry->priv->timeout = 0;
	}

	entry->priv->clearing = TRUE;

	gtk_entry_set_text (GTK_ENTRY (entry), "");

	entry->priv->clearing = FALSE;
}

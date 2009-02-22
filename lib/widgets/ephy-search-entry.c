/* 
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <glib/gi18n.h>

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

static guint ephy_search_entry_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EphySearchEntry, ephy_search_entry, GTK_TYPE_ENTRY)

static void
ephy_search_entry_class_init (EphySearchEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
	g_signal_emit (entry, ephy_search_entry_signals[SEARCH], 0,
		       gtk_entry_get_text (GTK_ENTRY (entry)));
	entry->priv->timeout = 0;

	return FALSE;
}

static void
ephy_search_entry_changed_cb (GtkEditable *editable,
			      EphySearchEntry *entry)
{
	if (entry->priv->clearing == TRUE)
	{
		g_signal_emit (entry, ephy_search_entry_signals[SEARCH], 0,
			       gtk_entry_get_text (GTK_ENTRY (entry)));
		return;
	}

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

static gboolean
search_entry_clear_cb (GtkWidget *entry,
		       GtkEntryIconPosition position,
		       GdkEventButton *event,
		       gpointer user_data)
{
	guint state = event->state & gtk_accelerator_get_default_mod_mask ();
	
	if (event->button == 1 /* left */ && 
	    state == 0 &&
	    position == GTK_ENTRY_ICON_SECONDARY)
	{	
		ephy_search_entry_clear (EPHY_SEARCH_ENTRY (entry));
		
		return TRUE;
	}
	
	return FALSE;
}

static void
ephy_search_entry_init (EphySearchEntry *entry)
{
	entry->priv = EPHY_SEARCH_ENTRY_GET_PRIVATE (entry);

	gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				       GTK_ENTRY_ICON_SECONDARY,
				       GTK_STOCK_CLEAR);
	gtk_entry_set_icon_activatable (GTK_ENTRY (entry),
					GTK_ENTRY_ICON_SECONDARY,
					TRUE);
	gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
					 GTK_ENTRY_ICON_SECONDARY,
					 _("Clear"));
	g_signal_connect (entry,
			  "icon-press",
			  G_CALLBACK (search_entry_clear_cb),
			  NULL);
	g_signal_connect (entry,
			  "destroy",
			  G_CALLBACK (ephy_search_entry_destroy_cb),
			  entry);
	g_signal_connect (entry,
			  "changed",
			  G_CALLBACK (ephy_search_entry_changed_cb),
			  entry);
}

/**
 * ephy_search_entry_new:
 *
 * Creates a new #EphySearchEntry.
 *
 * Returns: a new #EphySearchEntry, as a #GtkWidget
 **/
GtkWidget *
ephy_search_entry_new (void)
{
	return gtk_widget_new (EPHY_TYPE_SEARCH_ENTRY, NULL);
}

/**
 * ephy_search_entry_clear:
 * @entry: an #EphySearchEntry
 *
 * Clears the text of the internal #GtkEntry of @entry.
 *
 **/
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

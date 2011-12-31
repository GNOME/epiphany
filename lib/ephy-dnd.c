/*
 *  Copyright © 2000, 2001, 2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-dnd.h"

#include "ephy-node.h"

#include <gtk/gtk.h>
#include <string.h>

/* Encode a "_NETSCAPE_URL_" selection.
 * As far as I can tell, Netscape is expecting a single
 * URL to be returned.  I cannot discover a way to construct
 * a list to be returned that Netscape can understand.
 * GMC also fails to do this as well.
 */
static void
add_one_netscape_url (const char *url, const char *title, gpointer data)
{
        GString *result;

        result = (GString *) data;
        if (result->len == 0)
	{
                g_string_append (result, url);
		if (title)
		{
			g_string_append (result, "\n");
	                g_string_append (result, title);
		}
        }
}

static void
add_one_uri (const char *uri, const char *title, gpointer data)
{
        GString *result;

        result = (GString *) data;

        g_string_append (result, uri);
        g_string_append (result, "\r\n");
}

static void
add_one_topic (const char *uri, const char *title, gpointer data)
{
        GString *result;

        result = (GString *) data;

        g_string_append (result, uri);
        g_string_append (result, "\r\n");
}

gboolean
ephy_dnd_drag_data_get (GtkWidget *widget,
                        GdkDragContext *context,
                        GtkSelectionData *selection_data,
                        guint32 time,
                        gpointer container_context,
                        EphyDragEachSelectedItemIterator each_selected_item_iterator)
{
        GString *result = NULL;
	GdkAtom target;

	target = gtk_selection_data_get_target (selection_data);

	if (target == gdk_atom_intern (EPHY_DND_URI_LIST_TYPE, FALSE) ||
	    target == gdk_atom_intern (EPHY_DND_TEXT_TYPE, FALSE))
	{
		result = g_string_new (NULL);
                (* each_selected_item_iterator) (add_one_uri, container_context, result);
	}
	else if (target == gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE))
	{
		result = g_string_new (NULL);
                (* each_selected_item_iterator) (add_one_netscape_url, container_context, result);
	}
	else if (target == gdk_atom_intern (EPHY_DND_TOPIC_TYPE, FALSE))
	{
		result = g_string_new (NULL);
                (* each_selected_item_iterator) (add_one_topic, container_context, result);
		g_string_erase (result, result->len - 2, -1);
	}
	else
	{
		g_assert_not_reached ();
	}

        gtk_selection_data_set (selection_data,
                                target,
                                8, (const guchar *) result->str, result->len);

	g_string_free (result, TRUE);

        return TRUE;
}

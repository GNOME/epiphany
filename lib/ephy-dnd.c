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

#include "ephy-dnd.h"
#include "ephy-string.h"
#include "ephy-node.h"

#include <gtk/gtkselection.h>
#include <gtk/gtktreeview.h>
#include <string.h>

/* Encode a "_NETSCAPE_URL_" selection.
 * As far as I can tell, Netscape is expecting a single
 * URL to be returned.  I cannot discover a way to construct
 * a list to be returned that Netscape can understand.
 * GMC also fails to do this as well.
 */
static void
add_one_netscape_url (const char *url, int x, int y, int w, int h, gpointer data)
{
        GString *result;

        result = (GString *) data;
        if (result->len == 0) {
                g_string_append (result, url);
        }
}

static void
add_one_uri (const char *uri, int x, int y, int w, int h, gpointer data)
{
        GString *result;

        result = (GString *) data;

        g_string_append (result, uri);
        g_string_append (result, "\r\n");
}

static void
add_one_node (const char *uri, int x, int y, int w, int h, gpointer data)
{
        GString *result;

        result = (GString *) data;

        g_string_append (result, uri);
        g_string_append (result, ";");
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

	target = selection_data->target;

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
	else if (target == gdk_atom_intern (EPHY_DND_TOPIC_TYPE, FALSE) ||
	         target == gdk_atom_intern (EPHY_DND_BOOKMARK_TYPE, FALSE))
	{
		result = g_string_new (NULL);
                (* each_selected_item_iterator) (add_one_node, container_context, result);
	}
	else
	{
		g_assert_not_reached ();
	}

        gtk_selection_data_set (selection_data,
                                selection_data->target,
                                8, result->str, result->len);

	g_string_free (result, TRUE);

        return TRUE;
}

GList *
ephy_dnd_node_list_extract_nodes (const char *node_list)
{
	EphyNodeDb *db;
	GList *result = NULL;
	char **nodes;
	int i;

	nodes = g_strsplit (node_list, ";", -1);

	db = ephy_node_db_get_by_name (nodes[i]);
	g_return_val_if_fail (db != NULL, NULL);

	for (i = 1; nodes[i] != NULL; i++)
	{
		gulong id;

		if (ephy_str_to_int (nodes[i], &id))
		{
			EphyNode *node;

			node = ephy_node_db_get_node_from_id (db, id);
			g_return_val_if_fail (node != NULL, NULL);
			result = g_list_append (result, node);
		}
	}

	return result;
}

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

#include <gtk/gtkselection.h>
#include <gtk/gtktreeview.h>

static GtkTargetEntry url_drag_types [] =
{
        { EPHY_DND_URI_LIST_TYPE,   0, EPHY_DND_URI_LIST },
        { EPHY_DND_TEXT_TYPE,       0, EPHY_DND_TEXT },
        { EPHY_DND_URL_TYPE,        0, EPHY_DND_URL }
};

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

gboolean
ephy_dnd_drag_data_get (GtkWidget *widget,
                        GdkDragContext *context,
                        GtkSelectionData *selection_data,
                        guint info,
                        guint32 time,
                        gpointer container_context,
                        EphyDragEachSelectedItemIterator each_selected_item_iterator)
{
        GString *result;

        switch (info) {
        case EPHY_DND_URI_LIST:
        case EPHY_DND_TEXT:
		result = g_string_new (NULL);
                (* each_selected_item_iterator) (add_one_uri, container_context, result);
                break;
        case EPHY_DND_URL:
		result = g_string_new (NULL);
                (* each_selected_item_iterator) (add_one_netscape_url, container_context, result);
                break;
        default:
                return FALSE;
        }

        gtk_selection_data_set (selection_data,
                                selection_data->target,
                                8, result->str, result->len);

        return TRUE;
}

void
ephy_dnd_url_drag_source_set (GtkWidget *widget)
{
	gtk_drag_source_set (widget,
                             GDK_BUTTON1_MASK,
			     url_drag_types,
			     G_N_ELEMENTS (url_drag_types),
                             GDK_ACTION_COPY);
}

void
ephy_dnd_enable_model_drag_source (GtkWidget *treeview)
{
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (treeview),
						GDK_BUTTON1_MASK,
						url_drag_types, G_N_ELEMENTS (url_drag_types),
						GDK_ACTION_COPY);
}


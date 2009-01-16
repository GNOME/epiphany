/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_DND_H
#define EPHY_DND_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Drag & Drop target names. */
#define EPHY_DND_URI_LIST_TYPE          "text/uri-list"
#define EPHY_DND_TEXT_TYPE              "text/plain"
#define EPHY_DND_URL_TYPE               "_NETSCAPE_URL"
#define EPHY_DND_TOPIC_TYPE		"ephy_topic_node"

typedef void (* EphyDragEachSelectedItemDataGet)    (const char *url,
                                                     const char *title,
                                                     gpointer data);

typedef void (* EphyDragEachSelectedItemIterator)   (EphyDragEachSelectedItemDataGet iteratee,
                                                     gpointer iterator_context,
                                                     gpointer data);

gboolean ephy_dnd_drag_data_get			(GtkWidget *widget,
						 GdkDragContext *context,
						 GtkSelectionData *selection_data,
						 guint32 time,
						 gpointer container_context,
						 EphyDragEachSelectedItemIterator each_selected_item_iterator);

G_END_DECLS

#endif

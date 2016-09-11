/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Drag & Drop target names. */
#define EPHY_DND_URI_LIST_TYPE          "text/uri-list"
#define EPHY_DND_TEXT_TYPE              "text/plain"
#define EPHY_DND_URL_TYPE               "_NETSCAPE_URL"
#define EPHY_DND_TOPIC_TYPE             "ephy_topic_node"

typedef void (* EphyDragEachSelectedItemDataGet)    (const char *url,
                                                     const char *title,
                                                     gpointer data);

typedef void (* EphyDragEachSelectedItemIterator)   (EphyDragEachSelectedItemDataGet iteratee,
                                                     gpointer iterator_context,
                                                     gpointer data);

gboolean ephy_dnd_drag_data_get                 (GtkWidget *widget,
                                                 GdkDragContext *context,
                                                 GtkSelectionData *selection_data,
                                                 guint32 time,
                                                 gpointer container_context,
                                                 EphyDragEachSelectedItemIterator each_selected_item_iterator);

G_END_DECLS

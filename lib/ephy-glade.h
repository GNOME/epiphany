/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
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

#ifndef EPHY_GLADE_H
#define EPHY_GLADE_H

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>

typedef struct
{
        const gchar *name;
        GtkWidget **ptr;
} WidgetLookup;

G_BEGIN_DECLS

GladeXML   *ephy_glade_widget_new	(const char *file,
					 const char *widget_name,
					 GtkWidget **root,
					 gpointer data);

G_END_DECLS

#endif

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

#include "ephy-glade.h"
#include "ephy-file-helpers.h"

#include <glade/glade-xml.h>
#include <gtk/gtkmenu.h>
#include <gmodule.h>

static void
glade_signal_connect_func (const gchar *cb_name, GObject *obj,
			   const gchar *signal_name, const gchar *signal_data,
			   GObject *conn_obj, gboolean conn_after,
			   gpointer user_data);

/**
 * ephy_widget_new: build a new widget of the provided name, with all
 * signals attached and data set to the provided parameter.
 */
GladeXML *
ephy_glade_widget_new (const char *file, const char *widget_name,
		       GtkWidget **root, gpointer data)
{
	GladeXML *gxml;

	/* build the widget */
	/* note that libglade automatically caches the parsed file,
	 * so we don't need to worry about the efficiency of this */
	gxml = glade_xml_new (file, widget_name, NULL);
	g_return_val_if_fail (gxml != NULL, NULL);

	/* lookup the root widget if requested */
	if (root != NULL)
	{
		*root = glade_xml_get_widget (gxml, widget_name);
	}

	/* connect signals and data */
	glade_xml_signal_autoconnect_full
		(gxml, (GladeXMLConnectFunc)glade_signal_connect_func, data);

	/* return xml document for subsequent widget lookups */
	return gxml;
}

/*
 * glade_signal_connect_func: used by glade_xml_signal_autoconnect_full
 */
static void
glade_signal_connect_func (const gchar *cb_name, GObject *obj,
			   const gchar *signal_name, const gchar *signal_data,
			   GObject *conn_obj, gboolean conn_after,
			   gpointer user_data)
{
	/** Module with all the symbols of the program */
	static GModule *mod_self = NULL;
	gpointer handler_func;

	/* initialize gmodule */
	if (mod_self == NULL)
	{
		mod_self = g_module_open (NULL, 0);
		g_assert (mod_self != NULL);
	}

	/*g_print( "glade_signal_connect_func: cb_name = '%s', signal_name = '%s', signal_data = '%s'\n",
	  cb_name, signal_name, signal_data ); */

	if (g_module_symbol (mod_self, cb_name, &handler_func))
	{
		/* found callback */
		if (conn_obj)
		{
			if (conn_after)
			{
				g_signal_connect_object
                                        (obj, signal_name,
                                         handler_func, conn_obj,
                                         G_CONNECT_AFTER);
			}
			else
			{
				g_signal_connect_object
                                        (obj, signal_name, 
                                         handler_func, conn_obj,
                                         G_CONNECT_SWAPPED);
			}
		}
		else
		{
			/* no conn_obj; use standard connect */
			gpointer data = NULL;

			data = user_data;

			if (conn_after)
			{
				g_signal_connect_after
					(obj, signal_name,
					 handler_func, data);
			}
			else
			{
				g_signal_connect
					(obj, signal_name,
					 handler_func, data);
			}
		}
	}
	else
	{
		g_warning("callback function not found: %s", cb_name);
	}
}

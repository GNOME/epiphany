/*
 *  Copyright (C) 2001 Matthew Mueller
 *            (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtkpaned.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktreeview.h>
#include <libgnomeui/gnome-app.h>
#include <bonobo/bonobo-dock.h>
#include <bonobo/bonobo-dock-layout.h>

#include "ephy-state.h"
#include "eel-gconf-extensions.h"

#define CONF_GUL_STATE_PATH "/apps/epiphany/state"

static void
window_save_size (GtkWidget *window, const gchar *name)
{
	int width, height;
	gchar *buf;

	gtk_window_get_size (GTK_WINDOW(window),
			     &width, &height);

	buf = g_strdup_printf (CONF_GUL_STATE_PATH "/%s/width",name);
        eel_gconf_set_integer (buf, width);
	g_free (buf);

	buf = g_strdup_printf (CONF_GUL_STATE_PATH "/%s/height",name);
	eel_gconf_set_integer (buf, height);
	g_free (buf);
}

/**
 * ephy_state_save_window: save the window dimensions
 */
void
ephy_state_save_window (GtkWidget *window,
			const gchar *name)
{
	GdkWindowState state;
	gboolean maximized;
	gchar *buf;

	state = gdk_window_get_state (GTK_WIDGET (window)->window);
	maximized = state && GDK_WINDOW_STATE_MAXIMIZED;

	buf = g_strdup_printf (CONF_GUL_STATE_PATH "/%s/maximized",name);
	eel_gconf_set_boolean (buf, maximized);
	g_free (buf);

	if (!maximized)
	{
		window_save_size (window, name);
	}
}

/**
 * ephy_window_load_state: load the window state
 */
void
ephy_state_load_window (GtkWidget *window,
			const gchar *name,
		        int default_width,
			int default_height,
		        gboolean position)
{
	gchar *buf;
	gint width, height;
	gboolean maximized;

	buf = g_strdup_printf (CONF_GUL_STATE_PATH "/%s/maximized",name);
	maximized = eel_gconf_get_boolean (buf);
	g_free (buf);

	buf = g_strdup_printf (CONF_GUL_STATE_PATH "/%s/width",name);
        width = eel_gconf_get_integer (buf);
	g_free (buf);

	buf = g_strdup_printf (CONF_GUL_STATE_PATH "/%s/height",name);
	height = eel_gconf_get_integer (buf);
	g_free (buf);

	/* try default size */
	if (width == 0 && height == 0 &&
	    default_width != -1 && default_height != -1)
	{
		width = default_width;
		height = default_height;
	}

	if (width != 0 && height != 0)
	{
		gtk_window_set_default_size
			(GTK_WINDOW (window), width, height);
	}

	if (maximized)
	{
		gtk_window_maximize (GTK_WINDOW(window));
	}
}

/**
 * ephy_state_load_pane_pos: load the paned position
 */
void
ephy_state_load_pane_pos (GtkWidget *pane,
			  const gchar *name)
{
	if (pane != NULL)
	{
		gint pane_pos;
		gchar *buf = g_strdup_printf (CONF_GUL_STATE_PATH "/%s", name);
		pane_pos = eel_gconf_get_integer (buf);
		g_free (buf);

		if (pane_pos != -1)
			gtk_paned_set_position (GTK_PANED (pane), pane_pos);
	}
}

/**
 * gul_state_save_pane_pos: save the paned position
 */
void
ephy_state_save_pane_pos (GtkWidget *pane,
			  const gchar *name)
{
	if (pane != NULL)
	{
		gchar *buf = g_strdup_printf (CONF_GUL_STATE_PATH "/%s", name);
		eel_gconf_set_integer (buf, GTK_PANED (pane)->child1_size);
		g_free (buf);
	}
}

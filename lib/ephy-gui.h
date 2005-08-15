/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#ifndef EPHY_GUI_H
#define EPHY_GUI_H

#include <gtk/gtkmenu.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkwindow.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>

G_BEGIN_DECLS

void		ephy_gui_sanitise_popup_position	 (GtkMenu *menu,
							  GtkWidget *parent_widget,
							  gint *x,
							  gint *y);

void		ephy_gui_menu_position_tree_selection    (GtkMenu   *menu,
							  gint      *x,
							  gint      *y,
							  gboolean  *push_in,
							  gpointer  user_data);

void		ephy_gui_menu_position_under_widget	 (GtkMenu   *menu,
							  gint      *x,
							  gint      *y,
							  gboolean  *push_in,
							  gpointer  user_data);

void		ephy_gui_menu_position_on_toolbar	 (GtkMenu   *menu,
							  gint      *x,
							  gint      *y,
							  gboolean  *push_in,
							  gpointer  user_data);

void		ephy_gui_menu_position_on_panel		 (GtkMenu *menu,
							  gint      *x,
							  gint      *y,
							  gboolean  *push_in,
							  gpointer  user_data);

GtkWindowGroup *ephy_gui_ensure_window_group		 (GtkWindow *window);

gboolean	ephy_gui_is_middle_click		 (void);

gboolean	ephy_gui_confirm_overwrite_file	         (GtkWidget *parent,
							  const char *filename);

void		ephy_gui_help				 (GtkWindow *parent,
							  const char *file_name,
							  const char *link_id);

void		ephy_gui_window_update_user_time	 (GtkWidget *window,
							  guint32 user_time);

void		ephy_gui_window_present			 (GtkWindow *window,
							  guint32 user_time);

G_END_DECLS

#endif

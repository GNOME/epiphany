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
 */

#ifndef EPHY_GUI_H
#define EPHY_GUI_H

/* system includes */
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomeui/gnome-dialog.h>
#include <gnome.h>

G_BEGIN_DECLS

void		 ephy_gui_menu_position_under_widget	 (GtkMenu   *menu,
							  gint      *x,
							  gint      *y,
							  gboolean  *push_in,
							  gpointer  user_data);

gint		 ephy_gui_gtk_radio_button_get		 (GtkRadioButton *radio_button);

void		 ephy_gui_gtk_radio_button_set		 (GtkRadioButton *radio_button,
							  gint index);

GList		*ephy_gui_treeview_get_selection_refs	 (GtkTreeView *treeview);

GtkWidget	*ephy_gui_append_new_menuitem		 (GtkWidget  *menu,
							  const char *mnemonic,
							  GCallback   callback,
							  gpointer    data);

GtkWidget	*ephy_gui_append_new_menuitem_stock	 (GtkWidget  *menu,
							  const char *stock_id,
							  GCallback   callback,
							  gpointer    data);

GtkWidget	*ephy_gui_append_new_menuitem_stock_icon (GtkWidget *menu,
							  const char *stock_id,
							  const char *mnemonic,
							  GCallback callback,
							  gpointer data);

GtkWidget      *ephy_gui_append_new_check_menuitem       (GtkWidget  *menu,
                                                          const char *mnemonic,
                                                          gboolean value,
                                                          GCallback callback,
                                                          gpointer data);

GtkWidget      *ephy_gui_append_separator	         (GtkWidget *menu);

gboolean	ephy_gui_confirm_overwrite_file	         (GtkWidget *parent,
							  const char *filename);

guint32		ephy_gui_rgb_shift_color		 (guint32 color,
							  float shift_by);

guint32		ephy_gui_gdk_color_to_rgb		 (const GdkColor *color);

GdkColor	ephy_gui_gdk_rgb_to_color		 (guint32 color);

G_END_DECLS

#endif

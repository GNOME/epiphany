/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gul-bonobo-extensions.h - interface for new functions that conceptually
                             belong in bonobo. Perhaps some of these will be
                             actually rolled into bonobo someday.


            This file is based on nautilus-bonobo-extensions.h from
            libnautilus-private.


   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef EPHY_BONOBO_EXTENSIONS_H
#define EPHY_BONOBO_EXTENSIONS_H

#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-control.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkwidget.h>

void		ephy_bonobo_set_accelerator			(BonoboUIComponent *ui,
								 const char *path,
								 const char *accelerator);
char           *ephy_bonobo_get_label				(BonoboUIComponent *ui,
								 const char *path);
void		ephy_bonobo_set_label				(BonoboUIComponent *ui,
								 const char *path,
								 const char *label);
void		ephy_bonobo_set_tip				(BonoboUIComponent *ui,
								 const char *path,
								 const char *tip);
void		ephy_bonobo_set_sensitive			(BonoboUIComponent *ui,
								 const char *path,
								 gboolean sensitive);
void		ephy_bonobo_set_toggle_state			(BonoboUIComponent *ui,
								 const char *path,
								 gboolean state);
void		ephy_bonobo_set_hidden				(BonoboUIComponent *ui,
								 const char *path,
								 gboolean hidden);
gboolean	ephy_bonobo_get_hidden				(BonoboUIComponent *ui,
								 const char *path);
void		ephy_bonobo_add_numbered_menu_item		(BonoboUIComponent *ui,
								 const char *container_path,
								 guint index,
								 const char *label,
								 GdkPixbuf *pixbuf);
void		ephy_bonobo_add_numbered_toggle_menu_item	(BonoboUIComponent *ui,
								 const char *container_path,
								 guint index,
								 const char *label);
void		ephy_bonobo_add_numbered_radio_menu_item	(BonoboUIComponent *ui,
								 const char *container_path,
								 guint index,
								 const char *label,
								 const char *radio_group_name);
char           *ephy_bonobo_get_numbered_menu_item_command	(BonoboUIComponent *ui,
								 const char *container_path,
								 guint  index);
char           *ephy_bonobo_get_numbered_menu_item_path		(BonoboUIComponent *ui,
								 const char *container_path,
								 guint index);
guint		ephy_bonobo_get_numbered_menu_item_index_from_command (const char *command);
char           *ephy_bonobo_get_numbered_menu_item_container_path_from_command (const char *command);
void		ephy_bonobo_add_submenu				(BonoboUIComponent *ui,
								 const char *container_path,
								 const char *label,
								 GdkPixbuf *pixbuf);
void		ephy_bonobo_add_numbered_submenu		(BonoboUIComponent *ui,
								 const char *container_path,
								 guint index,
								 const char *label,
								 GdkPixbuf *pixbuf);
void		ephy_bonobo_add_numbered_submenu_no_verb	(BonoboUIComponent *ui,
								 const char *container_path,
								 guint index,
								 const char *label,
								 GdkPixbuf *pixbuf);
void		ephy_bonobo_add_menu_separator			(BonoboUIComponent *ui,
								 const char *path);
void		ephy_bonobo_remove_menu_items_and_commands	(BonoboUIComponent *ui,
								 const char *container_path);
void		ephy_bonobo_set_label_for_menu_item_and_command	(BonoboUIComponent *ui,
								 const char *menu_item_path,
								 const char *command_path,
								 const char *label_with_underscore);
void		ephy_bonobo_set_icon				(BonoboUIComponent *ui,
								 const char *path,
								 const char *icon_relative_path);
gchar          *ephy_bonobo_add_dockitem			(BonoboUIComponent *uic,
								 const char *name,
								 int band_num);
BonoboControl  *ephy_bonobo_add_numbered_control		(BonoboUIComponent *uic,
								 GtkWidget *w, guint index,
								 const char *path);
void		ephy_bonobo_clear_path				(BonoboUIComponent *uic,
								 const gchar *path);
void		ephy_bonobo_replace_path			(BonoboUIComponent *uic,
								 const gchar *path_src,
								 const char *path_dst);
void		ephy_bonobo_add_numbered_widget			(BonoboUIComponent *uic, GtkWidget *w,
								 guint index, const char *container_path);

#endif /* EPHY_BONOBO_EXTENSIONS_H */

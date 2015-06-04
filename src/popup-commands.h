/*
 *  Copyright © 2000, 2001, 2002 Marco Pesenti Gritti
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

#ifndef POPUP_COMMANDS_H
#define POPUP_COMMANDS_H

#include <gtk/gtk.h>

#include "ephy-window.h"

G_BEGIN_DECLS

void popup_cmd_link_in_new_window	(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_link_in_new_tab		(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_link_in_incognito_window (GtkAction *action,
					 EphyWindow *window);

void popup_cmd_bookmark_link		(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_view_source		(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_copy_link_address	(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_copy_link_location       (GtkAction *action,
					 EphyWindow *window);

void popup_cmd_download_link_as		(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_set_image_as_background  (GtkAction *action,
					 EphyWindow *window);

void popup_cmd_copy_image_location	(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_view_image_in_new_tab	(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_download_link		(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_save_image_as		(GtkAction *action,
					 EphyWindow *window);

void popup_cmd_media_in_new_window      (GtkAction *action,
                                         EphyWindow *window);

void popup_cmd_media_in_new_tab         (GtkAction *action,
                                         EphyWindow *window);

void popup_cmd_copy_media_location      (GtkAction *action,
                                         EphyWindow *window);

void popup_cmd_save_media_as            (GtkAction *action,
                                         EphyWindow *window);

void popup_cmd_search_selection         (GtkAction *action,
					 EphyWindow *window);

G_END_DECLS

#endif

/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#include <gtk/gtkaction.h>

#include "ephy-window.h"

void window_cmd_edit_find	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_view_stop	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_go_location	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_go_myportal	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_go_location	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_view_reload	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_new		(GtkAction *action,
				 EphyWindow *window);

void window_cmd_file_new_window	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_file_new_tab	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_file_bookmark_page(GtkAction *action,
				  EphyWindow *window);

void window_cmd_go_bookmarks	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_file_open	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_file_save_as    (GtkAction *action,
				 EphyWindow *window);

void window_cmd_file_print_setup (GtkAction *action,
				  EphyWindow *window);

void window_cmd_file_print_preview (GtkAction *action,
				    EphyWindow *window);

void window_cmd_file_print	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_file_send_to	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_file_work_offline (GtkAction *action,
				   EphyWindow *window);

void window_cmd_file_close_window (GtkAction *action,
				    EphyWindow *window);

void window_cmd_edit_undo	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_redo	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_cut	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_copy	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_paste	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_select_all (GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_find_next	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_find_prev	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_view_fullscreen	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_view_zoom_in	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_view_zoom_out	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_view_zoom_normal(GtkAction *action,
				 EphyWindow *window);

void window_cmd_view_page_source(GtkAction *action,
				 EphyWindow *window);

void window_cmd_view_page_security_info (GtkAction *action,
					 EphyWindow *window);

void window_cmd_go_history	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_personal_data (GtkAction *action,
				    EphyWindow *window);

void window_cmd_edit_certificates (GtkAction *action,
				    EphyWindow *window);

void window_cmd_edit_prefs      (GtkAction *action,
				 EphyWindow *window);

void window_cmd_edit_toolbar	(GtkAction *action,
				 EphyWindow *window);

void window_cmd_help_contents (GtkAction *action,
				 EphyWindow *window);

void window_cmd_help_about      (GtkAction *action,
				 GtkWidget *window);

void window_cmd_tabs_next       (GtkAction *action,
				 EphyWindow *window);

void window_cmd_tabs_previous   (GtkAction *action,
				 EphyWindow *window);

void window_cmd_tabs_move_left  (GtkAction *action,
				 EphyWindow *window);

void window_cmd_tabs_move_right (GtkAction *action,
				 EphyWindow *window);

void window_cmd_tabs_detach     (GtkAction *action,
				 EphyWindow *window);

void window_cmd_load_location   (GtkAction *action,
				 EphyWindow *window);

void window_cmd_browse_with_caret (GtkAction *action,
				   EphyWindow *window);


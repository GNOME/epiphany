/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#include "egg-action.h"
#include "ephy-window.h"
#include "ephy-embed-utils.h"

void window_cmd_edit_find	(EggAction *action,
				 EphyWindow *window);

void window_cmd_file_print	(EggAction *action,
				 EphyWindow *window);

void window_cmd_view_stop	(EggAction *action,
				 EphyWindow *window);

void window_cmd_go_back		(EggAction *action,
				 EphyWindow *window);

void window_cmd_go_forward	(EggAction *action,
				 EphyWindow *window);

void window_cmd_go_location	(EggAction *action,
				 EphyWindow *window);

void window_cmd_go_up		(EggAction *action,
				 EphyWindow *window);

void window_cmd_go_home		(EggAction *action,
				 EphyWindow *window);

void window_cmd_go_myportal	(EggAction *action,
				 EphyWindow *window);

void window_cmd_go_location	(EggAction *action,
				 EphyWindow *window);

void window_cmd_view_reload	(EggAction *action,
				 EphyWindow *window);

void window_cmd_new		(EggAction *action,
				 EphyWindow *window);

void window_cmd_file_new_window	(EggAction *action,
				 EphyWindow *window);

void window_cmd_file_new_tab	(EggAction *action,
				 EphyWindow *window);

void window_cmd_file_add_bookmark(EggAction *action,
				  EphyWindow *window);

void window_cmd_go_bookmarks	(EggAction *action,
				 EphyWindow *window);

void window_cmd_file_open	(EggAction *action,
				 EphyWindow *window);

void window_cmd_file_save_as    (EggAction *action,
				 EphyWindow *window);

void window_cmd_file_send_to	(EggAction *action,
				 EphyWindow *window);

void window_cmd_file_close_tab	  (EggAction *action,
				   EphyWindow *window);

void window_cmd_file_close_window (EggAction *action,
				   EphyWindow *window);

void window_cmd_edit_cut	(EggAction *action,
				 EphyWindow *window);

void window_cmd_edit_copy	(EggAction *action,
				 EphyWindow *window);

void window_cmd_edit_paste	(EggAction *action,
				 EphyWindow *window);

void window_cmd_edit_select_all (EggAction *action,
				 EphyWindow *window);

void window_cmd_edit_find_next	(EggAction *action,
				 EphyWindow *window);

void window_cmd_edit_find_prev	(EggAction *action,
				 EphyWindow *window);

void window_cmd_view_statusbar	(EggAction *action,
				 EphyWindow *window);

void window_cmd_view_fullscreen	(EggAction *action,
				 EphyWindow *window);

void window_cmd_view_zoom_in	(EggAction *action,
				 EphyWindow *window);

void window_cmd_view_zoom_out	(EggAction *action,
				 EphyWindow *window);

void window_cmd_view_zoom_normal(EggAction *action,
				 EphyWindow *window);

void window_cmd_view_page_source(EggAction *action,
				 EphyWindow *window);

void window_cmd_go_history	(EggAction *action,
				 EphyWindow *window);

void window_cmd_edit_personal_data (EggAction *action,
				    EphyWindow *window);

void window_cmd_edit_prefs      (EggAction *action,
				 EphyWindow *window);

void window_cmd_edit_toolbar	(EggAction *action,
				 EphyWindow *window);

void window_cmd_help_about      (EggAction *action,
				 EphyWindow *window);

void window_cmd_tabs_next       (EggAction *action,
				 EphyWindow *window);

void window_cmd_tabs_previous   (EggAction *action,
				 EphyWindow *window);

void window_cmd_tabs_move_left  (EggAction *action,
				 EphyWindow *window);

void window_cmd_tabs_move_right (EggAction *action,
				 EphyWindow *window);

void window_cmd_tabs_detach     (EggAction *action,
				 EphyWindow *window);


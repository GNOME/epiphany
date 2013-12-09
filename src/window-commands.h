/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef WINDOW_COMMANDS_H
#define WINDOW_COMMANDS_H

#include "ephy-window.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

void window_cmd_edit_find                 (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_view_stop                 (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_go_location               (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_view_reload               (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_new                       (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_bookmark_page        (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_open                 (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_save_as              (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_save_as_application  (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_print                (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_undo_close_tab            (GtkAction *action,
                                           EphyWindow *window);
void window_cmd_file_send_to              (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_close_window         (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_undo                 (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_redo                 (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_cut                  (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_copy                 (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_paste                (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_delete               (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_select_all           (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_find_next            (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_find_prev            (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_view_fullscreen           (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_view_zoom_in              (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_view_zoom_out             (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_view_zoom_normal          (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_view_page_source          (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_help_contents             (GtkAction  *action,
                                           GtkWidget  *window);
void window_cmd_help_about                (GtkAction  *action,
                                           GtkWidget  *window);
void window_cmd_tabs_next                 (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_tabs_previous             (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_tabs_move_left            (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_tabs_move_right           (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_tabs_detach               (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_load_location             (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_browse_with_caret         (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_quit                 (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_bookmarks            (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_history              (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_new_window           (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_file_new_incognito_window (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_preferences          (GtkAction  *action,
                                           EphyWindow *window);

G_END_DECLS

#endif

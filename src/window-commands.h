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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WINDOW_COMMANDS_H
#define WINDOW_COMMANDS_H

#include "ephy-window.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

void window_cmd_new_window                (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_new_incognito_window      (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_show_bookmarks            (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_show_history              (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_show_preferences          (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_show_shortcuts            (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_show_help                 (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_show_about                (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_quit                      (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_reopen_closed_tab         (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_navigation                (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_navigation_new_tab        (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_view_stop                 (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_view_reload               (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_combined_stop_reload      (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_file_new_tab              (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_file_open                 (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_file_save_as              (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_file_save_as_application  (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_undo                 (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_redo                 (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_cut                  (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_copy                 (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_paste                (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_delete               (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_file_print                (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_find                 (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_find_prev            (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_edit_find_next            (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_open_bookmark             (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_file_bookmark_page        (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_view_zoom_in              (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_view_zoom_out             (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_view_zoom_normal          (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_view_encoding             (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_view_page_source          (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_view_toggle_inspector     (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_load_location             (GtkAction  *action,
                                           EphyWindow *window);
void window_cmd_edit_select_all           (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_file_send_to              (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_go_location               (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void window_cmd_change_browse_with_caret  (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
void window_cmd_change_fullscreen_state   (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
void window_cmd_tabs_previous             (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
void window_cmd_tabs_next                 (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
void window_cmd_tabs_move_left            (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
void window_cmd_tabs_move_right           (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
void window_cmd_tabs_duplicate            (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
void window_cmd_tabs_detach               (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
void window_cmd_tabs_close                (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
G_END_DECLS

#endif

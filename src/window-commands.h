/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "ephy-window.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

void window_cmd_new_window                      (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_new_incognito_window            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_import_bookmarks                (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_export_bookmarks                (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_history                    (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_firefox_sync               (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_clear_data_view            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_preferences                (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_shortcuts                  (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_help                       (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_about                      (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_quit                            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_reopen_closed_tab               (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_navigation                      (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_navigation_new_tab              (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_stop                            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_reload                          (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_reload_bypass_cache             (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_combined_stop_reload            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_page_menu                       (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_new_tab                         (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_open                            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_save_as                         (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_save_as_application             (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_screenshot                      (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_undo                            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_redo                            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_cut                             (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_copy                            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_paste                           (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_paste_as_plain_text             (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_delete                          (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_print                           (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_find                            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_find_prev                       (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_find_next                       (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_open_bookmark                   (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_bookmark_page                   (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_bookmarks                       (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_downloads                  (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_zoom_in                         (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_zoom_out                        (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_zoom_normal                     (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_encoding                        (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_page_source                     (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_toggle_inspector                (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_select_all                      (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_go_location                     (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_location_search                 (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_go_home                         (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_go_tabs_view                    (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_change_browse_with_caret_state  (GSimpleAction *action,
                                                 GVariant      *state,
                                                 gpointer       user_data);
void window_cmd_change_fullscreen_state         (GSimpleAction *action,
                                                 GVariant      *state,
                                                 gpointer       user_data);
void window_cmd_tabs_duplicate                  (GSimpleAction *action,
                                                 GVariant      *state,
                                                 gpointer       user_data);
void window_cmd_tabs_close                      (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_tabs_close_left                 (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_tabs_close_right                (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_tabs_reload                     (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_tabs_reload_all_tabs            (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_tabs_reopen_closed_tab          (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_tabs_close_others               (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_show_tab                        (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_change_show_tab_state           (GSimpleAction *action,
                                                 GVariant      *state,
                                                 gpointer       user_data);
void window_cmd_toggle_reader_mode              (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_open_application_manager        (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_homepage_new_tab                (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_new_tab_from_clipboard          (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_tabs_pin                        (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_tabs_unpin                      (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_change_tabs_mute_state          (GSimpleAction *action,
                                                 GVariant      *state,
                                                 gpointer       user_data);
void window_cmd_import_passwords                (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_switch_new_tab                  (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_privacy_report                  (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_passwords                       (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_export_passwords                (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_close_all_tabs                  (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
void window_cmd_security_and_permissions        (GSimpleAction *action,
                                                 GVariant      *parameter,
                                                 gpointer       user_data);
G_END_DECLS

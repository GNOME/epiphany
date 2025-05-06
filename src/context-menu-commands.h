/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000, 2001, 2002 Marco Pesenti Gritti
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

#include <gtk/gtk.h>

#include "ephy-window.h"

G_BEGIN_DECLS

void context_cmd_link_in_new_window       (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_link_in_new_tab          (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_link_in_incognito_window (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_view_source              (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_copy_link_address        (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_send_via_email           (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_copy_link_location       (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_download_link_as         (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_set_image_as_background  (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_copy_image_location      (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_view_image_in_new_tab    (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_download_link            (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_save_image_as            (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_media_in_new_window      (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_media_in_new_tab         (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_copy_media_location      (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_save_media_as            (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_search_selection         (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_open_selection           (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data);
void context_cmd_open_selection_in_new_tab          (GSimpleAction *action,
                                                     GVariant      *parameter,
                                                     gpointer       user_data);
void context_cmd_open_selection_in_new_window       (GSimpleAction *action,
                                                     GVariant      *parameter,
                                                     gpointer       user_data);
void context_cmd_open_selection_in_incognito_window (GSimpleAction *action,
                                                     GVariant      *parameter,
                                                     gpointer       user_data);
void context_cmd_add_link_to_bookmarks              (GSimpleAction *action,
                                                     GVariant      *parameter,
                                                     gpointer       user_data);
G_END_DECLS

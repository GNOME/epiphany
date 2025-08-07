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

#include "ephy-bookmarks-dialog.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-embed.h"
#include "ephy-location-controller.h"
#include "ephy-tab-view.h"
#include "ephy-web-view.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WINDOW (ephy_window_get_type ())

G_DECLARE_FINAL_TYPE (EphyWindow, ephy_window, EPHY, WINDOW, AdwApplicationWindow)

typedef enum
{
        EPHY_WINDOW_CHROME_HEADER_BAR    = 1 << 0,
        EPHY_WINDOW_CHROME_MENU          = 1 << 1,
        EPHY_WINDOW_CHROME_LOCATION      = 1 << 2,
        EPHY_WINDOW_CHROME_TABSBAR       = 1 << 3,
        EPHY_WINDOW_CHROME_BOOKMARKS     = 1 << 4,
        EPHY_WINDOW_CHROME_DEFAULT       = (EPHY_WINDOW_CHROME_HEADER_BAR | EPHY_WINDOW_CHROME_MENU | EPHY_WINDOW_CHROME_LOCATION | EPHY_WINDOW_CHROME_TABSBAR | EPHY_WINDOW_CHROME_BOOKMARKS)
} EphyWindowChrome;

EphyWindow       *ephy_window_new                 (void);

EphyEmbed        *ephy_window_get_active_embed    (EphyWindow *window);

EphyTabView      *ephy_window_get_tab_view        (EphyWindow *window);

void              ephy_window_toggle_tab_overview (EphyWindow *window);
gboolean          ephy_window_is_tab_overview_open (EphyWindow *window);

void              ephy_window_load_url            (EphyWindow *window,
                                                   const char *url);

void              ephy_window_set_zoom            (EphyWindow *window,
                                                   double zoom);

void              ephy_window_focus_location_entry (EphyWindow *window);
void              ephy_window_location_search     (EphyWindow *window);
const char       *ephy_window_get_location        (EphyWindow *window);

GtkWidget        *ephy_window_get_header_bar      (EphyWindow *window);

gboolean          ephy_window_can_close           (EphyWindow *window);
gboolean          ephy_window_close               (EphyWindow *window);

void              ephy_window_handle_quit_with_modified_forms (EphyWindow *window);

guint             ephy_window_get_has_modified_forms (EphyWindow *window);

EphyWindowChrome  ephy_window_get_chrome          (EphyWindow *window);

EphyLocationController  *ephy_window_get_location_controller (EphyWindow *window);

WebKitHitTestResult *ephy_window_get_context_event     (EphyWindow *window);

GtkWidget        *ephy_window_get_current_find_toolbar (EphyWindow *window);

void              ephy_window_set_location             (EphyWindow *window,
                                                        const char *address);

void              ephy_window_set_default_size         (EphyWindow *window,
                                                        gint        width,
                                                        gint        height);
void              ephy_window_show_fullscreen_header_bar (EphyWindow *window);

void              ephy_window_update_entry_focus         (EphyWindow  *window,
                                                          EphyWebView *view);

gboolean          ephy_window_is_maximized               (EphyWindow *window);

gboolean          ephy_window_is_fullscreen              (EphyWindow *window);

GActionGroup     *ephy_window_get_action_group           (EphyWindow  *window,
                                                          const char  *prefix);

guint64           ephy_window_get_uid                    (EphyWindow *window);

gboolean          ephy_window_get_sidebar_shown          (EphyWindow *window);

void              ephy_window_switch_to_new_tab_toast    (EphyWindow *window,
                                                          GtkWidget  *tab);

void              ephy_window_switch_to_new_tab          (EphyWindow *window);

gboolean          ephy_window_get_show_sidebar           (EphyWindow *window);

void              ephy_window_bookmark_removed_toast     (EphyWindow   *window,
                                                          EphyBookmark *bookmark,
                                                          AdwToast     *toast);

void              ephy_window_toggle_bookmarks           (EphyWindow *window);

void              ephy_window_show_toast                 (EphyWindow *window,
                                                          const char *text);

G_END_DECLS

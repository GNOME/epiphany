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

#include "ephy-bookmarks-manager.h"
#include "ephy-embed-event.h"
#include "ephy-location-controller.h"
#include "ephy-web-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WINDOW (ephy_window_get_type ())

G_DECLARE_FINAL_TYPE (EphyWindow, ephy_window, EPHY, WINDOW, DzlApplicationWindow)

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

GtkWidget        *ephy_window_get_notebook        (EphyWindow *window);

void              ephy_window_load_url            (EphyWindow *window,
                                                   const char *url);

void              ephy_window_set_zoom            (EphyWindow *window,
                                                   double zoom);

void              ephy_window_activate_location   (EphyWindow *window);
const char       *ephy_window_get_location        (EphyWindow *window);

GtkWidget        *ephy_window_get_header_bar      (EphyWindow *window);

gboolean          ephy_window_close               (EphyWindow *window);

EphyWindowChrome  ephy_window_get_chrome          (EphyWindow *window);

EphyLocationController  *ephy_window_get_location_controller (EphyWindow *window);

EphyEmbedEvent   *ephy_window_get_context_event        (EphyWindow *window);

GtkWidget        *ephy_window_get_current_find_toolbar (EphyWindow *window);

void              ephy_window_set_location             (EphyWindow *window,
                                                        const char *address);

void              ephy_window_set_default_size         (EphyWindow *window,
                                                        gint        width,
                                                        gint        height);
void              ephy_window_set_default_position     (EphyWindow *window,
                                                        gint        x,
                                                        gint        y);

G_END_DECLS

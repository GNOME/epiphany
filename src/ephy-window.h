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

#ifndef EPHY_WINDOW_H
#define EPHY_WINDOW_H

#include "ephy-web-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WINDOW (ephy_window_get_type ())

G_DECLARE_FINAL_TYPE (EphyWindow, ephy_window, EPHY, WINDOW, GtkApplicationWindow)

typedef enum
{
        EPHY_WINDOW_CHROME_TOOLBAR       = 1 << 0,
        EPHY_WINDOW_CHROME_MENU          = 1 << 1,
        EPHY_WINDOW_CHROME_LOCATION      = 1 << 2,
        EPHY_WINDOW_CHROME_TABSBAR       = 1 << 3,
        EPHY_WINDOW_CHROME_DEFAULT       = (EPHY_WINDOW_CHROME_TOOLBAR | EPHY_WINDOW_CHROME_MENU | EPHY_WINDOW_CHROME_LOCATION | EPHY_WINDOW_CHROME_TABSBAR)
} EphyWindowChrome;

EphyWindow	 *ephy_window_new		  (void);

GtkUIManager	 *ephy_window_get_ui_manager	  (EphyWindow *window);

GtkWidget	 *ephy_window_get_notebook	  (EphyWindow *window);

void		  ephy_window_load_url		  (EphyWindow *window,
						   const char *url);

void		  ephy_window_set_zoom		  (EphyWindow *window,
						   float zoom);

void		  ephy_window_activate_location	  (EphyWindow *window);
const char       *ephy_window_get_location        (EphyWindow *window);

gboolean          ephy_window_close               (EphyWindow *window);

EphyWindowChrome  ephy_window_get_chrome          (EphyWindow *window);

G_END_DECLS

#endif

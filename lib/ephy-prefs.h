/*
 *  Copyright (C) 2000-2002 Marco Pesenti Gritti
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

#ifndef EPHY_PREFS_H
#define EPHY_PREFS_H

G_BEGIN_DECLS

/* General */
#define CONF_GENERAL_HOMEPAGE "/apps/epiphany/general/start_page"

/* Interface */
#define CONF_TABS_TABBED "/apps/epiphany/interface/open_in_tab"
#define CONF_TABS_TABBED_AUTOJUMP "/apps/epiphany/interface/jumpto_tab"
#define CONF_WINDOWS_SHOW_TOOLBARS "/apps/epiphany/interface/show_toolbars"
#define CONF_WINDOWS_SHOW_BOOKMARKS_BAR "/apps/epiphany/interface/show_bookmarks_bar"
#define CONF_WINDOWS_SHOW_STATUSBAR "/apps/epiphany/interface/show_statusbar"
#define CONF_TOOLBAR_SPINNER_THEME "/apps/epiphany/interface/spinner_theme"
#define CONF_BOOKMARKS_SELECTED_NODE "/apps/epiphany/interface/bookmark_keyword_selected_node"
#define CONF_INTERFACE_MIDDLE_CLICK_OPEN_URL "/apps/epiphany/interface/middle_click_open_url"

/* Downloader */
#define CONF_DOWNLOADING_SHOW_DETAILS "/apps/epiphany/downloader/show_details"

/* Directories */
#define CONF_STATE_SAVE_DIR           "/apps/epiphany/directories/save"
#define CONF_STATE_SAVE_IMAGE_DIR     "/apps/epiphany/directories/saveimage"
#define CONF_STATE_OPEN_DIR           "/apps/epiphany/directories/open"
#define CONF_STATE_DOWNLOADING_DIR    "/apps/epiphany/directories/downloading"

/* System prefs */
#define CONF_DESKTOP_FTP_HANDLER "/desktop/gnome/url-handlers/ftp/command"
#define CONF_DESKTOP_TOOLBAR_STYLE "/desktop/gnome/interface/toolbar_style"

G_END_DECLS

#endif

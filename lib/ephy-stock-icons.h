/*
 *  Copyright © 2002 Jorn Baayen
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
 *  $Id$
 */

#ifndef EPHY_STOCK_ICONS_H
#define EPHY_STOCK_ICONS_H

G_BEGIN_DECLS

#define EPHY_STOCK_EPHY		   "web-browser"

/* Custom Epiphany named icons */
#define EPHY_STOCK_POPUPS	   "popup-hidden"
#define EPHY_STOCK_HISTORY         "history-view"
#define EPHY_STOCK_BOOKMARK        "bookmark-web"
#define EPHY_STOCK_BOOKMARKS       "bookmark-view"
#define EPHY_STOCK_ENTRY	   "location-entry"
#define EPHY_STOCK_DOWNLOAD	   "download"

/* Named icons defined in fd.o Icon Naming Spec */
#define STOCK_NEW_TAB              "tab-new"
#define STOCK_NEW_WINDOW           "window-new"
#define STOCK_SEND_MAIL            "mail-forward"
#define STOCK_ADD_BOOKMARK         "bookmark-new"
#define STOCK_PRINT_SETUP          "document-page-setup"

/* Deprecated named icons from g-i-t */
#define STOCK_LOCK_INSECURE        "stock_lock-open"
#define STOCK_LOCK_SECURE          "stock_lock"
#define STOCK_LOCK_BROKEN	   "stock_lock-broken"

void ephy_stock_icons_init (void);

G_END_DECLS

#endif /* EPHY_STOCK_ICONS_H */

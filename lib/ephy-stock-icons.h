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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_STOCK_ICONS_H
#define EPHY_STOCK_ICONS_H

G_BEGIN_DECLS

#define EPHY_STOCK_POPUPS	   "epiphany-popup-hidden"
#define EPHY_STOCK_HISTORY         "epiphany-history"
#define EPHY_STOCK_BOOKMARKS       "epiphany-bookmarks"
#define EPHY_STOCK_ENTRY	   "epiphany-entry"
#define EPHY_STOCK_DOWNLOAD	   "epiphany-download"

#define STOCK_NEW_TAB              "tab-new"
#define STOCK_NEW_WINDOW           "window-new"
#define STOCK_VIEW_SOURCE	   "stock_view-html-source"
#define STOCK_SEND_MAIL            "mail-forward"
#define STOCK_ADD_BOOKMARK         "bookmark-new"
#define STOCK_BOOKMARK             "stock_bookmark"
#define STOCK_PRINT_SETUP          "stock_print-setup" /*document-page-setup*/
#define STOCK_LOCK_INSECURE        "stock_lock-open"
#define STOCK_LOCK_SECURE          "stock_lock"
#define STOCK_LOCK_BROKEN	   "stock_lock-broken"
#define STOCK_DRAG_MODE	   	   "stock_drag-mode"

void ephy_stock_icons_init (void);

G_END_DECLS

#endif /* EPHY_STOCK_ICONS_H */

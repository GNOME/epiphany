/*
 *  Copyright © 2004  Tommi Komulainen
 *  Copyright © 2004, 2005  Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EPHY_FIND_TOOLBAR_H
#define EPHY_FIND_TOOLBAR_H

#include <gtk/gtk.h>

#include "ephy-web-view.h"

G_BEGIN_DECLS

#define EPHY_TYPE_FIND_TOOLBAR (ephy_find_toolbar_get_type ())

G_DECLARE_FINAL_TYPE (EphyFindToolbar, ephy_find_toolbar, EPHY, FIND_TOOLBAR, GtkSearchBar)

EphyFindToolbar *ephy_find_toolbar_new		 (WebKitWebView *web_view);

const char	*ephy_find_toolbar_get_text	 (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_find_next	 (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_find_previous (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_open		 (EphyFindToolbar *toolbar,
						  gboolean links_only,
						  gboolean clear_search);

void		 ephy_find_toolbar_close	 (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_request_close (EphyFindToolbar *toolbar);

void		 ephy_find_toolbar_toggle_state	 (EphyFindToolbar *toolbar);

G_END_DECLS

#endif /* EPHY_FIND_TOOLBAR_H */

/*
 *  Copyright © 2002 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2005, 2006 Peter A. Harvey
 *  Copyright © 2006 Christian Persch
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

#ifndef EPHY_BOOKMARK_PROPERTIES_H
#define EPHY_BOOKMARK_PROPERTIES_H

#include "ephy-bookmarks.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_BOOKMARK_PROPERTIES (ephy_bookmark_properties_get_type ())
G_DECLARE_FINAL_TYPE (EphyBookmarkProperties, ephy_bookmark_properties, EPHY, BOOKMARK_PROPERTIES, GtkDialog);

GtkWidget	*ephy_bookmark_properties_new		(EphyBookmarks *bookmarks,
							 EphyNode *bookmark,
							 gboolean creating);

EphyNode	*ephy_bookmark_properties_get_node	(EphyBookmarkProperties *properties);

G_END_DECLS

#endif /* EPHY_BOOKMARK_PROPERTIES_H */

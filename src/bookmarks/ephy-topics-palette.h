/*
 *  Copyright © 2002 Marco Pesenti Gritti <mpeseng@tin.it>
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

#ifndef EPHY_TOPICS_PALETTE_H
#define EPHY_TOPICS_PALETTE_H

#include "ephy-bookmarks.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TOPICS_PALETTE (ephy_topics_palette_get_type ())
G_DECLARE_FINAL_TYPE (EphyTopicsPalette, ephy_topics_palette, EPHY, TOPICS_PALETTE, GtkListStore);

enum
{
	EPHY_TOPICS_PALETTE_COLUMN_TITLE,
	EPHY_TOPICS_PALETTE_COLUMN_NODE,
	EPHY_TOPICS_PALETTE_COLUMN_SELECTED,
	EPHY_TOPICS_PALETTE_COLUMNS
};

EphyTopicsPalette *ephy_topics_palette_new         (EphyBookmarks     *bookmarks,
                                                    EphyNode          *bookmark);

void               ephy_topics_palette_update_list (EphyTopicsPalette *self);

G_END_DECLS

#endif /* EPHY_TOPICS_PALETTE_H */

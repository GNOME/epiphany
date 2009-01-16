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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_TOPICS_PALETTE_H
#define EPHY_TOPICS_PALETTE_H

#include "ephy-bookmarks.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TOPICS_PALETTE	 (ephy_topics_palette_get_type ())
#define EPHY_TOPICS_PALETTE(o)		 (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TOPICS_PALETTE, EphyTopicsPalette))
#define EPHY_TOPICS_PALETTE_CLASS(k)	 (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TOPICS_PALETTE, EphyTopicsPaletteClass))
#define EPHY_IS_TOPICS_PALETTE(o)	 (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TOPICS_PALETTE))
#define EPHY_IS_TOPICS_PALETTE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TOPICS_PALETTE))
#define EPHY_TOPICS_PALETTE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TOPICS_PALETTE, EphyTopicsPaletteClass))

typedef struct _EphyTopicsPalettePrivate EphyTopicsPalettePrivate;

typedef struct
{
	GtkTreeView parent;

	/*< private >*/
	EphyTopicsPalettePrivate *priv;
} EphyTopicsPalette;

typedef struct
{
	GtkTreeViewClass parent;
} EphyTopicsPaletteClass;

GType		     ephy_topics_palette_get_type  (void);

GtkWidget	    *ephy_topics_palette_new       (EphyBookmarks *bookmarks,
						    EphyNode *bookmark);

void                 ephy_topics_palette_new_topic (EphyTopicsPalette *palette);

G_END_DECLS

#endif /* EPHY_TOPICS_PALETTE_H */

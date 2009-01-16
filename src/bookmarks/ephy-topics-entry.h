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

#ifndef EPHY_TOPICS_ENTRY_H
#define EPHY_TOPICS_ENTRY_H

#include "ephy-bookmarks.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_TOPICS_ENTRY	 	(ephy_topics_entry_get_type ())
#define EPHY_TOPICS_ENTRY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_TOPICS_ENTRY, EphyTopicsEntry))
#define EPHY_TOPICS_ENTRY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_TOPICS_ENTRY, EphyTopicsEntryClass))
#define EPHY_IS_TOPICS_ENTRY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_TOPICS_ENTRY))
#define EPHY_IS_TOPICS_ENTRY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_TOPICS_ENTRY))
#define EPHY_TOPICS_ENTRY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_TOPICS_ENTRY, EphyTopicsEntryClass))

typedef struct _EphyTopicsEntryPrivate EphyTopicsEntryPrivate;

typedef struct
{
	GtkEntry parent;

	/*< private >*/
	EphyTopicsEntryPrivate *priv;
} EphyTopicsEntry;

typedef struct
{
	GtkEntryClass parent;
} EphyTopicsEntryClass;

GType		     ephy_topics_entry_get_type        (void);

GtkWidget	    *ephy_topics_entry_new             (EphyBookmarks *bookmarks,
							EphyNode *bookmark);

void                 ephy_topics_entry_insert_topic    (EphyTopicsEntry *entry, 
							const char *title);

G_END_DECLS

#endif /* EPHY_TOPICS_ENTRY_H */

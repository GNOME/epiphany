/*
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
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
 *  $Id$
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_SEARCH_ENTRY_H
#define EPHY_SEARCH_ENTRY_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SEARCH_ENTRY		(ephy_search_entry_get_type ())
#define EPHY_SEARCH_ENTRY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_SEARCH_ENTRY, EphySearchEntry))
#define EPHY_SEARCH_ENTRY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_SEARCH_ENTRY, EphySearchEntryClass))
#define EPHY_IS_SEARCH_ENTRY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_SEARCH_ENTRY))
#define EPHY_IS_SEARCH_ENTRY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_SEARCH_ENTRY))
#define EPHY_SEARCH_ENTRY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_SEARCH_ENTRY, EphySearchEntryClass))

typedef struct _EphySearchEntryClass	EphySearchEntryClass;
typedef struct _EphySearchEntry		EphySearchEntry;
typedef struct _EphySearchEntryPrivate	EphySearchEntryPrivate;

struct _EphySearchEntryClass
{
	GtkEntryClass parent;

	void (*search) (EphySearchEntry *view, const char *text);
};

struct _EphySearchEntry
{
	GtkEntry parent;

	/*< private >*/
	EphySearchEntryPrivate *priv;
};

GType            ephy_search_entry_get_type (void);

GtkWidget       *ephy_search_entry_new      (void);

void             ephy_search_entry_clear    (EphySearchEntry *entry);

G_END_DECLS

#endif /* __EPHY_SEARCH_ENTRY_H */

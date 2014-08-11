/*
 *  Copyright © 2002  Ricardo Fernández Pascual
 *  Copyright © 2003, 2004  Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005  Christian Persch
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
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_LOCATION_ENTRY_H
#define EPHY_LOCATION_ENTRY_H

#include <gtk/gtk.h>

#include "ephy-security-levels.h"

G_BEGIN_DECLS

#define EPHY_TYPE_LOCATION_ENTRY		(ephy_location_entry_get_type())
#define EPHY_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntry))
#define EPHY_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntryClass))
#define EPHY_IS_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_IS_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_LOCATION_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntryClass))

typedef struct _EphyLocationEntryClass		EphyLocationEntryClass;
typedef struct _EphyLocationEntry		EphyLocationEntry;
typedef struct _EphyLocationEntryPrivate	EphyLocationEntryPrivate;

struct _EphyLocationEntryClass
{
	GtkEntryClass parent_class;

	/* Signals */
	void   (* user_changed)	(EphyLocationEntry *entry);
	void   (* lock_clicked)	(EphyLocationEntry *entry);
	/* for getting the drag data */
	char * (* get_location)	(EphyLocationEntry *entry);
	char * (* get_title)	(EphyLocationEntry *entry);
};

struct _EphyLocationEntry
{
	GtkEntry parent_object;

	/*< private >*/
	EphyLocationEntryPrivate *priv;
};

GType		ephy_location_entry_get_type		(void);

GtkWidget      *ephy_location_entry_new			(void);

void		ephy_location_entry_set_completion	(EphyLocationEntry *entry,
							 GtkTreeModel *model,
							 guint text_col,
							 guint action_col,
							 guint keywords_col,
							 guint relevance_col,
							 guint url_col,
							 guint extra_col,
							 guint favicon_col);

void		ephy_location_entry_set_location	(EphyLocationEntry *entry,
							 const char *address);

void		ephy_location_entry_set_match_func	(EphyLocationEntry *entry, 
							 GtkEntryCompletionMatchFunc match_func,
							 gpointer user_data,
							 GDestroyNotify notify);
					
const char     *ephy_location_entry_get_location	(EphyLocationEntry *entry);

gboolean	ephy_location_entry_get_can_undo	(EphyLocationEntry *entry);

gboolean	ephy_location_entry_get_can_redo	(EphyLocationEntry *entry);

GSList         *ephy_location_entry_get_search_terms	(EphyLocationEntry *entry);

gboolean	ephy_location_entry_reset		(EphyLocationEntry *entry);

void		ephy_location_entry_undo_reset		(EphyLocationEntry *entry);

void		ephy_location_entry_activate		(EphyLocationEntry *entry);

void		ephy_location_entry_set_favicon		(EphyLocationEntry *entry,
							 GdkPixbuf *pixbuf);

void            ephy_location_entry_set_show_favicon    (EphyLocationEntry *entry,
							 gboolean show_favicon);

void		ephy_location_entry_set_security_level	(EphyLocationEntry *entry,
							 EphySecurityLevel security_level);

void		ephy_location_entry_set_lock_tooltip	(EphyLocationEntry *entry,
							 const char *tooltip);

G_END_DECLS

#endif

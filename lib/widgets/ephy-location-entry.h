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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_LOCATION_ENTRY_H
#define EPHY_LOCATION_ENTRY_H

#include "ephy-node.h"

#include <gtk/gtkwidget.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define EPHY_TYPE_LOCATION_ENTRY		(ephy_location_entry_get_type())
#define EPHY_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntry))
#define EPHY_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntryClass))
#define EPHY_IS_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_IS_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_LOCATION_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_LOCATION_ENTRY, EphyLocationEntryClass))

typedef struct _EphyLocationEntry		EphyLocationEntry;
typedef struct _EphyLocationEntryClass		EphyLocationEntryClass;
typedef struct _EphyLocationEntryPrivate	EphyLocationEntryPrivate;

struct _EphyLocationEntryClass
{
	GtkToolItemClass parent_class;

	/* Signals */
	void   (* user_changed)	(EphyLocationEntry *entry);
	void   (* lock_clicked)	(EphyLocationEntry *entry);
	/* for getting the drag data */
	char * (* get_location)	(EphyLocationEntry *entry);
	char * (* get_title)	(EphyLocationEntry *entry);
};

struct _EphyLocationEntry
{
	GtkToolItem parent_object;

	/*< private >*/
	EphyLocationEntryPrivate *priv;
};

GType		ephy_location_entry_get_type		(void);

GtkWidget      *ephy_location_entry_new			(void);

void		ephy_location_entry_set_completion	(EphyLocationEntry *le,
							 GtkTreeModel *model,
							 guint text_col,
							 guint action_col,
							 guint keywords_col,
							 guint relevance_col);

void		ephy_location_entry_set_location	(EphyLocationEntry *le,
							 const char *address,
							 const char *typed_address);

const char     *ephy_location_entry_get_location	(EphyLocationEntry *le);

gboolean	ephy_location_entry_reset		(EphyLocationEntry *entry);

void		ephy_location_entry_activate		(EphyLocationEntry *le);

GtkWidget      *ephy_location_entry_get_entry		(EphyLocationEntry *entry);

void		ephy_location_entry_set_favicon		(EphyLocationEntry *entry,
							 GdkPixbuf *pixbuf);

void		ephy_location_entry_set_secure		(EphyLocationEntry *entry,
							 gboolean secure);

void		ephy_location_entry_set_show_lock	(EphyLocationEntry *entry,
							 gboolean show_lock);

void		ephy_location_entry_set_lock_stock	(EphyLocationEntry *entry,
							 const char *stock_id);

void		ephy_location_entry_set_lock_tooltip	(EphyLocationEntry *entry,
							 const char *tooltip);

G_END_DECLS

#endif

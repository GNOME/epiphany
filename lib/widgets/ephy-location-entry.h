/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
 *  Copyright (C) 2003  Marco Pesenti Gritti
 *  Copyright (C) 2003  Christian Persch
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

#include <gtk/gtktoolitem.h>
#include <gtk/gtktreemodel.h>

#define EPHY_TYPE_LOCATION_ENTRY		(ephy_location_entry_get_type())
#define EPHY_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyLocationEntry))
#define EPHY_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyLocationEntryClass))
#define EPHY_IS_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_IS_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_LOCATION_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyLocationEntryClass))

typedef struct _EphyLocationEntry EphyLocationEntry;
typedef struct _EphyLocationEntryClass EphyLocationEntryClass;
typedef struct _EphyLocationEntryPrivate EphyLocationEntryPrivate;

struct _EphyLocationEntryClass
{
	GtkToolItemClass parent_class;

	void		(*user_changed)	(EphyLocationEntry *le);
};

struct _EphyLocationEntry
{
	GtkToolItem parent_object;

	EphyLocationEntryPrivate *priv;
};

GType			ephy_location_entry_get_type		(void);

GtkWidget              *ephy_location_entry_new			(void);

GtkWidget	       *ephy_location_entry_get_entry		(EphyLocationEntry *le);

void			ephy_location_entry_set_completion	(EphyLocationEntry *le,
								 GtkTreeModel *model,
								 guint text_col,
								 guint action_col,
								 guint keywords_col,
								 guint relevance_col);

void			ephy_location_entry_set_location	(EphyLocationEntry *le,
								 const gchar *new_location);

const char	       *ephy_location_entry_get_location	(EphyLocationEntry *le);

void			ephy_location_entry_activate		(EphyLocationEntry *le);

void			ephy_location_entry_clear_history	(EphyLocationEntry *le);

#endif

/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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
 */

#ifndef EPHY_LOCATION_ENTRY_H
#define EPHY_LOCATION_ENTRY_H

#include <glib-object.h>
#include <gtk/gtkhbox.h>

#include "ephy-autocompletion.h"

/* object forward declarations */

typedef struct _EphyLocationEntry EphyLocationEntry;
typedef struct _EphyLocationEntryClass EphyLocationEntryClass;
typedef struct _EphyLocationEntryPrivate EphyLocationEntryPrivate;

/**
 * EphyFolderTbWidget object
 */

#define EPHY_TYPE_LOCATION_ENTRY		(ephy_location_entry_get_type())
#define EPHY_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyLocationEntry))
#define EPHY_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyLocationEntryClass))
#define EPHY_IS_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_IS_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_LOCATION_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyLocationEntryClass))

struct _EphyLocationEntryClass
{
	GtkHBoxClass parent_class;

	/* signals */
	void		(*activated)	(EphyLocationEntry *w,
					 const char *content,
					 const char *target);
};

/* Remember: fields are public read-only */
struct _EphyLocationEntry
{
	GtkHBox parent_object;

	EphyLocationEntryPrivate *priv;
};

GType			ephy_location_entry_get_type		(void);
GtkWidget              *ephy_location_entry_new			(void);
void			ephy_location_entry_set_location	(EphyLocationEntry *w,
								 const gchar *new_location);
gchar		       *ephy_location_entry_get_location	(EphyLocationEntry *w);
void			ephy_location_entry_set_autocompletion  (EphyLocationEntry *w,
								 EphyAutocompletion *ac);
void			ephy_location_entry_activate		(EphyLocationEntry *w);
void			ephy_location_entry_clear_history	(EphyLocationEntry *w);

#endif

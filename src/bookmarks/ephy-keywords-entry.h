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

#ifndef EPHY_KEYWORDS_ENTRY_H
#define EPHY_KEYWORDS_ENTRY_H

#include "ephy-bookmarks.h"

#include <glib-object.h>
#include <gtk/gtkentry.h>

/* object forward declarations */

typedef struct _EphyKeywordsEntry EphyKeywordsEntry;
typedef struct _EphyKeywordsEntryClass EphyKeywordsEntryClass;
typedef struct _EphyKeywordsEntryPrivate EphyKeywordsEntryPrivate;

/**
 * EphyFolderTbWidget object
 */

#define EPHY_TYPE_LOCATION_ENTRY		(ephy_keywords_entry_get_type())
#define EPHY_KEYWORDS_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyKeywordsEntry))
#define EPHY_KEYWORDS_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyKeywordsEntryClass))
#define EPHY_IS_LOCATION_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_IS_LOCATION_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_LOCATION_ENTRY))
#define EPHY_KEYWORDS_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_LOCATION_ENTRY,\
						 EphyKeywordsEntryClass))

struct _EphyKeywordsEntryClass
{
	GtkEntryClass parent_class;

	void (* keywords_changed)  (EphyKeywordsEntry *entry);
};

struct _EphyKeywordsEntry
{
	GtkEntry parent_object;

	EphyKeywordsEntryPrivate *priv;
};

GType			ephy_keywords_entry_get_type		(void);

GtkWidget              *ephy_keywords_entry_new			(void);

void			ephy_keywords_entry_set_bookmarks	(EphyKeywordsEntry *w,
								 EphyBookmarks *bookmarks);

#endif

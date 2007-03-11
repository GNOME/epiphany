/*
 *  Copyright © 2003, 2004, 2005  Christian Persch
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 *  Adapted and modified from gtk+ code:
 *
 *  Copyright © 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *  Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 *  file in the gtk+ distribution for a list of people on the GTK+ Team.
 *  See the ChangeLog in the gtk+ distribution files for a list of changes.
 *  These files are distributed with GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 *
 *  $Id$
 */

#ifndef EPHY_ICON_ENTRY_H
#define EPHY_ICON_ENTRY_H

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ICON_ENTRY		(ephy_icon_entry_get_type())
#define EPHY_ICON_ENTRY(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_ICON_ENTRY, EphyIconEntry))
#define EPHY_ICON_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_ICON_ENTRY, EphyIconEntryClass))
#define EPHY_IS_ICON_ENTRY(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_ICON_ENTRY))
#define EPHY_IS_ICON_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_ICON_ENTRY))
#define EPHY_ICON_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_ICON_ENTRY, EphyIconEntryClass))

typedef struct _EphyIconEntryClass	EphyIconEntryClass;
typedef struct _EphyIconEntry		EphyIconEntry;
typedef struct _EphyIconEntryPrivate	EphyIconEntryPrivate;

struct _EphyIconEntryClass
{
	GtkBinClass parent_class;
};

struct _EphyIconEntry
{
	GtkBin parent_object;

	/*< public >*/
	GtkWidget *entry;

	/*< private >*/
	EphyIconEntryPrivate *priv;
};

GType		ephy_icon_entry_get_type	(void);

GtkWidget      *ephy_icon_entry_new		(void);

void		ephy_icon_entry_pack_widget	(EphyIconEntry *entry,
						 GtkWidget *widget,
						 gboolean start);

GtkWidget      *ephy_icon_entry_get_entry	(EphyIconEntry *entry);

G_END_DECLS

#endif

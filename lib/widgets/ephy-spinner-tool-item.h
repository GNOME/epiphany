/*
 *  Copyright © 2006 Christian Persch
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef EPHY_SPINNER_TOOL_ITEM_H
#define EPHY_SPINNER_TOOL_ITEM_H

#include <gtk/gtktoolitem.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SPINNER_TOOL_ITEM		(ephy_spinner_tool_item_get_type ())
#define EPHY_SPINNER_TOOL_ITEM(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_SPINNER_TOOL_ITEM, EphySpinnerToolItem))
#define EPHY_SPINNER_TOOL_ITEM_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_SPINNER_TOOL_ITEM, EphySpinnerToolItemClass))
#define EPHY_IS_SPINNER_TOOL_ITEM(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_SPINNER_TOOL_ITEM))
#define EPHY_IS_SPINNER_TOOL_ITEM_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_SPINNER_TOOL_ITEM))
#define EPHY_SPINNER_TOOL_ITEM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_SPINNER_TOOL_ITEM, EphySpinnerToolItemClass))

typedef struct _EphySpinnerToolItem		EphySpinnerToolItem;
typedef struct _EphySpinnerToolItemClass	EphySpinnerToolItemClass;
typedef struct _EphySpinnerToolItemDetails	EphySpinnerToolItemDetails;

struct _EphySpinnerToolItem
{
	GtkToolItem parent;

	/*< private >*/
	EphySpinnerToolItemDetails *details;
};

struct _EphySpinnerToolItemClass
{
	GtkToolItemClass parent_class;
};

GType		ephy_spinner_tool_item_get_type		(void);

GtkToolItem    *ephy_spinner_tool_item_new		(void);

void		ephy_spinner_tool_item_set_spinning	(EphySpinnerToolItem *item,
							 gboolean spinning);

G_END_DECLS

#endif /* !EPHY_SPINNER_TOOL_ITEM_H */
